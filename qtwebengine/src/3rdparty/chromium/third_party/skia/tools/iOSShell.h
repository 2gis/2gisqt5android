/*
 * Copyright 2014 Skia
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef iOSShell_DEFINED
#define iOSShell_DEFINED

#include "SkWindow.h"

class SkCanvas;
class SkEvent;
class SkViewFactory;

class ShellWindow : public SkOSWindow {
public:
    ShellWindow(void* hwnd, int argc, char** argv);
    virtual ~ShellWindow();

protected:
    virtual void onSizeChange() SK_OVERRIDE;

    virtual bool onDispatchClick(int x, int y, Click::State, void* owner,
                                 unsigned modi) SK_OVERRIDE;

private:
    typedef SkOSWindow INHERITED;
};

#endif
