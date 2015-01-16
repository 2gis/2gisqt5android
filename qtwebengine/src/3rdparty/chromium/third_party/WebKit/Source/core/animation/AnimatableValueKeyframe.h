// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimatableValueKeyframe_h
#define AnimatableValueKeyframe_h

#include "core/animation/AnimatableValue.h"
#include "core/animation/Keyframe.h"

namespace WebCore {

class AnimatableValueKeyframe : public Keyframe {
public:
    static PassRefPtrWillBeRawPtr<AnimatableValueKeyframe> create()
    {
        return adoptRefWillBeNoop(new AnimatableValueKeyframe);
    }
    void setPropertyValue(CSSPropertyID property, PassRefPtrWillBeRawPtr<AnimatableValue> value)
    {
        m_propertyValues.add(property, value);
    }
    void clearPropertyValue(CSSPropertyID property) { m_propertyValues.remove(property); }
    AnimatableValue* propertyValue(CSSPropertyID property) const
    {
        ASSERT(m_propertyValues.contains(property));
        return m_propertyValues.get(property);
    }
    virtual PropertySet properties() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

    class PropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
    public:
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const AnimatableValue*, AnimationEffect::CompositeOperation);

        AnimatableValue* value() const { return m_value.get(); }
        virtual const PassRefPtrWillBeRawPtr<AnimatableValue> getAnimatableValue() const OVERRIDE FINAL { return m_value; }

        virtual PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const OVERRIDE FINAL;
        virtual PassRefPtrWillBeRawPtr<Interpolation> createInterpolation(CSSPropertyID, WebCore::Keyframe::PropertySpecificKeyframe* end, Element*) const OVERRIDE FINAL;

        virtual void trace(Visitor*) OVERRIDE;

    private:
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, PassRefPtrWillBeRawPtr<AnimatableValue>);

        virtual PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> cloneWithOffset(double offset) const OVERRIDE;
        virtual bool isAnimatableValuePropertySpecificKeyframe() const OVERRIDE { return true; }

        RefPtrWillBeMember<AnimatableValue> m_value;
    };

private:
    AnimatableValueKeyframe() { }

    AnimatableValueKeyframe(const AnimatableValueKeyframe& copyFrom);

    virtual PassRefPtrWillBeRawPtr<Keyframe> clone() const OVERRIDE;
    virtual PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> createPropertySpecificKeyframe(CSSPropertyID) const OVERRIDE;

    virtual bool isAnimatableValueKeyframe() const OVERRIDE { return true; }

    typedef HashMap<CSSPropertyID, RefPtrWillBeMember<AnimatableValue> > PropertyValueMap;
    PropertyValueMap m_propertyValues;
};

typedef AnimatableValueKeyframe::PropertySpecificKeyframe AnimatableValuePropertySpecificKeyframe;

DEFINE_TYPE_CASTS(AnimatableValueKeyframe, Keyframe, value, value->isAnimatableValueKeyframe(), value.isAnimatableValueKeyframe());
DEFINE_TYPE_CASTS(AnimatableValuePropertySpecificKeyframe, Keyframe::PropertySpecificKeyframe, value, value->isAnimatableValuePropertySpecificKeyframe(), value.isAnimatableValuePropertySpecificKeyframe());

}

#endif
