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

#ifndef QGEOPOSITIONINFOSOURCECL_H
#define QGEOPOSITIONINFOSOURCECL_H

#import <CoreLocation/CoreLocation.h>

#include "qgeopositioninfosource.h"
#include "qgeopositioninfo.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

class QGeoPositionInfoSourceCL : public QGeoPositionInfoSource
{
    Q_OBJECT
public:
    QGeoPositionInfoSourceCL(QObject *parent = 0);
    ~QGeoPositionInfoSourceCL();

    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const;
    PositioningMethods supportedPositioningMethods() const;

    void setUpdateInterval(int msec);
    int minimumUpdateInterval() const;
    Error error() const;

    void locationDataAvailable(QGeoPositionInfo location);
    void setError(QGeoPositionInfoSource::Error positionError);

private:
    bool enableLocationManager();
    void setTimeoutInterval(int msec);

public Q_SLOTS:
    void startUpdates();
    void stopUpdates();

    void requestUpdate(int timeout = 0);

protected:
    virtual void timerEvent(QTimerEvent *event);

private:
    Q_DISABLE_COPY(QGeoPositionInfoSourceCL);
    CLLocationManager *m_locationManager;
    bool m_started;

    QGeoPositionInfo m_lastUpdate;

    int m_updateTimer;
    int m_updateTimeout;

    QGeoPositionInfoSource::Error m_positionError;
};

QT_END_NAMESPACE

#endif // QGEOPOSITIONINFOSOURCECL_H
