/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QBLUETOOTHSERVER_P_H
#define QBLUETOOTHSERVER_P_H

#include <QtGlobal>
#include <QList>
#include <QtBluetooth/QBluetoothSocket>
#include "qbluetoothserver.h"
#include "qbluetooth.h"

#ifdef QT_QNX_BLUETOOTH
#include "qnx/ppshelpers_p.h"
#endif

#ifdef QT_BLUEZ_BLUETOOTH
QT_FORWARD_DECLARE_CLASS(QSocketNotifier)
#endif

#ifdef QT_ANDROID_BLUETOOTH
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtBluetooth/QBluetoothUuid>

class ServerAcceptanceThread;
#endif

QT_BEGIN_NAMESPACE

class QBluetoothAddress;
class QBluetoothSocket;

class QBluetoothServer;

class QBluetoothServerPrivate
#ifdef QT_QNX_BLUETOOTH
: public QObject
{
    Q_OBJECT
#else
{
#endif
    Q_DECLARE_PUBLIC(QBluetoothServer)

public:
    QBluetoothServerPrivate(QBluetoothServiceInfo::Protocol serverType);
    ~QBluetoothServerPrivate();

#ifdef QT_BLUEZ_BLUETOOTH
    void _q_newConnection();
#endif

public:
    QBluetoothSocket *socket;

    int maxPendingConnections;
    QBluetooth::SecurityFlags securityFlags;
    QBluetoothServiceInfo::Protocol serverType;

#ifdef QT_QNX_BLUETOOTH
#ifdef QT_QNX_BT_BLUETOOTH
    static void btCallback(long param, int socket);
    Q_INVOKABLE void setBtCallbackParameters(int receivedSocket);
#endif
    QList<QBluetoothSocket *> activeSockets;
    QString m_serviceName;
#endif

protected:
    QBluetoothServer *q_ptr;

private:
    QBluetoothServer::Error m_lastError;
#ifdef QT_QNX_BLUETOOTH
    QBluetoothUuid m_uuid;
    bool serverRegistered;
    QString nextClientAddress;

private Q_SLOTS:
#ifndef QT_QNX_BT_BLUETOOTH
    void controlReply(ppsResult result);
    void controlEvent(ppsResult result);
#endif
#elif defined(QT_BLUEZ_BLUETOOTH)
    QSocketNotifier *socketNotifier;
#elif defined(QT_ANDROID_BLUETOOTH)
    ServerAcceptanceThread *thread;
    QString m_serviceName;
    QBluetoothUuid m_uuid;
public:
    bool isListening() const;
    bool initiateActiveListening(const QBluetoothUuid& uuid, const QString &serviceName);
    bool deactivateActiveListening();

#endif
};

QT_END_NAMESPACE

#endif
