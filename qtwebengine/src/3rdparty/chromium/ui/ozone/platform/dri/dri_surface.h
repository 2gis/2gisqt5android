// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_SURFACE_H_
#define UI_OZONE_PLATFORM_DRI_DRI_SURFACE_H_

#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

class SkCanvas;
class SkSurface;

namespace ui {

class DriBuffer;
class DriWindowDelegate;
class DriWrapper;
class HardwareDisplayController;

class DriSurface : public SurfaceOzoneCanvas {
 public:
  DriSurface(DriWindowDelegate* window_delegate, DriWrapper* dri);
  ~DriSurface() override;

  // SurfaceOzoneCanvas:
  skia::RefPtr<SkCanvas> GetCanvas() override;
  void ResizeCanvas(const gfx::Size& viewport_size) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  void UpdateNativeSurface(const gfx::Rect& damage);

  DriWindowDelegate* window_delegate_;

  // Stores the connection to the graphics card. Pointer not owned by this
  // class.
  DriWrapper* dri_;

  // The actual buffers used for painting.
  scoped_refptr<DriBuffer> buffers_[2];

  // Keeps track of which bitmap is |buffers_| is the frontbuffer.
  int front_buffer_;

  skia::RefPtr<SkSurface> surface_;
  gfx::Rect last_damage_;

  DISALLOW_COPY_AND_ASSIGN(DriSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_SURFACE_H_
