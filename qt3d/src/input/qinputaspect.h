/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#ifndef QT3D_QINPUTASPECT_H
#define QT3D_QINPUTASPECT_H

#include <Qt3DInput/qt3dinput_global.h>
#include <Qt3DCore/qabstractaspect.h>

QT_BEGIN_NAMESPACE

namespace Qt3D {

class QCamera;
class QInputAspectPrivate;

class QT3DINPUTSHARED_EXPORT QInputAspect : public QAbstractAspect
{
    Q_OBJECT
    Q_PROPERTY(Qt3D::QCamera* camera READ camera WRITE setCamera)
public:
    explicit QInputAspect(QObject *parent = 0);

    QCamera *camera() const;

public Q_SLOTS:
    void setCamera(QCamera *camera);

public:
    QVector<QAspectJobPtr> jobsToExecute(qint64 time) Q_DECL_OVERRIDE;

    void sceneNodeAdded(QSceneChangePtr &e) Q_DECL_OVERRIDE;
    void sceneNodeRemoved(QSceneChangePtr &e) Q_DECL_OVERRIDE;

private:
    void setRootEntity(QEntity *rootObject) Q_DECL_OVERRIDE;
    void onInitialize(const QVariantMap &data) Q_DECL_OVERRIDE;
    void onStartup() Q_DECL_OVERRIDE;
    void onShutdown() Q_DECL_OVERRIDE;
    void onCleanup() Q_DECL_OVERRIDE;

    void visitNode(QNode *node);

    Q_DECLARE_PRIVATE(QInputAspect)
};

} // Qt3D

QT_END_NAMESPACE

#endif // QT3D_QINPUTASPECT_H
