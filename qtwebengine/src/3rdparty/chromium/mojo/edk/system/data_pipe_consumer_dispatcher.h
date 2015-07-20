// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace system {

class DataPipe;

// This is the |Dispatcher| implementation for the consumer handle for data
// pipes (created by the Mojo primitive |MojoCreateDataPipe()|). This class is
// thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipeConsumerDispatcher : public Dispatcher {
 public:
  DataPipeConsumerDispatcher();

  // Must be called before any other methods.
  void Init(scoped_refptr<DataPipe> data_pipe);

  // |Dispatcher| public methods:
  Type GetType() const override;

 private:
  ~DataPipeConsumerDispatcher() override;

  // |Dispatcher| protected methods:
  void CancelAllWaitersNoLock() override;
  void CloseImplNoLock() override;
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseImplNoLock()
      override;
  MojoResult ReadDataImplNoLock(UserPointer<void> elements,
                                UserPointer<uint32_t> num_bytes,
                                MojoReadDataFlags flags) override;
  MojoResult BeginReadDataImplNoLock(UserPointer<const void*> buffer,
                                     UserPointer<uint32_t> buffer_num_bytes,
                                     MojoReadDataFlags flags) override;
  MojoResult EndReadDataImplNoLock(uint32_t num_bytes_read) override;
  HandleSignalsState GetHandleSignalsStateImplNoLock() const override;
  MojoResult AddWaiterImplNoLock(Waiter* waiter,
                                 MojoHandleSignals signals,
                                 uint32_t context,
                                 HandleSignalsState* signals_state) override;
  void RemoveWaiterImplNoLock(Waiter* waiter,
                              HandleSignalsState* signals_state) override;
  bool IsBusyNoLock() const override;

  // Protected by |lock()|:
  scoped_refptr<DataPipe> data_pipe_;  // This will be null if closed.

  DISALLOW_COPY_AND_ASSIGN(DataPipeConsumerDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
