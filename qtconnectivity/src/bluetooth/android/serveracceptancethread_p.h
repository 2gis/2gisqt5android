/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#ifndef SERVERACCEPTANCETHREAD_H
#define SERVERACCEPTANCETHREAD_H

#include <QtCore/QMutex>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtBluetooth/QBluetoothServer>
#include <QtBluetooth/QBluetoothUuid>
#include "qbluetooth.h"


class ServerAcceptanceThread : public QObject
{
    Q_OBJECT
public:
    explicit ServerAcceptanceThread(QObject *parent = 0);
    ~ServerAcceptanceThread();
    void setServiceDetails(const QBluetoothUuid &uuid, const QString &serviceName,
                           QBluetooth::SecurityFlags securityFlags);


    bool hasPendingConnections() const;
    QAndroidJniObject nextPendingConnection();
    void setMaxPendingConnections(int maximumCount);

    void javaThreadErrorOccurred(int errorCode);
    void javaNewSocket(jobject socket);

    void run();
    void stop();
    bool isRunning() const;

signals:
    void newConnection();
    void error(QBluetoothServer::Error);

private:
    bool validSetup() const;
    void shutdownPendingConnections();

    QList<QAndroidJniObject> pendingSockets;
    mutable QMutex m_mutex;
    QString m_serviceName;
    QBluetoothUuid m_uuid;
    int maxPendingConnections;
    QBluetooth::SecurityFlags secFlags;

    QAndroidJniObject javaThread;

};

#endif // SERVERACCEPTANCETHREAD_H
