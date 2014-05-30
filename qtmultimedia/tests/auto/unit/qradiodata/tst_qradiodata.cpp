/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <QDebug>
#include <QTimer>
#include <QtCore/QMap>

#include <qmediaobject.h>
#include <qmediacontrol.h>
#include <qmediaservice.h>
#include <qradiodatacontrol.h>
#include <qradiodata.h>
#include <qradiotuner.h>

#include "mockmediaserviceprovider.h"
#include "mockmediaservice.h"
#include "mockavailabilitycontrol.h"
#include "mockradiotunercontrol.h"
#include "mockradiodatacontrol.h"

QT_USE_NAMESPACE

class tst_QRadioData: public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();

private slots:
    void testNullService();
    void testNullControl();
    void testAlternativeFrequencies();
    void testRadioDataUpdates();

private:
    MockAvailabilityControl  *mockAvailability;
    MockRadioTunerControl    *mockTuner;
    MockRadioDataControl     *mockData;
    MockMediaService     *service;
    MockMediaServiceProvider    *provider;
    QRadioTuner   * radio;
    QRadioData    *radioData;
};

void tst_QRadioData::initTestCase()
{
    qRegisterMetaType<QRadioData::ProgramType>("QRadioData::ProgramType");

    mockAvailability = new MockAvailabilityControl(QMultimedia::Available);
    mockTuner = new MockRadioTunerControl(this);
    mockData = new MockRadioDataControl(this);

    QMap<QString, QMediaControl *> map;
    map.insert(QRadioTunerControl_iid, mockTuner);
    map.insert(QRadioDataControl_iid, mockData);
    map.insert(QMediaAvailabilityControl_iid, mockAvailability);

    service = new MockMediaService(this, map);
    provider = new MockMediaServiceProvider(service);
    QMediaServiceProvider::setDefaultServiceProvider(provider);

    radio = new QRadioTuner;
    radioData = radio->radioData();

    QVERIFY(radioData->availability() == QMultimedia::Available);
}

void tst_QRadioData::cleanupTestCase()
{
    QVERIFY(radioData->error() == QRadioData::NoError);
    QVERIFY(radioData->errorString().isEmpty());

    delete radio;
    delete service;
    delete provider;
    delete mockAvailability;
}

void tst_QRadioData::testNullService()
{
    const QPair<int, int> nullRange(0, 0);

    MockMediaServiceProvider nullProvider(0);
    QMediaServiceProvider::setDefaultServiceProvider(&nullProvider);
    QRadioTuner radio;
    QRadioData *nullRadioData = radio.radioData();

    QVERIFY(nullRadioData == 0);

    QRadioData radioData(&radio);

    QCOMPARE(radioData.error(), QRadioData::ResourceError);
    QCOMPARE(radioData.errorString(), QString());
    QCOMPARE(radioData.stationId(), QString());
    QCOMPARE(radioData.programType(), QRadioData::Undefined);
    QCOMPARE(radioData.programTypeName(), QString());
    QCOMPARE(radioData.stationName(), QString());
    QCOMPARE(radioData.radioText(), QString());
    QCOMPARE(radioData.isAlternativeFrequenciesEnabled(), false);

}

void tst_QRadioData::testNullControl()
{
    const QPair<int, int> nullRange(0, 0);

    MockMediaService service(0, 0);
    MockMediaServiceProvider provider(&service);
    QMediaServiceProvider::setDefaultServiceProvider(&provider);
    QRadioTuner radio;
    QRadioData *radioData = radio.radioData();
    QCOMPARE(radioData->error(), QRadioData::ResourceError);
    QCOMPARE(radioData->errorString(), QString());

    QCOMPARE(radioData->stationId(), QString());
    QCOMPARE(radioData->programType(), QRadioData::Undefined);
    QCOMPARE(radioData->programTypeName(), QString());
    QCOMPARE(radioData->stationName(), QString());
    QCOMPARE(radioData->radioText(), QString());
    QCOMPARE(radioData->isAlternativeFrequenciesEnabled(), false);
    {
        QSignalSpy spy(radioData, SIGNAL(alternativeFrequenciesEnabledChanged(bool)));

        radioData->setAlternativeFrequenciesEnabled(true);
        QCOMPARE(radioData->isAlternativeFrequenciesEnabled(), false);
        QCOMPARE(spy.count(), 0);
    }
}

void tst_QRadioData::testAlternativeFrequencies()
{
    QSignalSpy readSignal(radioData, SIGNAL(alternativeFrequenciesEnabledChanged(bool)));
    radioData->setAlternativeFrequenciesEnabled(true);
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(radioData->isAlternativeFrequenciesEnabled() == true);
    QVERIFY(readSignal.count() == 1);
}

void tst_QRadioData::testRadioDataUpdates()
{
    QSignalSpy rtSpy(radioData, SIGNAL(radioTextChanged(QString)));
    QSignalSpy ptyPTYSpy(radioData, SIGNAL(programTypeChanged(QRadioData::ProgramType)));
    QSignalSpy ptynSpy(radioData, SIGNAL(programTypeNameChanged(QString)));
    QSignalSpy piSpy(radioData, SIGNAL(stationIdChanged(QString)));
    QSignalSpy psSpy(radioData, SIGNAL(stationNameChanged(QString)));
    mockData->forceRT("Mock Radio Text");
    mockData->forceProgramType(static_cast<int>(QRadioData::Sport));
    mockData->forcePTYN("Mock Programme Type Name");
    mockData->forcePI("Mock Programme Identification");
    mockData->forcePS("Mock Programme Service");
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(rtSpy.count() == 1);
    QVERIFY(ptyPTYSpy.count() == 1);
    QVERIFY(ptynSpy.count() == 1);
    QVERIFY(piSpy.count() == 1);
    QVERIFY(psSpy.count() == 1);
    QCOMPARE(radioData->radioText(), QString("Mock Radio Text"));
    QCOMPARE(radioData->programType(), QRadioData::Sport);
    QCOMPARE(radioData->programTypeName(), QString("Mock Programme Type Name"));
    QCOMPARE(radioData->stationId(), QString("Mock Programme Identification"));
    QCOMPARE(radioData->stationName(), QString("Mock Programme Service"));
}

QTEST_GUILESS_MAIN(tst_QRadioData)
#include "tst_qradiodata.moc"
