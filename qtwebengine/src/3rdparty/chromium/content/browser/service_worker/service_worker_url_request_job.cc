// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_request_job.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/service_worker_context.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "ui/base/page_transition_types.h"

namespace content {

ServiceWorkerURLRequestJob::ServiceWorkerURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    scoped_refptr<ResourceRequestBody> body)
    : net::URLRequestJob(request, network_delegate),
      provider_host_(provider_host),
      response_type_(NOT_DETERMINED),
      is_started_(false),
      service_worker_response_type_(blink::WebServiceWorkerResponseTypeDefault),
      blob_storage_context_(blob_storage_context),
      request_mode_(request_mode),
      credentials_mode_(credentials_mode),
      request_context_type_(request_context_type),
      frame_type_(frame_type),
      fall_back_required_(false),
      body_(body),
      weak_factory_(this) {
}

void ServiceWorkerURLRequestJob::FallbackToNetwork() {
  DCHECK_EQ(NOT_DETERMINED, response_type_);
  response_type_ = FALLBACK_TO_NETWORK;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::ForwardToServiceWorker() {
  DCHECK_EQ(NOT_DETERMINED, response_type_);
  response_type_ = FORWARD_TO_SERVICE_WORKER;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Start() {
  is_started_ = true;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Kill() {
  net::URLRequestJob::Kill();
  fetch_dispatcher_.reset();
  blob_request_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

net::LoadState ServiceWorkerURLRequestJob::GetLoadState() const {
  // TODO(kinuko): refine this for better debug.
  return net::URLRequestJob::GetLoadState();
}

bool ServiceWorkerURLRequestJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

bool ServiceWorkerURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

void ServiceWorkerURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
  info->response_time = response_time_;
}

void ServiceWorkerURLRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  *load_timing_info = load_timing_info_;
}

int ServiceWorkerURLRequestJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

void ServiceWorkerURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  std::vector<net::HttpByteRange> ranges;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header) ||
      !net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
    return;
  }

  // We don't support multiple range requests in one single URL request.
  if (ranges.size() == 1U)
    byte_range_ = ranges[0];
}

bool ServiceWorkerURLRequestJob::ReadRawData(
    net::IOBuffer* buf, int buf_size, int *bytes_read) {
  if (!blob_request_) {
    *bytes_read = 0;
    return true;
  }

  blob_request_->Read(buf, buf_size, bytes_read);
  net::URLRequestStatus status = blob_request_->status();
  SetStatus(status);
  if (status.is_io_pending())
    return false;
  return status.is_success();
}

void ServiceWorkerURLRequestJob::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnBeforeNetworkStart(net::URLRequest* request,
                                                      bool* defer) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnResponseStarted(net::URLRequest* request) {
  // TODO(falken): Add Content-Length, Content-Type if they were not provided in
  // the ServiceWorkerResponse.
  response_time_ = base::Time::Now();
  CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::OnReadCompleted(net::URLRequest* request,
                                                 int bytes_read) {
  SetStatus(request->status());
  if (!request->status().is_success()) {
    NotifyDone(request->status());
    return;
  }
  NotifyReadComplete(bytes_read);
  if (bytes_read == 0)
    NotifyDone(request->status());
}

const net::HttpResponseInfo* ServiceWorkerURLRequestJob::http_info() const {
  if (!http_response_info_)
    return NULL;
  if (range_response_info_)
    return range_response_info_.get();
  return http_response_info_.get();
}

void ServiceWorkerURLRequestJob::GetExtraResponseInfo(
    bool* was_fetched_via_service_worker,
    bool* was_fallback_required_by_service_worker,
    GURL* original_url_via_service_worker,
    blink::WebServiceWorkerResponseType* response_type_via_service_worker,
    base::TimeTicks* fetch_start_time,
    base::TimeTicks* fetch_ready_time,
    base::TimeTicks* fetch_end_time) const {
  if (response_type_ != FORWARD_TO_SERVICE_WORKER) {
    *was_fetched_via_service_worker = false;
    *was_fallback_required_by_service_worker = false;
    *original_url_via_service_worker = GURL();
    *response_type_via_service_worker =
        blink::WebServiceWorkerResponseTypeDefault;
    return;
  }
  *was_fetched_via_service_worker = true;
  *was_fallback_required_by_service_worker = fall_back_required_;
  *original_url_via_service_worker = response_url_;
  *response_type_via_service_worker = service_worker_response_type_;
  *fetch_start_time = fetch_start_time_;
  *fetch_ready_time = fetch_ready_time_;
  *fetch_end_time = fetch_end_time_;
}


ServiceWorkerURLRequestJob::~ServiceWorkerURLRequestJob() {
}

void ServiceWorkerURLRequestJob::MaybeStartRequest() {
  if (is_started_ && response_type_ != NOT_DETERMINED) {
    // Start asynchronously.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ServiceWorkerURLRequestJob::StartRequest,
                   weak_factory_.GetWeakPtr()));
  }
}

void ServiceWorkerURLRequestJob::StartRequest() {
  switch (response_type_) {
    case NOT_DETERMINED:
      NOTREACHED();
      return;

    case FALLBACK_TO_NETWORK:
      // Restart the request to create a new job. Our request handler will
      // return NULL, and the default job (which will hit network) should be
      // created.
      NotifyRestartRequired();
      return;

    case FORWARD_TO_SERVICE_WORKER:
      DCHECK(provider_host_ && provider_host_->active_version());
      DCHECK(!fetch_dispatcher_);
      // Send a fetch event to the ServiceWorker associated to the
      // provider_host.
      fetch_dispatcher_.reset(new ServiceWorkerFetchDispatcher(
          CreateFetchRequest(),
          provider_host_->active_version(),
          base::Bind(&ServiceWorkerURLRequestJob::DidPrepareFetchEvent,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&ServiceWorkerURLRequestJob::DidDispatchFetchEvent,
                     weak_factory_.GetWeakPtr())));
      fetch_start_time_ = base::TimeTicks::Now();
      load_timing_info_.send_start = fetch_start_time_;
      fetch_dispatcher_->Run();
      return;
  }

  NOTREACHED();
}

scoped_ptr<ServiceWorkerFetchRequest>
ServiceWorkerURLRequestJob::CreateFetchRequest() {
  std::string blob_uuid;
  uint64 blob_size = 0;
  CreateRequestBodyBlob(&blob_uuid, &blob_size);
  scoped_ptr<ServiceWorkerFetchRequest> request(
      new ServiceWorkerFetchRequest());
  request->mode = request_mode_;
  request->request_context_type = request_context_type_;
  request->frame_type = frame_type_;
  request->url = request_->url();
  request->method = request_->method();
  const net::HttpRequestHeaders& headers = request_->extra_request_headers();
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();) {
    if (ServiceWorkerContext::IsExcludedHeaderNameForFetchEvent(it.name()))
      continue;
    request->headers[it.name()] = it.value();
  }
  request->blob_uuid = blob_uuid;
  request->blob_size = blob_size;
  request->referrer = GURL(request_->referrer());
  request->credentials_mode = credentials_mode_;
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (info) {
    request->is_reload = ui::PageTransitionCoreTypeIs(
        info->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
  }
  return request.Pass();
}

bool ServiceWorkerURLRequestJob::CreateRequestBodyBlob(std::string* blob_uuid,
                                                       uint64* blob_size) {
  if (!body_.get() || !blob_storage_context_)
    return false;

  std::vector<const ResourceRequestBody::Element*> resolved_elements;
  for (size_t i = 0; i < body_->elements()->size(); ++i) {
    const ResourceRequestBody::Element& element = (*body_->elements())[i];
    if (element.type() != ResourceRequestBody::Element::TYPE_BLOB) {
      resolved_elements.push_back(&element);
      continue;
    }
    scoped_ptr<storage::BlobDataHandle> handle =
        blob_storage_context_->GetBlobDataFromUUID(element.blob_uuid());
    if (handle->data()->items().empty())
      continue;
    for (size_t i = 0; i < handle->data()->items().size(); ++i) {
      const storage::BlobData::Item& item = handle->data()->items().at(i);
      DCHECK_NE(storage::BlobData::Item::TYPE_BLOB, item.type());
      resolved_elements.push_back(&item);
    }
  }

  const std::string uuid(base::GenerateGUID());
  uint64 total_size = 0;
  scoped_refptr<storage::BlobData> blob_data = new storage::BlobData(uuid);
  for (size_t i = 0; i < resolved_elements.size(); ++i) {
    const ResourceRequestBody::Element& element = *resolved_elements[i];
    if (total_size != kuint64max && element.length() != kuint64max)
      total_size += element.length();
    else
      total_size = kuint64max;
    switch (element.type()) {
      case ResourceRequestBody::Element::TYPE_BYTES:
        blob_data->AppendData(element.bytes(), element.length());
        break;
      case ResourceRequestBody::Element::TYPE_FILE:
        blob_data->AppendFile(element.path(),
                              element.offset(),
                              element.length(),
                              element.expected_modification_time());
        break;
      case ResourceRequestBody::Element::TYPE_BLOB:
        // Blob elements should be resolved beforehand.
        NOTREACHED();
        break;
      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        blob_data->AppendFileSystemFile(element.filesystem_url(),
                                        element.offset(),
                                        element.length(),
                                        element.expected_modification_time());
        break;
      default:
        NOTIMPLEMENTED();
    }
  }

  request_body_blob_data_handle_ =
      blob_storage_context_->AddFinishedBlob(blob_data.get());
  *blob_uuid = uuid;
  *blob_size = total_size;
  return true;
}

void ServiceWorkerURLRequestJob::DidPrepareFetchEvent() {
  fetch_ready_time_ = base::TimeTicks::Now();
}

void ServiceWorkerURLRequestJob::DidDispatchFetchEvent(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  fetch_dispatcher_.reset();

  // Check if we're not orphaned.
  if (!request())
    return;

  if (status != SERVICE_WORKER_OK) {
    // Dispatching event has been failed, falling back to the network.
    // (Tentative behavior described on github)
    // TODO(kinuko): consider returning error if we've come here because
    // unexpected worker termination etc (so that we could fix bugs).
    // TODO(kinuko): Would be nice to log the error case.
    response_type_ = FALLBACK_TO_NETWORK;
    NotifyRestartRequired();
    return;
  }

  if (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK) {
    // When the request_mode is |CORS| or |CORS-with-forced-preflight| we can't
    // simply fallback to the network in the browser process. It is because the
    // CORS preflight logic is implemented in the renderer. So we returns a
    // fall_back_required response to the renderer.
    if (request_mode_ == FETCH_REQUEST_MODE_CORS ||
        request_mode_ == FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT) {
      fall_back_required_ = true;
      CreateResponseHeader(
          400, "Service Worker Fallback Required", ServiceWorkerHeaderMap());
      CommitResponseHeader();
      return;
    }
    // Change the response type and restart the request to fallback to
    // the network.
    response_type_ = FALLBACK_TO_NETWORK;
    NotifyRestartRequired();
    return;
  }

  // We should have a response now.
  DCHECK_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, fetch_result);

  // Treat a response whose status is 0 as a Network Error.
  if (response.status_code == 0) {
    NotifyDone(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  fetch_end_time_ = base::TimeTicks::Now();
  load_timing_info_.send_end = fetch_end_time_;

  // Set up a request for reading the blob.
  if (!response.blob_uuid.empty() && blob_storage_context_) {
    scoped_ptr<storage::BlobDataHandle> blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response.blob_uuid);
    if (!blob_data_handle) {
      // The renderer gave us a bad blob UUID.
      DeliverErrorResponse();
      return;
    }
    blob_request_ = storage::BlobProtocolHandler::CreateBlobRequest(
        blob_data_handle.Pass(), request()->context(), this);
    blob_request_->Start();
  }

  response_url_ = response.url;
  service_worker_response_type_ = response.response_type;
  CreateResponseHeader(
      response.status_code, response.status_text, response.headers);
  load_timing_info_.receive_headers_end = base::TimeTicks::Now();
  if (!blob_request_)
    CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::CreateResponseHeader(
    int status_code,
    const std::string& status_text,
    const ServiceWorkerHeaderMap& headers) {
  // TODO(kinuko): If the response has an identifier to on-disk cache entry,
  // pull response header from the disk.
  std::string status_line(
      base::StringPrintf("HTTP/1.1 %d %s", status_code, status_text.c_str()));
  status_line.push_back('\0');
  http_response_headers_ = new net::HttpResponseHeaders(status_line);
  for (ServiceWorkerHeaderMap::const_iterator it = headers.begin();
       it != headers.end();
       ++it) {
    std::string header;
    header.reserve(it->first.size() + 2 + it->second.size());
    header.append(it->first);
    header.append(": ");
    header.append(it->second);
    http_response_headers_->AddHeader(header);
  }
}

void ServiceWorkerURLRequestJob::CommitResponseHeader() {
  http_response_info_.reset(new net::HttpResponseInfo());
  http_response_info_->headers.swap(http_response_headers_);
  NotifyHeadersComplete();
}

void ServiceWorkerURLRequestJob::DeliverErrorResponse() {
  // TODO(falken): Print an error to the console of the ServiceWorker and of
  // the requesting page.
  CreateResponseHeader(
      500, "Service Worker Response Error", ServiceWorkerHeaderMap());
  CommitResponseHeader();
}

}  // namespace content
