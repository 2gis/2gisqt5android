// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_CONTEXT_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_CONTEXT_MESSAGE_FILTER_H_

#include "content/child/child_message_filter.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class EmbeddedWorkerContextMessageFilter : public ChildMessageFilter {
 public:
  EmbeddedWorkerContextMessageFilter();

 protected:
  ~EmbeddedWorkerContextMessageFilter() override;

  // ChildMessageFilter implementation:
  base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& msg) override;
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerContextMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_CONTEXT_MESSAGE_FILTER_H_
