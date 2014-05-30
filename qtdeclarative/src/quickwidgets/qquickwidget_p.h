/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QQUICKWIDGET_P_H
#define QQUICKWIDGET_P_H

#include "qquickwidget.h"
#include <private/qwidget_p.h>

#include <QtCore/qurl.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qtimer.h>
#include <QtCore/qpointer.h>
#include <QtCore/QWeakPointer>

#include <QtQml/qqmlengine.h>

#include "private/qquickitemchangelistener_p.h"

QT_BEGIN_NAMESPACE

class QQmlContext;
class QQmlError;
class QQuickItem;
class QQmlComponent;
class QQuickRenderControl;
class QOpenGLContext;
class QOffscreenSurface;

class QQuickWidgetPrivate
        : public QWidgetPrivate,
          public QQuickItemChangeListener
{
    Q_DECLARE_PUBLIC(QQuickWidget)
public:
    static QQuickWidgetPrivate* get(QQuickWidget *view) { return view->d_func(); }
    static const QQuickWidgetPrivate* get(const QQuickWidget *view) { return view->d_func(); }

    QQuickWidgetPrivate();
    ~QQuickWidgetPrivate();

    void execute();
    void itemGeometryChanged(QQuickItem *item, const QRectF &newGeometry, const QRectF &oldGeometry);
    void initResize();
    void updateSize();
    void updateFrambufferObjectSize();
    void setRootObject(QObject *);
    void renderSceneGraph();
    void createContext();
    void destroyContext();
    void handleContextCreationFailure(const QSurfaceFormat &format, bool isEs);
    void createOffscreenSurface();

    GLuint textureId() const Q_DECL_OVERRIDE;

    void init(QQmlEngine* e = 0);
    void handleWindowChange();

    QSize rootObjectSize() const;

    QPointer<QQuickItem> root;

    QUrl source;

    QPointer<QQmlEngine> engine;
    QQmlComponent *component;
    QBasicTimer resizetimer;
    QQuickWindow *offscreenWindow;
    QOffscreenSurface *offscreenSurface;
    QQuickRenderControl *renderControl;
    QOpenGLFramebufferObject *fbo;
    QOpenGLContext *context;

    QQuickWidget::ResizeMode resizeMode;
    QSize initialSize;
    QElapsedTimer frameTimer;

    QBasicTimer updateTimer;
    bool eventPending;
    bool updatePending;
    bool fakeHidden;
};

QT_END_NAMESPACE

#endif // QQuickWidget_P_H
