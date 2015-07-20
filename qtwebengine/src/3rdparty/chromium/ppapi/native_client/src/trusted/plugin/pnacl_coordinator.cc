// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/pnacl_coordinator.h"

#include <algorithm>
#include <utility>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_uma_private.h"

#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/native_client/src/trusted/plugin/pnacl_translate_thread.h"
#include "ppapi/native_client/src/trusted/plugin/service_runtime.h"
#include "ppapi/native_client/src/trusted/plugin/temporary_file.h"

namespace plugin {

namespace {

const int32_t kSizeKBMin = 1;
const int32_t kSizeKBMax = 512*1024;       // very large .pexe / .nexe.
const uint32_t kSizeKBBuckets = 100;

const int32_t kRatioMin = 10;
const int32_t kRatioMax = 10*100;          // max of 10x difference.
const uint32_t kRatioBuckets = 100;

void HistogramSizeKB(pp::UMAPrivate& uma,
                     const std::string& name, int32_t kb) {
  if (kb < 0) return;
  uma.HistogramCustomCounts(name,
                            kb,
                            kSizeKBMin, kSizeKBMax,
                            kSizeKBBuckets);
}

void HistogramRatio(pp::UMAPrivate& uma,
                    const std::string& name, int64_t a, int64_t b) {
  if (a < 0 || b <= 0) return;
  uma.HistogramCustomCounts(name,
                            100 * a / b,
                            kRatioMin, kRatioMax,
                            kRatioBuckets);
}

std::string GetArchitectureAttributes(Plugin* plugin) {
  pp::Var attrs_var(pp::PASS_REF,
                    plugin->nacl_interface()->GetCpuFeatureAttrs());
  return attrs_var.AsString();
}

void DidCacheHit(void* user_data, PP_FileHandle nexe_file_handle) {
  PnaclCoordinator* coordinator = static_cast<PnaclCoordinator*>(user_data);
  coordinator->BitcodeStreamCacheHit(nexe_file_handle);
}

void DidCacheMiss(void* user_data, int64_t expected_pexe_size,
                  PP_FileHandle temp_nexe_file) {
  PnaclCoordinator* coordinator = static_cast<PnaclCoordinator*>(user_data);
  coordinator->BitcodeStreamCacheMiss(expected_pexe_size,
                                      temp_nexe_file);
}

void DidStreamData(void* user_data, const void* stream_data, int32_t length) {
  PnaclCoordinator* coordinator = static_cast<PnaclCoordinator*>(user_data);
  coordinator->BitcodeStreamGotData(stream_data, length);
}

void DidFinishStream(void* user_data, int32_t pp_error) {
  PnaclCoordinator* coordinator = static_cast<PnaclCoordinator*>(user_data);
  coordinator->BitcodeStreamDidFinish(pp_error);
}

PPP_PexeStreamHandler kPexeStreamHandler = {
  &DidCacheHit,
  &DidCacheMiss,
  &DidStreamData,
  &DidFinishStream
};

}  // namespace

PnaclCoordinator* PnaclCoordinator::BitcodeToNative(
    Plugin* plugin,
    const std::string& pexe_url,
    const PP_PNaClOptions& pnacl_options,
    const pp::CompletionCallback& translate_notify_callback) {
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeToNative (plugin=%p, pexe=%s)\n",
                 static_cast<void*>(plugin), pexe_url.c_str()));
  PnaclCoordinator* coordinator =
      new PnaclCoordinator(plugin, pexe_url,
                           pnacl_options,
                           translate_notify_callback);

  GetNaClInterface()->SetPNaClStartTime(plugin->pp_instance());
  int cpus = plugin->nacl_interface()->GetNumberOfProcessors();
  coordinator->split_module_count_ = std::min(4, std::max(1, cpus));

  // First start a network request for the pexe, to tickle the component
  // updater's On-Demand resource throttler, and to get Last-Modified/ETag
  // cache information. We can cancel the request later if there's
  // a bitcode->nexe cache hit.
  coordinator->OpenBitcodeStream();
  return coordinator;
}

PnaclCoordinator::PnaclCoordinator(
    Plugin* plugin,
    const std::string& pexe_url,
    const PP_PNaClOptions& pnacl_options,
    const pp::CompletionCallback& translate_notify_callback)
  : translate_finish_error_(PP_OK),
    plugin_(plugin),
    translate_notify_callback_(translate_notify_callback),
    translation_finished_reported_(false),
    pexe_url_(pexe_url),
    pnacl_options_(pnacl_options),
    architecture_attributes_(GetArchitectureAttributes(plugin)),
    split_module_count_(1),
    error_already_reported_(false),
    pexe_size_(0),
    pexe_bytes_compiled_(0),
    expected_pexe_size_(-1) {
  callback_factory_.Initialize(this);
}

PnaclCoordinator::~PnaclCoordinator() {
  PLUGIN_PRINTF(("PnaclCoordinator::~PnaclCoordinator (this=%p, "
                 "translate_thread=%p\n",
                 static_cast<void*>(this), translate_thread_.get()));
  // Stopping the translate thread will cause the translate thread to try to
  // run translation_complete_callback_ on the main thread.  This destructor is
  // running from the main thread, and by the time it exits, callback_factory_
  // will have been destroyed.  This will result in the cancellation of
  // translation_complete_callback_, so no notification will be delivered.
  if (translate_thread_.get() != NULL)
    translate_thread_->AbortSubprocesses();
  if (!translation_finished_reported_) {
    plugin_->nacl_interface()->ReportTranslationFinished(
        plugin_->pp_instance(),
        PP_FALSE, 0, 0, 0);
  }
  // Force deleting the translate_thread now. It must be deleted
  // before any scoped_* fields hanging off of PnaclCoordinator
  // since the thread may be accessing those fields.
  // It will also be accessing obj_files_.
  translate_thread_.reset(NULL);
  for (size_t i = 0; i < obj_files_.size(); i++)
    delete obj_files_[i];
}

PP_FileHandle PnaclCoordinator::TakeTranslatedFileHandle() {
  DCHECK(temp_nexe_file_ != NULL);
  return temp_nexe_file_->TakeFileHandle();
}

void PnaclCoordinator::ReportNonPpapiError(PP_NaClError err_code,
                                           const std::string& message) {
  ErrorInfo error_info;
  error_info.SetReport(err_code, message);
  plugin_->ReportLoadError(error_info);
  ExitWithError();
}

void PnaclCoordinator::ReportPpapiError(PP_NaClError err_code,
                                        int32_t pp_error,
                                        const std::string& message) {
  std::stringstream ss;
  ss << "PnaclCoordinator: " << message << " (pp_error=" << pp_error << ").";
  ErrorInfo error_info;
  error_info.SetReport(err_code, ss.str());
  plugin_->ReportLoadError(error_info);
  ExitWithError();
}

void PnaclCoordinator::ExitWithError() {
  PLUGIN_PRINTF(("PnaclCoordinator::ExitWithError\n"));
  // Free all the intermediate callbacks we ever created.
  // Note: this doesn't *cancel* the callbacks from the factories attached
  // to the various helper classes (e.g., pnacl_resources). Thus, those
  // callbacks may still run asynchronously.  We let those run but ignore
  // any other errors they may generate so that they do not end up running
  // translate_notify_callback_, which has already been freed.
  callback_factory_.CancelAll();
  if (!error_already_reported_) {
    error_already_reported_ = true;
    translation_finished_reported_ = true;
    plugin_->nacl_interface()->ReportTranslationFinished(
        plugin_->pp_instance(),
        PP_FALSE, 0, 0, 0);
    translate_notify_callback_.Run(PP_ERROR_FAILED);
  }
}

// Signal that Pnacl translation completed normally.
void PnaclCoordinator::TranslateFinished(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::TranslateFinished (pp_error=%"
                 NACL_PRId32 ")\n", pp_error));
  // Bail out if there was an earlier error (e.g., pexe load failure),
  // or if there is an error from the translation thread.
  if (translate_finish_error_ != PP_OK || pp_error != PP_OK) {
    plugin_->ReportLoadError(error_info_);
    ExitWithError();
    return;
  }

  // Send out one last progress event, to finish up the progress events
  // that were delayed (see the delay inserted in BitcodeGotCompiled).
  if (expected_pexe_size_ != -1) {
    pexe_bytes_compiled_ = expected_pexe_size_;
    GetNaClInterface()->DispatchEvent(plugin_->pp_instance(),
                                      PP_NACL_EVENT_PROGRESS,
                                      pexe_url_.c_str(),
                                      PP_TRUE,
                                      pexe_bytes_compiled_,
                                      expected_pexe_size_);
  }
  struct nacl_abi_stat stbuf;
  struct NaClDesc* desc = temp_nexe_file_->read_wrapper()->desc();
  if (0 == (*((struct NaClDescVtbl const *)desc->base.vtbl)->Fstat)(desc,
                                                                    &stbuf)) {
    size_t nexe_size = stbuf.nacl_abi_st_size;
    HistogramSizeKB(plugin_->uma_interface(),
                    "NaCl.Perf.Size.PNaClTranslatedNexe",
                    static_cast<int64_t>(nexe_size / 1024));
    HistogramRatio(plugin_->uma_interface(),
                   "NaCl.Perf.Size.PexeNexeSizePct", pexe_size_, nexe_size);
  }
  // The nexe is written to the temp_nexe_file_.  We must Reset() the file
  // pointer to be able to read it again from the beginning.
  temp_nexe_file_->Reset();

  // Report to the browser that translation finished. The browser will take
  // care of storing the nexe in the cache.
  translation_finished_reported_ = true;
  plugin_->nacl_interface()->ReportTranslationFinished(
      plugin_->pp_instance(), PP_TRUE, pnacl_options_.opt_level,
      pexe_size_, translate_thread_->GetCompileTime());

  NexeReadDidOpen(PP_OK);
}

void PnaclCoordinator::NexeReadDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::NexeReadDidOpen (pp_error=%"
                 NACL_PRId32 ")\n", pp_error));
  if (pp_error != PP_OK) {
    if (pp_error == PP_ERROR_FILENOTFOUND) {
      ReportPpapiError(PP_NACL_ERROR_PNACL_CACHE_FETCH_NOTFOUND,
                       pp_error,
                       "Failed to open translated nexe (not found).");
      return;
    }
    if (pp_error == PP_ERROR_NOACCESS) {
      ReportPpapiError(PP_NACL_ERROR_PNACL_CACHE_FETCH_NOACCESS,
                       pp_error,
                       "Failed to open translated nexe (no access).");
      return;
    }
    ReportPpapiError(PP_NACL_ERROR_PNACL_CACHE_FETCH_OTHER,
                     pp_error,
                     "Failed to open translated nexe.");
    return;
  }

  translate_notify_callback_.Run(PP_OK);
}

void PnaclCoordinator::OpenBitcodeStream() {
  // Even though we haven't started downloading, create the translation
  // thread object immediately. This ensures that any pieces of the file
  // that get downloaded before the compilation thread is accepting
  // SRPCs won't get dropped.
  translate_thread_.reset(new PnaclTranslateThread());
  if (translate_thread_ == NULL) {
    ReportNonPpapiError(
        PP_NACL_ERROR_PNACL_THREAD_CREATE,
        "PnaclCoordinator: could not allocate translation thread.");
    return;
  }

  GetNaClInterface()->StreamPexe(plugin_->pp_instance(),
                                 pexe_url_.c_str(),
                                 pnacl_options_.opt_level,
                                 &kPexeStreamHandler,
                                 this);
}

void PnaclCoordinator::BitcodeStreamCacheHit(PP_FileHandle handle) {
  if (handle == PP_kInvalidFileHandle) {
    ReportNonPpapiError(
        PP_NACL_ERROR_PNACL_CREATE_TEMP,
        std::string(
            "PnaclCoordinator: Got bad temp file handle from GetNexeFd"));
    BitcodeStreamDidFinish(PP_ERROR_FAILED);
    return;
  }
  temp_nexe_file_.reset(new TempFile(plugin_, handle));
  // Open it for reading as the cached nexe file.
  NexeReadDidOpen(temp_nexe_file_->Open(false));
}

void PnaclCoordinator::BitcodeStreamCacheMiss(int64_t expected_pexe_size,
                                              PP_FileHandle nexe_handle) {
  // IMPORTANT: Make sure that PnaclResources::StartLoad() is only
  // called after you receive a response to a request for a .pexe file.
  //
  // The component updater's resource throttles + OnDemand update/install
  // should block the URL request until the compiler is present. Now we
  // can load the resources (e.g. llc and ld nexes).
  resources_.reset(new PnaclResources(plugin_));
  CHECK(resources_ != NULL);

  // The first step of loading resources: read the resource info file.
  if (!resources_->ReadResourceInfo()) {
    ExitWithError();
    return;
  }

  // Second step of loading resources: call StartLoad to load pnacl-llc
  // and pnacl-ld, based on the filenames found in the resource info file.
  if (!resources_->StartLoad()) {
    ReportNonPpapiError(
        PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
        std::string("The Portable Native Client (pnacl) component is not "
                     "installed. Please consult chrome://components for more "
                      "information."));
    return;
  }

  expected_pexe_size_ = expected_pexe_size;

  for (int i = 0; i < split_module_count_; i++) {
    PP_FileHandle obj_handle =
        plugin_->nacl_interface()->CreateTemporaryFile(plugin_->pp_instance());
    nacl::scoped_ptr<TempFile> temp_file(new TempFile(plugin_, obj_handle));
    int32_t pp_error = temp_file->Open(true);
    if (pp_error != PP_OK) {
      ReportPpapiError(PP_NACL_ERROR_PNACL_CREATE_TEMP,
                       pp_error,
                       "Failed to open scratch object file.");
      return;
    } else {
      obj_files_.push_back(temp_file.release());
    }
  }
  invalid_desc_wrapper_.reset(plugin_->wrapper_factory()->MakeInvalid());

  temp_nexe_file_.reset(new TempFile(plugin_, nexe_handle));
  // Open the nexe file for connecting ld and sel_ldr.
  // Start translation when done with this last step of setup!
  RunTranslate(temp_nexe_file_->Open(true));
}

void PnaclCoordinator::BitcodeStreamGotData(const void* data, int32_t length) {
  DCHECK(translate_thread_.get());

  translate_thread_->PutBytes(data, length);
  if (data && length > 0)
    pexe_size_ += length;
}

void PnaclCoordinator::BitcodeStreamDidFinish(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::BitcodeStreamDidFinish (pp_error=%"
                 NACL_PRId32 ")\n", pp_error));
  if (pp_error != PP_OK) {
    // Defer reporting the error and cleanup until after the translation
    // thread returns, because it may be accessing the coordinator's
    // objects or writing to the files.
    translate_finish_error_ = pp_error;
    if (pp_error == PP_ERROR_ABORTED) {
      error_info_.SetReport(PP_NACL_ERROR_PNACL_PEXE_FETCH_ABORTED,
                            "PnaclCoordinator: pexe load failed (aborted).");
    }
    if (pp_error == PP_ERROR_NOACCESS) {
      error_info_.SetReport(PP_NACL_ERROR_PNACL_PEXE_FETCH_NOACCESS,
                            "PnaclCoordinator: pexe load failed (no access).");
    } else {
      std::stringstream ss;
      ss << "PnaclCoordinator: pexe load failed (pp_error=" << pp_error << ").";
      error_info_.SetReport(PP_NACL_ERROR_PNACL_PEXE_FETCH_OTHER, ss.str());
    }

    if (translate_thread_->started())
      translate_thread_->AbortSubprocesses();
    else
      TranslateFinished(pp_error);
  } else {
    // Compare download completion pct (100% now), to compile completion pct.
    HistogramRatio(plugin_->uma_interface(),
                   "NaCl.Perf.PNaClLoadTime.PctCompiledWhenFullyDownloaded",
                   pexe_bytes_compiled_, pexe_size_);
    translate_thread_->EndStream();
  }
}

void PnaclCoordinator::BitcodeGotCompiled(int32_t pp_error,
                                          int64_t bytes_compiled) {
  DCHECK(pp_error == PP_OK);
  pexe_bytes_compiled_ += bytes_compiled;
  // Hold off reporting the last few bytes of progress, since we don't know
  // when they are actually completely compiled.  "bytes_compiled" only means
  // that bytes were sent to the compiler.
  if (expected_pexe_size_ != -1) {
    if (!ShouldDelayProgressEvent()) {
      GetNaClInterface()->DispatchEvent(plugin_->pp_instance(),
                                        PP_NACL_EVENT_PROGRESS,
                                        pexe_url_.c_str(),
                                        PP_TRUE,
                                        pexe_bytes_compiled_,
                                        expected_pexe_size_);
    }
  } else {
    GetNaClInterface()->DispatchEvent(plugin_->pp_instance(),
                                      PP_NACL_EVENT_PROGRESS,
                                      pexe_url_.c_str(),
                                      PP_FALSE,
                                      pexe_bytes_compiled_,
                                      expected_pexe_size_);
  }
}

pp::CompletionCallback PnaclCoordinator::GetCompileProgressCallback(
    int64_t bytes_compiled) {
  return callback_factory_.NewCallback(&PnaclCoordinator::BitcodeGotCompiled,
                                       bytes_compiled);
}

void PnaclCoordinator::GetCurrentProgress(int64_t* bytes_loaded,
                                          int64_t* bytes_total) {
  *bytes_loaded = pexe_bytes_compiled_;
  *bytes_total = expected_pexe_size_;
}

void PnaclCoordinator::RunTranslate(int32_t pp_error) {
  PLUGIN_PRINTF(("PnaclCoordinator::RunTranslate (pp_error=%"
                 NACL_PRId32 ")\n", pp_error));
  // Invoke llc followed by ld off the main thread.  This allows use of
  // blocking RPCs that would otherwise block the JavaScript main thread.
  pp::CompletionCallback report_translate_finished =
      callback_factory_.NewCallback(&PnaclCoordinator::TranslateFinished);

  CHECK(translate_thread_ != NULL);
  translate_thread_->RunTranslate(report_translate_finished,
                                  &obj_files_,
                                  temp_nexe_file_.get(),
                                  invalid_desc_wrapper_.get(),
                                  &error_info_,
                                  resources_.get(),
                                  &pnacl_options_,
                                  architecture_attributes_,
                                  this,
                                  plugin_);
}

}  // namespace plugin
