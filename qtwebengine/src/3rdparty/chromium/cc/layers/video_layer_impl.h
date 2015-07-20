// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIDEO_LAYER_IMPL_H_
#define CC_LAYERS_VIDEO_LAYER_IMPL_H_

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/release_callback_impl.h"
#include "cc/resources/video_resource_updater.h"
#include "media/base/video_rotation.h"

namespace media {
class VideoFrame;
}

namespace cc {
class VideoFrameProvider;
class VideoFrameProviderClientImpl;

class CC_EXPORT VideoLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<VideoLayerImpl> Create(LayerTreeImpl* tree_impl,
                                           int id,
                                           VideoFrameProvider* provider,
                                           media::VideoRotation video_rotation);
  ~VideoLayerImpl() override;

  // LayerImpl implementation.
  scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;
  bool WillDraw(DrawMode draw_mode,
                ResourceProvider* resource_provider) override;
  void AppendQuads(RenderPass* render_pass,
                   const Occlusion& occlusion_in_content_space,
                   AppendQuadsData* append_quads_data) override;
  void DidDraw(ResourceProvider* resource_provider) override;
  void DidBecomeActive() override;
  void ReleaseResources() override;

  void SetNeedsRedraw();

  void SetProviderClientImpl(
      scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl);

  media::VideoRotation video_rotation() const { return video_rotation_; }

 private:
  VideoLayerImpl(LayerTreeImpl* tree_impl,
                 int id,
                 media::VideoRotation video_rotation);

  const char* LayerTypeAsString() const override;

  scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl_;

  scoped_refptr<media::VideoFrame> frame_;

  media::VideoRotation video_rotation_;

  scoped_ptr<VideoResourceUpdater> updater_;
  VideoFrameExternalResources::ResourceType frame_resource_type_;
  std::vector<ResourceProvider::ResourceId> frame_resources_;

  // TODO(danakj): Remove these, hide software path inside ResourceProvider and
  // ExternalResource (aka TextureMailbox) classes.
  std::vector<unsigned> software_resources_;
  // Called once for each software resource.
  ReleaseCallbackImpl software_release_callback_;

  DISALLOW_COPY_AND_ASSIGN(VideoLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_VIDEO_LAYER_IMPL_H_
