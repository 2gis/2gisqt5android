/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "winrtaccelerometer.h"
#include "winrtcompass.h"
#include "winrtgyroscope.h"
#include "winrtrotationsensor.h"
#ifndef Q_OS_WINPHONE
#  include "winrtambientlightsensor.h"
#  include "winrtorientationsensor.h"
#endif
#include <QtSensors/QAccelerometer>
#include <QtSensors/QAmbientLightSensor>
#include <QtSensors/QCompass>
#include <QtSensors/QGyroscope>
#include <QtSensors/QOrientationSensor>
#include <QtSensors/QRotationSensor>
#include <QtSensors/QSensorBackendFactory>
#include <QtSensors/QSensorManager>
#include <QtSensors/QSensorPluginInterface>

class WinRtSensorPlugin : public QObject, public QSensorPluginInterface, public QSensorBackendFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.qt-project.Qt.QSensorPluginInterface/1.0" FILE "plugin.json")
    Q_INTERFACES(QSensorPluginInterface)
public:
    void registerSensors()
    {
        QSensorManager::registerBackend(QAccelerometer::type, QByteArrayLiteral("WinRtAccelerometer"), this);
        QSensorManager::registerBackend(QCompass::type, QByteArrayLiteral("WinRtCompass"), this);
        QSensorManager::registerBackend(QGyroscope::type, QByteArrayLiteral("WinRtGyroscope"), this);
        QSensorManager::registerBackend(QRotationSensor::type, QByteArrayLiteral("WinRtRotationSensor"), this);
#ifndef Q_OS_WINPHONE
        QSensorManager::registerBackend(QAmbientLightSensor::type, QByteArrayLiteral("WinRtAmbientLightSensor"), this);
        QSensorManager::registerBackend(QOrientationSensor::type, QByteArrayLiteral("WinRtOrientationSensor"), this);
#endif
    }

    QSensorBackend *createBackend(QSensor *sensor)
    {
        if (sensor->identifier() == QByteArrayLiteral("WinRtAccelerometer"))
            return new WinRtAccelerometer(sensor);

        if (sensor->identifier() == QByteArrayLiteral("WinRtCompass"))
            return new WinRtCompass(sensor);

        if (sensor->identifier() == QByteArrayLiteral("WinRtGyroscope"))
            return new WinRtGyroscope(sensor);

        if (sensor->identifier() == QByteArrayLiteral("WinRtRotationSensor"))
            return new WinRtRotationSensor(sensor);

#ifndef Q_OS_WINPHONE
        if (sensor->identifier() == QByteArrayLiteral("WinRtAmbientLightSensor"))
            return new WinRtAmbientLightSensor(sensor);

        if (sensor->identifier() == QByteArrayLiteral("WinRtOrientationSensor"))
            return new WinRtOrientationSensor(sensor);
#endif // !Q_OS_WINPHONE

        return 0;
    }
};

#include "main.moc"

