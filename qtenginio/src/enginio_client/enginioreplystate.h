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

#ifndef ENGINIOREPLYBASE_H
#define ENGINIOREPLYBASE_H

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qtypeinfo.h>
#include <QtCore/qmetatype.h>
#include <QtNetwork/qnetworkreply.h>

#include <Enginio/enginioclient_global.h>
#include <Enginio/enginio.h>

QT_BEGIN_NAMESPACE

class EnginioClientConnectionPrivate;
class EnginioReplyStatePrivate;
class ENGINIOCLIENT_EXPORT EnginioReplyState: public QObject
{
    Q_OBJECT
    Q_ENUMS(QNetworkReply::NetworkError); // TODO remove me QTBUG-33577
    Q_ENUMS(Enginio::ErrorType); // TODO remove me QTBUG-33577

    Q_PROPERTY(Enginio::ErrorType errorType READ errorType NOTIFY dataChanged)
    Q_PROPERTY(QNetworkReply::NetworkError networkError READ networkError NOTIFY dataChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY dataChanged)
    Q_PROPERTY(int backendStatus READ backendStatus NOTIFY dataChanged)
    Q_PROPERTY(QString requestId READ requestId CONSTANT)

    Q_DECLARE_PRIVATE(EnginioReplyState)

public:
    ~EnginioReplyState();


    Enginio::ErrorType errorType() const Q_REQUIRED_RESULT;
    QNetworkReply::NetworkError networkError() const Q_REQUIRED_RESULT;
    QString errorString() const Q_REQUIRED_RESULT;
    QString requestId() const Q_REQUIRED_RESULT;
    int backendStatus() const Q_REQUIRED_RESULT;

    bool isError() const Q_REQUIRED_RESULT;
    bool isFinished() const Q_REQUIRED_RESULT;

    void setDelayFinishedSignal(bool delay);
    bool delayFinishedSignal() Q_REQUIRED_RESULT;

    void swapNetworkReply(EnginioReplyState *other);
    void setNetworkReply(QNetworkReply *reply);

    QJsonObject data() const Q_REQUIRED_RESULT;

public Q_SLOTS:
    void dumpDebugInfo() const;

Q_SIGNALS:
    void dataChanged();
    void progress(qint64 bytesSent, qint64 bytesTotal);

protected:
    EnginioReplyState(EnginioClientConnectionPrivate *parent, QNetworkReply *reply, EnginioReplyStatePrivate *priv);
    friend class EnginioClient;
    friend class EnginioClientConnectionPrivate;
};

QT_END_NAMESPACE

#endif // ENGINIOREPLYBASE_H
