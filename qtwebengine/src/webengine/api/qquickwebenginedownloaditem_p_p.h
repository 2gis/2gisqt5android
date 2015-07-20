/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKWEBENGINEDOWNLOADITEM_P_P_H
#define QQUICKWEBENGINEDOWNLOADITEM_P_P_H

#include "browser_context_adapter_client.h"
#include "qquickwebenginedownloaditem_p.h"
#include <private/qtwebengineglobal_p.h>
#include <QString>

QT_BEGIN_NAMESPACE
class QQuickWebEngineProfilePrivate;

class QQuickWebEngineDownloadItemPrivate {
    QQuickWebEngineDownloadItem *q_ptr;
    QQuickWebEngineProfilePrivate* profile;
    friend class QQuickWebEngineProfilePrivate;
public:
    Q_DECLARE_PUBLIC(QQuickWebEngineDownloadItem)
    QQuickWebEngineDownloadItemPrivate(QQuickWebEngineProfilePrivate *p);
    ~QQuickWebEngineDownloadItemPrivate();

    quint32 downloadId;
    QQuickWebEngineDownloadItem::DownloadState downloadState;
    qint64 totalBytes;
    qint64 receivedBytes;
    QString downloadPath;

    void update(const QtWebEngineCore::BrowserContextAdapterClient::DownloadItemInfo &info);
    void updateState(QQuickWebEngineDownloadItem::DownloadState newState);
};

QT_END_NAMESPACE

#endif // QQUICKWEBENGINEDOWNLOADITEM_P_P_H
