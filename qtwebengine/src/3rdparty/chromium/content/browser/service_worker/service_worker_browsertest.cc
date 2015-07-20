// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/common/blob/blob_data.h"

namespace content {

namespace {

struct FetchResult {
  ServiceWorkerStatusCode status;
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  scoped_ptr<storage::BlobDataHandle> blob_data_handle;
};

void RunAndQuit(const base::Closure& closure,
                const base::Closure& quit,
                base::MessageLoopProxy* original_message_loop) {
  closure.Run();
  original_message_loop->PostTask(FROM_HERE, quit);
}

void RunOnIOThread(const base::Closure& closure) {
  base::RunLoop run_loop;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RunAndQuit, closure, run_loop.QuitClosure(),
                 base::MessageLoopProxy::current()));
  run_loop.Run();
}

void RunOnIOThread(
    const base::Callback<void(const base::Closure& continuation)>& closure) {
  base::RunLoop run_loop;
  base::Closure quit_on_original_thread =
      base::Bind(base::IgnoreResult(&base::MessageLoopProxy::PostTask),
                 base::MessageLoopProxy::current().get(),
                 FROM_HERE,
                 run_loop.QuitClosure());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(closure, quit_on_original_thread));
  run_loop.Run();
}

void ReceivePrepareResult(bool* is_prepared) {
  *is_prepared = true;
}

base::Closure CreatePrepareReceiver(bool* is_prepared) {
  return base::Bind(&ReceivePrepareResult, is_prepared);
}

// Contrary to the style guide, the output parameter of this function comes
// before input parameters so Bind can be used on it to create a FetchCallback
// to pass to DispatchFetchEvent.
void ReceiveFetchResult(BrowserThread::ID run_quit_thread,
                        const base::Closure& quit,
                        ChromeBlobStorageContext* blob_context,
                        FetchResult* out_result,
                        ServiceWorkerStatusCode actual_status,
                        ServiceWorkerFetchEventResult actual_result,
                        const ServiceWorkerResponse& actual_response) {
  out_result->status = actual_status;
  out_result->result = actual_result;
  out_result->response = actual_response;
  if (!actual_response.blob_uuid.empty()) {
    out_result->blob_data_handle =
        blob_context->context()->GetBlobDataFromUUID(
            actual_response.blob_uuid);
  }
  if (!quit.is_null())
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit);
}

ServiceWorkerVersion::FetchCallback CreateResponseReceiver(
    BrowserThread::ID run_quit_thread,
    const base::Closure& quit,
    ChromeBlobStorageContext* blob_context,
    FetchResult* result) {
  return base::Bind(&ReceiveFetchResult, run_quit_thread, quit,
                    make_scoped_refptr<ChromeBlobStorageContext>(blob_context),
                    result);
}

void ReadResponseBody(std::string* body,
                      storage::BlobDataHandle* blob_data_handle) {
  ASSERT_TRUE(blob_data_handle);
  ASSERT_EQ(1U, blob_data_handle->data()->items().size());
  *body = std::string(blob_data_handle->data()->items()[0].bytes(),
                      blob_data_handle->data()->items()[0].length());
}

void ExpectResultAndRun(bool expected,
                        const base::Closure& continuation,
                        bool actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

class WorkerActivatedObserver
    : public ServiceWorkerContextObserver,
      public base::RefCountedThreadSafe<WorkerActivatedObserver> {
 public:
  explicit WorkerActivatedObserver(ServiceWorkerContextWrapper* context)
      : context_(context) {}
  void Init() {
    RunOnIOThread(base::Bind(&WorkerActivatedObserver::InitOnIOThread, this));
  }
  // ServiceWorkerContextObserver overrides.
  void OnVersionStateChanged(int64 version_id) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    const ServiceWorkerVersion* version =
        context_->context()->GetLiveVersion(version_id);
    if (version->status() == ServiceWorkerVersion::ACTIVATED) {
      context_->RemoveObserver(this);
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(&WorkerActivatedObserver::Quit, this));
    }
  }
  void Wait() { run_loop_.Run(); }

 private:
  friend class base::RefCountedThreadSafe<WorkerActivatedObserver>;
  ~WorkerActivatedObserver() override {}
  void InitOnIOThread() { context_->AddObserver(this); }
  void Quit() { run_loop_.Quit(); }

  base::RunLoop run_loop_;
  ServiceWorkerContextWrapper* context_;
  DISALLOW_COPY_AND_ASSIGN(WorkerActivatedObserver);
};

scoped_ptr<net::test_server::HttpResponse> VerifyServiceWorkerHeaderInRequest(
    const net::test_server::HttpRequest& request) {
  EXPECT_EQ(request.relative_url, "/service_worker/generated_sw.js");
  std::map<std::string, std::string>::const_iterator it =
      request.headers.find("Service-Worker");
  EXPECT_TRUE(it != request.headers.end());
  EXPECT_EQ("script", it->second);

  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_content_type("text/javascript");
  return http_response.Pass();
}

// The ImportsBustMemcache test requires that the imported script
// would naturally be cached in blink's memcache, but the embedded
// test server doesn't produce headers that allow the blink's memcache
// to do that. This interceptor injects headers that give the import
// an experiration far in the future.
class LongLivedResourceInterceptor : public net::URLRequestInterceptor {
 public:
  LongLivedResourceInterceptor(const std::string& body)
      : body_(body) {}
  ~LongLivedResourceInterceptor() override {}

  // net::URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    const char kHeaders[] =
        "HTTP/1.1 200 OK\0"
        "Content-Type: text/javascript\0"
        "Expires: Thu, 1 Jan 2100 20:00:00 GMT\0"
        "\0";
    std::string headers(kHeaders, arraysize(kHeaders));
    return new net::URLRequestTestJob(
        request, network_delegate, headers, body_, true);
  }

 private:
  std::string body_;
  DISALLOW_COPY_AND_ASSIGN(LongLivedResourceInterceptor);
};

void CreateLongLivedResourceInterceptors(
    const GURL& worker_url, const GURL& import_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_ptr<net::URLRequestInterceptor> interceptor;

  interceptor.reset(new LongLivedResourceInterceptor(
      "importScripts('long_lived_import.js');"));
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      worker_url, interceptor.Pass());

  interceptor.reset(new LongLivedResourceInterceptor(
      "// the imported script does nothing"));
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      import_url, interceptor.Pass());
}

void CountScriptResources(
    ServiceWorkerContextWrapper* wrapper,
    const GURL& scope,
    int* num_resources) {
  *num_resources = -1;

  std::vector<ServiceWorkerRegistrationInfo> infos =
     wrapper->context()->GetAllLiveRegistrationInfo();
  if (infos.empty())
    return;

  int version_id;
  size_t index = infos.size() - 1;
  if (infos[index].installing_version.version_id !=
      kInvalidServiceWorkerVersionId)
    version_id = infos[index].installing_version.version_id;
  else if (infos[index].waiting_version.version_id !=
           kInvalidServiceWorkerVersionId)
    version_id = infos[1].waiting_version.version_id;
  else if (infos[index].active_version.version_id !=
           kInvalidServiceWorkerVersionId)
    version_id = infos[index].active_version.version_id;
  else
    return;

  ServiceWorkerVersion* version =
      wrapper->context()->GetLiveVersion(version_id);
  *num_resources = static_cast<int>(version->script_cache_map()->size());
}

}  // namespace

class ServiceWorkerBrowserTest : public ContentBrowserTest {
 protected:
  typedef ServiceWorkerBrowserTest self;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    StoragePartition* partition = BrowserContext::GetDefaultStoragePartition(
        shell()->web_contents()->GetBrowserContext());
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());

    // Navigate to the page to set up a renderer page (where we can embed
    // a worker).
    NavigateToURLBlockUntilNavigationsComplete(
        shell(),
        embedded_test_server()->GetURL("/service_worker/empty.html"), 1);

    RunOnIOThread(base::Bind(&self::SetUpOnIOThread, this));
  }

  void TearDownOnMainThread() override {
    RunOnIOThread(base::Bind(&self::TearDownOnIOThread, this));
    wrapper_ = NULL;
  }

  virtual void SetUpOnIOThread() {}
  virtual void TearDownOnIOThread() {}

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

  void AssociateRendererProcessToPattern(const GURL& pattern) {
    wrapper_->process_manager()->AddProcessReferenceToPattern(
        pattern, shell()->web_contents()->GetRenderProcessHost()->GetID());
  }

 private:
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

class EmbeddedWorkerBrowserTest : public ServiceWorkerBrowserTest,
                                  public EmbeddedWorkerInstance::Listener {
 public:
  typedef EmbeddedWorkerBrowserTest self;

  EmbeddedWorkerBrowserTest()
      : last_worker_status_(EmbeddedWorkerInstance::STOPPED),
        pause_mode_(DONT_PAUSE) {}
  ~EmbeddedWorkerBrowserTest() override {}

  void TearDownOnIOThread() override {
    if (worker_) {
      worker_->RemoveListener(this);
      worker_.reset();
    }
  }

  void StartOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    worker_ = wrapper()->context()->embedded_worker_registry()->CreateWorker();
    EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker_->status());
    worker_->AddListener(this);


    const int64 service_worker_version_id = 33L;
    const GURL pattern = embedded_test_server()->GetURL("/");
    const GURL script_url = embedded_test_server()->GetURL(
        "/service_worker/worker.js");
    AssociateRendererProcessToPattern(pattern);
    int process_id = shell()->web_contents()->GetRenderProcessHost()->GetID();
    wrapper()->process_manager()->AddProcessReferenceToPattern(
        pattern, process_id);
    worker_->Start(
        service_worker_version_id,
        pattern,
        script_url,
        pause_mode_ != DONT_PAUSE,
        base::Bind(&EmbeddedWorkerBrowserTest::StartOnIOThread2, this));
  }
  void StartOnIOThread2(ServiceWorkerStatusCode status) {
    last_worker_status_ = worker_->status();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
    EXPECT_EQ(EmbeddedWorkerInstance::STARTING, last_worker_status_);

    if (status != SERVICE_WORKER_OK && !done_closure_.is_null())
      done_closure_.Run();
  }

  void StopOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker_->status());

    ServiceWorkerStatusCode status = worker_->Stop();

    last_worker_status_ = worker_->status();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
    EXPECT_EQ(EmbeddedWorkerInstance::STOPPING, last_worker_status_);

    if (status != SERVICE_WORKER_OK && !done_closure_.is_null())
      done_closure_.Run();
  }

 protected:
  // EmbeddedWorkerInstance::Observer overrides:
  void OnStarted() override {
    ASSERT_TRUE(worker_ != NULL);
    ASSERT_FALSE(done_closure_.is_null());
    last_worker_status_ = worker_->status();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done_closure_);
  }
  void OnStopped() override {
    ASSERT_TRUE(worker_ != NULL);
    ASSERT_FALSE(done_closure_.is_null());
    last_worker_status_ = worker_->status();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done_closure_);
  }
  void OnPausedAfterDownload() override {
    if (pause_mode_ == PAUSE_THEN_RESUME)
      worker_->ResumeAfterDownload();
    else if (pause_mode_ == PAUSE_THEN_STOP)
      worker_->Stop();
    else
      ASSERT_TRUE(false);
  }
  void OnReportException(const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url) override {}
  void OnReportConsoleMessage(int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override {}
  bool OnMessageReceived(const IPC::Message& message) override { return false; }

  scoped_ptr<EmbeddedWorkerInstance> worker_;
  EmbeddedWorkerInstance::Status last_worker_status_;

  enum {
    DONT_PAUSE,
    PAUSE_THEN_RESUME,
    PAUSE_THEN_STOP,
  } pause_mode_;

  // Called by EmbeddedWorkerInstance::Observer overrides so that
  // test code can wait for the worker status notifications.
  base::Closure done_closure_;
};

class ServiceWorkerVersionBrowserTest : public ServiceWorkerBrowserTest {
 public:
  typedef ServiceWorkerVersionBrowserTest self;

  ~ServiceWorkerVersionBrowserTest() override {}

  void TearDownOnIOThread() override {
    registration_ = NULL;
    version_ = NULL;
  }

  void InstallTestHelper(const std::string& worker_url,
                         ServiceWorkerStatusCode expected_status) {
    RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                             worker_url));

    // Dispatch install on a worker.
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop install_run_loop;
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&self::InstallOnIOThread, this,
                                       install_run_loop.QuitClosure(),
                                       &status));
    install_run_loop.Run();
    ASSERT_EQ(expected_status, status);

    // Stop the worker.
    status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop stop_run_loop;
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&self::StopOnIOThread, this,
                                       stop_run_loop.QuitClosure(),
                                       &status));
    stop_run_loop.Run();
    ASSERT_EQ(SERVICE_WORKER_OK, status);
  }

  void ActivateTestHelper(
      const std::string& worker_url,
      ServiceWorkerStatusCode expected_status) {
    RunOnIOThread(
        base::Bind(&self::SetUpRegistrationOnIOThread, this, worker_url));
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop run_loop;
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            &self::ActivateOnIOThread, this, run_loop.QuitClosure(), &status));
    run_loop.Run();
    ASSERT_EQ(expected_status, status);
  }

  void FetchOnRegisteredWorker(
      ServiceWorkerFetchEventResult* result,
      ServiceWorkerResponse* response,
      scoped_ptr<storage::BlobDataHandle>* blob_data_handle) {
    blob_context_ = ChromeBlobStorageContext::GetFor(
        shell()->web_contents()->GetBrowserContext());
    bool prepare_result = false;
    FetchResult fetch_result;
    fetch_result.status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop fetch_run_loop;
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(&self::FetchOnIOThread,
                                       this,
                                       fetch_run_loop.QuitClosure(),
                                       &prepare_result,
                                       &fetch_result));
    fetch_run_loop.Run();
    ASSERT_TRUE(prepare_result);
    *result = fetch_result.result;
    *response = fetch_result.response;
    *blob_data_handle = fetch_result.blob_data_handle.Pass();
    ASSERT_EQ(SERVICE_WORKER_OK, fetch_result.status);
  }

  void FetchTestHelper(const std::string& worker_url,
                       ServiceWorkerFetchEventResult* result,
                       ServiceWorkerResponse* response,
                       scoped_ptr<storage::BlobDataHandle>* blob_data_handle) {
    RunOnIOThread(
        base::Bind(&self::SetUpRegistrationOnIOThread, this, worker_url));
    FetchOnRegisteredWorker(result, response, blob_data_handle);
  }

  void SetUpRegistrationOnIOThread(const std::string& worker_url) {
    const GURL pattern = embedded_test_server()->GetURL("/");
    registration_ = new ServiceWorkerRegistration(
        pattern,
        wrapper()->context()->storage()->NewRegistrationId(),
        wrapper()->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(),
        embedded_test_server()->GetURL(worker_url),
        wrapper()->context()->storage()->NewVersionId(),
        wrapper()->context()->AsWeakPtr());
    AssociateRendererProcessToPattern(pattern);
  }

  void StartOnIOThread(const base::Closure& done,
                       ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->StartWorker(CreateReceiver(BrowserThread::UI, done, result));
  }

  void InstallOnIOThread(const base::Closure& done,
                         ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SetStatus(ServiceWorkerVersion::INSTALLING);
    version_->DispatchInstallEvent(
        -1, CreateReceiver(BrowserThread::UI, done, result));
  }

  void ActivateOnIOThread(const base::Closure& done,
                          ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
    version_->DispatchActivateEvent(
        CreateReceiver(BrowserThread::UI, done, result));
  }

  void FetchOnIOThread(const base::Closure& done,
                       bool* prepare_result,
                       FetchResult* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ServiceWorkerFetchRequest request(
        embedded_test_server()->GetURL("/service_worker/empty.html"),
        "GET",
        ServiceWorkerHeaderMap(),
        GURL(""),
        false);
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    version_->DispatchFetchEvent(
        request,
        CreatePrepareReceiver(prepare_result),
        CreateResponseReceiver(
            BrowserThread::UI, done, blob_context_.get(), result));
  }

  void StopOnIOThread(const base::Closure& done,
                      ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(version_.get());
    version_->StopWorker(CreateReceiver(BrowserThread::UI, done, result));
  }

  void SyncEventOnIOThread(const base::Closure& done,
                           ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    version_->DispatchSyncEvent(
        CreateReceiver(BrowserThread::UI, done, result));
  }

 protected:
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  scoped_refptr<ChromeBlobStorageContext> blob_context_;
};

IN_PROC_BROWSER_TEST_F(EmbeddedWorkerBrowserTest, StartAndStop) {
  // Start a worker and wait until OnStarted() is called.
  base::RunLoop start_run_loop;
  done_closure_ = start_run_loop.QuitClosure();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this));
  start_run_loop.Run();

  ASSERT_EQ(EmbeddedWorkerInstance::RUNNING, last_worker_status_);

  // Stop a worker and wait until OnStopped() is called.
  base::RunLoop stop_run_loop;
  done_closure_ = stop_run_loop.QuitClosure();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StopOnIOThread, this));
  stop_run_loop.Run();

  ASSERT_EQ(EmbeddedWorkerInstance::STOPPED, last_worker_status_);
}

IN_PROC_BROWSER_TEST_F(EmbeddedWorkerBrowserTest, StartPaused_ThenResume) {
  pause_mode_ = PAUSE_THEN_RESUME;
  base::RunLoop start_run_loop;
  done_closure_ = start_run_loop.QuitClosure();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this));
  start_run_loop.Run();
  ASSERT_EQ(EmbeddedWorkerInstance::RUNNING, last_worker_status_);
}

IN_PROC_BROWSER_TEST_F(EmbeddedWorkerBrowserTest,
                       StartPaused_ThenStop) {
  pause_mode_ = PAUSE_THEN_STOP;
  base::RunLoop start_run_loop;
  done_closure_ = start_run_loop.QuitClosure();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this));
  start_run_loop.Run();
  ASSERT_EQ(EmbeddedWorkerInstance::STOPPED, last_worker_status_);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartAndStop) {
  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/worker.js"));

  // Start a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop start_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this,
                                     start_run_loop.QuitClosure(),
                                     &status));
  start_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  // Stop the worker.
  status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop stop_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StopOnIOThread, this,
                                     stop_run_loop.QuitClosure(),
                                     &status));
  stop_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_OK, status);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartNotFound) {
  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/nonexistent.js"));

  // Start a worker for nonexistent URL.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop start_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this,
                                     start_run_loop.QuitClosure(),
                                     &status));
  start_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, Install) {
  InstallTestHelper("/service_worker/worker.js", SERVICE_WORKER_OK);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_Fulfilled) {
  InstallTestHelper("/service_worker/worker_install_fulfilled.js",
                    SERVICE_WORKER_OK);
}

// Check that ServiceWorker script requests set a "Service-Worker: script"
// header.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       ServiceWorkerScriptHeader) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifyServiceWorkerHeaderInRequest));
  InstallTestHelper("/service_worker/generated_sw.js", SERVICE_WORKER_OK);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       Activate_NoEventListener) {
  ActivateTestHelper("/service_worker/worker.js", SERVICE_WORKER_OK);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, Activate_Rejected) {
  ActivateTestHelper("/service_worker/worker_activate_rejected.js",
                     SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_Rejected) {
  InstallTestHelper("/service_worker/worker_install_rejected.js",
                    SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchEvent_Response) {
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  scoped_ptr<storage::BlobDataHandle> blob_data_handle;
  FetchTestHelper("/service_worker/fetch_event.js",
                  &result, &response, &blob_data_handle);
  ASSERT_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, result);
  EXPECT_EQ(301, response.status_code);
  EXPECT_EQ("Moved Permanently", response.status_text);
  ServiceWorkerHeaderMap expected_headers;
  expected_headers["content-language"] = "fi";
  expected_headers["content-type"] = "text/html; charset=UTF-8";
  EXPECT_EQ(expected_headers, response.headers);

  std::string body;
  RunOnIOThread(
      base::Bind(&ReadResponseBody,
                 &body, base::Owned(blob_data_handle.release())));
  EXPECT_EQ("This resource is gone. Gone, gone, gone.", body);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       SyncAbortedWithoutFlag) {
  RunOnIOThread(base::Bind(
      &self::SetUpRegistrationOnIOThread, this, "/service_worker/sync.js"));

  // Run the sync event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop sync_run_loop;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&self::SyncEventOnIOThread,
                                     this,
                                     sync_run_loop.QuitClosure(),
                                     &status));
  sync_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_ERROR_ABORT, status);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, SyncEventHandled) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kEnableServiceWorkerSync);

  RunOnIOThread(base::Bind(
      &self::SetUpRegistrationOnIOThread, this, "/service_worker/sync.js"));
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  scoped_ptr<storage::BlobDataHandle> blob_data_handle;
  // Should 404 before sync event.
  FetchOnRegisteredWorker(&result, &response, &blob_data_handle);
  EXPECT_EQ(404, response.status_code);

  // Run the sync event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop sync_run_loop;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&self::SyncEventOnIOThread,
                                     this,
                                     sync_run_loop.QuitClosure(),
                                     &status));
  sync_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  // Should 200 after sync event.
  FetchOnRegisteredWorker(&result, &response, &blob_data_handle);
  EXPECT_EQ(200, response.status_code);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, Reload) {
  const std::string kPageUrl = "/service_worker/reload.html";
  const std::string kWorkerUrl = "/service_worker/fetch_event_reload.js";
  {
    scoped_refptr<WorkerActivatedObserver> observer =
        new WorkerActivatedObserver(wrapper());
    observer->Init();
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kPageUrl),
        embedded_test_server()->GetURL(kWorkerUrl),
        base::Bind(&ExpectResultAndRun, true, base::Bind(&base::DoNothing)));
    observer->Wait();
  }
  {
    const base::string16 title = base::ASCIIToUTF16("reload=false");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }
  {
    const base::string16 title = base::ASCIIToUTF16("reload=true");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    ReloadBlockUntilNavigationsComplete(shell(), 1);
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }
  shell()->Close();
  {
    base::RunLoop run_loop;
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL(kPageUrl),
        base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, ImportsBustMemcache) {
  const std::string kScopeUrl = "/service_worker/imports_bust_memcache_scope/";
  const std::string kPageUrl = "/service_worker/imports_bust_memcache.html";
  const std::string kScriptUrl = "/service_worker/worker_with_one_import.js";
  const std::string kImportUrl = "/service_worker/long_lived_import.js";
  const base::string16 kOKTitle(base::ASCIIToUTF16("OK"));
  const base::string16 kFailTitle(base::ASCIIToUTF16("FAIL"));

  RunOnIOThread(
      base::Bind(&CreateLongLivedResourceInterceptors,
                 embedded_test_server()->GetURL(kScriptUrl),
                 embedded_test_server()->GetURL(kImportUrl)));

  TitleWatcher title_watcher(shell()->web_contents(), kOKTitle);
  title_watcher.AlsoWaitForTitle(kFailTitle);
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
  base::string16 title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(kOKTitle, title);

  // Verify the number of resources in the implicit script cache is correct.
  const int kExpectedNumResources = 2;
  int num_resources = 0;
  RunOnIOThread(
      base::Bind(&CountScriptResources,
                 base::Unretained(wrapper()),
                 embedded_test_server()->GetURL(kScopeUrl),
                 &num_resources));
  EXPECT_EQ(kExpectedNumResources, num_resources);
}

class ServiceWorkerBlackBoxBrowserTest : public ServiceWorkerBrowserTest {
 public:
  typedef ServiceWorkerBlackBoxBrowserTest self;

  void FindRegistrationOnIO(const GURL& document_url,
                            ServiceWorkerStatusCode* status,
                            const base::Closure& continuation) {
    wrapper()->context()->storage()->FindRegistrationForDocument(
        document_url,
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::FindRegistrationOnIO2,
                   this,
                   status,
                   continuation));
  }

  void FindRegistrationOnIO2(
      ServiceWorkerStatusCode* out_status,
      const base::Closure& continuation,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration) {
    *out_status = status;
    if (!registration.get())
      EXPECT_NE(SERVICE_WORKER_OK, status);
    continuation.Run();
  }
};

static int CountRenderProcessHosts() {
  int result = 0;
  for (RenderProcessHost::iterator iter(RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd();
       iter.Advance()) {
    result++;
  }
  return result;
}

// Flaky timeouts on CrOS: http://crbug.com/387045
#if defined(OS_CHROMEOS)
#define MAYBE_Registration DISABLED_Registration
#else
#define MAYBE_Registration Registration
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerBlackBoxBrowserTest, MAYBE_Registration) {
  // Close the only window to be sure we're not re-using its RenderProcessHost.
  shell()->Close();
  EXPECT_EQ(0, CountRenderProcessHosts());

  const std::string kWorkerUrl = "/service_worker/fetch_event.js";

  // Unregistering nothing should return false.
  {
    base::RunLoop run_loop;
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL("/"),
        base::Bind(&ExpectResultAndRun, false, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // If we use a worker URL that doesn't exist, registration fails.
  {
    base::RunLoop run_loop;
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL("/"),
        embedded_test_server()->GetURL("/does/not/exist"),
        base::Bind(&ExpectResultAndRun, false, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(0, CountRenderProcessHosts());

  // Register returns when the promise would be resolved.
  {
    base::RunLoop run_loop;
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL("/"),
        embedded_test_server()->GetURL(kWorkerUrl),
        base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(1, CountRenderProcessHosts());

  // Registering again should succeed, although the algo still
  // might not be complete.
  {
    base::RunLoop run_loop;
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL("/"),
        embedded_test_server()->GetURL(kWorkerUrl),
        base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // The registration algo might not be far enough along to have
  // stored the registration data, so it may not be findable
  // at this point.

  // Unregistering something should return true.
  {
    base::RunLoop run_loop;
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL("/"),
        base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_GE(1, CountRenderProcessHosts()) << "Unregistering doesn't stop the "
                                             "workers eagerly, so their RPHs "
                                             "can still be running.";

  // Should not be able to find it.
  {
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    RunOnIOThread(
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::FindRegistrationOnIO,
                   this,
                   embedded_test_server()->GetURL("/service_worker/empty.html"),
                   &status));
    EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND, status);
  }
}

}  // namespace content
