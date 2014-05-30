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

#ifndef QDECLARATIVEGEOSHAPE_H
#define QDECLARATIVEGEOSHAPE_H

#include <QtQml/private/qqmlvaluetype_p.h>
#include <QtPositioning/QGeoShape>

QT_BEGIN_NAMESPACE

class GeoShapeValueType : public QQmlValueTypeBase<QGeoShape>
{
    Q_OBJECT

    Q_PROPERTY(ShapeType type READ type)
    Q_PROPERTY(bool isValid READ isValid)
    Q_PROPERTY(bool isEmpty READ isEmpty)

    Q_ENUMS(ShapeType)

public:
    explicit GeoShapeValueType(QObject *parent = 0);
    ~GeoShapeValueType();

    enum ShapeType {
        UnknownType = QGeoShape::UnknownType,
        RectangleType = QGeoShape::RectangleType,
        CircleType = QGeoShape::CircleType
    };

    ShapeType type() const;
    bool isValid() const;
    bool isEmpty() const;

    Q_INVOKABLE bool contains(const QGeoCoordinate &coordinate) const;

    QString toString() const Q_DECL_OVERRIDE;
    void setValue(const QVariant &value) Q_DECL_OVERRIDE;
    bool isEqual(const QVariant &other) const Q_DECL_OVERRIDE;

protected:
    explicit GeoShapeValueType(int userType, QObject *parent = 0);
};

QT_END_NAMESPACE

#endif
