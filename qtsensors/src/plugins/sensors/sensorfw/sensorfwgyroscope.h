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


#ifndef SENSORFWGYROSCOPE_H
#define SENSORFWGYROSCOPE_H

#include "sensorfwsensorbase.h"
#include <QtSensors/qgyroscope.h>
#include <datatypes/xyz.h>
#include <gyroscopesensor_i.h>




class SensorfwGyroscope : public SensorfwSensorBase
{
    Q_OBJECT

public:
    static char const * const id;
    SensorfwGyroscope(QSensor *sensor);
protected:
    bool doConnect() Q_DECL_OVERRIDE;
    QString sensorName() const Q_DECL_OVERRIDE;
    qreal correctionFactor() const Q_DECL_OVERRIDE;
    virtual void init();

private:
    QGyroscopeReading m_reading;
    bool m_initDone;
    static const float MILLI;
private slots:
    void slotDataAvailable(const XYZ& data);
    void slotFrameAvailable(const QVector<XYZ>&);

};


#endif // sensorfwGYROSCOPE_H
