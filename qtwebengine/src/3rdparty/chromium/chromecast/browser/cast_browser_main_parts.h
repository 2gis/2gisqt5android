// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"

namespace chromecast {
namespace shell {
class CastBrowserProcess;
class URLRequestContextFactory;

class CastBrowserMainParts : public content::BrowserMainParts {
 public:
  CastBrowserMainParts(
      const content::MainFunctionParams& parameters,
      URLRequestContextFactory* url_request_context_factory);
  virtual ~CastBrowserMainParts();

  // content::BrowserMainParts implementation:
  virtual void PreMainMessageLoopStart() override;
  virtual void PostMainMessageLoopStart() override;
  virtual int PreCreateThreads() override;
  virtual void PreMainMessageLoopRun() override;
  virtual bool MainMessageLoopRun(int* result_code) override;
  virtual void PostMainMessageLoopRun() override;

 private:
  scoped_ptr<CastBrowserProcess> cast_browser_process_;
  const content::MainFunctionParams parameters_;  // For running browser tests.
  URLRequestContextFactory* const url_request_context_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastBrowserMainParts);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_
