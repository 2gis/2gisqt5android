/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include <QtTest/QtTest>
#include <QDebug>

#include <qaudioformat.h>

#include <qaudiorecorder.h>
#include <qaudioencodersettingscontrol.h>
#include <qmediarecordercontrol.h>
#include <qaudioinputselectorcontrol.h>
#include <qaudiodeviceinfo.h>
#include <qaudioinput.h>
#include <qmediaobject.h>

//TESTED_COMPONENT=src/multimedia

#include "mockmediaserviceprovider.h"
#include "mockmediarecorderservice.h"

QT_USE_NAMESPACE

class tst_QAudioRecorder: public QObject
{
    Q_OBJECT

public slots:
    void init();
    void cleanup();

private slots:
    void testNullService();
    void testNullControl();
    void testAudioSource();
    void testDevices();
    void testAvailability();
    void testAvailableAudioInputChangedSignal();

private:
    QAudioRecorder *audiosource;
    MockMediaRecorderService  *mockMediaRecorderService;
    MockMediaServiceProvider *mockProvider;
};

void tst_QAudioRecorder::init()
{
    mockMediaRecorderService = new MockMediaRecorderService(this, new MockMediaRecorderControl(this));
    mockProvider = new MockMediaServiceProvider(mockMediaRecorderService);
    audiosource = 0;

    QMediaServiceProvider::setDefaultServiceProvider(mockProvider);
}

void tst_QAudioRecorder::cleanup()
{
    delete mockMediaRecorderService;
    delete mockProvider;
    delete audiosource;
    mockMediaRecorderService = 0;
    mockProvider = 0;
    audiosource = 0;
}

void tst_QAudioRecorder::testNullService()
{
    mockProvider->service = 0;
    QAudioRecorder source;

    QVERIFY(!source.isAvailable());
    QCOMPARE(source.availability(), QMultimedia::ServiceMissing);

    QCOMPARE(source.audioInputs().size(), 0);
    QCOMPARE(source.defaultAudioInput(), QString());
    QCOMPARE(source.audioInput(), QString());
}


void tst_QAudioRecorder::testNullControl()
{
    mockMediaRecorderService->hasControls = false;
    QAudioRecorder source;

    QVERIFY(!source.isAvailable());
    QCOMPARE(source.availability(), QMultimedia::ServiceMissing);

    QCOMPARE(source.audioInputs().size(), 0);
    QCOMPARE(source.defaultAudioInput(), QString());
    QCOMPARE(source.audioInput(), QString());

    QCOMPARE(source.audioInputDescription("blah"), QString());

    QSignalSpy deviceNameSpy(&source, SIGNAL(audioInputChanged(QString)));

    source.setAudioInput("blah");
    QCOMPARE(deviceNameSpy.count(), 0);
}

void tst_QAudioRecorder::testAudioSource()
{
    audiosource = new QAudioRecorder;

    QCOMPARE(audiosource->mediaObject()->service(),(QMediaService *) mockMediaRecorderService);
}

void tst_QAudioRecorder::testDevices()
{
    audiosource = new QAudioRecorder;
    QList<QString> devices = audiosource->audioInputs();
    QVERIFY(devices.size() > 0);
    QVERIFY(devices.at(0).compare("device1") == 0);
    QVERIFY(audiosource->audioInputDescription("device1").compare("dev1 comment") == 0);
    QVERIFY(audiosource->defaultAudioInput() == "device1");
    QVERIFY(audiosource->isAvailable() == true);

    QSignalSpy checkSignal(audiosource, SIGNAL(audioInputChanged(QString)));
    audiosource->setAudioInput("device2");
    QVERIFY(audiosource->audioInput().compare("device2") == 0);
    QVERIFY(checkSignal.count() == 1);
    QVERIFY(audiosource->isAvailable() == true);
}

void tst_QAudioRecorder::testAvailability()
{
    QAudioRecorder source;

    QVERIFY(source.isAvailable());
    QCOMPARE(source.availability(), QMultimedia::Available);
}

void tst_QAudioRecorder::testAvailableAudioInputChangedSignal()
{
    // The availabilityChangedSignal is implemented in QAudioRecorder. SO using it to test the signal.
    audiosource = new QAudioRecorder;

    /* Spy the signal availableInputsChanged and audioInputchanged */
    QSignalSpy changed(mockMediaRecorderService->mockAudioInputSelector, SIGNAL(availableInputsChanged()));
    QSignalSpy audioInputchange(audiosource, SIGNAL(availableAudioInputsChanged()));

    /* Add the end points and verify if the available end point changed signal is emitted. */
    QMetaObject::invokeMethod(mockMediaRecorderService->mockAudioInputSelector, "addInputs");
    QVERIFY(changed.count() == 1);
    QVERIFY(audioInputchange.count() == 1);

    /* Now try removes */
    changed.clear();
    audioInputchange.clear();
    QMetaObject::invokeMethod(mockMediaRecorderService->mockAudioInputSelector, "removeInputs");
    QVERIFY(changed.count() == 1);
    QVERIFY(audioInputchange.count() == 1);
}

QTEST_GUILESS_MAIN(tst_QAudioRecorder)

#include "tst_qaudiorecorder.moc"
