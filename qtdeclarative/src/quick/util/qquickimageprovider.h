/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QQUICKIMAGEPROVIDER_H
#define QQUICKIMAGEPROVIDER_H

#include <QtQuick/qtquickglobal.h>
#include <QtGui/qimage.h>
#include <QtGui/qpixmap.h>
#include <QtQml/qqmlengine.h>

QT_BEGIN_NAMESPACE


class QQuickImageProviderPrivate;
class QSGTexture;
class QQuickWindow;

class Q_QUICK_EXPORT QQuickTextureFactory : public QObject
{
public:
    QQuickTextureFactory();
    virtual ~QQuickTextureFactory();

    virtual QSGTexture *createTexture(QQuickWindow *window) const = 0;
    virtual QSize textureSize() const = 0;
    virtual int textureByteCount() const = 0;
    virtual QImage image() const;
};

class Q_QUICK_EXPORT QQuickImageProvider : public QQmlImageProviderBase
{
public:
    QQuickImageProvider(ImageType type, Flags flags = 0);
    virtual ~QQuickImageProvider();

    ImageType imageType() const;
    Flags flags() const;

    virtual QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize);
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize& requestedSize);
    virtual QQuickTextureFactory *requestTexture(const QString &id, QSize *size, const QSize &requestedSize);

private:
    QQuickImageProviderPrivate *d;
};

QT_END_NAMESPACE

#endif // QQUICKIMAGEPROVIDER_H
