/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGEOCODINGMANAGERENGINE_H
#define QGEOCODINGMANAGERENGINE_H

#include <QtCore/QObject>
#include <QtLocation/qlocationglobal.h>
#include <QtLocation/QGeoCodeReply>

QT_BEGIN_NAMESPACE

class QGeoAddress;
class QGeoShape;
class QGeoCodingManagerEnginePrivate;

class Q_LOCATION_EXPORT QGeoCodingManagerEngine : public QObject
{
    Q_OBJECT
public:
    QGeoCodingManagerEngine(const QVariantMap &parameters, QObject *parent = 0);
    virtual ~QGeoCodingManagerEngine();

    QString managerName() const;
    int managerVersion() const;

    virtual QGeoCodeReply *geocode(const QGeoAddress &address, const QGeoShape &bounds);
    virtual QGeoCodeReply *geocode(const QString &address,
                                   int limit,
                                   int offset,
                                   const QGeoShape &bounds);
    virtual QGeoCodeReply *reverseGeocode(const QGeoCoordinate &coordinate,
                                          const QGeoShape &bounds);


    void setLocale(const QLocale &locale);
    QLocale locale() const;

Q_SIGNALS:
    void finished(QGeoCodeReply *reply);
    void error(QGeoCodeReply *reply, QGeoCodeReply::Error error, QString errorString = QString());

private:
    void setManagerName(const QString &managerName);
    void setManagerVersion(int managerVersion);

    QGeoCodingManagerEnginePrivate *d_ptr;
    Q_DISABLE_COPY(QGeoCodingManagerEngine)

    friend class QGeoServiceProvider;
    friend class QGeoServiceProviderPrivate;
};

QT_END_NAMESPACE

#endif
