/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#ifndef QDECLARATIVEVIDEOOUTPUT_BACKEND_P_H
#define QDECLARATIVEVIDEOOUTPUT_BACKEND_P_H

#include <QtCore/qpointer.h>
#include <QtCore/qsize.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qsgnode.h>
#include <private/qtmultimediaquickdefs_p.h>

QT_BEGIN_NAMESPACE

class QAbstractVideoSurface;
class QDeclarativeVideoOutput;
class QMediaService;

class Q_MULTIMEDIAQUICK_EXPORT QDeclarativeVideoBackend
{
public:
    explicit QDeclarativeVideoBackend(QDeclarativeVideoOutput *parent)
        : q(parent)
    {}

    virtual ~QDeclarativeVideoBackend()
    {}

    virtual bool init(QMediaService *service) = 0;
    virtual void releaseSource() = 0;
    virtual void releaseControl() = 0;
    virtual void itemChange(QQuickItem::ItemChange change,
                            const QQuickItem::ItemChangeData &changeData) = 0;
    virtual QSize nativeSize() const = 0;
    virtual void updateGeometry() = 0;
    virtual QSGNode *updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *data) = 0;
    virtual QAbstractVideoSurface *videoSurface() const = 0;

    // The viewport, adjusted for the pixel aspect ratio
    virtual QRectF adjustedViewport() const = 0;

protected:
    QDeclarativeVideoOutput *q;
    QPointer<QMediaService> m_service;
};

class QDeclarativeVideoBackendFactoryInterface
{
public:
    virtual QDeclarativeVideoBackend *create(QDeclarativeVideoOutput *parent) = 0;
};

#define QDeclarativeVideoBackendFactoryInterface_iid "org.qt-project.qt.declarativevideobackendfactory/5.2"
Q_DECLARE_INTERFACE(QDeclarativeVideoBackendFactoryInterface, QDeclarativeVideoBackendFactoryInterface_iid)

/*
 * Helper - returns true if the given orientation has the same aspect as the default (e.g. 180*n)
 */
namespace {

inline bool qIsDefaultAspect(int o)
{
    return (o % 180) == 0;
}

/*
 * Return the orientation normalized to 0-359
 */
inline int qNormalizedOrientation(int o)
{
    // Negative orientations give negative results
    int o2 = o % 360;
    if (o2 < 0)
        o2 += 360;
    return o2;
}

}

QT_END_NAMESPACE

#endif
