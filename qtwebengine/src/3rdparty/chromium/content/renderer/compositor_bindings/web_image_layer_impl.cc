// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/compositor_bindings/web_image_layer_impl.h"

#include "cc/layers/image_layer.h"
#include "cc/layers/picture_image_layer.h"
#include "content/renderer/compositor_bindings/web_layer_impl.h"
#include "content/renderer/compositor_bindings/web_layer_impl_fixed_bounds.h"

namespace content {

WebImageLayerImpl::WebImageLayerImpl() {
  if (WebLayerImpl::UsingPictureLayer())
    layer_.reset(new WebLayerImplFixedBounds(cc::PictureImageLayer::Create()));
  else
    layer_.reset(new WebLayerImpl(cc::ImageLayer::Create()));
}

WebImageLayerImpl::~WebImageLayerImpl() {
}

blink::WebLayer* WebImageLayerImpl::layer() {
  return layer_.get();
}

void WebImageLayerImpl::setBitmap(SkBitmap bitmap) {
  if (WebLayerImpl::UsingPictureLayer()) {
    static_cast<cc::PictureImageLayer*>(layer_->layer())->SetBitmap(bitmap);
    static_cast<WebLayerImplFixedBounds*>(layer_.get())->SetFixedBounds(
        gfx::Size(bitmap.width(), bitmap.height()));
  } else {
    static_cast<cc::ImageLayer*>(layer_->layer())->SetBitmap(bitmap);
  }
}

}  // namespace content

