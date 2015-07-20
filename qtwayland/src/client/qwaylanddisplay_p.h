/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWAYLANDDISPLAY_H
#define QWAYLANDDISPLAY_H

#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QPointer>

#include <QtCore/QWaitCondition>

#include <wayland-client.h>

#include <QtWaylandClient/private/qwayland-wayland.h>
#include <QtWaylandClient/private/qwaylandclientexport_p.h>
#include <QtWaylandClient/private/qwayland-xdg-shell.h>

struct wl_cursor_image;

QT_BEGIN_NAMESPACE

class QAbstractEventDispatcher;
class QSocketNotifier;
class QPlatformScreen;

namespace QtWayland {
    class qt_output_extension;
    class qt_shell;
    class qt_sub_surface_extension;
    class qt_surface_extension;
    class wl_text_input_manager;
    class xdg_shell;
}

namespace QtWaylandClient {

class QWaylandInputDevice;
class QWaylandBuffer;
class QWaylandScreen;
class QWaylandClientBufferIntegration;
class QWaylandWindowManagerIntegration;
class QWaylandDataDeviceManager;
class QWaylandTouchExtension;
class QWaylandQtKeyExtension;
class QWaylandWindow;
class QWaylandEventThread;
class QWaylandIntegration;
class QWaylandHardwareIntegration;
class QWaylandXdgShell;
class QWaylandShellSurface;

typedef void (*RegistryListener)(void *data,
                                 struct wl_registry *registry,
                                 uint32_t id,
                                 const QString &interface,
                                 uint32_t version);

class Q_WAYLAND_CLIENT_EXPORT QWaylandDisplay : public QObject, public QtWayland::wl_registry {
    Q_OBJECT

public:
    QWaylandDisplay(QWaylandIntegration *waylandIntegration);
    ~QWaylandDisplay(void);

    QList<QWaylandScreen *> screens() const { return mScreens; }

    QWaylandScreen *screenForOutput(struct wl_output *output) const;

    struct wl_surface *createSurface(void *handle);
    QWaylandShellSurface *createShellSurface(QWaylandWindow *window);
    struct ::wl_region *createRegion(const QRegion &qregion);

    QWaylandClientBufferIntegration *clientBufferIntegration() const;

    QWaylandWindowManagerIntegration *windowManagerIntegration() const;

    void setCursor(struct wl_buffer *buffer, struct wl_cursor_image *image);

    struct wl_display *wl_display() const { return mDisplay; }
    struct wl_event_queue *wl_event_queue() const { return mEventQueue; }
    struct ::wl_registry *wl_registry() { return object(); }

    const struct wl_compositor *wl_compositor() const { return mCompositor.object(); }
    QtWayland::wl_compositor *compositor() { return &mCompositor; }
    int compositorVersion() const { return mCompositorVersion; }

    QtWayland::wl_shell *shell() { return mShell.data(); }
    QtWayland::xdg_shell *shellXdg();

    QList<QWaylandInputDevice *> inputDevices() const { return mInputDevices; }
    QWaylandInputDevice *defaultInputDevice() const;
    QWaylandInputDevice *currentInputDevice() const { return defaultInputDevice(); }

    QWaylandInputDevice *lastKeyboardFocusInputDevice() const;
    void setLastKeyboardFocusInputDevice(QWaylandInputDevice *device);

    QWaylandDataDeviceManager *dndSelectionHandler() const { return mDndSelectionHandler.data(); }

    QtWayland::qt_surface_extension *windowExtension() const { return mWindowExtension.data(); }
    QtWayland::qt_sub_surface_extension *subSurfaceExtension() const { return mSubSurfaceExtension.data(); }
    QtWayland::qt_output_extension *outputExtension() const { return mOutputExtension.data(); }
    QWaylandTouchExtension *touchExtension() const { return mTouchExtension.data(); }
    QtWayland::wl_text_input_manager *textInputManager() const { return mTextInputManager.data(); }
    QWaylandHardwareIntegration *hardwareIntegration() const { return mHardwareIntegration.data(); }

    struct RegistryGlobal {
        uint32_t id;
        QString interface;
        uint32_t version;
        struct ::wl_registry *registry;
        RegistryGlobal(uint32_t id_, const QString &interface_, uint32_t version_, struct ::wl_registry *registry_)
            : id(id_), interface(interface_), version(version_), registry(registry_) { }
    };
    QList<RegistryGlobal> globals() const { return mGlobals; }

    /* wl_registry_add_listener does not add but rather sets a listener, so this function is used
     * to enable many listeners at once. */
    void addRegistryListener(RegistryListener listener, void *data);

    struct wl_shm *shm() const { return mShm; }

    static uint32_t currentTimeMillisec();

    void forceRoundTrip();

    bool supportsWindowDecoration() const;

    uint32_t lastInputSerial() const { return mLastInputSerial; }
    QWaylandInputDevice *lastInputDevice() const { return mLastInputDevice; }
    QWaylandWindow *lastInputWindow() const;
    void setLastInputDevice(QWaylandInputDevice *device, uint32_t serial, QWaylandWindow *window);

public slots:
    void blockingReadEvents();
    void flushRequests();

private:
    void waitForScreens();
    void exitWithError();

    struct Listener {
        RegistryListener listener;
        void *data;
    };

    struct wl_display *mDisplay;
    struct wl_event_queue *mEventQueue;
    QtWayland::wl_compositor mCompositor;
    struct wl_shm *mShm;
    QThread *mEventThread;
    QWaylandEventThread *mEventThreadObject;
    QScopedPointer<QtWayland::wl_shell> mShell;
    QScopedPointer<QWaylandXdgShell> mShellXdg;
    QList<QWaylandScreen *> mScreens;
    QList<QWaylandInputDevice *> mInputDevices;
    QList<Listener> mRegistryListeners;
    QWaylandIntegration *mWaylandIntegration;
    QWaylandInputDevice *mLastKeyboardFocusInputDevice;
    QScopedPointer<QWaylandDataDeviceManager> mDndSelectionHandler;
    QScopedPointer<QtWayland::qt_surface_extension> mWindowExtension;
    QScopedPointer<QtWayland::qt_sub_surface_extension> mSubSurfaceExtension;
    QScopedPointer<QtWayland::qt_output_extension> mOutputExtension;
    QScopedPointer<QWaylandTouchExtension> mTouchExtension;
    QScopedPointer<QWaylandQtKeyExtension> mQtKeyExtension;
    QScopedPointer<QWaylandWindowManagerIntegration> mWindowManagerIntegration;
    QScopedPointer<QtWayland::wl_text_input_manager> mTextInputManager;
    QScopedPointer<QWaylandHardwareIntegration> mHardwareIntegration;
    QSocketNotifier *mReadNotifier;
    int mFd;
    int mWritableNotificationFd;
    QList<RegistryGlobal> mGlobals;
    int mCompositorVersion;
    uint32_t mLastInputSerial;
    QWaylandInputDevice *mLastInputDevice;
    QPointer<QWaylandWindow> mLastInputWindow;

    void registry_global(uint32_t id, const QString &interface, uint32_t version) Q_DECL_OVERRIDE;
    void registry_global_remove(uint32_t id) Q_DECL_OVERRIDE;

    static void shellHandleConfigure(void *data, struct wl_shell *shell,
                                     uint32_t time, uint32_t edges,
                                     struct wl_surface *surface,
                                     int32_t width, int32_t height);
};

}

QT_END_NAMESPACE

#endif // QWAYLANDDISPLAY_H
