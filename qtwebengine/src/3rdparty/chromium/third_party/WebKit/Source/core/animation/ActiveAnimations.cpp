/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/ActiveAnimations.h"

#include "core/rendering/RenderObject.h"

namespace blink {

ActiveAnimations::ActiveAnimations()
    : m_animationStyleChange(false)
{
}

ActiveAnimations::~ActiveAnimations()
{
#if !ENABLE(OILPAN)
    for (Animation* animation : m_animations)
        animation->notifyElementDestroyed();
    m_animations.clear();
#endif
}

void ActiveAnimations::updateAnimationFlags(RenderStyle& style)
{
    for (const auto& entry : m_players) {
        const AnimationPlayer& player = *entry.key;
        ASSERT(player.source());
        // FIXME: Needs to consider AnimationGroup once added.
        ASSERT(player.source()->isAnimation());
        const Animation& animation = *toAnimation(player.source());
        if (animation.isCurrent()) {
            if (animation.affects(CSSPropertyOpacity))
                style.setHasCurrentOpacityAnimation(true);
            if (animation.affects(CSSPropertyTransform))
                style.setHasCurrentTransformAnimation(true);
            if (animation.affects(CSSPropertyWebkitFilter))
                style.setHasCurrentFilterAnimation(true);
        }
    }

    if (style.hasCurrentOpacityAnimation())
        style.setIsRunningOpacityAnimationOnCompositor(m_defaultStack.hasActiveAnimationsOnCompositor(CSSPropertyOpacity));
    if (style.hasCurrentTransformAnimation())
        style.setIsRunningTransformAnimationOnCompositor(m_defaultStack.hasActiveAnimationsOnCompositor(CSSPropertyTransform));
    if (style.hasCurrentFilterAnimation())
        style.setIsRunningFilterAnimationOnCompositor(m_defaultStack.hasActiveAnimationsOnCompositor(CSSPropertyWebkitFilter));
}

void ActiveAnimations::cancelAnimationOnCompositor()
{
    for (const auto& entry : m_players)
        entry.key->cancelAnimationOnCompositor();
}

void ActiveAnimations::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_cssAnimations);
    visitor->trace(m_defaultStack);
    visitor->trace(m_players);
#endif
}

const RenderStyle* ActiveAnimations::baseRenderStyle() const
{
#if !ENABLE(ASSERT)
    if (isAnimationStyleChange())
        return m_baseRenderStyle.get();
#endif
    return nullptr;
}

void ActiveAnimations::updateBaseRenderStyle(const RenderStyle* renderStyle)
{
    if (!isAnimationStyleChange()) {
        m_baseRenderStyle = nullptr;
        return;
    }
#if ENABLE(ASSERT)
    if (m_baseRenderStyle && renderStyle)
        ASSERT(*m_baseRenderStyle == *renderStyle);
#endif
    m_baseRenderStyle = RenderStyle::clone(renderStyle);
}

void ActiveAnimations::clearBaseRenderStyle()
{
    m_baseRenderStyle = nullptr;
}

} // namespace blink
