/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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

#ifndef ENGINIOBACKENDCONNECTION_P_H
#define ENGINIOBACKENDCONNECTION_P_H

#include <QtCore/qbasictimer.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtNetwork/qabstractsocket.h>

#include <Enginio/enginioclient_global.h>

QT_BEGIN_NAMESPACE

class EnginioClient;
class EnginioClientConnectionPrivate;
class EnginioReply;
class QTcpSocket;

class ENGINIOCLIENT_EXPORT EnginioBackendConnection : public QObject
{
    Q_OBJECT

    enum WebSocketOpcode
    {
        ContinuationFrameOp = 0x0,
        TextFrameOp = 0x1,
        BinaryFrameOp = 0x2,
        // %x3-7 are reserved for further non-control frames
        ConnectionCloseOp = 0x8,
        PingOp = 0x9,
        PongOp = 0xA
        // %xB-F are reserved for further control frames
    } _protocolOpcode;

    enum ProtocolDecodeState
    {
        HandshakePending,
        FrameHeaderPending,
        PayloadDataPending
    } _protocolDecodeState;

    bool _sentCloseFrame;
    bool _isFinalFragment;
    bool _isPayloadMasked;
    quint64 _payloadLength;
    QByteArray _applicationData;

    QUrl _socketUrl;
    QByteArray _handshakeReply;
    QTcpSocket *_tcpSocket;
    QBasicTimer _keepAliveTimer;
    QBasicTimer _pingTimeoutTimer;

public:
    enum WebSocketCloseStatus
    {
        UnknownCloseStatus = -1,
        NormalCloseStatus = 1000,
        GoingAwayCloseStatus = 1001,
        ProtocolErrorCloseStatus = 1002,
        UnsupportedDataTypeCloseStatus = 1003,
        // 1004 Reserved. Not used.
        // 1005 Missing status code. Not used.
        // 1006 Abnormal disconnect. Not used.
        InconsistentDataTypeCloseStatus = 1007,
        PolicyViolationCloseStatus = 1008,
        MessageTooBigCloseStatus = 1009,
        MissingExtensionClientCloseStatus = 1010,
        BadOperationServerCloseStatus = 1011
        // 1015 TLS handshake failed. Not used.
    };

    enum ConnectionState {
        DisconnectedState,
        ConnectingState,
        ConnectedState
    };

    explicit EnginioBackendConnection(QObject *parent = 0);

    bool isConnected() { return _protocolDecodeState > HandshakePending; }
    void connectToBackend(EnginioClientConnectionPrivate *client, const QJsonObject& messageFilter = QJsonObject());
    void connectToBackend(EnginioClient *client, const QJsonObject& messageFilter = QJsonObject());

    void close(WebSocketCloseStatus closeStatus = NormalCloseStatus);
    void ping();

signals:
    void stateChanged(ConnectionState state);
    void dataReceived(QJsonObject data);
    void timeOut();
    void pong();

private slots:
    void onEnginioFinished(EnginioReply *);
    void onSocketStateChanged(QAbstractSocket::SocketState);
    void onSocketConnectionError(QAbstractSocket::SocketError);
    void onSocketReadyRead();

private:
    void timerEvent(QTimerEvent *event);
    void protocolError(const char* message, WebSocketCloseStatus status = ProtocolErrorCloseStatus);
};

QT_END_NAMESPACE

#endif // ENGINIOBACKENDCONNECTION_P_H
