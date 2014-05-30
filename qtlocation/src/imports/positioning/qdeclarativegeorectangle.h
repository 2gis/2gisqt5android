/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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
***************************************************************************/

#ifndef QDECLARATIVEGEORECTANGLE_H
#define QDECLARATIVEGEORECTANGLE_H

#include "qdeclarativegeoshape.h"

QT_BEGIN_NAMESPACE

class GeoRectangleValueType : public GeoShapeValueType
{
    Q_OBJECT

    Q_PROPERTY(QGeoCoordinate bottomLeft READ bottomLeft WRITE setBottomLeft)
    Q_PROPERTY(QGeoCoordinate bottomRight READ bottomRight WRITE setBottomRight)
    Q_PROPERTY(QGeoCoordinate topLeft READ topLeft WRITE setTopLeft)
    Q_PROPERTY(QGeoCoordinate topRight READ topRight WRITE setTopRight)
    Q_PROPERTY(QGeoCoordinate center READ center WRITE setCenter)
    Q_PROPERTY(double height READ height WRITE setHeight)
    Q_PROPERTY(double width READ width WRITE setWidth)

public:
    explicit GeoRectangleValueType(QObject *parent = 0);
    ~GeoRectangleValueType();

    QGeoCoordinate bottomLeft();
    void setBottomLeft(const QGeoCoordinate &coordinate);
    QGeoCoordinate bottomRight();
    void setBottomRight(const QGeoCoordinate &coordinate);
    QGeoCoordinate topLeft();
    void setTopLeft(const QGeoCoordinate &coordinate);
    QGeoCoordinate topRight();
    void setTopRight(QGeoCoordinate &coordinate);
    QGeoCoordinate center();
    void setCenter(const QGeoCoordinate &coordinate);
    double height();
    void setHeight(double height);
    double width();
    void setWidth(double width);

    QString toString() const Q_DECL_OVERRIDE;
    void setValue(const QVariant &value) Q_DECL_OVERRIDE;
    QVariant value() Q_DECL_OVERRIDE;
    void write(QObject *obj, int idx, QQmlPropertyPrivate::WriteFlags flags) Q_DECL_OVERRIDE;
    void writeVariantValue(QObject *obj, int idx, QQmlPropertyPrivate::WriteFlags flags, QVariant *from) Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif
