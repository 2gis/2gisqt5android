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

#include <Enginio/private/enginiofakereply_p.h>
#include <Enginio/private/enginioclient_p.h>
#include <QtCore/qmetaobject.h>
#include <QtNetwork/qnetworkrequest.h>

QT_BEGIN_NAMESPACE

struct FinishedFunctor
{
    QNetworkAccessManager *_qnam;
    EnginioFakeReply *_reply;
    void operator ()()
    {
        _qnam->finished(_reply);
    }
};

EnginioFakeReply::EnginioFakeReply(EnginioClientConnectionPrivate *parent, const QByteArray &msg)
    : QNetworkReply(parent->q_ptr)
    , _msg(msg)
{
    init(parent->networkManager());
}

EnginioFakeReply::EnginioFakeReply(QObject *parent, const QByteArray &msg)
    : QNetworkReply(parent)
    , _msg(msg)
{
    init(EnginioClientConnectionPrivate::prepareNetworkManagerInThread().data());
}

void EnginioFakeReply::init(QNetworkAccessManager *qnam)
{
    QIODevice::open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    setError(ContentNotFoundError, QString::fromUtf8(_msg));
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 400);
    setFinished(true);
    FinishedFunctor fin = {qnam, this};
    QObject::connect(this, &EnginioFakeReply::finished, fin);
    QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
}

void EnginioFakeReply::abort() {}

bool EnginioFakeReply::isSequential() const
{
    return false;
}

qint64 EnginioFakeReply::size() const
{
    return _msg.size();
}

qint64 EnginioFakeReply::readData(char *dest, qint64 n)
{
    if (pos() > _msg.size())
        return -1;
    qint64 size = qMin(qint64(_msg.size() - pos()), n);
    memcpy(dest, _msg.constData(), size);
    return size;
}

qint64 EnginioFakeReply::writeData(const char *data, qint64 maxSize)
{
    Q_UNUSED(data);
    Q_UNUSED(maxSize);
    return -1;
}

QT_END_NAMESPACE
