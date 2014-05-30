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


#ifndef QSGDEFAULTIMAGENODE_P_H
#define QSGDEFAULTIMAGENODE_P_H

#include <private/qsgadaptationlayer_p.h>
#include <QtQuick/qsgtexturematerial.h>

QT_BEGIN_NAMESPACE

class Q_QUICK_PRIVATE_EXPORT QSGSmoothTextureMaterial : public QSGTextureMaterial
{
public:
    QSGSmoothTextureMaterial();

    void setTexture(QSGTexture *texture);

protected:
    virtual QSGMaterialType *type() const;
    virtual QSGMaterialShader *createShader() const;
};

class Q_QUICK_PRIVATE_EXPORT QSGDefaultImageNode : public QSGImageNode
{
public:
    QSGDefaultImageNode();
    virtual void setTargetRect(const QRectF &rect);
    virtual void setInnerTargetRect(const QRectF &rect);
    virtual void setInnerSourceRect(const QRectF &rect);
    virtual void setSubSourceRect(const QRectF &rect);
    virtual void setTexture(QSGTexture *t);
    virtual void setAntialiasing(bool antialiasing);
    virtual void setMirror(bool mirror);
    virtual void update();

    virtual void setMipmapFiltering(QSGTexture::Filtering filtering);
    virtual void setFiltering(QSGTexture::Filtering filtering);
    virtual void setHorizontalWrapMode(QSGTexture::WrapMode wrapMode);
    virtual void setVerticalWrapMode(QSGTexture::WrapMode wrapMode);

    virtual void preprocess();

private:
    void updateGeometry();

    QRectF m_targetRect;
    QRectF m_innerTargetRect;
    QRectF m_innerSourceRect;
    QRectF m_subSourceRect;

    QSGOpaqueTextureMaterial m_material;
    QSGTextureMaterial m_materialO;
    QSGSmoothTextureMaterial m_smoothMaterial;

    uint m_antialiasing : 1;
    uint m_mirror : 1;
    uint m_dirtyGeometry : 1;

    QSGGeometry m_geometry;
};

QT_END_NAMESPACE

#endif
