// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLocalFrame_h
#define WebLocalFrame_h

#include "WebFrame.h"

namespace blink {

// Interface for interacting with in process frames. This contains methods that
// require interacting with a frame's document.
// FIXME: Move lots of methods from WebFrame in here.
class WebLocalFrame : public WebFrame {
public:
    // Creates a WebFrame. Delete this WebFrame by calling WebFrame::close().
    // It is valid to pass a null client pointer.
    BLINK_EXPORT static WebLocalFrame* create(WebFrameClient*);

    // Returns the WebFrame associated with the current V8 context. This
    // function can return 0 if the context is associated with a Document that
    // is not currently being displayed in a Frame.
    BLINK_EXPORT static WebLocalFrame* frameForCurrentContext();

    // Returns the frame corresponding to the given context. This can return 0
    // if the context is detached from the frame, or if the context doesn't
    // correspond to a frame (e.g., workers).
    BLINK_EXPORT static WebLocalFrame* frameForContext(v8::Handle<v8::Context>);

    // Returns the frame inside a given frame or iframe element. Returns 0 if
    // the given element is not a frame, iframe or if the frame is empty.
    BLINK_EXPORT static WebLocalFrame* fromFrameOwnerElement(const WebElement&);

    // Navigation Transitions  ---------------------------------------------
    virtual void addStyleSheetByURL(const WebString& url) = 0;
};

} // namespace blink

#endif // WebLocalFrame_h
