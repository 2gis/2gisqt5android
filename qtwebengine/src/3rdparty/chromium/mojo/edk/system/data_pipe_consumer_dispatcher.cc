// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/data_pipe_consumer_dispatcher.h"

#include "base/logging.h"
#include "mojo/edk/system/data_pipe.h"
#include "mojo/edk/system/memory.h"

namespace mojo {
namespace system {

DataPipeConsumerDispatcher::DataPipeConsumerDispatcher() {
}

void DataPipeConsumerDispatcher::Init(scoped_refptr<DataPipe> data_pipe) {
  DCHECK(data_pipe.get());
  data_pipe_ = data_pipe;
}

Dispatcher::Type DataPipeConsumerDispatcher::GetType() const {
  return kTypeDataPipeConsumer;
}

DataPipeConsumerDispatcher::~DataPipeConsumerDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the pipe.
  DCHECK(!data_pipe_.get());
}

void DataPipeConsumerDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  data_pipe_->ConsumerCancelAllWaiters();
}

void DataPipeConsumerDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  data_pipe_->ConsumerClose();
  data_pipe_ = nullptr;
}

scoped_refptr<Dispatcher>
DataPipeConsumerDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  scoped_refptr<DataPipeConsumerDispatcher> rv =
      new DataPipeConsumerDispatcher();
  rv->Init(data_pipe_);
  data_pipe_ = nullptr;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult DataPipeConsumerDispatcher::ReadDataImplNoLock(
    UserPointer<void> elements,
    UserPointer<uint32_t> num_bytes,
    MojoReadDataFlags flags) {
  lock().AssertAcquired();

  if ((flags & MOJO_READ_DATA_FLAG_DISCARD)) {
    // These flags are mutally exclusive.
    if ((flags & MOJO_READ_DATA_FLAG_QUERY) ||
        (flags & MOJO_READ_DATA_FLAG_PEEK))
      return MOJO_RESULT_INVALID_ARGUMENT;
    DVLOG_IF(2, !elements.IsNull())
        << "Discard mode: ignoring non-null |elements|";
    return data_pipe_->ConsumerDiscardData(
        num_bytes, (flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE));
  }

  if ((flags & MOJO_READ_DATA_FLAG_QUERY)) {
    if ((flags & MOJO_READ_DATA_FLAG_PEEK))
      return MOJO_RESULT_INVALID_ARGUMENT;
    DCHECK(!(flags & MOJO_READ_DATA_FLAG_DISCARD));  // Handled above.
    DVLOG_IF(2, !elements.IsNull())
        << "Query mode: ignoring non-null |elements|";
    return data_pipe_->ConsumerQueryData(num_bytes);
  }

  return data_pipe_->ConsumerReadData(
      elements,
      num_bytes,
      !!(flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE),
      !!(flags & MOJO_READ_DATA_FLAG_PEEK));
}

MojoResult DataPipeConsumerDispatcher::BeginReadDataImplNoLock(
    UserPointer<const void*> buffer,
    UserPointer<uint32_t> buffer_num_bytes,
    MojoReadDataFlags flags) {
  lock().AssertAcquired();

  // These flags may not be used in two-phase mode.
  if ((flags & MOJO_READ_DATA_FLAG_DISCARD) ||
      (flags & MOJO_READ_DATA_FLAG_QUERY) ||
      (flags & MOJO_READ_DATA_FLAG_PEEK))
    return MOJO_RESULT_INVALID_ARGUMENT;

  return data_pipe_->ConsumerBeginReadData(
      buffer, buffer_num_bytes, (flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE));
}

MojoResult DataPipeConsumerDispatcher::EndReadDataImplNoLock(
    uint32_t num_bytes_read) {
  lock().AssertAcquired();

  return data_pipe_->ConsumerEndReadData(num_bytes_read);
}

HandleSignalsState DataPipeConsumerDispatcher::GetHandleSignalsStateImplNoLock()
    const {
  lock().AssertAcquired();
  return data_pipe_->ConsumerGetHandleSignalsState();
}

MojoResult DataPipeConsumerDispatcher::AddWaiterImplNoLock(
    Waiter* waiter,
    MojoHandleSignals signals,
    uint32_t context,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  return data_pipe_->ConsumerAddWaiter(waiter, signals, context, signals_state);
}

void DataPipeConsumerDispatcher::RemoveWaiterImplNoLock(
    Waiter* waiter,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  data_pipe_->ConsumerRemoveWaiter(waiter, signals_state);
}

bool DataPipeConsumerDispatcher::IsBusyNoLock() const {
  lock().AssertAcquired();
  return data_pipe_->ConsumerIsBusy();
}

}  // namespace system
}  // namespace mojo
