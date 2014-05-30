/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:LGPL$
 ** Commercial License Usage
 ** Licensees holding valid commercial Qt licenses may use this file in
 ** accordance with the commercial license agreement provided with the
 ** Software or, alternatively, in accordance with the terms contained in
 ** a written agreement between you and Digia.  For licensing terms and
 ** conditions see http://qt.digia.com/licensing.  For further information
 ** use the contact form at http://qt.digia.com/contact-us.
 **
 ** GNU Lesser General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU Lesser
 ** General Public License version 2.1 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.LGPL included in the
 ** packaging of this file.  Please review the following information to
 ** ensure the GNU Lesser General Public License version 2.1 requirements
 ** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Digia gives you certain additional
 ** rights.  These rights are described in the Digia Qt LGPL Exception
 ** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
 **
 ** GNU General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU
 ** General Public License version 3.0 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.GPL included in the
 ** packaging of this file.  Please review the following information to
 ** ensure the GNU General Public License version 3.0 requirements will be
 ** met: http://www.gnu.org/copyleft/gpl.html.
 **
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

#include "qwinfunctions_p.h"

#include <qt_windows.h>

QT_BEGIN_NAMESPACE

// in order to allow binary to load on WinXP...

void qt_winextras_init();

typedef HRESULT (STDAPICALLTYPE *DwmGetColorizationColor_t)(DWORD *, BOOL *);
static DwmGetColorizationColor_t pDwmGetColorizationColor = 0;

typedef HRESULT (STDAPICALLTYPE *DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
static DwmSetWindowAttribute_t pDwmSetWindowAttribute = 0;

typedef HRESULT (STDAPICALLTYPE *DwmGetWindowAttribute_t)(HWND, DWORD, PVOID, DWORD);
static DwmGetWindowAttribute_t pDwmGetWindowAttribute = 0;

typedef HRESULT (STDAPICALLTYPE *DwmExtendFrameIntoClientArea_t)(HWND, const MARGINS *);
static DwmExtendFrameIntoClientArea_t pDwmExtendFrameIntoClientArea = 0;

typedef HRESULT (STDAPICALLTYPE *DwmEnableBlurBehindWindow_t)(HWND, const qt_DWM_BLURBEHIND *);
DwmEnableBlurBehindWindow_t pDwmEnableBlurBehindWindow = 0;

typedef HRESULT (STDAPICALLTYPE *DwmIsCompositionEnabled_t)(BOOL *);
static DwmIsCompositionEnabled_t pDwmIsCompositionEnabled = 0;

typedef HRESULT (STDAPICALLTYPE *DwmEnableComposition_t)(UINT);
static DwmEnableComposition_t pDwmEnableComposition = 0;

typedef HRESULT (STDAPICALLTYPE *SHCreateItemFromParsingName_t)(PCWSTR, IBindCtx *, REFIID, void **);
static SHCreateItemFromParsingName_t pSHCreateItemFromParsingName = 0;

typedef HRESULT (STDAPICALLTYPE *SetCurrentProcessExplicitAppUserModelID_t)(PCWSTR);
static SetCurrentProcessExplicitAppUserModelID_t pSetCurrentProcessExplicitAppUserModelID = 0;

HRESULT qt_DwmGetColorizationColor(DWORD *colorization, BOOL *opaqueBlend)
{
    qt_winextras_init();
    if (pDwmGetColorizationColor)
        return pDwmGetColorizationColor(colorization, opaqueBlend);
    else
        return E_FAIL;
}

HRESULT qt_DwmSetWindowAttribute(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)
{
    qt_winextras_init();
    if (pDwmSetWindowAttribute)
        return pDwmSetWindowAttribute(hwnd, dwAttribute, pvAttribute, cbAttribute);
    else
        return E_FAIL;
}

HRESULT qt_DwmGetWindowAttribute(HWND hwnd, DWORD dwAttribute, PVOID pvAttribute, DWORD cbAttribute)
{
    qt_winextras_init();
    if (pDwmGetWindowAttribute)
        return pDwmGetWindowAttribute(hwnd, dwAttribute, pvAttribute, cbAttribute);
    else
        return E_FAIL;
}

HRESULT qt_DwmExtendFrameIntoClientArea(HWND hwnd, const MARGINS *margins)
{
    qt_winextras_init();
    if (pDwmExtendFrameIntoClientArea)
        return pDwmExtendFrameIntoClientArea(hwnd, margins);
    else
        return E_FAIL;
}

HRESULT qt_DwmEnableBlurBehindWindow(HWND hwnd, const qt_DWM_BLURBEHIND *blurBehind)
{
    qt_winextras_init();
    if (pDwmEnableBlurBehindWindow)
        return pDwmEnableBlurBehindWindow(hwnd, blurBehind);
    else
        return E_FAIL;
}

HRESULT qt_DwmIsCompositionEnabled(BOOL *enabled)
{
    qt_winextras_init();
    if (pDwmIsCompositionEnabled)
        return pDwmIsCompositionEnabled(enabled);
    else
        return E_FAIL;
}

HRESULT qt_DwmEnableComposition(UINT enabled)
{
    qt_winextras_init();
    if (pDwmEnableComposition)
        return pDwmEnableComposition(enabled);
    else
        return E_FAIL;
}

HRESULT qt_SHCreateItemFromParsingName(PCWSTR path, IBindCtx *bindcontext, REFIID riid, void **ppv)
{
    qt_winextras_init();
    if (pSHCreateItemFromParsingName)
        return pSHCreateItemFromParsingName(path, bindcontext, riid, ppv);
    else
        return E_FAIL;
}

HRESULT qt_SetCurrentProcessExplicitAppUserModelID(PCWSTR appId)
{
    qt_winextras_init();
    if (pSetCurrentProcessExplicitAppUserModelID)
        return pSetCurrentProcessExplicitAppUserModelID(appId);
    else
        return E_FAIL;
}

void qt_winextras_init()
{
    static bool initialized = false;
    if (initialized)
        return;
    HMODULE dwmapi  = LoadLibraryW(L"dwmapi.dll");
    HMODULE shell32 = LoadLibraryW(L"shell32.dll");
    if (dwmapi) {
        pDwmExtendFrameIntoClientArea = (DwmExtendFrameIntoClientArea_t) GetProcAddress(dwmapi, "DwmExtendFrameIntoClientArea");
        pDwmEnableBlurBehindWindow = (DwmEnableBlurBehindWindow_t) GetProcAddress(dwmapi, "DwmEnableBlurBehindWindow");
        pDwmGetColorizationColor = (DwmGetColorizationColor_t) GetProcAddress(dwmapi, "DwmGetColorizationColor");
        pDwmSetWindowAttribute   = (DwmSetWindowAttribute_t)   GetProcAddress(dwmapi, "DwmSetWindowAttribute");
        pDwmGetWindowAttribute   = (DwmGetWindowAttribute_t)   GetProcAddress(dwmapi, "DwmGetWindowAttribute");
        pDwmIsCompositionEnabled = (DwmIsCompositionEnabled_t) GetProcAddress(dwmapi, "DwmIsCompositionEnabled");
        pDwmEnableComposition    = (DwmEnableComposition_t)    GetProcAddress(dwmapi, "DwmEnableComposition");
    }
    if (shell32) {
        pSHCreateItemFromParsingName = (SHCreateItemFromParsingName_t) GetProcAddress(shell32, "SHCreateItemFromParsingName");
        pSetCurrentProcessExplicitAppUserModelID = (SetCurrentProcessExplicitAppUserModelID_t) GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
    }
}

QT_END_NAMESPACE
