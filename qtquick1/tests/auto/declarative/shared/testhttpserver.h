/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef TESTHTTPSERVER_H
#define TESTHTTPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QUrl>
#include <QPair>

class TestHTTPServer : public QObject
{
    Q_OBJECT
public:
    TestHTTPServer(quint16 port);

    bool isValid() const;

    enum Mode { Normal, Delay, Disconnect };
    bool serveDirectory(const QString &, Mode = Normal);

    bool wait(const QUrl &expect, const QUrl &reply, const QUrl &body);
    bool hasFailed() const;

    void addAlias(const QString &filename, const QString &aliasName);
    void addRedirect(const QString &filename, const QString &redirectName);

private slots:
    void newConnection();
    void disconnected();
    void readyRead();
    void sendOne();

private:
    void serveGET(QTcpSocket *, const QByteArray &);
    bool reply(QTcpSocket *, const QByteArray &);

    QList<QPair<QString, Mode> > dirs;
    QHash<QTcpSocket *, QByteArray> dataCache;
    QList<QPair<QTcpSocket *, QByteArray> > toSend;

    QByteArray waitData;
    QByteArray replyData;
    QByteArray bodyData;
    bool m_hasFailed;

    QHash<QString,QString> aliases;
    QHash<QString,QString> redirects;

    QTcpServer server;
};

#endif // TESTHTTPSERVER_H

