// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_H_

#include <assert.h>

#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// DataPipeProducerHandle and DataPipeConsumerHandle ---------------------------

class DataPipeProducerHandle : public Handle {
 public:
  DataPipeProducerHandle() {}
  explicit DataPipeProducerHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

static_assert(sizeof(DataPipeProducerHandle) == sizeof(Handle),
              "Bad size for C++ DataPipeProducerHandle");

typedef ScopedHandleBase<DataPipeProducerHandle> ScopedDataPipeProducerHandle;
static_assert(sizeof(ScopedDataPipeProducerHandle) ==
                  sizeof(DataPipeProducerHandle),
              "Bad size for C++ ScopedDataPipeProducerHandle");

class DataPipeConsumerHandle : public Handle {
 public:
  DataPipeConsumerHandle() {}
  explicit DataPipeConsumerHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

static_assert(sizeof(DataPipeConsumerHandle) == sizeof(Handle),
              "Bad size for C++ DataPipeConsumerHandle");

typedef ScopedHandleBase<DataPipeConsumerHandle> ScopedDataPipeConsumerHandle;
static_assert(sizeof(ScopedDataPipeConsumerHandle) ==
                  sizeof(DataPipeConsumerHandle),
              "Bad size for C++ ScopedDataPipeConsumerHandle");

inline MojoResult CreateDataPipe(
    const MojoCreateDataPipeOptions* options,
    ScopedDataPipeProducerHandle* data_pipe_producer,
    ScopedDataPipeConsumerHandle* data_pipe_consumer) {
  assert(data_pipe_producer);
  assert(data_pipe_consumer);
  DataPipeProducerHandle producer_handle;
  DataPipeConsumerHandle consumer_handle;
  MojoResult rv = MojoCreateDataPipe(options,
                                     producer_handle.mutable_value(),
                                     consumer_handle.mutable_value());
  // Reset even on failure (reduces the chances that a "stale"/incorrect handle
  // will be used).
  data_pipe_producer->reset(producer_handle);
  data_pipe_consumer->reset(consumer_handle);
  return rv;
}

inline MojoResult WriteDataRaw(DataPipeProducerHandle data_pipe_producer,
                               const void* elements,
                               uint32_t* num_bytes,
                               MojoWriteDataFlags flags) {
  return MojoWriteData(data_pipe_producer.value(), elements, num_bytes, flags);
}

inline MojoResult BeginWriteDataRaw(DataPipeProducerHandle data_pipe_producer,
                                    void** buffer,
                                    uint32_t* buffer_num_bytes,
                                    MojoWriteDataFlags flags) {
  return MojoBeginWriteData(
      data_pipe_producer.value(), buffer, buffer_num_bytes, flags);
}

inline MojoResult EndWriteDataRaw(DataPipeProducerHandle data_pipe_producer,
                                  uint32_t num_bytes_written) {
  return MojoEndWriteData(data_pipe_producer.value(), num_bytes_written);
}

inline MojoResult ReadDataRaw(DataPipeConsumerHandle data_pipe_consumer,
                              void* elements,
                              uint32_t* num_bytes,
                              MojoReadDataFlags flags) {
  return MojoReadData(data_pipe_consumer.value(), elements, num_bytes, flags);
}

inline MojoResult BeginReadDataRaw(DataPipeConsumerHandle data_pipe_consumer,
                                   const void** buffer,
                                   uint32_t* buffer_num_bytes,
                                   MojoReadDataFlags flags) {
  return MojoBeginReadData(
      data_pipe_consumer.value(), buffer, buffer_num_bytes, flags);
}

inline MojoResult EndReadDataRaw(DataPipeConsumerHandle data_pipe_consumer,
                                 uint32_t num_bytes_read) {
  return MojoEndReadData(data_pipe_consumer.value(), num_bytes_read);
}

// A wrapper class that automatically creates a data pipe and owns both handles.
// TODO(vtl): Make an even more friendly version? (Maybe templatized for a
// particular type instead of some "element"? Maybe functions that take
// vectors?)
class DataPipe {
 public:
  DataPipe();
  explicit DataPipe(const MojoCreateDataPipeOptions& options);
  ~DataPipe();

  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
};

inline DataPipe::DataPipe() {
  MojoResult result =
      CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
  MOJO_ALLOW_UNUSED_LOCAL(result);
  assert(result == MOJO_RESULT_OK);
}

inline DataPipe::DataPipe(const MojoCreateDataPipeOptions& options) {
  MojoResult result =
      CreateDataPipe(&options, &producer_handle, &consumer_handle);
  MOJO_ALLOW_UNUSED_LOCAL(result);
  assert(result == MOJO_RESULT_OK);
}

inline DataPipe::~DataPipe() {
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_H_
