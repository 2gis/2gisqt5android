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

#include "enginioplugin_p.h"
#include "enginioqmlclient_p.h"
#include "enginioqmlmodel_p.h"
#include "enginioqmlreply_p.h"
#include <Enginio/enginiomodel.h>
#include <Enginio/enginioreply.h>
#include <Enginio/enginioidentity.h>
#include <Enginio/enginiooauth2authentication.h>
#include <Enginio/private/enginioclient_p.h>

#include <qqml.h>
#include <QtQml/qqmlnetworkaccessmanagerfactory.h>
#include <QtQml/qqmlengine.h>

QT_BEGIN_NAMESPACE

/*!
 * \qmlmodule Enginio 1.0
 * \title Enginio QML Plugin
 * 
 * The Enginio QML plugin provides access to the Enginio service through a set of
 * QML types.
 */

class EnginioNetworkAccessManagerFactory: public QQmlNetworkAccessManagerFactory
{
public:
    virtual QNetworkAccessManager *create(QObject *parent) Q_DECL_OVERRIDE
    {
        // Make sure we use the parent to remove the reference but not actually destroy
        // the QNAM.
        QNetworkAccessManagerHolder *holder = new QNetworkAccessManagerHolder(parent);
        holder->_guard = EnginioClientConnectionPrivate::prepareNetworkManagerInThread();
        return holder->_guard.data();
    }
};

void EnginioPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri);

    if (!engine->networkAccessManagerFactory()) {
        static EnginioNetworkAccessManagerFactory factory;
        engine->setNetworkAccessManagerFactory(&factory);
    } else {
        qWarning() << "Enginio client failed to install QQmlNetworkAccessManagerFactory"
                      "on QML engine because a differnt factory is already attached, It"
                      " is recomanded to use QNetworkAccessManager delivered by Enginio";
    }
}

void EnginioPlugin::registerTypes(const char *uri)
{
    // @uri Enginio
    qmlRegisterUncreatableType<Enginio>(uri, 1, 0, "Enginio", "Enginio is an enum container and can not be constructed");
    qmlRegisterUncreatableType<EnginioClientConnection>(uri, 1, 0, "EnginioClientConnection", "EnginioClientConnection should not be instantiated in QML directly.");
    qmlRegisterType<EnginioQmlClient>(uri, 1, 0, "EnginioClient");
    qmlRegisterUncreatableType<EnginioBaseModel>(uri, 1, 0, "EnginioBaseModel", "EnginioBaseModel should not be instantiated in QML directly.");
    qmlRegisterType<EnginioQmlModel>(uri, 1, 0, "EnginioModel");
    qmlRegisterUncreatableType<EnginioReplyState>(uri, 1, 0, "EnginioReplyState", "EnginioReplyState cannot be instantiated.");
    qmlRegisterUncreatableType<EnginioQmlReply>(uri, 1, 0, "EnginioReply", "EnginioReply cannot be instantiated.");
    qmlRegisterUncreatableType<EnginioIdentity>(uri, 1, 0, "EnginioIdentity", "EnginioIdentity can not be instantiated directly");
    qmlRegisterType<EnginioOAuth2Authentication>(uri, 1, 0, "EnginioOAuth2Authentication");
    qmlRegisterUncreatableType<QNetworkReply>(uri, 1, 0, "QNetworkReply", "QNetworkReply is abstract and it can not be constructed");
}

QT_END_NAMESPACE
