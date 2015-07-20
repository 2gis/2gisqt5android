// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/ozone_platform_gbm.h"

#include <dlfcn.h>
#include <gbm.h>
#include <stdlib.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/ozone/platform/dri/dri_cursor.h"
#include "ui/ozone/platform/dri/dri_gpu_platform_support.h"
#include "ui/ozone/platform/dri/dri_gpu_platform_support_host.h"
#include "ui/ozone/platform/dri/dri_window.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/dri_window_manager.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/gbm_buffer.h"
#include "ui/ozone/platform/dri/gbm_surface.h"
#include "ui/ozone/platform/dri/gbm_surface_factory.h"
#include "ui/ozone/platform/dri/native_display_delegate_dri.h"
#include "ui/ozone/platform/dri/native_display_delegate_proxy.h"
#include "ui/ozone/platform/dri/scanout_buffer.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

const char kDefaultGraphicsCardPath[] = "/dev/dri/card0";

class GbmBufferGenerator : public ScanoutBufferGenerator {
 public:
  GbmBufferGenerator(DriWrapper* dri)
      : dri_(dri),
        glapi_lib_(dlopen("libglapi.so.0", RTLD_LAZY | RTLD_GLOBAL)),
        device_(gbm_create_device(dri_->get_fd())) {
    if (!device_)
      LOG(FATAL) << "Unable to initialize gbm for " << kDefaultGraphicsCardPath;
  }
  virtual ~GbmBufferGenerator() {
    gbm_device_destroy(device_);
    if (glapi_lib_)
      dlclose(glapi_lib_);
  }

  gbm_device* device() const { return device_; }

  scoped_refptr<ScanoutBuffer> Create(const gfx::Size& size) override {
    return GbmBuffer::CreateBuffer(
        dri_, device_, SurfaceFactoryOzone::RGBA_8888, size, true);
  }

 protected:
  DriWrapper* dri_;  // Not owned.

  // HACK: gbm drivers have broken linkage
  void *glapi_lib_;

  gbm_device* device_;

  DISALLOW_COPY_AND_ASSIGN(GbmBufferGenerator);
};

class OzonePlatformGbm : public OzonePlatform {
 public:
  OzonePlatformGbm(bool use_surfaceless) : use_surfaceless_(use_surfaceless) {
    base::AtExitManager::RegisterTask(
        base::Bind(&base::DeletePointer<OzonePlatformGbm>, this));
  }
  ~OzonePlatformGbm() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }
  GpuPlatformSupport* GetGpuPlatformSupport() override {
    return gpu_platform_support_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    scoped_ptr<DriWindow> platform_window(
        new DriWindow(delegate, bounds, gpu_platform_support_host_.get(),
                      event_factory_ozone_.get(), window_manager_.get()));
    platform_window->Initialize();
    return platform_window.Pass();
  }
  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateProxy(
        gpu_platform_support_host_.get(), device_manager_.get()));
  }
  void InitializeUI() override {
    // Needed since the browser process creates the accelerated widgets and that
    // happens through SFO.
    surface_factory_ozone_.reset(new GbmSurfaceFactory(use_surfaceless_));
    device_manager_ = CreateDeviceManager();
    gpu_platform_support_host_.reset(new DriGpuPlatformSupportHost());
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);
    window_manager_.reset(
        new DriWindowManager(gpu_platform_support_host_.get()));
    event_factory_ozone_.reset(new EventFactoryEvdev(window_manager_->cursor(),
                                                     device_manager_.get()));
  }

  void InitializeGPU() override {
    dri_.reset(new DriWrapper(kDefaultGraphicsCardPath));
    dri_->Initialize();
    buffer_generator_.reset(new GbmBufferGenerator(dri_.get()));
    screen_manager_.reset(new ScreenManager(dri_.get(),
                                            buffer_generator_.get()));
    window_delegate_manager_.reset(new DriWindowDelegateManager());
    if (!surface_factory_ozone_)
      surface_factory_ozone_.reset(new GbmSurfaceFactory(use_surfaceless_));

    surface_factory_ozone_->InitializeGpu(
        dri_.get(), buffer_generator_->device(), screen_manager_.get(),
        window_delegate_manager_.get());
    gpu_platform_support_.reset(new DriGpuPlatformSupport(
        surface_factory_ozone_.get(), window_delegate_manager_.get(),
        screen_manager_.get(),
        scoped_ptr<NativeDisplayDelegateDri>(new NativeDisplayDelegateDri(
            dri_.get(), screen_manager_.get(), NULL))));
    if (surface_factory_ozone_->InitializeHardware() !=
        DriSurfaceFactory::INITIALIZED)
      LOG(FATAL) << "failed to initialize display hardware";
  }

 private:
  bool use_surfaceless_;
  scoped_ptr<DriWrapper> dri_;
  scoped_ptr<GbmBufferGenerator> buffer_generator_;
  scoped_ptr<ScreenManager> screen_manager_;
  scoped_ptr<DeviceManager> device_manager_;

  scoped_ptr<GbmSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;

  scoped_ptr<DriGpuPlatformSupport> gpu_platform_support_;
  scoped_ptr<DriGpuPlatformSupportHost> gpu_platform_support_host_;

  scoped_ptr<DriWindowDelegateManager> window_delegate_manager_;
  // Browser side object only.
  scoped_ptr<DriWindowManager> window_manager_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformGbm);
};

}  // namespace

OzonePlatform* CreateOzonePlatformGbm() {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  return new OzonePlatformGbm(cmd->HasSwitch(switches::kOzoneUseSurfaceless));
}

}  // namespace ui
