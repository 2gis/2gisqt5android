// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "ui/events/gestures/motion_event_aura.h"

#include <cmath>

#include "base/logging.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace ui {

MotionEventAura::MotionEventAura()
    : pointer_count_(0), cached_action_index_(-1) {
}

MotionEventAura::MotionEventAura(
    size_t pointer_count,
    const base::TimeTicks& last_touch_time,
    Action cached_action,
    int cached_action_index,
    int flags,
    const PointData (&active_touches)[MotionEvent::MAX_TOUCH_POINT_COUNT])
    : pointer_count_(pointer_count),
      last_touch_time_(last_touch_time),
      cached_action_(cached_action),
      cached_action_index_(cached_action_index),
      flags_(flags) {
  DCHECK(pointer_count_);
  for (size_t i = 0; i < pointer_count; ++i)
    active_touches_[i] = active_touches[i];
}

MotionEventAura::~MotionEventAura() {}

MotionEventAura::PointData MotionEventAura::GetPointDataFromTouchEvent(
    const TouchEvent& touch) {
  PointData point_data;
  point_data.x = touch.x();
  point_data.y = touch.y();
  point_data.raw_x = touch.root_location_f().x();
  point_data.raw_y = touch.root_location_f().y();
  point_data.touch_id = touch.touch_id();
  point_data.pressure = touch.force();
  point_data.source_device_id = touch.source_device_id();

  float radius_x = touch.radius_x();
  float radius_y = touch.radius_y();
  float rotation_angle_rad = touch.rotation_angle() * M_PI / 180.f;
  DCHECK_GE(radius_x, 0) << "Unexpected x-radius < 0";
  DCHECK_GE(radius_y, 0) << "Unexpected y-radius < 0";
  DCHECK(0 <= rotation_angle_rad && rotation_angle_rad <= M_PI_2)
      << "Unexpected touch rotation angle";

  if (radius_x > radius_y) {
    // The case radius_x == radius_y is omitted from here on purpose: for
    // circles, we want to pass the angle (which could be any value in such
    // cases but always seem to be set to zero) unchanged.
    point_data.touch_major = 2.f * radius_x;
    point_data.touch_minor = 2.f * radius_y;
    point_data.orientation = rotation_angle_rad - M_PI_2;
  } else {
    point_data.touch_major = 2.f * radius_y;
    point_data.touch_minor = 2.f * radius_x;
    point_data.orientation = rotation_angle_rad;
  }

  if (!point_data.touch_major) {
    point_data.touch_major =
        2.f * GestureConfiguration::GetInstance()->default_radius();
    point_data.touch_minor =
        2.f * GestureConfiguration::GetInstance()->default_radius();
    point_data.orientation = 0;
  }

  return point_data;
}

void MotionEventAura::OnTouch(const TouchEvent& touch) {
  switch (touch.type()) {
    case ET_TOUCH_PRESSED:
      AddTouch(touch);
      break;
    case ET_TOUCH_RELEASED:
    case ET_TOUCH_CANCELLED:
      // Removing these touch points needs to be postponed until after the
      // MotionEvent has been dispatched. This cleanup occurs in
      // CleanupRemovedTouchPoints.
      UpdateTouch(touch);
      break;
    case ET_TOUCH_MOVED:
      UpdateTouch(touch);
      break;
    default:
      NOTREACHED();
      break;
  }

  UpdateCachedAction(touch);
  flags_ = touch.flags();
  last_touch_time_ = touch.time_stamp() + base::TimeTicks();
}

int MotionEventAura::GetId() const {
  return GetPointerId(0);
}

MotionEvent::Action MotionEventAura::GetAction() const {
  return cached_action_;
}

int MotionEventAura::GetActionIndex() const {
  DCHECK(cached_action_ == ACTION_POINTER_DOWN ||
         cached_action_ == ACTION_POINTER_UP);
  DCHECK_GE(cached_action_index_, 0);
  DCHECK_LT(cached_action_index_, static_cast<int>(pointer_count_));
  return cached_action_index_;
}

size_t MotionEventAura::GetPointerCount() const { return pointer_count_; }

int MotionEventAura::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointer_count_);
  return active_touches_[pointer_index].touch_id;
}

float MotionEventAura::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointer_count_);
  return active_touches_[pointer_index].x;
}

float MotionEventAura::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointer_count_);
  return active_touches_[pointer_index].y;
}

float MotionEventAura::GetRawX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointer_count_);
  return active_touches_[pointer_index].raw_x;
}

float MotionEventAura::GetRawY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointer_count_);
  return active_touches_[pointer_index].raw_y;
}

float MotionEventAura::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointer_count_);
  return active_touches_[pointer_index].touch_major;
}

float MotionEventAura::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].touch_minor;
}

float MotionEventAura::GetOrientation(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].orientation;
}

float MotionEventAura::GetPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointer_count_);
  return active_touches_[pointer_index].pressure;
}

MotionEvent::ToolType MotionEventAura::GetToolType(size_t pointer_index) const {
  // TODO(jdduke): Plumb tool type from the platform, crbug.com/404128.
  DCHECK_LT(pointer_index, pointer_count_);
  return MotionEvent::TOOL_TYPE_UNKNOWN;
}

int MotionEventAura::GetButtonState() const {
  return 0;
}

int MotionEventAura::GetFlags() const {
  return flags_;
}

base::TimeTicks MotionEventAura::GetEventTime() const {
  return last_touch_time_;
}

void MotionEventAura::CleanupRemovedTouchPoints(const TouchEvent& event) {
  if (event.type() != ET_TOUCH_RELEASED &&
      event.type() != ET_TOUCH_CANCELLED) {
    return;
  }

  DCHECK(pointer_count_);
  int index_to_delete = GetIndexFromId(event.touch_id());
  cached_action_index_ = 0;
  pointer_count_--;
  active_touches_[index_to_delete] = active_touches_[pointer_count_];
}

MotionEventAura::PointData::PointData()
    : x(0),
      y(0),
      raw_x(0),
      raw_y(0),
      touch_id(0),
      pressure(0),
      source_device_id(0),
      touch_major(0),
      touch_minor(0),
      orientation(0) {
}

int MotionEventAura::GetSourceDeviceId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointer_count_);
  return active_touches_[pointer_index].source_device_id;
}

void MotionEventAura::AddTouch(const TouchEvent& touch) {
  if (pointer_count_ == MotionEvent::MAX_TOUCH_POINT_COUNT)
    return;

  active_touches_[pointer_count_] = GetPointDataFromTouchEvent(touch);
  pointer_count_++;
}


void MotionEventAura::UpdateTouch(const TouchEvent& touch) {
  active_touches_[GetIndexFromId(touch.touch_id())] =
      GetPointDataFromTouchEvent(touch);
}

void MotionEventAura::UpdateCachedAction(const TouchEvent& touch) {
  DCHECK(pointer_count_);
  switch (touch.type()) {
    case ET_TOUCH_PRESSED:
      if (pointer_count_ == 1) {
        cached_action_ = ACTION_DOWN;
      } else {
        cached_action_ = ACTION_POINTER_DOWN;
        cached_action_index_ = GetIndexFromId(touch.touch_id());
      }
      break;
    case ET_TOUCH_RELEASED:
      if (pointer_count_ == 1) {
        cached_action_ = ACTION_UP;
      } else {
        cached_action_ = ACTION_POINTER_UP;
        cached_action_index_ = GetIndexFromId(touch.touch_id());
      }
      break;
    case ET_TOUCH_CANCELLED:
      cached_action_ = ACTION_CANCEL;
      break;
    case ET_TOUCH_MOVED:
      cached_action_ = ACTION_MOVE;
      break;
    default:
      NOTREACHED();
      break;
  }
}

int MotionEventAura::GetIndexFromId(int id) const {
  int index = FindPointerIndexOfId(id);
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(pointer_count_));
  return index;
}

}  // namespace ui
