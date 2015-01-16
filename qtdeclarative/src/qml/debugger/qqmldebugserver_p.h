/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQMLDEBUGSERVER_H
#define QQMLDEBUGSERVER_H

#include <QtQml/qtqmlglobal.h>
#include <private/qqmldebugserverconnection_p.h>
#include <private/qqmldebugservice_p.h>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE


class QQmlDebugServerPrivate;
class Q_QML_PRIVATE_EXPORT QQmlDebugServer : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQmlDebugServer)
    Q_DISABLE_COPY(QQmlDebugServer)
public:
    ~QQmlDebugServer();

    static QQmlDebugServer *instance();

    bool hasDebuggingClient() const;
    bool blockingMode() const;

    QList<QQmlDebugService*> services() const;
    QStringList serviceNames() const;

    void addEngine(QQmlEngine *engine);
    void removeEngine(QQmlEngine *engine);

    bool addService(QQmlDebugService *service);
    bool removeService(QQmlDebugService *service);

    void receiveMessage(const QByteArray &message);

    void sendMessages(QQmlDebugService *service, const QList<QByteArray> &messages);

private slots:
    void wakeEngine(QQmlEngine *engine);

private:
    friend class QQmlDebugService;
    friend class QQmlDebugServicePrivate;
    friend class QQmlDebugServerThread;
    friend struct QQmlDebugServerInstanceWrapper;
    QQmlDebugServer();
    Q_PRIVATE_SLOT(d_func(), void _q_changeServiceState(const QString &serviceName,
                                                        QQmlDebugService::State state))
    Q_PRIVATE_SLOT(d_func(), void _q_sendMessages(QList<QByteArray>))

public:
    static int s_dataStreamVersion;
};

QT_END_NAMESPACE

#endif // QQMLDEBUGSERVICE_H
