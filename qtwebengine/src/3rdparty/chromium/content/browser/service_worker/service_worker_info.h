// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_

#include <vector>

#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class CONTENT_EXPORT ServiceWorkerVersionInfo {
 public:
  ServiceWorkerVersionInfo();
  ServiceWorkerVersionInfo(ServiceWorkerVersion::RunningStatus running_status,
                           ServiceWorkerVersion::Status status,
                           const GURL& script_url,
                           int64 version_id,
                           int process_id,
                           int thread_id,
                           int devtools_agent_route_id);
  ~ServiceWorkerVersionInfo();

  ServiceWorkerVersion::RunningStatus running_status;
  ServiceWorkerVersion::Status status;
  GURL script_url;
  int64 version_id;
  int process_id;
  int thread_id;
  int devtools_agent_route_id;
};

class CONTENT_EXPORT ServiceWorkerRegistrationInfo {
 public:
  ServiceWorkerRegistrationInfo();
  ServiceWorkerRegistrationInfo(
      const GURL& pattern,
      int64 registration_id,
      const ServiceWorkerVersionInfo& active_version,
      const ServiceWorkerVersionInfo& waiting_version,
      const ServiceWorkerVersionInfo& installing_version,
      int64_t active_version_total_size_bytes);
  ~ServiceWorkerRegistrationInfo();

  GURL pattern;
  int64 registration_id;
  ServiceWorkerVersionInfo controlling_version;
  ServiceWorkerVersionInfo active_version;
  ServiceWorkerVersionInfo waiting_version;
  ServiceWorkerVersionInfo installing_version;

  int64_t stored_version_size_bytes;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_
