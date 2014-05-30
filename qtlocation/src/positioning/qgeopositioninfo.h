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
****************************************************************************/
#ifndef QGEOPOSITIONINFO_H
#define QGEOPOSITIONINFO_H

#include <QtPositioning/QGeoCoordinate>

#include <QDateTime>

QT_BEGIN_NAMESPACE

class QDebug;
class QDataStream;

class QGeoPositionInfoPrivate;
class Q_POSITIONING_EXPORT QGeoPositionInfo
{
public:
    enum Attribute {
        Direction,
        GroundSpeed,
        VerticalSpeed,
        MagneticVariation,
        HorizontalAccuracy,
        VerticalAccuracy
    };

    QGeoPositionInfo();
    QGeoPositionInfo(const QGeoCoordinate &coordinate, const QDateTime &updateTime);
    QGeoPositionInfo(const QGeoPositionInfo &other);
    ~QGeoPositionInfo();

    QGeoPositionInfo &operator=(const QGeoPositionInfo &other);

    bool operator==(const QGeoPositionInfo &other) const;
    inline bool operator!=(const QGeoPositionInfo &other) const {
        return !operator==(other);
    }

    bool isValid() const;

    void setTimestamp(const QDateTime &timestamp);
    QDateTime timestamp() const;

    void setCoordinate(const QGeoCoordinate &coordinate);
    QGeoCoordinate coordinate() const;

    void setAttribute(Attribute attribute, qreal value);
    qreal attribute(Attribute attribute) const;
    void removeAttribute(Attribute attribute);
    bool hasAttribute(Attribute attribute) const;

private:
#ifndef QT_NO_DEBUG_STREAM
    friend Q_POSITIONING_EXPORT QDebug operator<<(QDebug dbg, const QGeoPositionInfo &info);
#endif
#ifndef QT_NO_DATASTREAM
    friend Q_POSITIONING_EXPORT QDataStream &operator<<(QDataStream &stream, const QGeoPositionInfo &info);
    friend Q_POSITIONING_EXPORT QDataStream &operator>>(QDataStream &stream, QGeoPositionInfo &info);
#endif
    QGeoPositionInfoPrivate *d;
};

#ifndef QT_NO_DEBUG_STREAM
Q_POSITIONING_EXPORT QDebug operator<<(QDebug dbg, const QGeoPositionInfo &info);
#endif

#ifndef QT_NO_DATASTREAM
Q_POSITIONING_EXPORT QDataStream &operator<<(QDataStream &stream, QGeoPositionInfo::Attribute attr);
Q_POSITIONING_EXPORT QDataStream &operator>>(QDataStream &stream, QGeoPositionInfo::Attribute &attr);
Q_POSITIONING_EXPORT QDataStream &operator<<(QDataStream &stream, const QGeoPositionInfo &info);
Q_POSITIONING_EXPORT QDataStream &operator>>(QDataStream &stream, QGeoPositionInfo &info);
#endif

QT_END_NAMESPACE

#endif
