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

#include "qimagevideobuffer_p.h"

#include "qabstractvideobuffer_p.h"

#include <qimage.h>
#include <qvariant.h>

QT_BEGIN_NAMESPACE

/*!
 * \class QImageVideoBuffer
 * \internal
 *
 * A video buffer class for a QImage.
 */
class QImageVideoBufferPrivate : public QAbstractVideoBufferPrivate
{
public:
    QImageVideoBufferPrivate()
        : mapMode(QAbstractVideoBuffer::NotMapped)
    {
    }

    QAbstractVideoBuffer::MapMode mapMode;
    QImage image;
};

QImageVideoBuffer::QImageVideoBuffer(const QImage &image)
    : QAbstractVideoBuffer(*new QImageVideoBufferPrivate, NoHandle)
{
    Q_D(QImageVideoBuffer);

    d->image = image;
}

QImageVideoBuffer::~QImageVideoBuffer()
{
}

QAbstractVideoBuffer::MapMode QImageVideoBuffer::mapMode() const
{
    return d_func()->mapMode;
}

uchar *QImageVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    Q_D(QImageVideoBuffer);

    if (d->mapMode == NotMapped && d->image.bits() && mode != NotMapped) {
        d->mapMode = mode;

        if (numBytes)
            *numBytes = d->image.byteCount();

        if (bytesPerLine)
            *bytesPerLine = d->image.bytesPerLine();

        return d->image.bits();
    } else {
        return 0;
    }
}

void QImageVideoBuffer::unmap()
{
    Q_D(QImageVideoBuffer);

    d->mapMode = NotMapped;
}

QT_END_NAMESPACE
