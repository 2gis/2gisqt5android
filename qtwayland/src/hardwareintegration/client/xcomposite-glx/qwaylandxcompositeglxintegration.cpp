/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwaylandxcompositeglxintegration.h"

#include "qwaylandxcompositeglxwindow.h"

#include <QtCore/QDebug>

#include "wayland-xcomposite-client-protocol.h"

#include <QtWaylandClient/private/qwaylanddisplay_p.h>

QT_BEGIN_NAMESPACE

QWaylandXCompositeGLXIntegration::QWaylandXCompositeGLXIntegration()
    : mWaylandDisplay(0)
    , mWaylandComposite(0)
    , mDisplay(0)
    , mScreen(0)
    , mRootWindow(0)
{
    qDebug() << "Using XComposite-GLX";
}

QWaylandXCompositeGLXIntegration::~QWaylandXCompositeGLXIntegration()
{
    XCloseDisplay(mDisplay);
}

void QWaylandXCompositeGLXIntegration::initialize(QWaylandDisplay *display)
{
    mWaylandDisplay = display;
    mWaylandDisplay->addRegistryListener(QWaylandXCompositeGLXIntegration::wlDisplayHandleGlobal, this);
    while (!mDisplay) {
        display->flushRequests();
        display->blockingReadEvents();
    }
}

QWaylandWindow * QWaylandXCompositeGLXIntegration::createEglWindow(QWindow *window)
{
    return new QWaylandXCompositeGLXWindow(window, this);
}

QPlatformOpenGLContext *QWaylandXCompositeGLXIntegration::createPlatformOpenGLContext(const QSurfaceFormat &glFormat, QPlatformOpenGLContext *share) const
{
    return new QWaylandXCompositeGLXContext(glFormat, share, mDisplay, mScreen);
}

Display * QWaylandXCompositeGLXIntegration::xDisplay() const
{
    return mDisplay;
}

int QWaylandXCompositeGLXIntegration::screen() const
{
    return mScreen;
}

Window QWaylandXCompositeGLXIntegration::rootWindow() const
{
    return mRootWindow;
}

QWaylandDisplay * QWaylandXCompositeGLXIntegration::waylandDisplay() const
{
    return mWaylandDisplay;
}
qt_xcomposite * QWaylandXCompositeGLXIntegration::waylandXComposite() const
{
    return mWaylandComposite;
}

const struct qt_xcomposite_listener QWaylandXCompositeGLXIntegration::xcomposite_listener = {
    QWaylandXCompositeGLXIntegration::rootInformation
};

void QWaylandXCompositeGLXIntegration::wlDisplayHandleGlobal(void *data, wl_registry *registry, uint32_t id, const QString &interface, uint32_t version)
{
    Q_UNUSED(version);
    if (interface == "qt_xcomposite") {
        qDebug("XComposite-GLX: got qt_xcomposite global");
        QWaylandXCompositeGLXIntegration *integration = static_cast<QWaylandXCompositeGLXIntegration *>(data);
        integration->mWaylandComposite = static_cast<struct qt_xcomposite *>(wl_registry_bind(registry, id, &qt_xcomposite_interface, 1));
        qt_xcomposite_add_listener(integration->mWaylandComposite,&xcomposite_listener,integration);
    }

}

void QWaylandXCompositeGLXIntegration::rootInformation(void *data, qt_xcomposite *xcomposite, const char *display_name, uint32_t root_window)
{
    Q_UNUSED(xcomposite);
    QWaylandXCompositeGLXIntegration *integration = static_cast<QWaylandXCompositeGLXIntegration *>(data);

    qDebug("XComposite-GLX: xcomposite listener callback");

    integration->mDisplay = XOpenDisplay(display_name);
    integration->mRootWindow = (Window) root_window;
    integration->mScreen = XDefaultScreen(integration->mDisplay);
}

QT_END_NAMESPACE
