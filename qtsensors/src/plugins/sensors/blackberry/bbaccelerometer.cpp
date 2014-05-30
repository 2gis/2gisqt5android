/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
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
#include "bbaccelerometer.h"

BbAccelerometer::BbAccelerometer(QSensor *sensor)
    : BbSensorBackend<QAccelerometerReading>(devicePath(), SENSOR_TYPE_ACCELEROMETER, sensor)
{
    setDescription(QLatin1String("X, Y, and Z axes accelerations in m/s^2"));

    QAccelerometer * const accelerometer = qobject_cast<QAccelerometer *>(sensor);
    if (accelerometer) {
        connect(accelerometer, SIGNAL(accelerationModeChanged(AccelerationMode)),
                this, SLOT(applyAccelerationMode()));
    }
}

bool BbAccelerometer::updateReadingFromEvent(const sensor_event_t &event, QAccelerometerReading *reading)
{
    float x = event.motion.dsp.x;
    float y = event.motion.dsp.y;
    float z = event.motion.dsp.z;
    remapAxes(&x, &y, &z);
    reading->setX(x);
    reading->setY(y);
    reading->setZ(z);
    return true;
}

void BbAccelerometer::start()
{
    applyAccelerationMode();
    BbSensorBackend<QAccelerometerReading>::start();
}

QString BbAccelerometer::devicePath()
{
    return QLatin1String("/dev/sensor/accel");
}

void BbAccelerometer::applyAccelerationMode()
{
    const QAccelerometer * const accelerometer = qobject_cast<QAccelerometer *>(sensor());
    if (accelerometer) {
        QString fileName;
        sensor_type_e sensorType;
        switch (accelerometer->accelerationMode()) {
        case QAccelerometer::Gravity:
            fileName = QLatin1String("/dev/sensor/gravity");
            sensorType = SENSOR_TYPE_GRAVITY;
            break;
        case QAccelerometer::User:
            fileName = QLatin1String("/dev/sensor/linAccel");
            sensorType = SENSOR_TYPE_LINEAR_ACCEL;
            break;
        default:
        case QAccelerometer::Combined:
            fileName = devicePath();
            sensorType = SENSOR_TYPE_ACCELEROMETER;
            break;
        }

        setDevice(fileName, sensorType);
    }
}
