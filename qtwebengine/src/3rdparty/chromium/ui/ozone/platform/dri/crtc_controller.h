// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_CRTC_CONTROLLER_H_
#define UI_OZONE_PLATFORM_DRI_CRTC_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include "ui/ozone/platform/dri/overlay_plane.h"
#include "ui/ozone/platform/dri/scoped_drm_types.h"

namespace ui {

class DriWrapper;

// Wrapper around a CRTC.
//
// One CRTC can be paired up with one or more connectors. The simplest
// configuration represents one CRTC driving one monitor, while pairing up a
// CRTC with multiple connectors results in hardware mirroring.
class CrtcController {
 public:
  CrtcController(DriWrapper* drm, uint32_t crtc, uint32_t connector);
  ~CrtcController();

  uint32_t crtc() const { return crtc_; }
  uint32_t connector() const { return connector_; }
  DriWrapper* drm() const { return drm_; }
  bool is_disabled() const { return is_disabled_; }
  bool page_flip_pending() const { return page_flip_pending_; }
  uint64_t time_of_last_flip() const { return time_of_last_flip_; }

  // Perform the initial modesetting operation using |plane| as the buffer for
  // the primary plane. The CRTC configuration is specified by |mode|.
  bool Modeset(const OverlayPlane& plane, drmModeModeInfo mode);

  // Disables the controller.
  bool Disable();

  // Schedule a page flip event and present the overlays in |planes|.
  bool SchedulePageFlip(const OverlayPlaneList& planes);

  // Called when the page flip event occurred. The event is provided by the
  // kernel when a VBlank event finished. This allows the controller to
  // update internal state and propagate the update to the surface.
  // The tuple (seconds, useconds) represents the event timestamp. |seconds|
  // represents the number of seconds while |useconds| represents the
  // microseconds (< 1 second) in the timestamp.
  void OnPageFlipEvent(unsigned int frame,
                       unsigned int seconds,
                       unsigned int useconds);

  bool SetCursor(const scoped_refptr<ScanoutBuffer>& buffer);
  bool UnsetCursor();
  bool MoveCursor(const gfx::Point& location);

 private:
  DriWrapper* drm_;  // Not owned.

  // Buffers need to be declared first so that they are destroyed last. Needed
  // since the controllers may reference the buffers.
  OverlayPlaneList current_planes_;
  OverlayPlaneList pending_planes_;
  scoped_refptr<ScanoutBuffer> cursor_buffer_;

  uint32_t crtc_;

  // TODO(dnicoara) Add support for hardware mirroring (multiple connectors).
  uint32_t connector_;

  drmModeModeInfo mode_;

  // Store the state of the CRTC before we took over. Used to restore the CRTC
  // once we no longer need it.
  ScopedDrmCrtcPtr saved_crtc_;

  // Keeps track of the CRTC state. If a surface has been bound, then the value
  // is set to false. Otherwise it is true.
  bool is_disabled_;

  // True if a successful SchedulePageFlip occurred. Reset to false by a modeset
  // operation or when the OnPageFlipEvent callback is triggered.
  bool page_flip_pending_;

  // The time of the last page flip event as reported by the kernel callback.
  uint64_t time_of_last_flip_;

  DISALLOW_COPY_AND_ASSIGN(CrtcController);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_CRTC_CONTROLLER_H_
