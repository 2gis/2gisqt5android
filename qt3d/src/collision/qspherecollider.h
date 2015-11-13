/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QT3D_QCOLLISIONCOMPONENT_H
#define QT3D_QCOLLISIONCOMPONENT_H

#include <Qt3DCollision/qabstractcollider.h>
#include <Qt3DCollision/qt3dcollision_global.h>
#include <QtGui/qvector3d.h>

QT_BEGIN_NAMESPACE

namespace Qt3D {

class QSphereColliderPrivate;

class QT3DCOLLISIONSHARED_EXPORT QSphereCollider : public QAbstractCollider
{
    Q_OBJECT
    Q_PROPERTY(QVector3D center READ center WRITE setCenter NOTIFY centerChanged)
    Q_PROPERTY(float radius READ radius WRITE setRadius NOTIFY radiusChanged)

public:
    explicit QSphereCollider(QNode *parent = 0);
    ~QSphereCollider();

    QVector3D center() const;
    float radius() const;

public Q_SLOTS:
    void setCenter(const QVector3D &center);
    void setRadius(float radius);

Q_SIGNALS:
    void centerChanged(QVector3D center);
    void radiusChanged(float radius);

protected:
    QSphereCollider(QSphereColliderPrivate &dd, QNode *parent = 0);

private:
    Q_DECLARE_PRIVATE(QSphereCollider)
    QT3D_CLONEABLE(QSphereCollider)
};

}

QT_END_NAMESPACE

#endif // QT3D_QCOLLISIONCOMPONENT_H
