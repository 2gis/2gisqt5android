// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_LOCAL_DATA_PIPE_H_
#define MOJO_EDK_SYSTEM_LOCAL_DATA_PIPE_H_

#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/edk/system/data_pipe.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace system {

// |LocalDataPipe| is a subclass that "implements" |DataPipe| for data pipes
// whose producer and consumer are both local. This class is thread-safe (with
// protection provided by |DataPipe|'s |lock_|.
class MOJO_SYSTEM_IMPL_EXPORT LocalDataPipe : public DataPipe {
 public:
  // |validated_options| should be the output of |DataPipe::ValidateOptions()|.
  // In particular: |struct_size| is ignored (so |validated_options| must be the
  // current version of the struct) and |capacity_num_bytes| must be nonzero.
  explicit LocalDataPipe(const MojoCreateDataPipeOptions& validated_options);

 private:
  friend class base::RefCountedThreadSafe<LocalDataPipe>;
  ~LocalDataPipe() override;

  // |DataPipe| implementation:
  void ProducerCloseImplNoLock() override;
  MojoResult ProducerWriteDataImplNoLock(
      UserPointer<const void> elements,
      UserPointer<uint32_t> num_bytes,
      uint32_t max_num_bytes_to_write,
      uint32_t min_num_bytes_to_write) override;
  MojoResult ProducerBeginWriteDataImplNoLock(
      UserPointer<void*> buffer,
      UserPointer<uint32_t> buffer_num_bytes,
      uint32_t min_num_bytes_to_write) override;
  MojoResult ProducerEndWriteDataImplNoLock(
      uint32_t num_bytes_written) override;
  HandleSignalsState ProducerGetHandleSignalsStateImplNoLock() const override;
  void ConsumerCloseImplNoLock() override;
  MojoResult ConsumerReadDataImplNoLock(
      UserPointer<void> elements,
      UserPointer<uint32_t> num_bytes,
      uint32_t max_num_bytes_to_read,
      uint32_t min_num_bytes_to_read,
      bool peek) override;
  MojoResult ConsumerDiscardDataImplNoLock(
      UserPointer<uint32_t> num_bytes,
      uint32_t max_num_bytes_to_discard,
      uint32_t min_num_bytes_to_discard) override;
  MojoResult ConsumerQueryDataImplNoLock(
      UserPointer<uint32_t> num_bytes) override;
  MojoResult ConsumerBeginReadDataImplNoLock(
      UserPointer<const void*> buffer,
      UserPointer<uint32_t> buffer_num_bytes,
      uint32_t min_num_bytes_to_read) override;
  MojoResult ConsumerEndReadDataImplNoLock(uint32_t num_bytes_read) override;
  HandleSignalsState ConsumerGetHandleSignalsStateImplNoLock() const override;

  void EnsureBufferNoLock();
  void DestroyBufferNoLock();

  // Get the maximum (single) write/read size right now (in number of elements);
  // result fits in a |uint32_t|.
  size_t GetMaxNumBytesToWriteNoLock();
  size_t GetMaxNumBytesToReadNoLock();

  // Marks the given number of bytes as consumed/discarded. |num_bytes| must be
  // greater than |current_num_bytes_|.
  void MarkDataAsConsumedNoLock(size_t num_bytes);

  // The members below are protected by |DataPipe|'s |lock_|:
  scoped_ptr<char, base::AlignedFreeDeleter> buffer_;
  // Circular buffer.
  size_t start_index_;
  size_t current_num_bytes_;

  DISALLOW_COPY_AND_ASSIGN(LocalDataPipe);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_LOCAL_DATA_PIPE_H_
