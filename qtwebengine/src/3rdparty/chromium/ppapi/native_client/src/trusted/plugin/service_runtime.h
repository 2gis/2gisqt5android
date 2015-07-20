/* -*- c++ -*- */
/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A class containing information regarding a socket connection to a
// service runtime instance.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/reverse_service/reverse_service.h"
#include "native_client/src/trusted/weak_ref/weak_ref.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

struct NaClFileInfo;

namespace plugin {

class ErrorInfo;
class Plugin;
class SelLdrLauncherChrome;
class SrpcClient;
class ServiceRuntime;

// Struct of params used by StartSelLdr.  Use a struct so that callback
// creation templates aren't overwhelmed with too many parameters.
struct SelLdrStartParams {
  SelLdrStartParams(const std::string& url,
                    const PP_NaClFileInfo& file_info,
                    PP_NaClAppProcessType process_type)
      : url(url),
        file_info(file_info),
        process_type(process_type) {
  }
  std::string url;
  PP_NaClFileInfo file_info;
  PP_NaClAppProcessType process_type;
};

// Callback resources are essentially our continuation state.
struct OpenManifestEntryResource {
 public:
  OpenManifestEntryResource(const std::string& target_url,
                            struct NaClFileInfo* finfo,
                            bool* op_complete)
      : url(target_url),
        file_info(finfo),
        op_complete_ptr(op_complete) {}
  ~OpenManifestEntryResource();

  std::string url;
  struct NaClFileInfo* file_info;
  PP_NaClFileInfo pp_file_info;
  bool* op_complete_ptr;
};

// Do not invoke from the main thread, since the main methods will
// invoke CallOnMainThread and then wait on a condvar for the task to
// complete: if invoked from the main thread, the main method not
// returning (and thus unblocking the main thread) means that the
// main-thread continuation methods will never get called, and thus
// we'd get a deadlock.
class PluginReverseInterface: public nacl::ReverseInterface {
 public:
  PluginReverseInterface(nacl::WeakRefAnchor* anchor,
                         PP_Instance pp_instance,
                         ServiceRuntime* service_runtime,
                         pp::CompletionCallback init_done_cb);

  virtual ~PluginReverseInterface();

  void ShutDown();

  virtual void DoPostMessage(std::string message);

  virtual void StartupInitializationComplete();

  virtual bool OpenManifestEntry(std::string url_key,
                                 struct NaClFileInfo *info);

  virtual void ReportCrash();

  virtual void ReportExitStatus(int exit_status);

  // TODO(teravest): Remove this method once it's gone from
  // nacl::ReverseInterface.
  virtual int64_t RequestQuotaForWrite(std::string file_id,
                                       int64_t offset,
                                       int64_t bytes_to_write);

 protected:
  virtual void OpenManifestEntry_MainThreadContinuation(
      OpenManifestEntryResource* p,
      int32_t err);

  virtual void StreamAsFile_MainThreadContinuation(
      OpenManifestEntryResource* p,
      int32_t result);

 private:
  nacl::WeakRefAnchor* anchor_;  // holds a ref
  // Should be used only in main thread in WeakRef-protected callbacks.
  PP_Instance pp_instance_;
  ServiceRuntime* service_runtime_;
  NaClMutex mu_;
  NaClCondVar cv_;
  bool shutting_down_;

  pp::CompletionCallback init_done_cb_;
};

//  ServiceRuntime abstracts a NativeClient sel_ldr instance.
class ServiceRuntime {
 public:
  ServiceRuntime(Plugin* plugin,
                 PP_Instance pp_instance,
                 bool main_service_runtime,
                 bool uses_nonsfi_mode,
                 pp::CompletionCallback init_done_cb);
  // The destructor terminates the sel_ldr process.
  ~ServiceRuntime();

  // Spawn the sel_ldr instance.
  void StartSelLdr(const SelLdrStartParams& params,
                   pp::CompletionCallback callback);

  // If starting sel_ldr from a background thread, wait for sel_ldr to
  // actually start. Returns |false| if timed out waiting for the process
  // to start. Otherwise, returns |true| if StartSelLdr is complete
  // (either successfully or unsuccessfully).
  bool WaitForSelLdrStart();

  // Signal to waiting threads that StartSelLdr is complete (either
  // successfully or unsuccessfully).
  void SignalStartSelLdrDone();

  // If starting the nexe from a background thread, wait for the nexe to
  // actually start. Returns |true| is the nexe started successfully.
  bool WaitForNexeStart();

  // Signal to waiting threads that LoadNexeAndStart is complete (either
  // successfully or unsuccessfully).
  void SignalNexeStarted(bool ok);

  // Establish an SrpcClient to the sel_ldr instance and start the nexe.
  // This function must be called on the main thread.
  // This function must only be called once.
  void StartNexe();

  // Starts the application channel to the nexe.
  SrpcClient* SetupAppChannel();

  bool RemoteLog(int severity, const std::string& msg);
  Plugin* plugin() const { return plugin_; }
  void Shutdown();

  bool main_service_runtime() const { return main_service_runtime_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ServiceRuntime);
  bool StartNexeInternal();

  bool SetupCommandChannel();
  bool InitReverseService();
  bool StartModule();
  void ReapLogs();

  void ReportLoadError(const ErrorInfo& error_info);

  NaClSrpcChannel command_channel_;
  Plugin* plugin_;
  PP_Instance pp_instance_;
  bool main_service_runtime_;
  bool uses_nonsfi_mode_;
  nacl::ReverseService* reverse_service_;
  nacl::scoped_ptr<SelLdrLauncherChrome> subprocess_;

  nacl::WeakRefAnchor* anchor_;

  PluginReverseInterface* rev_interface_;

  // Mutex and CondVar to protect start_sel_ldr_done_ and nexe_started_.
  NaClMutex mu_;
  NaClCondVar cond_;
  bool start_sel_ldr_done_;
  bool start_nexe_done_;
  bool nexe_started_ok_;

  NaClHandle bootstrap_channel_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
