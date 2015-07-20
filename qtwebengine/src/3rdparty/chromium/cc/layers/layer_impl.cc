// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include "base/debug/trace_event.h"
#include "base/debug/trace_event_argument.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/debug/micro_benchmark_impl.h"
#include "cc/debug/traced_value.h"
#include "cc/input/layer_scroll_offset_delegate.h"
#include "cc/layers/layer_utils.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/proxy.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
LayerImpl::LayerImpl(LayerTreeImpl* tree_impl, int id)
    : parent_(nullptr),
      scroll_parent_(nullptr),
      clip_parent_(nullptr),
      mask_layer_id_(-1),
      replica_layer_id_(-1),
      layer_id_(id),
      layer_tree_impl_(tree_impl),
      scroll_offset_delegate_(nullptr),
      scroll_clip_layer_(nullptr),
      should_scroll_on_main_thread_(false),
      have_wheel_event_handlers_(false),
      have_scroll_event_handlers_(false),
      user_scrollable_horizontal_(true),
      user_scrollable_vertical_(true),
      stacking_order_changed_(false),
      double_sided_(true),
      should_flatten_transform_(true),
      layer_property_changed_(false),
      masks_to_bounds_(false),
      contents_opaque_(false),
      is_root_for_isolated_group_(false),
      use_parent_backface_visibility_(false),
      draw_checkerboard_for_missing_tiles_(false),
      draws_content_(false),
      hide_layer_and_subtree_(false),
      force_render_surface_(false),
      transform_is_invertible_(true),
      is_container_for_fixed_position_layers_(false),
      background_color_(0),
      opacity_(1.0),
      blend_mode_(SkXfermode::kSrcOver_Mode),
      num_descendants_that_draw_content_(0),
      draw_depth_(0.f),
      needs_push_properties_(false),
      num_dependents_need_push_properties_(0),
      sorting_context_id_(0),
      current_draw_mode_(DRAW_MODE_NONE) {
  DCHECK_GT(layer_id_, 0);
  DCHECK(layer_tree_impl_);
  layer_tree_impl_->RegisterLayer(this);
  AnimationRegistrar* registrar = layer_tree_impl_->animationRegistrar();
  layer_animation_controller_ =
      registrar->GetAnimationControllerForId(layer_id_);
  layer_animation_controller_->AddValueObserver(this);
  if (IsActive()) {
    layer_animation_controller_->set_value_provider(this);
    layer_animation_controller_->set_layer_animation_delegate(this);
  }
  SetNeedsPushProperties();
}

LayerImpl::~LayerImpl() {
  DCHECK_EQ(DRAW_MODE_NONE, current_draw_mode_);

  layer_animation_controller_->RemoveValueObserver(this);
  layer_animation_controller_->remove_value_provider(this);
  layer_animation_controller_->remove_layer_animation_delegate(this);

  if (!copy_requests_.empty() && layer_tree_impl_->IsActiveTree())
    layer_tree_impl()->RemoveLayerWithCopyOutputRequest(this);
  layer_tree_impl_->UnregisterLayer(this);

  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerImpl", this);
}

void LayerImpl::AddChild(scoped_ptr<LayerImpl> child) {
  child->SetParent(this);
  DCHECK_EQ(layer_tree_impl(), child->layer_tree_impl());
  children_.push_back(child.Pass());
  layer_tree_impl()->set_needs_update_draw_properties();
}

scoped_ptr<LayerImpl> LayerImpl::RemoveChild(LayerImpl* child) {
  for (OwnedLayerImplList::iterator it = children_.begin();
       it != children_.end();
       ++it) {
    if (*it == child) {
      scoped_ptr<LayerImpl> ret = children_.take(it);
      children_.erase(it);
      layer_tree_impl()->set_needs_update_draw_properties();
      return ret.Pass();
    }
  }
  return nullptr;
}

void LayerImpl::SetParent(LayerImpl* parent) {
  if (parent_should_know_need_push_properties()) {
    if (parent_)
      parent_->RemoveDependentNeedsPushProperties();
    if (parent)
      parent->AddDependentNeedsPushProperties();
  }
  parent_ = parent;
}

void LayerImpl::ClearChildList() {
  if (children_.empty())
    return;

  children_.clear();
  layer_tree_impl()->set_needs_update_draw_properties();
}

bool LayerImpl::HasAncestor(const LayerImpl* ancestor) const {
  if (!ancestor)
    return false;

  for (const LayerImpl* layer = this; layer; layer = layer->parent()) {
    if (layer == ancestor)
      return true;
  }

  return false;
}

void LayerImpl::SetScrollParent(LayerImpl* parent) {
  if (scroll_parent_ == parent)
    return;

  // Having both a scroll parent and a scroll offset delegate is unsupported.
  DCHECK(!scroll_offset_delegate_);

  if (parent)
    DCHECK_EQ(layer_tree_impl()->LayerById(parent->id()), parent);

  scroll_parent_ = parent;
  SetNeedsPushProperties();
}

void LayerImpl::SetDebugInfo(
    scoped_refptr<base::debug::ConvertableToTraceFormat> other) {
  debug_info_ = other;
  SetNeedsPushProperties();
}

void LayerImpl::SetScrollChildren(std::set<LayerImpl*>* children) {
  if (scroll_children_.get() == children)
    return;
  scroll_children_.reset(children);
  SetNeedsPushProperties();
}

void LayerImpl::SetNumDescendantsThatDrawContent(int num_descendants) {
  if (num_descendants_that_draw_content_ == num_descendants)
    return;
  num_descendants_that_draw_content_ = num_descendants;
  SetNeedsPushProperties();
}

void LayerImpl::SetClipParent(LayerImpl* ancestor) {
  if (clip_parent_ == ancestor)
    return;

  clip_parent_ = ancestor;
  SetNeedsPushProperties();
}

void LayerImpl::SetClipChildren(std::set<LayerImpl*>* children) {
  if (clip_children_.get() == children)
    return;
  clip_children_.reset(children);
  SetNeedsPushProperties();
}

void LayerImpl::PassCopyRequests(ScopedPtrVector<CopyOutputRequest>* requests) {
  if (requests->empty())
    return;

  bool was_empty = copy_requests_.empty();
  copy_requests_.insert_and_take(copy_requests_.end(), requests);
  requests->clear();

  if (was_empty && layer_tree_impl()->IsActiveTree())
    layer_tree_impl()->AddLayerWithCopyOutputRequest(this);
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::TakeCopyRequestsAndTransformToTarget(
    ScopedPtrVector<CopyOutputRequest>* requests) {
  DCHECK(!copy_requests_.empty());
  DCHECK(layer_tree_impl()->IsActiveTree());

  size_t first_inserted_request = requests->size();
  requests->insert_and_take(requests->end(), &copy_requests_);
  copy_requests_.clear();

  for (size_t i = first_inserted_request; i < requests->size(); ++i) {
    CopyOutputRequest* request = requests->at(i);
    if (!request->has_area())
      continue;

    gfx::Rect request_in_layer_space = request->area();
    gfx::Rect request_in_content_space =
        LayerRectToContentRect(request_in_layer_space);
    request->set_area(MathUtil::MapEnclosingClippedRect(
        draw_properties_.target_space_transform, request_in_content_space));
  }

  layer_tree_impl()->RemoveLayerWithCopyOutputRequest(this);
}

void LayerImpl::CreateRenderSurface() {
  DCHECK(!draw_properties_.render_surface);
  draw_properties_.render_surface =
      make_scoped_ptr(new RenderSurfaceImpl(this));
  draw_properties_.render_target = this;
}

void LayerImpl::ClearRenderSurface() {
  draw_properties_.render_surface = nullptr;
}

void LayerImpl::ClearRenderSurfaceLayerList() {
  if (draw_properties_.render_surface)
    draw_properties_.render_surface->layer_list().clear();
}

void LayerImpl::PopulateSharedQuadState(SharedQuadState* state) const {
  state->SetAll(draw_properties_.target_space_transform,
                draw_properties_.content_bounds,
                draw_properties_.visible_content_rect,
                draw_properties_.clip_rect,
                draw_properties_.is_clipped,
                draw_properties_.opacity,
                blend_mode_,
                sorting_context_id_);
}

bool LayerImpl::WillDraw(DrawMode draw_mode,
                         ResourceProvider* resource_provider) {
  // WillDraw/DidDraw must be matched.
  DCHECK_NE(DRAW_MODE_NONE, draw_mode);
  DCHECK_EQ(DRAW_MODE_NONE, current_draw_mode_);
  current_draw_mode_ = draw_mode;
  return true;
}

void LayerImpl::DidDraw(ResourceProvider* resource_provider) {
  DCHECK_NE(DRAW_MODE_NONE, current_draw_mode_);
  current_draw_mode_ = DRAW_MODE_NONE;
}

bool LayerImpl::ShowDebugBorders() const {
  return layer_tree_impl()->debug_state().show_debug_borders;
}

void LayerImpl::GetDebugBorderProperties(SkColor* color, float* width) const {
  if (draws_content_) {
    *color = DebugColors::ContentLayerBorderColor();
    *width = DebugColors::ContentLayerBorderWidth(layer_tree_impl());
    return;
  }

  if (masks_to_bounds_) {
    *color = DebugColors::MaskingLayerBorderColor();
    *width = DebugColors::MaskingLayerBorderWidth(layer_tree_impl());
    return;
  }

  *color = DebugColors::ContainerLayerBorderColor();
  *width = DebugColors::ContainerLayerBorderWidth(layer_tree_impl());
}

void LayerImpl::AppendDebugBorderQuad(
    RenderPass* render_pass,
    const gfx::Size& content_bounds,
    const SharedQuadState* shared_quad_state,
    AppendQuadsData* append_quads_data) const {
  SkColor color;
  float width;
  GetDebugBorderProperties(&color, &width);
  AppendDebugBorderQuad(render_pass,
                        content_bounds,
                        shared_quad_state,
                        append_quads_data,
                        color,
                        width);
}

void LayerImpl::AppendDebugBorderQuad(RenderPass* render_pass,
                                      const gfx::Size& content_bounds,
                                      const SharedQuadState* shared_quad_state,
                                      AppendQuadsData* append_quads_data,
                                      SkColor color,
                                      float width) const {
  if (!ShowDebugBorders())
    return;

  gfx::Rect quad_rect(content_bounds);
  gfx::Rect visible_quad_rect(quad_rect);
  DebugBorderDrawQuad* debug_border_quad =
      render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debug_border_quad->SetNew(
      shared_quad_state, quad_rect, visible_quad_rect, color, width);
}

bool LayerImpl::HasDelegatedContent() const {
  return false;
}

bool LayerImpl::HasContributingDelegatedRenderPasses() const {
  return false;
}

RenderPassId LayerImpl::FirstContributingRenderPassId() const {
  return RenderPassId(0, 0);
}

RenderPassId LayerImpl::NextContributingRenderPassId(RenderPassId id) const {
  return RenderPassId(0, 0);
}

void LayerImpl::GetContentsResourceId(ResourceProvider::ResourceId* resource_id,
                                      gfx::Size* resource_size) const {
  NOTREACHED();
  *resource_id = 0;
}

void LayerImpl::SetSentScrollDelta(const gfx::Vector2dF& sent_scroll_delta) {
  // Pending tree never has sent scroll deltas
  DCHECK(layer_tree_impl()->IsActiveTree());

  if (sent_scroll_delta_ == sent_scroll_delta)
    return;

  sent_scroll_delta_ = sent_scroll_delta;
}

gfx::Vector2dF LayerImpl::ScrollBy(const gfx::Vector2dF& scroll) {
  gfx::Vector2dF adjusted_scroll = scroll;
  if (layer_tree_impl()->settings().use_pinch_virtual_viewport) {
    if (!user_scrollable_horizontal_)
      adjusted_scroll.set_x(0);
    if (!user_scrollable_vertical_)
      adjusted_scroll.set_y(0);
  }
  DCHECK(scrollable());
  gfx::Vector2dF min_delta = -ScrollOffsetToVector2dF(scroll_offset_);
  gfx::Vector2dF max_delta = MaxScrollOffset().DeltaFrom(scroll_offset_);
  // Clamp new_delta so that position + delta stays within scroll bounds.
  gfx::Vector2dF new_delta = (ScrollDelta() + adjusted_scroll);
  new_delta.SetToMax(min_delta);
  new_delta.SetToMin(max_delta);
  gfx::Vector2dF unscrolled =
      ScrollDelta() + scroll - new_delta;
  SetScrollDelta(new_delta);

  return unscrolled;
}

void LayerImpl::SetScrollClipLayer(int scroll_clip_layer_id) {
  scroll_clip_layer_ = layer_tree_impl()->LayerById(scroll_clip_layer_id);
}

bool LayerImpl::user_scrollable(ScrollbarOrientation orientation) const {
  return (orientation == HORIZONTAL) ? user_scrollable_horizontal_
                                     : user_scrollable_vertical_;
}

void LayerImpl::ApplySentScrollDeltasFromAbortedCommit() {
  if (sent_scroll_delta_.IsZero())
    return;

  // Pending tree never has sent scroll deltas
  DCHECK(layer_tree_impl()->IsActiveTree());

  // The combination of pending tree and aborted commits with impl scrolls
  // shouldn't happen; we don't know how to update its deltas correctly.
  DCHECK(!layer_tree_impl()->FindPendingTreeLayerById(id()));

  // Apply sent scroll deltas to scroll position / scroll delta as if the
  // main thread had applied them and then committed those values.
  SetScrollOffsetAndDelta(
      scroll_offset_ + gfx::ScrollOffset(sent_scroll_delta_),
      ScrollDelta() - sent_scroll_delta_);
  SetSentScrollDelta(gfx::Vector2dF());
}

void LayerImpl::ApplyScrollDeltasSinceBeginMainFrame() {
  // Only the pending tree can have missing scrolls.
  DCHECK(layer_tree_impl()->IsPendingTree());
  if (!scrollable())
    return;

  // Pending tree should never have sent scroll deltas.
  DCHECK(sent_scroll_delta().IsZero());

  LayerImpl* active_twin = layer_tree_impl()->FindActiveTreeLayerById(id());
  if (active_twin) {
    // Scrolls that happens after begin frame (where the sent scroll delta
    // comes from) and commit need to be applied to the pending tree
    // so that it is up to date with the total scroll.
    SetScrollDelta(active_twin->ScrollDelta() -
                   active_twin->sent_scroll_delta());
  }
}

InputHandler::ScrollStatus LayerImpl::TryScroll(
    const gfx::PointF& screen_space_point,
    InputHandler::ScrollInputType type) const {
  if (should_scroll_on_main_thread()) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Failed ShouldScrollOnMainThread");
    return InputHandler::ScrollOnMainThread;
  }

  if (!screen_space_transform().IsInvertible()) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Ignored NonInvertibleTransform");
    return InputHandler::ScrollIgnored;
  }

  if (!non_fast_scrollable_region().IsEmpty()) {
    bool clipped = false;
    gfx::Transform inverse_screen_space_transform(
        gfx::Transform::kSkipInitialization);
    if (!screen_space_transform().GetInverse(&inverse_screen_space_transform)) {
      // TODO(shawnsingh): We shouldn't be applying a projection if screen space
      // transform is uninvertible here. Perhaps we should be returning
      // ScrollOnMainThread in this case?
    }

    gfx::PointF hit_test_point_in_content_space =
        MathUtil::ProjectPoint(inverse_screen_space_transform,
                               screen_space_point,
                               &clipped);
    gfx::PointF hit_test_point_in_layer_space =
        gfx::ScalePoint(hit_test_point_in_content_space,
                        1.f / contents_scale_x(),
                        1.f / contents_scale_y());
    if (!clipped &&
        non_fast_scrollable_region().Contains(
            gfx::ToRoundedPoint(hit_test_point_in_layer_space))) {
      TRACE_EVENT0("cc",
                   "LayerImpl::tryScroll: Failed NonFastScrollableRegion");
      return InputHandler::ScrollOnMainThread;
    }
  }

  if (type == InputHandler::Wheel && have_wheel_event_handlers()) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Failed WheelEventHandlers");
    return InputHandler::ScrollOnMainThread;
  }

  if (!scrollable()) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Ignored not scrollable");
    return InputHandler::ScrollIgnored;
  }

  gfx::ScrollOffset max_scroll_offset = MaxScrollOffset();
  if (max_scroll_offset.x() <= 0 && max_scroll_offset.y() <= 0) {
    TRACE_EVENT0("cc",
                 "LayerImpl::tryScroll: Ignored. Technically scrollable,"
                 " but has no affordance in either direction.");
    return InputHandler::ScrollIgnored;
  }

  return InputHandler::ScrollStarted;
}

gfx::Rect LayerImpl::LayerRectToContentRect(
    const gfx::RectF& layer_rect) const {
  gfx::RectF content_rect =
      gfx::ScaleRect(layer_rect, contents_scale_x(), contents_scale_y());
  // Intersect with content rect to avoid the extra pixel because for some
  // values x and y, ceil((x / y) * y) may be x + 1.
  content_rect.Intersect(gfx::Rect(content_bounds()));
  return gfx::ToEnclosingRect(content_rect);
}

skia::RefPtr<SkPicture> LayerImpl::GetPicture() {
  return skia::RefPtr<SkPicture>();
}

scoped_ptr<LayerImpl> LayerImpl::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return LayerImpl::Create(tree_impl, layer_id_);
}

void LayerImpl::PushPropertiesTo(LayerImpl* layer) {
  layer->SetTransformOrigin(transform_origin_);
  layer->SetBackgroundColor(background_color_);
  layer->SetBounds(bounds_);
  layer->SetContentBounds(content_bounds());
  layer->SetContentsScale(contents_scale_x(), contents_scale_y());
  layer->SetDoubleSided(double_sided_);
  layer->SetDrawCheckerboardForMissingTiles(
      draw_checkerboard_for_missing_tiles_);
  layer->SetForceRenderSurface(force_render_surface_);
  layer->SetDrawsContent(DrawsContent());
  layer->SetHideLayerAndSubtree(hide_layer_and_subtree_);
  layer->SetFilters(filters());
  layer->SetBackgroundFilters(background_filters());
  layer->SetMasksToBounds(masks_to_bounds_);
  layer->SetShouldScrollOnMainThread(should_scroll_on_main_thread_);
  layer->SetHaveWheelEventHandlers(have_wheel_event_handlers_);
  layer->SetHaveScrollEventHandlers(have_scroll_event_handlers_);
  layer->SetNonFastScrollableRegion(non_fast_scrollable_region_);
  layer->SetTouchEventHandlerRegion(touch_event_handler_region_);
  layer->SetContentsOpaque(contents_opaque_);
  layer->SetOpacity(opacity_);
  layer->SetBlendMode(blend_mode_);
  layer->SetIsRootForIsolatedGroup(is_root_for_isolated_group_);
  layer->SetPosition(position_);
  layer->SetIsContainerForFixedPositionLayers(
      is_container_for_fixed_position_layers_);
  layer->SetPositionConstraint(position_constraint_);
  layer->SetShouldFlattenTransform(should_flatten_transform_);
  layer->SetUseParentBackfaceVisibility(use_parent_backface_visibility_);
  layer->SetTransformAndInvertibility(transform_, transform_is_invertible_);

  layer->SetScrollClipLayer(scroll_clip_layer_ ? scroll_clip_layer_->id()
                                               : Layer::INVALID_ID);
  layer->set_user_scrollable_horizontal(user_scrollable_horizontal_);
  layer->set_user_scrollable_vertical(user_scrollable_vertical_);

  // Save the difference but clear the sent delta so that we don't subtract
  // it again in SetScrollOffsetAndDelta's pending twin mirroring logic.
  gfx::Vector2dF remaining_delta =
      layer->ScrollDelta() - layer->sent_scroll_delta();
  layer->SetSentScrollDelta(gfx::Vector2dF());
  layer->SetScrollOffsetAndDelta(scroll_offset_, remaining_delta);

  layer->Set3dSortingContextId(sorting_context_id_);
  layer->SetNumDescendantsThatDrawContent(num_descendants_that_draw_content_);

  LayerImpl* scroll_parent = nullptr;
  if (scroll_parent_) {
    scroll_parent = layer->layer_tree_impl()->LayerById(scroll_parent_->id());
    DCHECK(scroll_parent);
  }

  layer->SetScrollParent(scroll_parent);
  if (scroll_children_) {
    std::set<LayerImpl*>* scroll_children = new std::set<LayerImpl*>;
    for (std::set<LayerImpl*>::iterator it = scroll_children_->begin();
         it != scroll_children_->end();
         ++it) {
      DCHECK_EQ((*it)->scroll_parent(), this);
      LayerImpl* scroll_child =
          layer->layer_tree_impl()->LayerById((*it)->id());
      DCHECK(scroll_child);
      scroll_children->insert(scroll_child);
    }
    layer->SetScrollChildren(scroll_children);
  } else {
    layer->SetScrollChildren(nullptr);
  }

  LayerImpl* clip_parent = nullptr;
  if (clip_parent_) {
    clip_parent = layer->layer_tree_impl()->LayerById(
        clip_parent_->id());
    DCHECK(clip_parent);
  }

  layer->SetClipParent(clip_parent);
  if (clip_children_) {
    std::set<LayerImpl*>* clip_children = new std::set<LayerImpl*>;
    for (std::set<LayerImpl*>::iterator it = clip_children_->begin();
        it != clip_children_->end(); ++it)
      clip_children->insert(layer->layer_tree_impl()->LayerById((*it)->id()));
    layer->SetClipChildren(clip_children);
  } else {
    layer->SetClipChildren(nullptr);
  }

  layer->PassCopyRequests(&copy_requests_);

  // If the main thread commits multiple times before the impl thread actually
  // draws, then damage tracking will become incorrect if we simply clobber the
  // update_rect here. The LayerImpl's update_rect needs to accumulate (i.e.
  // union) any update changes that have occurred on the main thread.
  update_rect_.Union(layer->update_rect());
  layer->SetUpdateRect(update_rect_);

  layer->SetStackingOrderChanged(stacking_order_changed_);
  layer->SetDebugInfo(debug_info_);

  // Reset any state that should be cleared for the next update.
  stacking_order_changed_ = false;
  update_rect_ = gfx::Rect();
  needs_push_properties_ = false;
  num_dependents_need_push_properties_ = 0;
}

gfx::Vector2dF LayerImpl::FixedContainerSizeDelta() const {
  if (!scroll_clip_layer_)
    return gfx::Vector2dF();

  float scale_delta = layer_tree_impl()->page_scale_delta();
  float scale = layer_tree_impl()->page_scale_factor();

  gfx::Vector2dF delta_from_scroll = scroll_clip_layer_->bounds_delta();

  // In virtual-viewport mode, we don't need to compensate for pinch zoom or
  // scale since the fixed container is the outer viewport, which sits below
  // the page scale.
  if (layer_tree_impl()->settings().use_pinch_virtual_viewport)
    return delta_from_scroll;

  delta_from_scroll.Scale(1.f / scale);

  // The delta-from-pinch component requires some explanation: A viewport of
  // size (w,h) will appear to be size (w/s,h/s) under scale s in the content
  // space. If s -> s' on the impl thread, where s' = s * ds, then the apparent
  // viewport size change in the content space due to ds is:
  //
  // (w/s',h/s') - (w/s,h/s) = (w,h)(1/s' - 1/s) = (w,h)(1 - ds)/(s ds)
  //
  gfx::Vector2dF delta_from_pinch =
      gfx::Rect(scroll_clip_layer_->bounds()).bottom_right() - gfx::PointF();
  delta_from_pinch.Scale((1.f - scale_delta) / (scale * scale_delta));

  return delta_from_scroll + delta_from_pinch;
}

base::DictionaryValue* LayerImpl::LayerTreeAsJson() const {
  base::DictionaryValue* result = new base::DictionaryValue;
  result->SetString("LayerType", LayerTypeAsString());

  base::ListValue* list = new base::ListValue;
  list->AppendInteger(bounds().width());
  list->AppendInteger(bounds().height());
  result->Set("Bounds", list);

  list = new base::ListValue;
  list->AppendDouble(position_.x());
  list->AppendDouble(position_.y());
  result->Set("Position", list);

  const gfx::Transform& gfx_transform = draw_properties_.target_space_transform;
  double transform[16];
  gfx_transform.matrix().asColMajord(transform);
  list = new base::ListValue;
  for (int i = 0; i < 16; ++i)
    list->AppendDouble(transform[i]);
  result->Set("DrawTransform", list);

  result->SetBoolean("DrawsContent", draws_content_);
  result->SetBoolean("Is3dSorted", Is3dSorted());
  result->SetDouble("Opacity", opacity());
  result->SetBoolean("ContentsOpaque", contents_opaque_);

  if (scrollable())
    result->SetBoolean("Scrollable", true);

  if (have_wheel_event_handlers_)
    result->SetBoolean("WheelHandler", have_wheel_event_handlers_);
  if (have_scroll_event_handlers_)
    result->SetBoolean("ScrollHandler", have_scroll_event_handlers_);
  if (!touch_event_handler_region_.IsEmpty()) {
    scoped_ptr<base::Value> region = touch_event_handler_region_.AsValue();
    result->Set("TouchRegion", region.release());
  }

  list = new base::ListValue;
  for (size_t i = 0; i < children_.size(); ++i)
    list->Append(children_[i]->LayerTreeAsJson());
  result->Set("Children", list);

  return result;
}

void LayerImpl::SetStackingOrderChanged(bool stacking_order_changed) {
  if (stacking_order_changed) {
    stacking_order_changed_ = true;
    NoteLayerPropertyChangedForSubtree();
  }
}

void LayerImpl::NoteLayerPropertyChanged() {
  layer_property_changed_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
  SetNeedsPushProperties();
}

void LayerImpl::NoteLayerPropertyChangedForSubtree() {
  layer_property_changed_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->NoteLayerPropertyChangedForDescendantsInternal();
  SetNeedsPushProperties();
}

void LayerImpl::NoteLayerPropertyChangedForDescendantsInternal() {
  layer_property_changed_ = true;
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->NoteLayerPropertyChangedForDescendantsInternal();
}

void LayerImpl::NoteLayerPropertyChangedForDescendants() {
  layer_tree_impl()->set_needs_update_draw_properties();
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->NoteLayerPropertyChangedForDescendantsInternal();
  SetNeedsPushProperties();
}

const char* LayerImpl::LayerTypeAsString() const {
  return "cc::LayerImpl";
}

void LayerImpl::ResetAllChangeTrackingForSubtree() {
  layer_property_changed_ = false;

  update_rect_ = gfx::Rect();
  damage_rect_ = gfx::RectF();

  if (draw_properties_.render_surface)
    draw_properties_.render_surface->ResetPropertyChangedFlag();

  if (mask_layer_)
    mask_layer_->ResetAllChangeTrackingForSubtree();

  if (replica_layer_) {
    // This also resets the replica mask, if it exists.
    replica_layer_->ResetAllChangeTrackingForSubtree();
  }

  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->ResetAllChangeTrackingForSubtree();

  needs_push_properties_ = false;
  num_dependents_need_push_properties_ = 0;
}

gfx::ScrollOffset LayerImpl::ScrollOffsetForAnimation() const {
  return TotalScrollOffset();
}

void LayerImpl::OnFilterAnimated(const FilterOperations& filters) {
  SetFilters(filters);
}

void LayerImpl::OnOpacityAnimated(float opacity) {
  SetOpacity(opacity);
}

void LayerImpl::OnTransformAnimated(const gfx::Transform& transform) {
  SetTransform(transform);
}

void LayerImpl::OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset) {
  // Only layers in the active tree should need to do anything here, since
  // layers in the pending tree will find out about these changes as a
  // result of the call to SetScrollDelta.
  if (!IsActive())
    return;

  SetScrollDelta(scroll_offset.DeltaFrom(scroll_offset_));

  layer_tree_impl_->DidAnimateScrollOffset();
}

void LayerImpl::OnAnimationWaitingForDeletion() {}

bool LayerImpl::IsActive() const {
  return layer_tree_impl_->IsActiveTree();
}

gfx::Size LayerImpl::bounds() const {
  gfx::Vector2d delta = gfx::ToCeiledVector2d(bounds_delta_);
  return gfx::Size(bounds_.width() + delta.x(),
                   bounds_.height() + delta.y());
}

gfx::SizeF LayerImpl::BoundsForScrolling() const {
  return gfx::SizeF(bounds_.width() + bounds_delta_.x(),
                    bounds_.height() + bounds_delta_.y());
}

void LayerImpl::SetBounds(const gfx::Size& bounds) {
  if (bounds_ == bounds)
    return;

  bounds_ = bounds;

  ScrollbarParametersDidChange(true);
  if (masks_to_bounds())
    NoteLayerPropertyChangedForSubtree();
  else
    NoteLayerPropertyChanged();
}

void LayerImpl::SetBoundsDelta(const gfx::Vector2dF& bounds_delta) {
  if (bounds_delta_ == bounds_delta)
    return;

  bounds_delta_ = bounds_delta;

  ScrollbarParametersDidChange(true);
  if (masks_to_bounds())
    NoteLayerPropertyChangedForSubtree();
  else
    NoteLayerPropertyChanged();
}

void LayerImpl::SetMaskLayer(scoped_ptr<LayerImpl> mask_layer) {
  int new_layer_id = mask_layer ? mask_layer->id() : -1;

  if (mask_layer) {
    DCHECK_EQ(layer_tree_impl(), mask_layer->layer_tree_impl());
    DCHECK_NE(new_layer_id, mask_layer_id_);
  } else if (new_layer_id == mask_layer_id_) {
    return;
  }

  mask_layer_ = mask_layer.Pass();
  mask_layer_id_ = new_layer_id;
  if (mask_layer_)
    mask_layer_->SetParent(this);
  NoteLayerPropertyChangedForSubtree();
}

scoped_ptr<LayerImpl> LayerImpl::TakeMaskLayer() {
  mask_layer_id_ = -1;
  return mask_layer_.Pass();
}

void LayerImpl::SetReplicaLayer(scoped_ptr<LayerImpl> replica_layer) {
  int new_layer_id = replica_layer ? replica_layer->id() : -1;

  if (replica_layer) {
    DCHECK_EQ(layer_tree_impl(), replica_layer->layer_tree_impl());
    DCHECK_NE(new_layer_id, replica_layer_id_);
  } else if (new_layer_id == replica_layer_id_) {
    return;
  }

  replica_layer_ = replica_layer.Pass();
  replica_layer_id_ = new_layer_id;
  if (replica_layer_)
    replica_layer_->SetParent(this);
  NoteLayerPropertyChangedForSubtree();
}

scoped_ptr<LayerImpl> LayerImpl::TakeReplicaLayer() {
  replica_layer_id_ = -1;
  return replica_layer_.Pass();
}

ScrollbarLayerImplBase* LayerImpl::ToScrollbarLayer() {
  return nullptr;
}

void LayerImpl::SetDrawsContent(bool draws_content) {
  if (draws_content_ == draws_content)
    return;

  draws_content_ = draws_content;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetHideLayerAndSubtree(bool hide) {
  if (hide_layer_and_subtree_ == hide)
    return;

  hide_layer_and_subtree_ = hide;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetTransformOrigin(const gfx::Point3F& transform_origin) {
  if (transform_origin_ == transform_origin)
    return;
  transform_origin_ = transform_origin;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  NoteLayerPropertyChanged();
}

SkColor LayerImpl::SafeOpaqueBackgroundColor() const {
  SkColor color = background_color();
  if (SkColorGetA(color) == 255 && !contents_opaque()) {
    color = SK_ColorTRANSPARENT;
  } else if (SkColorGetA(color) != 255 && contents_opaque()) {
    for (const LayerImpl* layer = parent(); layer;
         layer = layer->parent()) {
      color = layer->background_color();
      if (SkColorGetA(color) == 255)
        break;
    }
    if (SkColorGetA(color) != 255)
      color = layer_tree_impl()->background_color();
    if (SkColorGetA(color) != 255)
      color = SkColorSetA(color, 255);
  }
  return color;
}

void LayerImpl::SetFilters(const FilterOperations& filters) {
  if (filters_ == filters)
    return;

  filters_ = filters;
  NoteLayerPropertyChangedForSubtree();
}

bool LayerImpl::FilterIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::Filter);
}

bool LayerImpl::FilterIsAnimatingOnImplOnly() const {
  Animation* filter_animation =
      layer_animation_controller_->GetAnimation(Animation::Filter);
  return filter_animation && filter_animation->is_impl_only();
}

void LayerImpl::SetBackgroundFilters(
    const FilterOperations& filters) {
  if (background_filters_ == filters)
    return;

  background_filters_ = filters;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetMasksToBounds(bool masks_to_bounds) {
  if (masks_to_bounds_ == masks_to_bounds)
    return;

  masks_to_bounds_ = masks_to_bounds;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetContentsOpaque(bool opaque) {
  if (contents_opaque_ == opaque)
    return;

  contents_opaque_ = opaque;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetOpacity(float opacity) {
  if (opacity_ == opacity)
    return;

  opacity_ = opacity;
  NoteLayerPropertyChangedForSubtree();
}

bool LayerImpl::OpacityIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::Opacity);
}

bool LayerImpl::OpacityIsAnimatingOnImplOnly() const {
  Animation* opacity_animation =
      layer_animation_controller_->GetAnimation(Animation::Opacity);
  return opacity_animation && opacity_animation->is_impl_only();
}

void LayerImpl::SetBlendMode(SkXfermode::Mode blend_mode) {
  if (blend_mode_ == blend_mode)
    return;

  blend_mode_ = blend_mode;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetIsRootForIsolatedGroup(bool root) {
  if (is_root_for_isolated_group_ == root)
    return;

  is_root_for_isolated_group_ = root;
  SetNeedsPushProperties();
}

void LayerImpl::SetPosition(const gfx::PointF& position) {
  if (position_ == position)
    return;

  position_ = position;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetShouldFlattenTransform(bool flatten) {
  if (should_flatten_transform_ == flatten)
    return;

  should_flatten_transform_ = flatten;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::Set3dSortingContextId(int id) {
  if (id == sorting_context_id_)
    return;
  sorting_context_id_ = id;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetTransform(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;

  transform_ = transform;
  transform_is_invertible_ = transform_.IsInvertible();
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetTransformAndInvertibility(const gfx::Transform& transform,
                                             bool transform_is_invertible) {
  if (transform_ == transform) {
    DCHECK(transform_is_invertible_ == transform_is_invertible)
        << "Can't change invertibility if transform is unchanged";
    return;
  }
  transform_ = transform;
  transform_is_invertible_ = transform_is_invertible;
  NoteLayerPropertyChangedForSubtree();
}

bool LayerImpl::TransformIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::Transform);
}

bool LayerImpl::TransformIsAnimatingOnImplOnly() const {
  Animation* transform_animation =
      layer_animation_controller_->GetAnimation(Animation::Transform);
  return transform_animation && transform_animation->is_impl_only();
}

void LayerImpl::SetUpdateRect(const gfx::Rect& update_rect) {
  update_rect_ = update_rect;
  SetNeedsPushProperties();
}

void LayerImpl::AddDamageRect(const gfx::RectF& damage_rect) {
  damage_rect_ = gfx::UnionRects(damage_rect_, damage_rect);
}

void LayerImpl::SetContentBounds(const gfx::Size& content_bounds) {
  if (this->content_bounds() == content_bounds)
    return;

  draw_properties_.content_bounds = content_bounds;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetContentsScale(float contents_scale_x,
                                 float contents_scale_y) {
  if (this->contents_scale_x() == contents_scale_x &&
      this->contents_scale_y() == contents_scale_y)
    return;

  draw_properties_.contents_scale_x = contents_scale_x;
  draw_properties_.contents_scale_y = contents_scale_y;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetScrollOffsetDelegate(
    ScrollOffsetDelegate* scroll_offset_delegate) {
  // Having both a scroll parent and a scroll offset delegate is unsupported.
  DCHECK(!scroll_parent_);
  if (!scroll_offset_delegate && scroll_offset_delegate_) {
    scroll_delta_ = scroll_offset_delegate_->GetTotalScrollOffset().DeltaFrom(
        scroll_offset_);
  }
  gfx::ScrollOffset total_offset = TotalScrollOffset();
  scroll_offset_delegate_ = scroll_offset_delegate;
  if (scroll_offset_delegate_)
    scroll_offset_delegate_->SetTotalScrollOffset(total_offset);
}

bool LayerImpl::IsExternalFlingActive() const {
  return scroll_offset_delegate_ &&
         scroll_offset_delegate_->IsExternalFlingActive();
}

void LayerImpl::DidScroll() {
  NoteLayerPropertyChangedForSubtree();
  ScrollbarParametersDidChange(false);
}

void LayerImpl::SetScrollOffset(const gfx::ScrollOffset& scroll_offset) {
  SetScrollOffsetAndDelta(scroll_offset, ScrollDelta());
}

void LayerImpl::SetScrollOffsetAndDelta(const gfx::ScrollOffset& scroll_offset,
                                        const gfx::Vector2dF& scroll_delta) {
  bool changed = false;

  last_scroll_offset_ = scroll_offset;

  if (scroll_offset_ != scroll_offset) {
    changed = true;
    scroll_offset_ = scroll_offset;

    if (scroll_offset_delegate_)
      scroll_offset_delegate_->SetTotalScrollOffset(TotalScrollOffset());
  }

  if (ScrollDelta() != scroll_delta) {
    changed = true;
    if (layer_tree_impl()->IsActiveTree()) {
      LayerImpl* pending_twin =
          layer_tree_impl()->FindPendingTreeLayerById(id());
      if (pending_twin) {
        // The pending twin can't mirror the scroll delta of the active
        // layer.  Although the delta - sent scroll delta difference is
        // identical for both twins, the sent scroll delta for the pending
        // layer is zero, as anything that has been sent has been baked
        // into the layer's position/scroll offset as a part of commit.
        DCHECK(pending_twin->sent_scroll_delta().IsZero());
        pending_twin->SetScrollDelta(scroll_delta - sent_scroll_delta());
      }
    }

    if (scroll_offset_delegate_) {
      scroll_offset_delegate_->SetTotalScrollOffset(
          ScrollOffsetWithDelta(scroll_offset_, scroll_delta));
    } else {
      scroll_delta_ = scroll_delta;
    }
  }

  if (changed) {
    if (scroll_offset_delegate_)
      scroll_offset_delegate_->Update();
    DidScroll();
  }
}

gfx::Vector2dF LayerImpl::ScrollDelta() const {
  if (scroll_offset_delegate_) {
    return scroll_offset_delegate_->GetTotalScrollOffset().DeltaFrom(
        scroll_offset_);
  }
  return scroll_delta_;
}

void LayerImpl::SetScrollDelta(const gfx::Vector2dF& scroll_delta) {
  SetScrollOffsetAndDelta(scroll_offset_, scroll_delta);
}

gfx::ScrollOffset LayerImpl::TotalScrollOffset() const {
  return ScrollOffsetWithDelta(scroll_offset_, ScrollDelta());
}

void LayerImpl::SetDoubleSided(bool double_sided) {
  if (double_sided_ == double_sided)
    return;

  double_sided_ = double_sided;
  NoteLayerPropertyChangedForSubtree();
}

SimpleEnclosedRegion LayerImpl::VisibleContentOpaqueRegion() const {
  if (contents_opaque())
    return SimpleEnclosedRegion(visible_content_rect());
  return SimpleEnclosedRegion();
}

void LayerImpl::DidBeginTracing() {}

void LayerImpl::ReleaseResources() {}

gfx::ScrollOffset LayerImpl::MaxScrollOffset() const {
  if (!scroll_clip_layer_ || bounds().IsEmpty())
    return gfx::ScrollOffset();

  LayerImpl const* page_scale_layer = layer_tree_impl()->page_scale_layer();
  DCHECK(this != page_scale_layer);
  DCHECK(this != layer_tree_impl()->InnerViewportScrollLayer() ||
         IsContainerForFixedPositionLayers());

  gfx::SizeF scaled_scroll_bounds(BoundsForScrolling());

  float scale_factor = 1.f;
  for (LayerImpl const* current_layer = this;
       current_layer != scroll_clip_layer_;
       current_layer = current_layer->parent()) {
    DCHECK(current_layer);
    float current_layer_scale = 1.f;

    const gfx::Transform& layer_transform = current_layer->transform();
    if (current_layer == page_scale_layer) {
      DCHECK(layer_transform.IsIdentity());
      current_layer_scale = layer_tree_impl()->total_page_scale_factor();
    } else {
      // TODO(wjmaclean) Should we allow for translation too?
      DCHECK(layer_transform.IsScale2d());
      gfx::Vector2dF layer_scale = layer_transform.Scale2d();
      // TODO(wjmaclean) Allow for non-isotropic scales.
      DCHECK(layer_scale.x() == layer_scale.y());
      current_layer_scale = layer_scale.x();
    }

    scale_factor *= current_layer_scale;
  }
  // TODO(wjmaclean) Once we move to a model where the two-viewport model is
  // turned on in all builds, remove the next two lines. For now however, the
  // page scale layer may coincide with the clip layer, and so this is
  // necessary.
  if (page_scale_layer == scroll_clip_layer_)
    scale_factor *= layer_tree_impl()->total_page_scale_factor();

  scaled_scroll_bounds.SetSize(scale_factor * scaled_scroll_bounds.width(),
                               scale_factor * scaled_scroll_bounds.height());
  scaled_scroll_bounds = gfx::ToFlooredSize(scaled_scroll_bounds);

  gfx::ScrollOffset max_offset(
      scaled_scroll_bounds.width() - scroll_clip_layer_->bounds().width(),
      scaled_scroll_bounds.height() - scroll_clip_layer_->bounds().height());
  // We need the final scroll offset to be in CSS coords.
  max_offset.Scale(1 / scale_factor);
  max_offset.SetToMax(gfx::ScrollOffset());
  return max_offset;
}

gfx::Vector2dF LayerImpl::ClampScrollToMaxScrollOffset() {
  gfx::ScrollOffset max_offset = MaxScrollOffset();
  gfx::ScrollOffset old_offset = TotalScrollOffset();
  gfx::ScrollOffset clamped_offset = old_offset;

  clamped_offset.SetToMin(max_offset);
  clamped_offset.SetToMax(gfx::ScrollOffset());
  gfx::Vector2dF delta = clamped_offset.DeltaFrom(old_offset);
  if (!delta.IsZero())
    ScrollBy(delta);

  return delta;
}

void LayerImpl::SetScrollbarPosition(ScrollbarLayerImplBase* scrollbar_layer,
                                     LayerImpl* scrollbar_clip_layer,
                                     bool on_resize) const {
  DCHECK(scrollbar_layer);
  LayerImpl* page_scale_layer = layer_tree_impl()->page_scale_layer();

  DCHECK(this != page_scale_layer);
  DCHECK(scrollbar_clip_layer);
  DCHECK(this != layer_tree_impl()->InnerViewportScrollLayer() ||
         IsContainerForFixedPositionLayers());
  gfx::RectF clip_rect(gfx::PointF(),
                       scrollbar_clip_layer->BoundsForScrolling());

  // See comment in MaxScrollOffset() regarding the use of the content layer
  // bounds here.
  gfx::RectF scroll_rect(gfx::PointF(), BoundsForScrolling());

  if (scroll_rect.size().IsEmpty())
    return;

  // TODO(wjmaclean) This computation is nearly identical to the one in
  // MaxScrollOffset. Find some way to combine these.
  gfx::ScrollOffset current_offset;
  for (LayerImpl const* current_layer = this;
       current_layer != scrollbar_clip_layer;
       current_layer = current_layer->parent()) {
    DCHECK(current_layer);
    const gfx::Transform& layer_transform = current_layer->transform();
    if (current_layer == page_scale_layer) {
      DCHECK(layer_transform.IsIdentity());
      float scale_factor = layer_tree_impl()->total_page_scale_factor();
      current_offset.Scale(scale_factor);
      scroll_rect.Scale(scale_factor);
    } else {
      DCHECK(layer_transform.IsScale2d());
      gfx::Vector2dF layer_scale = layer_transform.Scale2d();
      DCHECK(layer_scale.x() == layer_scale.y());
      gfx::ScrollOffset new_offset = ScrollOffsetWithDelta(
          current_layer->scroll_offset(), current_layer->ScrollDelta());
      new_offset.Scale(layer_scale.x(), layer_scale.y());
      current_offset += new_offset;
    }
  }
  // TODO(wjmaclean) Once we move to a model where the two-viewport model is
  // turned on in all builds, remove the next two lines. For now however, the
  // page scale layer may coincide with the clip layer, and so this is
  // necessary.
  if (page_scale_layer == scrollbar_clip_layer) {
    scroll_rect.Scale(layer_tree_impl()->total_page_scale_factor());
    current_offset.Scale(layer_tree_impl()->total_page_scale_factor());
  }

  bool scrollbar_needs_animation = false;
  scrollbar_needs_animation |= scrollbar_layer->SetVerticalAdjust(
      scrollbar_clip_layer->bounds_delta().y());
  if (scrollbar_layer->orientation() == HORIZONTAL) {
    float visible_ratio = clip_rect.width() / scroll_rect.width();
    scrollbar_needs_animation |=
        scrollbar_layer->SetCurrentPos(current_offset.x());
    scrollbar_needs_animation |=
        scrollbar_layer->SetMaximum(scroll_rect.width() - clip_rect.width());
    scrollbar_needs_animation |=
        scrollbar_layer->SetVisibleToTotalLengthRatio(visible_ratio);
  } else {
    float visible_ratio = clip_rect.height() / scroll_rect.height();
    scrollbar_needs_animation |=
        scrollbar_layer->SetCurrentPos(current_offset.y());
    scrollbar_needs_animation |=
        scrollbar_layer->SetMaximum(scroll_rect.height() - clip_rect.height());
    scrollbar_needs_animation |=
        scrollbar_layer->SetVisibleToTotalLengthRatio(visible_ratio);
  }
  if (scrollbar_needs_animation) {
    layer_tree_impl()->set_needs_update_draw_properties();
    // TODO(wjmaclean) The scrollbar animator for the pinch-zoom scrollbars
    // should activate for every scroll on the main frame, not just the
    // scrolls that move the pinch virtual viewport (i.e. trigger from
    // either inner or outer viewport).
    if (scrollbar_animation_controller_) {
      // When both non-overlay and overlay scrollbars are both present, don't
      // animate the overlay scrollbars when page scale factor is at the min.
      // Non-overlay scrollbars also shouldn't trigger animations.
      bool is_animatable_scrollbar =
          scrollbar_layer->is_overlay_scrollbar() &&
          ((layer_tree_impl()->total_page_scale_factor() >
            layer_tree_impl()->min_page_scale_factor()) ||
           !layer_tree_impl()->settings().use_pinch_zoom_scrollbars);
      if (is_animatable_scrollbar)
        scrollbar_animation_controller_->DidScrollUpdate(on_resize);
    }
  }
}

void LayerImpl::DidBecomeActive() {
  if (layer_tree_impl_->settings().scrollbar_animator ==
      LayerTreeSettings::NoAnimator) {
    return;
  }

  bool need_scrollbar_animation_controller = scrollable() && scrollbars_;
  if (!need_scrollbar_animation_controller) {
    scrollbar_animation_controller_ = nullptr;
    return;
  }

  if (scrollbar_animation_controller_)
    return;

  scrollbar_animation_controller_ =
      layer_tree_impl_->CreateScrollbarAnimationController(this);
}

void LayerImpl::ClearScrollbars() {
  if (!scrollbars_)
    return;

  scrollbars_.reset(nullptr);
}

void LayerImpl::AddScrollbar(ScrollbarLayerImplBase* layer) {
  DCHECK(layer);
  DCHECK(!scrollbars_ || scrollbars_->find(layer) == scrollbars_->end());
  if (!scrollbars_)
    scrollbars_.reset(new ScrollbarSet());

  scrollbars_->insert(layer);
}

void LayerImpl::RemoveScrollbar(ScrollbarLayerImplBase* layer) {
  DCHECK(scrollbars_);
  DCHECK(layer);
  DCHECK(scrollbars_->find(layer) != scrollbars_->end());

  scrollbars_->erase(layer);
  if (scrollbars_->empty())
    scrollbars_ = nullptr;
}

bool LayerImpl::HasScrollbar(ScrollbarOrientation orientation) const {
  if (!scrollbars_)
    return false;

  for (ScrollbarSet::iterator it = scrollbars_->begin();
       it != scrollbars_->end();
       ++it)
    if ((*it)->orientation() == orientation)
      return true;

  return false;
}

void LayerImpl::ScrollbarParametersDidChange(bool on_resize) {
  if (!scrollbars_)
    return;

  for (ScrollbarSet::iterator it = scrollbars_->begin();
       it != scrollbars_->end();
       ++it) {
    bool is_scroll_layer = (*it)->ScrollLayerId() == layer_id_;
    bool scroll_layer_resized = is_scroll_layer && on_resize;
    (*it)->ScrollbarParametersDidChange(scroll_layer_resized);
  }
}

void LayerImpl::SetNeedsPushProperties() {
  if (needs_push_properties_)
    return;
  if (!parent_should_know_need_push_properties() && parent_)
    parent_->AddDependentNeedsPushProperties();
  needs_push_properties_ = true;
}

void LayerImpl::AddDependentNeedsPushProperties() {
  DCHECK_GE(num_dependents_need_push_properties_, 0);

  if (!parent_should_know_need_push_properties() && parent_)
    parent_->AddDependentNeedsPushProperties();

  num_dependents_need_push_properties_++;
}

void LayerImpl::RemoveDependentNeedsPushProperties() {
  num_dependents_need_push_properties_--;
  DCHECK_GE(num_dependents_need_push_properties_, 0);

  if (!parent_should_know_need_push_properties() && parent_)
      parent_->RemoveDependentNeedsPushProperties();
}

void LayerImpl::GetAllTilesForTracing(std::set<const Tile*>* tiles) const {
}

void LayerImpl::AsValueInto(base::debug::TracedValue* state) const {
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"),
      state,
      "cc::LayerImpl",
      LayerTypeAsString(),
      this);
  state->SetInteger("layer_id", id());
  state->BeginDictionary("bounds");
  MathUtil::AddToTracedValue(bounds_, state);
  state->EndDictionary();

  state->SetDouble("opacity", opacity());

  state->BeginArray("position");
  MathUtil::AddToTracedValue(position_, state);
  state->EndArray();

  state->SetInteger("draws_content", DrawsContent());
  state->SetInteger("gpu_memory_usage", GPUMemoryUsageInBytes());

  state->BeginArray("scroll_offset");
  MathUtil::AddToTracedValue(scroll_offset_, state);
  state->EndArray();

  state->BeginArray("transform_origin");
  MathUtil::AddToTracedValue(transform_origin_, state);
  state->EndArray();

  bool clipped;
  gfx::QuadF layer_quad = MathUtil::MapQuad(
      screen_space_transform(),
      gfx::QuadF(gfx::Rect(content_bounds())),
      &clipped);
  state->BeginArray("layer_quad");
  MathUtil::AddToTracedValue(layer_quad, state);
  state->EndArray();
  if (!touch_event_handler_region_.IsEmpty()) {
    state->BeginArray("touch_event_handler_region");
    touch_event_handler_region_.AsValueInto(state);
    state->EndArray();
  }
  if (have_wheel_event_handlers_) {
    gfx::Rect wheel_rect(content_bounds());
    Region wheel_region(wheel_rect);
    state->BeginArray("wheel_event_handler_region");
    wheel_region.AsValueInto(state);
    state->EndArray();
  }
  if (have_scroll_event_handlers_) {
    gfx::Rect scroll_rect(content_bounds());
    Region scroll_region(scroll_rect);
    state->BeginArray("scroll_event_handler_region");
    scroll_region.AsValueInto(state);
    state->EndArray();
  }
  if (!non_fast_scrollable_region_.IsEmpty()) {
    state->BeginArray("non_fast_scrollable_region");
    non_fast_scrollable_region_.AsValueInto(state);
    state->EndArray();
  }

  state->BeginArray("children");
  for (size_t i = 0; i < children_.size(); ++i) {
    state->BeginDictionary();
    children_[i]->AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();
  if (mask_layer_) {
    state->BeginDictionary("mask_layer");
    mask_layer_->AsValueInto(state);
    state->EndDictionary();
  }
  if (replica_layer_) {
    state->BeginDictionary("replica_layer");
    replica_layer_->AsValueInto(state);
    state->EndDictionary();
  }

  if (scroll_parent_)
    state->SetInteger("scroll_parent", scroll_parent_->id());

  if (clip_parent_)
    state->SetInteger("clip_parent", clip_parent_->id());

  state->SetBoolean("can_use_lcd_text", can_use_lcd_text());
  state->SetBoolean("contents_opaque", contents_opaque());

  state->SetBoolean(
      "has_animation_bounds",
      layer_animation_controller()->HasAnimationThatInflatesBounds());

  gfx::BoxF box;
  if (LayerUtils::GetAnimationBounds(*this, &box)) {
    state->BeginArray("animation_bounds");
    MathUtil::AddToTracedValue(box, state);
    state->EndArray();
  }

  if (debug_info_.get()) {
    std::string str;
    debug_info_->AppendAsTraceFormat(&str);
    base::JSONReader json_reader;
    scoped_ptr<base::Value> debug_info_value(json_reader.ReadToValue(str));

    if (debug_info_value->IsType(base::Value::TYPE_DICTIONARY)) {
      base::DictionaryValue* dictionary_value = nullptr;
      bool converted_to_dictionary =
          debug_info_value->GetAsDictionary(&dictionary_value);
      DCHECK(converted_to_dictionary);
      for (base::DictionaryValue::Iterator it(*dictionary_value); !it.IsAtEnd();
           it.Advance()) {
        state->SetValue(it.key().data(), it.value().DeepCopy());
      }
    } else {
      NOTREACHED();
    }
  }
}

bool LayerImpl::IsDrawnRenderSurfaceLayerListMember() const {
  return draw_properties_.last_drawn_render_surface_layer_list_id ==
         layer_tree_impl_->current_render_surface_list_id();
}

size_t LayerImpl::GPUMemoryUsageInBytes() const { return 0; }

void LayerImpl::RunMicroBenchmark(MicroBenchmarkImpl* benchmark) {
  benchmark->RunOnLayer(this);
}

int LayerImpl::NumDescendantsThatDrawContent() const {
  return num_descendants_that_draw_content_;
}

void LayerImpl::NotifyAnimationFinished(
    base::TimeTicks monotonic_time,
    Animation::TargetProperty target_property,
    int group) {
  if (target_property == Animation::ScrollOffset)
    layer_tree_impl_->InputScrollAnimationFinished();
}

}  // namespace cc
