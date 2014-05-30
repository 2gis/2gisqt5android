/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qquickiconloader_p.h"

#include <QUrl>
#include <QQmlEngine>
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPixmap>
#include <QQuickImageProvider>
#include <QQmlFile>
#include <qt_windows.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

QQuickIconLoader::QQuickIconLoader(QObject *parent) :
    QObject(parent)
{
}

void QQuickIconLoader::load(const QUrl &url, QQmlEngine *engine)
{
    m_icon = QIcon();
    QString scheme = url.scheme();
    if (scheme == QLatin1String("qrc") || scheme == QLatin1String("file"))
        loadFromFile(url);
    else if (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
        loadFromNetwork(url, engine);
    else if (scheme == QLatin1String("image"))
        loadFromImageProvider(url, engine);
}

QIcon QQuickIconLoader::icon() const
{
    return m_icon;
}

void QQuickIconLoader::onRequestFinished(QNetworkReply *reply)
{
    disconnect(reply->manager(), 0, this, 0);
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QPixmap pixmap;
        if (pixmap.loadFromData(data)) {
            m_icon = QIcon(pixmap);
            emit finished();
        }
    } else {
        qWarning().nospace() << "Cannot load " << reply->url().toString() << " (" << qPrintable(reply->errorString()) << ")";
    }
    reply->deleteLater();
}

void QQuickIconLoader::loadFromFile(const QUrl &url)
{
    QString path = QQmlFile::urlToLocalFileOrQrc(url);
    if (QFileInfo(path).exists()) {
        m_icon = QIcon(path);
        emit finished();
    }
}

void QQuickIconLoader::loadFromNetwork(const QUrl &url, QQmlEngine *engine)
{
    QNetworkRequest request(url);
    QNetworkAccessManager *manager = engine->networkAccessManager();
    connect(manager, SIGNAL(finished(QNetworkReply*)), SLOT(onRequestFinished(QNetworkReply*)));
    manager->get(request);
}

static inline QString imageProviderId(const QUrl &url)
{
    return url.host();
}

static inline QString imageId(const QUrl &url)
{
    return url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority).mid(1);
}

void QQuickIconLoader::loadFromImageProvider(const QUrl &url, QQmlEngine *engine)
{
    const QString providerId = url.host();
    const QString imageId = url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority).mid(1);
    QQuickImageProvider::ImageType imageType = QQuickImageProvider::Invalid;
    QQuickImageProvider *provider = static_cast<QQuickImageProvider *>(engine->imageProvider(providerId));
    QSize size;
    QSize requestSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    if (provider)
        imageType = provider->imageType();
    if (imageType == QQuickImageProvider::Image) {
        QImage image = provider->requestImage(imageId, &size, requestSize);
        if (!image.isNull())
            m_icon = QIcon(QPixmap::fromImage(image));
    } else if (imageType == QQuickImageProvider::Pixmap) {
        QPixmap pixmap = provider->requestPixmap(imageId, &size, requestSize);
        if (!pixmap.isNull())
            m_icon = QIcon(pixmap);
    }
    emit finished();
}

QT_END_NAMESPACE
