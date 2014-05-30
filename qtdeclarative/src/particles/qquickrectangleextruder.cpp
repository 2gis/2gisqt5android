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

#include "qquickrectangleextruder_p.h"
#include <stdlib.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype RectangleShape
    \instantiates QQuickRectangleExtruder
    \inqmlmodule QtQuick.Particles 2
    \brief For specifying an area for affectors and emitter
    \ingroup qtquick-particles

    Just a rectangle.
*/

QQuickRectangleExtruder::QQuickRectangleExtruder(QObject *parent) :
    QQuickParticleExtruder(parent), m_fill(true)
{
}

QPointF QQuickRectangleExtruder::extrude(const QRectF &rect)
{
    if (m_fill)
        return QPointF(((qreal)rand() / RAND_MAX) * rect.width() + rect.x(),
                       ((qreal)rand() / RAND_MAX) * rect.height() + rect.y());
    int side = rand() % 4;
    switch (side){//TODO: Doesn't this overlap the corners?
    case 0:
        return QPointF(rect.x(),
                       ((qreal)rand() / RAND_MAX) * rect.height() + rect.y());
    case 1:
        return QPointF(rect.width() + rect.x(),
                       ((qreal)rand() / RAND_MAX) * rect.height() + rect.y());
    case 2:
        return QPointF(((qreal)rand() / RAND_MAX) * rect.width() + rect.x(),
                       rect.y());
    default:
        return QPointF(((qreal)rand() / RAND_MAX) * rect.width() + rect.x(),
                       rect.height() + rect.y());
    }
}

bool QQuickRectangleExtruder::contains(const QRectF &bounds, const QPointF &point)
{
    return bounds.contains(point);
}

QT_END_NAMESPACE
