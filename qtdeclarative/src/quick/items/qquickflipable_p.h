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

#ifndef QQUICKFLIPABLE_P_H
#define QQUICKFLIPABLE_P_H

#include "qquickitem.h"

#include <QtGui/qtransform.h>
#include <QtGui/qvector3d.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class QQuickFlipablePrivate;
class Q_AUTOTEST_EXPORT QQuickFlipable : public QQuickItem
{
    Q_OBJECT

    Q_ENUMS(Side)
    Q_PROPERTY(QQuickItem *front READ front WRITE setFront NOTIFY frontChanged)
    Q_PROPERTY(QQuickItem *back READ back WRITE setBack NOTIFY backChanged)
    Q_PROPERTY(Side side READ side NOTIFY sideChanged)
    //### flipAxis
    //### flipRotation
public:
    QQuickFlipable(QQuickItem *parent=0);
    ~QQuickFlipable();

    QQuickItem *front();
    void setFront(QQuickItem *);

    QQuickItem *back();
    void setBack(QQuickItem *);

    enum Side { Front, Back };
    Side side() const;

Q_SIGNALS:
    void frontChanged();
    void backChanged();
    void sideChanged();

protected:
    virtual void updatePolish();

private Q_SLOTS:
    void retransformBack();

private:
    Q_DISABLE_COPY(QQuickFlipable)
    Q_DECLARE_PRIVATE(QQuickFlipable)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickFlipable)

#endif // QQUICKFLIPABLE_P_H
