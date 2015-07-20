// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
#define CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/child/child_thread.h"
#include "content/common/content_export.h"
#include "content/public/utility/utility_thread.h"

namespace base {
class FilePath;
}

namespace content {
class BlinkPlatformImpl;

// This class represents the background thread where the utility task runs.
class UtilityThreadImpl : public UtilityThread,
                          public ChildThread {
 public:
  UtilityThreadImpl();
  // Constructor that's used when running in single process mode.
  explicit UtilityThreadImpl(const std::string& channel_name);
  ~UtilityThreadImpl() override;
  void Shutdown() override;

  bool Send(IPC::Message* msg) override;
  void ReleaseProcessIfNeeded() override;
#if defined(OS_WIN)
  virtual void PreCacheFont(const LOGFONT& log_font) override;
  virtual void ReleaseCachedFonts() override;
#endif

 private:
  void Init();

  // ChildThread implementation.
  bool OnControlMessageReceived(const IPC::Message& msg) override;

  // IPC message handlers.
  void OnBatchModeStarted();
  void OnBatchModeFinished();

#if defined(OS_POSIX) && defined(ENABLE_PLUGINS)
  void OnLoadPlugins(const std::vector<base::FilePath>& plugin_paths);
#endif

  // True when we're running in batch mode.
  bool batch_mode_;

  // True if running in single process mode.
  bool single_process_;

  scoped_ptr<BlinkPlatformImpl> blink_platform_impl_;

  DISALLOW_COPY_AND_ASSIGN(UtilityThreadImpl);
};

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
