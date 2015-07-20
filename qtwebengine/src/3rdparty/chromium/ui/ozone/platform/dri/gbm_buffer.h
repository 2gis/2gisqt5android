// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GBM_BUFFER_H_
#define UI_OZONE_PLATFORM_DRI_GBM_BUFFER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/dri/gbm_buffer_base.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_factory_ozone.h"

struct gbm_bo;
struct gbm_device;

namespace ui {

class DriWrapper;

class GbmBuffer : public GbmBufferBase {
 public:
  static scoped_refptr<GbmBuffer> CreateBuffer(
      DriWrapper* dri,
      gbm_device* device,
      SurfaceFactoryOzone::BufferFormat format,
      const gfx::Size& size,
      bool scanout);

 private:
  GbmBuffer(DriWrapper* dri, gbm_bo* bo, bool scanout);
  ~GbmBuffer() override;

  DISALLOW_COPY_AND_ASSIGN(GbmBuffer);
};

class GbmPixmap : public NativePixmap {
 public:
  GbmPixmap(scoped_refptr<GbmBuffer> buffer);
  bool Initialize(DriWrapper* dri);

  // NativePixmap:
  void* GetEGLClientBuffer() override;
  int GetDmaBufFd() override;
  int GetDmaBufPitch() override;

  scoped_refptr<GbmBuffer> buffer() { return buffer_; }

 private:
  ~GbmPixmap() override;

  scoped_refptr<GbmBuffer> buffer_;
  int dma_buf_;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmap);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GBM_BUFFER_H_
