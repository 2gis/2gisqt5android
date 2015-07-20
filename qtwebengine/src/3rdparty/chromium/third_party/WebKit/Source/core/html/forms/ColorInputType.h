/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef ColorInputType_h
#define ColorInputType_h

#include "core/html/forms/BaseClickableWithKeyInputType.h"
#include "core/html/forms/ColorChooserClient.h"

namespace blink {

class ColorChooser;

class ColorInputType final : public BaseClickableWithKeyInputType, public ColorChooserClient {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ColorInputType);
public:
    static PassRefPtrWillBeRawPtr<InputType> create(HTMLInputElement&);
    virtual ~ColorInputType();
    virtual void trace(Visitor*) override;

    // ColorChooserClient implementation.
    virtual void didChooseColor(const Color&) override;
    virtual void didEndChooser() override;
    virtual Element& ownerElement() const override;
    virtual IntRect elementRectRelativeToRootView() const override;
    virtual Color currentColor() override;
    virtual bool shouldShowSuggestions() const override;
    virtual Vector<ColorSuggestion> suggestions() const override;
    ColorChooserClient* colorChooserClient() override;

private:
    ColorInputType(HTMLInputElement& element) : BaseClickableWithKeyInputType(element) { }
    virtual void countUsage() override;
    virtual const AtomicString& formControlType() const override;
    virtual bool supportsRequired() const override;
    virtual String fallbackValue() const override;
    virtual String sanitizeValue(const String&) const override;
    virtual void createShadowSubtree() override;
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;
    virtual void handleDOMActivateEvent(Event*) override;
    virtual void closePopupView() override;
    virtual bool shouldRespectListAttribute() override;
    virtual bool typeMismatchFor(const String&) const override;
    virtual void updateView() override;
    virtual AXObject* popupRootAXObject() override;

    Color valueAsColor() const;
    void endColorChooser();
    HTMLElement* shadowColorSwatch() const;

    OwnPtrWillBeMember<ColorChooser> m_chooser;
};

} // namespace blink

#endif // ColorInputType_h
