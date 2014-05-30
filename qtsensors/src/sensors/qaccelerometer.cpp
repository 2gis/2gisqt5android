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

#include "qaccelerometer.h"
#include "qaccelerometer_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QAccelerometerReading)

/*!
    \class QAccelerometerReading
    \ingroup sensors_reading
    \inmodule QtSensors

    \brief The QAccelerometerReading class reports on linear acceleration
           along the X, Y and Z axes.

    \section2 QAccelerometerReading Units
    The scale of the values is meters per second squared.
    The axes are arranged as follows.

    \image sensors-coordinates2.jpg

    A monoblock device sitting at rest, face up on a desk will experience
    a force of approximately 9.8 on the Z axis (ie. towards the roof).
    This is the proper acceleration the device experiences relative to
    freefall.
*/

/*!
    \property QAccelerometerReading::x
    \brief the acceleration on the X axis.

    The scale of the values is meters per second squared.
    \sa {QAccelerometerReading Units}
*/

qreal QAccelerometerReading::x() const
{
    return d->x;
}

/*!
    Sets the acceleration on the X axis to \a x.
*/
void QAccelerometerReading::setX(qreal x)
{
    d->x = x;
}

/*!
    \property QAccelerometerReading::y
    \brief the acceleration on the Y axis.

    The scale of the values is meters per second squared.
    \sa {QAccelerometerReading Units}
*/

qreal QAccelerometerReading::y() const
{
    return d->y;
}

/*!
    Sets the acceleration on the Y axis to \a y.
*/
void QAccelerometerReading::setY(qreal y)
{
    d->y = y;
}

/*!
    \property QAccelerometerReading::z
    \brief the acceleration on the Z axis.

    The scale of the values is meters per second squared.
    \sa {QAccelerometerReading Units}
*/

qreal QAccelerometerReading::z() const
{
    return d->z;
}

/*!
    Sets the acceleration on the Z axis to \a z.
*/
void QAccelerometerReading::setZ(qreal z)
{
    d->z = z;
}

// =====================================================================

/*!
    \class QAccelerometerFilter
    \ingroup sensors_filter
    \inmodule QtSensors

    \brief The QAccelerometerFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QAccelerometerReading
    instead of QSensorReading.
*/

/*!
    \fn QAccelerometerFilter::filter(QAccelerometerReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QAccelerometerFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QAccelerometerReading*>(reading));
}

char const * const QAccelerometer::type("QAccelerometer");

/*!
    \enum QAccelerometer::AccelerationMode

    \brief This enum represents the acceleration mode of an acceleration sensor.

    The acceleration mode controls how the sensor reports acceleration. QAccelerometer::Combined
    is the only mode in which the values can be directly physically measured, the others are an
    approximation.

    \value Combined Both the acceleration caused by gravity and the acceleration caused by the
                    user moving the device is reported combined.
    \value Gravity  Only the acceleration caused by gravity is reported. Movements of the device
                    caused by the user have no effect other than changing the direction when the
                    device is rotated.
    \value User     Only the acceleration caused by the user moving the device is reported, the
                    effect of gravity is canceled out. A device at rest therefore should report
                    values of, or close to, zero.
                    In other APIs, this mode might be known as \e {linear acceleration}.

    \sa QAccelerometer::accelerationMode
    \since 5.1
*/

/*!
    \class QAccelerometer
    \ingroup sensors_type
    \inmodule QtSensors

    \brief The QAccelerometer class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    It also supports changing the acceleration mode, which controls whether the force of gravity
    is included in the accelerometer values or not.

    Furthermore, this class features a reading() function that returns a QAccelerometerReading
    instead of a QSensorReading.

    For details about how the sensor works, see \l QAccelerometerReading.

    \sa QAccelerometerReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QAccelerometer::QAccelerometer(QObject *parent)
    : QSensor(QAccelerometer::type, *new QAccelerometerPrivate, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QAccelerometer::~QAccelerometer()
{
}

/*!
    \property QAccelerometer::accelerationMode
    \brief The acceleration mode controls how acceleration values are reported.
    \since 5.1

    The acceleration mode controls how the acceleration sensor reports its values.
    The default mode is QAccelerometer::Combined, which means the acceleration caused
    by gravity is included in the reported values.

    Acceleration caused by gravity and acceleration caused by the user moving the device
    are physically impossible to distinguish because of general relativity. Most devices use
    sensor fusion to figure out which parts of the acceleration is caused by gravity, for example
    by using a rotation sensor to calculate the gravity direction and assuming a fixed magnitude
    for gravity. Therefore the result is only an approximation and may be inaccurate.
    The QAccelerometer::Combined mode is the most accurate one, as it does not involve approximating
    the gravity.

    Not all backends and devices might support setting the acceleration mode. For those cases, the
    default mode QAccelerometer::Combined is used, changing it has no effect.
*/
QAccelerometer::AccelerationMode QAccelerometer::accelerationMode() const
{
    Q_D(const QAccelerometer);
    return d->accelerationMode;
}

/*!
    Sets the acceleration mode to \a accelerationMode.
    \since 5.1
*/
void QAccelerometer::setAccelerationMode(QAccelerometer::AccelerationMode accelerationMode)
{
    Q_D(QAccelerometer);
    if (d->accelerationMode != accelerationMode) {
        d->accelerationMode = accelerationMode;
        emit accelerationModeChanged(d->accelerationMode);
    }
}

/*!
    \fn QAccelerometer::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QAccelerometerReading *QAccelerometer::reading() const
{
    return static_cast<QAccelerometerReading*>(QSensor::reading());
}

/*!
    \fn QAccelerometer::accelerationModeChanged(AccelerationMode accelerationMode)

    Emitted when the acceleration mode was changed.

    \since 5.1
*/

#include "moc_qaccelerometer.cpp"
QT_END_NAMESPACE

