// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_database.h"

namespace content {

class ServiceWorkerMetrics {
 public:
  enum ReadResponseResult {
    READ_OK,
    READ_HEADERS_ERROR,
    READ_DATA_ERROR,
    NUM_READ_RESPONSE_RESULT_TYPES,
  };

  enum WriteResponseResult {
    WRITE_OK,
    WRITE_HEADERS_ERROR,
    WRITE_DATA_ERROR,
    NUM_WRITE_RESPONSE_RESULT_TYPES,
  };

  // Used for ServiceWorkerDiskCache.
  static void CountInitDiskCacheResult(bool result);
  static void CountReadResponseResult(ReadResponseResult result);
  static void CountWriteResponseResult(WriteResponseResult result);

  // Used for ServiceWorkerDatabase.
  static void CountOpenDatabaseResult(ServiceWorkerDatabase::Status status);
  static void CountReadDatabaseResult(ServiceWorkerDatabase::Status status);
  static void CountWriteDatabaseResult(ServiceWorkerDatabase::Status status);

  // Counts the number of page loads controlled by a Service Worker.
  static void CountControlledPageLoad();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceWorkerMetrics);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
