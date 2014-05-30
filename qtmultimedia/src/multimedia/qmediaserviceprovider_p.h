/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#ifndef QMEDIASERVICEPROVIDER_H
#define QMEDIASERVICEPROVIDER_H

#include <QtCore/qobject.h>
#include <QtCore/qshareddata.h>
#include <qtmultimediadefs.h>
#include "qmultimedia.h"
#include "qmediaserviceproviderplugin.h"

QT_BEGIN_NAMESPACE


class QMediaService;

class Q_MULTIMEDIA_EXPORT QMediaServiceProvider : public QObject
{
    Q_OBJECT

public:
    virtual QMediaService* requestService(const QByteArray &type, const QMediaServiceProviderHint &hint = QMediaServiceProviderHint()) = 0;
    virtual void releaseService(QMediaService *service) = 0;

    virtual QMultimedia::SupportEstimate hasSupport(const QByteArray &serviceType,
                                             const QString &mimeType,
                                             const QStringList& codecs,
                                             int flags = 0) const;
    virtual QStringList supportedMimeTypes(const QByteArray &serviceType, int flags = 0) const;

    virtual QByteArray defaultDevice(const QByteArray &serviceType) const;
    virtual QList<QByteArray> devices(const QByteArray &serviceType) const;
    virtual QString deviceDescription(const QByteArray &serviceType, const QByteArray &device);

    virtual QCamera::Position cameraPosition(const QByteArray &device) const;
    virtual int cameraOrientation(const QByteArray &device) const;

    static QMediaServiceProvider* defaultServiceProvider();
    static void setDefaultServiceProvider(QMediaServiceProvider *provider);
};

QT_END_NAMESPACE


#endif  // QMEDIASERVICEPROVIDER_H
