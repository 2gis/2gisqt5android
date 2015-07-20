// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/gamepad/WebKitGamepad.h"

namespace blink {

WebKitGamepad::WebKitGamepad()
{
}

WebKitGamepad::~WebKitGamepad()
{
}

void WebKitGamepad::setButtons(unsigned count, const WebGamepadButton* data)
{
    m_buttons.resize(count);
    for (unsigned i = 0; i < count; ++i)
        m_buttons[i] = data[i].value;
}

} // namespace blink
