// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_H_

#include "cc/output/viewport_selection_bound.h"
#include "content/browser/renderer_host/input/selection_event_type.h"
#include "content/browser/renderer_host/input/touch_handle.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {
class WebInputEvent;
}

namespace ui {
class MotionEvent;
}

namespace content {

// Interface through which |TouchSelectionController| issues selection-related
// commands, notifications and requests.
class CONTENT_EXPORT TouchSelectionControllerClient {
 public:
  virtual ~TouchSelectionControllerClient() {}

  virtual bool SupportsAnimation() const = 0;
  virtual void SetNeedsAnimate() = 0;
  virtual void MoveCaret(const gfx::PointF& position) = 0;
  virtual void MoveRangeSelectionExtent(const gfx::PointF& extent) = 0;
  virtual void SelectBetweenCoordinates(const gfx::PointF& base,
                                        const gfx::PointF& extent) = 0;
  virtual void OnSelectionEvent(SelectionEventType event,
                                const gfx::PointF& position) = 0;
  virtual scoped_ptr<TouchHandleDrawable> CreateDrawable() = 0;
};

// Controller for manipulating text selection via touch input.
class CONTENT_EXPORT TouchSelectionController : public TouchHandleClient {
 public:
  TouchSelectionController(TouchSelectionControllerClient* client,
                           base::TimeDelta tap_timeout,
                           float tap_slop);
  ~TouchSelectionController() override;

  // To be called when the selection bounds have changed.
  // Note that such updates will trigger handle updates only if preceded
  // by an appropriate call to allow automatic showing.
  void OnSelectionBoundsChanged(const cc::ViewportSelectionBound& start,
                                const cc::ViewportSelectionBound& end);

  // Allows touch-dragging of the handle.
  // Returns true iff the event was consumed, in which case the caller should
  // cease further handling of the event.
  bool WillHandleTouchEvent(const ui::MotionEvent& event);

  // To be called before forwarding a tap event. This allows automatically
  // showing the insertion handle from subsequent bounds changes.
  void OnTapEvent();

  // To be called before forwarding a longpress event. This allows automatically
  // showing the selection or insertion handles from subsequent bounds changes.
  void OnLongPressEvent();

  // Hide the handles and suppress bounds updates until the next explicit
  // showing allowance.
  void HideAndDisallowShowingAutomatically();

  // Override the handle visibility according to |hidden|.
  void SetTemporarilyHidden(bool hidden);

  // To be called when the editability of the focused region changes.
  void OnSelectionEditable(bool editable);

  // To be called when the contents of the focused region changes.
  void OnSelectionEmpty(bool empty);

  // Ticks an active animation, as requested to the client by |SetNeedsAnimate|.
  // Returns true if an animation is active and requires further ticking.
  bool Animate(base::TimeTicks animate_time);

 private:
  enum InputEventType { TAP, LONG_PRESS, INPUT_EVENT_TYPE_NONE };

  // TouchHandleClient implementation.
  void OnHandleDragBegin(const TouchHandle& handle) override;
  void OnHandleDragUpdate(const TouchHandle& handle,
                          const gfx::PointF& new_position) override;
  void OnHandleDragEnd(const TouchHandle& handle) override;
  void OnHandleTapped(const TouchHandle& handle) override;
  void SetNeedsAnimate() override;
  scoped_ptr<TouchHandleDrawable> CreateDrawable() override;
  base::TimeDelta GetTapTimeout() const override;
  float GetTapSlop() const override;

  void ShowInsertionHandleAutomatically();
  void ShowSelectionHandlesAutomatically();

  void OnInsertionChanged();
  void OnSelectionChanged();

  void ActivateInsertion();
  void DeactivateInsertion();
  void ActivateSelection();
  void DeactivateSelection();
  void ResetCachedValuesIfInactive();

  const gfx::PointF& GetStartPosition() const;
  const gfx::PointF& GetEndPosition() const;
  gfx::Vector2dF GetStartLineOffset() const;
  gfx::Vector2dF GetEndLineOffset() const;
  bool GetStartVisible() const;
  bool GetEndVisible() const;
  TouchHandle::AnimationStyle GetAnimationStyle(bool was_active) const;

  TouchSelectionControllerClient* const client_;
  const base::TimeDelta tap_timeout_;
  const float tap_slop_;

  InputEventType response_pending_input_event_;

  cc::ViewportSelectionBound start_;
  cc::ViewportSelectionBound end_;
  TouchHandleOrientation start_orientation_;
  TouchHandleOrientation end_orientation_;

  scoped_ptr<TouchHandle> insertion_handle_;
  bool is_insertion_active_;
  bool activate_insertion_automatically_;

  scoped_ptr<TouchHandle> start_selection_handle_;
  scoped_ptr<TouchHandle> end_selection_handle_;
  bool is_selection_active_;
  bool activate_selection_automatically_;

  bool selection_empty_;
  bool selection_editable_;

  bool temporarily_hidden_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_H_
