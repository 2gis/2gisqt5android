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

#include <qvideoprobe.h>
#include <qaudiorecorder.h>
#include <qmediaplayer.h>

//TESTED_COMPONENT=src/multimedia

#include "mockmediaserviceprovider.h"
#include "mockmediarecorderservice.h"
#include "mockmediaplayerservice.h"
#include "mockmediaobject.h"

QT_USE_NAMESPACE

class tst_QVideoProbe: public QObject
{
    Q_OBJECT

public slots:
    void init();
    void cleanup();

private slots:
    void testNullService();
    void testPlayer();
    void testPlayerDeleteRecorder();
    void testPlayerDeleteProbe();
    void testRecorder();

private:
    QMediaPlayer *player;
    MockMediaPlayerService *mockMediaPlayerService;
    MockMediaServiceProvider *mockProvider;

    MockMediaRecorderControl *mockMediaRecorderControl;
    MockMediaRecorderService *mockMediaRecorderService;
    MockMediaServiceProvider *mockProviderRecorder;
};

void tst_QVideoProbe::init()
{
    mockMediaPlayerService = new MockMediaPlayerService();
    mockProvider = new MockMediaServiceProvider(mockMediaPlayerService);
    mockProvider->deleteServiceOnRelease = true;
    player = 0;

    mockMediaRecorderControl = new MockMediaRecorderControl(this);
    mockMediaRecorderService = new MockMediaRecorderService(this, mockMediaRecorderControl);
    mockProviderRecorder = new MockMediaServiceProvider(mockMediaRecorderService);
    mockProviderRecorder->deleteServiceOnRelease = true;

    QMediaServiceProvider::setDefaultServiceProvider(mockProvider);
}

void tst_QVideoProbe::cleanup()
{
    delete player;
    delete mockProvider;
    mockMediaPlayerService = 0;
    mockProvider = 0;
    player = 0;

    delete mockMediaRecorderControl;
    delete mockProviderRecorder;
    mockMediaRecorderControl = 0;
    mockMediaRecorderService = 0;
    mockProviderRecorder = 0;
}

void tst_QVideoProbe::testNullService()
{
    mockProvider->service = 0;
    player = new QMediaPlayer;

    QVERIFY(!player->isAvailable());
    QCOMPARE(player->availability(), QMultimedia::ServiceMissing);

    QVideoProbe probe;
    QVERIFY(!probe.isActive());
    QVERIFY(!probe.setSource(player));
    QVERIFY(!probe.isActive());
    delete player;
    player = 0;
    QVERIFY(!probe.isActive());
}

void tst_QVideoProbe::testPlayer()
{
    player = new QMediaPlayer;
    QVERIFY(player->isAvailable());

    QVideoProbe probe;
    QVERIFY(!probe.isActive());
    QVERIFY(probe.setSource(player));
    QVERIFY(probe.isActive());
    probe.setSource((QMediaPlayer*)0);
    QVERIFY(!probe.isActive());
}

void tst_QVideoProbe::testPlayerDeleteRecorder()
{
    player = new QMediaPlayer;
    QVERIFY(player->isAvailable());

    QVideoProbe probe;
    QVERIFY(!probe.isActive());
    QVERIFY(probe.setSource(player));
    QVERIFY(probe.isActive());

    delete player;
    player = 0;
    QVERIFY(!probe.isActive());
    probe.setSource((QMediaPlayer*)0);
    QVERIFY(!probe.isActive());
}

void tst_QVideoProbe::testPlayerDeleteProbe()
{
    player = new QMediaPlayer;
    QVERIFY(player->isAvailable());

    QVideoProbe *probe = new QVideoProbe;
    QVERIFY(!probe->isActive());
    QVERIFY(probe->setSource(player));
    QVERIFY(probe->isActive());

    delete probe;
    QVERIFY(player->isAvailable());
}

void tst_QVideoProbe::testRecorder()
{
    QMediaServiceProvider::setDefaultServiceProvider(mockProviderRecorder);

    QAudioRecorder recorder;
    QVERIFY(recorder.isAvailable());

    QVideoProbe probe;
    QVERIFY(!probe.isActive());
    QVERIFY(!probe.setSource(&recorder)); // No QMediaVideoProbeControl available
    QVERIFY(!probe.isActive());
}

QTEST_GUILESS_MAIN(tst_QVideoProbe)

#include "tst_qvideoprobe.moc"
