/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "genericrotationsensor.h"
#include <QDebug>
#include <qmath.h>

#define RADIANS_TO_DEGREES 57.2957795

char const * const genericrotationsensor::id("generic.rotation");

genericrotationsensor::genericrotationsensor(QSensor *sensor)
    : QSensorBackend(sensor)
{
    accelerometer = new QAccelerometer(this);
    accelerometer->addFilter(this);
    accelerometer->connectToBackend();

    setReading<QRotationReading>(&m_reading);
    setDataRates(accelerometer);

    QRotationSensor * const rotationSensor = qobject_cast<QRotationSensor *>(sensor);
    if (rotationSensor)
        rotationSensor->setHasZ(false);
}

void genericrotationsensor::start()
{
    accelerometer->setDataRate(sensor()->dataRate());
    accelerometer->setAlwaysOn(sensor()->isAlwaysOn());
    accelerometer->start();
    if (!accelerometer->isActive())
        sensorStopped();
    if (accelerometer->isBusy())
        sensorBusy();
}

void genericrotationsensor::stop()
{
    accelerometer->stop();
}

bool genericrotationsensor::filter(QSensorReading *reading)
{
    QAccelerometerReading *ar = qobject_cast<QAccelerometerReading*>(reading);
    qreal pitch = 0;
    qreal roll = 0;

    qreal x = ar->x();
    qreal y = ar->y();
    qreal z = ar->z();

    // Note that the formula used come from this document:
    // http://www.freescale.com/files/sensors/doc/app_note/AN3461.pdf
    pitch = qAtan(y / sqrt(x*x + z*z)) * RADIANS_TO_DEGREES;
    roll = qAtan(x / sqrt(y*y + z*z)) * RADIANS_TO_DEGREES;
    // Roll is a left-handed rotation but we need right-handed rotation
    roll = -roll;

    // We need to fix up roll to the (-180,180] range required.
    // Check for negative theta values and apply an offset as required.
    // Note that theta is defined as the angle of the Z axis relative
    // to gravity (see referenced document). It's negative when the
    // face of the device points downward.
    qreal theta = qAtan(sqrt(x*x + y*y) / z) * RADIANS_TO_DEGREES;
    if (theta < 0) {
        if (roll > 0)
            roll = 180 - roll;
        else
            roll = -180 - roll;
    }

    m_reading.setTimestamp(ar->timestamp());
    m_reading.setFromEuler(pitch, roll, 0);
    newReadingAvailable();
    return false;
}

