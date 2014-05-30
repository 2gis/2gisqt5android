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

#ifndef QSGNODEUPDATER_P_H
#define QSGNODEUPDATER_P_H

#include <private/qtquickglobal_p.h>
#include <QtGui/private/qdatabuffer_p.h>

QT_BEGIN_NAMESPACE

class QSGNode;
class QSGTransformNode;
class QSGClipNode;
class QSGOpacityNode;
class QSGGeometryNode;
class QMatrix4x4;
class QSGRenderNode;

class Q_QUICK_PRIVATE_EXPORT QSGNodeUpdater
{
public:
    QSGNodeUpdater();
    virtual ~QSGNodeUpdater();

    virtual void updateStates(QSGNode *n);
    virtual bool isNodeBlocked(QSGNode *n, QSGNode *root) const;

protected:
    virtual void enterTransformNode(QSGTransformNode *);
    virtual void leaveTransformNode(QSGTransformNode *);
    void enterClipNode(QSGClipNode *c);
    void leaveClipNode(QSGClipNode *c);
    void enterOpacityNode(QSGOpacityNode *o);
    void leaveOpacityNode(QSGOpacityNode *o);
    void enterGeometryNode(QSGGeometryNode *);
    void leaveGeometryNode(QSGGeometryNode *);
    void enterRenderNode(QSGRenderNode *);
    void leaveRenderNode(QSGRenderNode *);

    void visitNode(QSGNode *n);
    void visitChildren(QSGNode *n);


    QDataBuffer<const QMatrix4x4 *> m_combined_matrix_stack;
    QDataBuffer<qreal> m_opacity_stack;
    const QSGClipNode *m_current_clip;

    int m_force_update;
};

QT_END_NAMESPACE

#endif
