/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSGTHREADEDRENDERLOOP_P_H
#define QSGTHREADEDRENDERLOOP_P_H

#include <QtCore/QThread>
#include <QtGui/QOpenGLContext>
#include <private/qsgcontext_p.h>

#include "qsgrenderloop_p.h"

QT_BEGIN_NAMESPACE

class QSGRenderThread;

class QSGThreadedRenderLoop : public QSGRenderLoop
{
    Q_OBJECT
public:
    QSGThreadedRenderLoop();

    void show(QQuickWindow *) {}
    void hide(QQuickWindow *);

    void windowDestroyed(QQuickWindow *window);
    void exposureChanged(QQuickWindow *window);

    QImage grab(QQuickWindow *);

    void update(QQuickWindow *window);
    void maybeUpdate(QQuickWindow *window);
    QSGContext *sceneGraphContext() const;
    QSGRenderContext *createRenderContext(QSGContext *) const;

    QAnimationDriver *animationDriver() const;

    void releaseResources(QQuickWindow *window);

    bool event(QEvent *);

    bool interleaveIncubation() const;

public Q_SLOTS:
    void animationStarted();
    void animationStopped();

private:
    struct Window {
        QQuickWindow *window;
        QSGRenderThread *thread;
        QSurfaceFormat actualWindowFormat;
        int timerId;
        uint updateDuringSync : 1;
        uint forceRenderPass : 1;
    };

    friend class QSGRenderThread;

    void releaseResources(Window *window, bool inDestructor);
    bool checkAndResetForceUpdate(QQuickWindow *window);
    Window *windowForTimer(int timerId) const;

    bool anyoneShowing() const;
    void initialize();

    void startOrStopAnimationTimer();
    void maybePostPolishRequest(Window *w);
    void waitForReleaseComplete();
    void polishAndSync(Window *w, bool inExpose = false);
    void maybeUpdate(Window *window);

    void handleExposure(QQuickWindow *w);
    void handleObscurity(Window *w);


    QSGContext *sg;
    QAnimationDriver *m_animation_driver;
    QList<Window> m_windows;

    int m_animation_timer;
    int m_exhaust_delay;

    bool m_lockedForSync;
};



QT_END_NAMESPACE

#endif // QSGTHREADEDRENDERLOOP_P_H
