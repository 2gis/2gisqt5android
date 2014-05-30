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

#include "qproximitysensor.h"
#include "qproximitysensor_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QProximityReading)

/*!
    \class QProximityReading
    \ingroup sensors_reading
    \inmodule QtSensors

    \brief The QProximityReading class represents one reading from the
           proximity sensor.

    \target QProximityReading_Units
    The proximity sensor can only indicate if an object is close or not.

    The distance at which an object is considered close is device-specific. This
    distance may be available in the QSensor::outputRanges property.
*/

/*!
    \property QProximityReading::close
    \brief a value indicating if something is close.

    Set to true if something is close.
    Set to false is nothing is close.

    \sa QProximityReading_Units
*/

bool QProximityReading::close() const
{
    return d->close;
}

/*!
    Sets the close value to \a close.
*/
void QProximityReading::setClose(bool close)
{
    d->close = close;
}

// =====================================================================

/*!
    \class QProximityFilter
    \ingroup sensors_filter
    \inmodule QtSensors

    \brief The QProximityFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QProximityReading
    instead of QSensorReading.
*/

/*!
    \fn QProximityFilter::filter(QProximityReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QProximityFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QProximityReading*>(reading));
}

char const * const QProximitySensor::type("QProximitySensor");

/*!
    \class QProximitySensor
    \ingroup sensors_type
    \inmodule QtSensors

    \brief The QProximitySensor class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QProximityReading instead of a QSensorReading.

    For details about how the sensor works, see \l QProximityReading.

    \sa QProximityReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QProximitySensor::QProximitySensor(QObject *parent)
    : QSensor(QProximitySensor::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QProximitySensor::~QProximitySensor()
{
}

/*!
    \fn QProximitySensor::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QProximityReading *QProximitySensor::reading() const
{
    return static_cast<QProximityReading*>(QSensor::reading());
}

#include "moc_qproximitysensor.cpp"
QT_END_NAMESPACE

