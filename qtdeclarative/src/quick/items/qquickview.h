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

#ifndef QQUICKVIEW_H
#define QQUICKVIEW_H

#include <QtQuick/qquickwindow.h>
#include <QtCore/qurl.h>
#include <QtQml/qqmldebug.h>

QT_BEGIN_NAMESPACE

class QQmlEngine;
class QQmlContext;
class QQmlError;
class QQuickItem;
class QQmlComponent;

class QQuickViewPrivate;
class Q_QUICK_EXPORT QQuickView : public QQuickWindow
{
    Q_OBJECT
    Q_PROPERTY(ResizeMode resizeMode READ resizeMode WRITE setResizeMode)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QUrl source READ source WRITE setSource DESIGNABLE true)
    Q_ENUMS(ResizeMode Status)
public:
    explicit QQuickView(QWindow *parent = 0);
    QQuickView(QQmlEngine* engine, QWindow *parent);
    QQuickView(const QUrl &source, QWindow *parent = 0);
    virtual ~QQuickView();

    QUrl source() const;

    QQmlEngine* engine() const;
    QQmlContext* rootContext() const;

    QQuickItem *rootObject() const;

    enum ResizeMode { SizeViewToRootObject, SizeRootObjectToView };
    ResizeMode resizeMode() const;
    void setResizeMode(ResizeMode);

    enum Status { Null, Ready, Loading, Error };
    Status status() const;

    QList<QQmlError> errors() const;

    QSize sizeHint() const;
    QSize initialSize() const;

public Q_SLOTS:
    void setSource(const QUrl&);
    void setContent(const QUrl& url, QQmlComponent *component, QObject *item);

Q_SIGNALS:
    void statusChanged(QQuickView::Status);

private Q_SLOTS:
    void continueExecute();

protected:
    virtual void resizeEvent(QResizeEvent *);
    virtual void timerEvent(QTimerEvent*);

    virtual void keyPressEvent(QKeyEvent *);
    virtual void keyReleaseEvent(QKeyEvent *);
    virtual void mousePressEvent(QMouseEvent *);
    virtual void mouseReleaseEvent(QMouseEvent *);
    virtual void mouseMoveEvent(QMouseEvent *);
private:
    Q_DISABLE_COPY(QQuickView)
    Q_DECLARE_PRIVATE(QQuickView)
};

QT_END_NAMESPACE

#endif // QQUICKVIEW_H
