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

#include "qmediaservice.h"
#include "qmediaservice_p.h"

#include <QtCore/qtimer.h>



QT_BEGIN_NAMESPACE


/*!
    \class QMediaService
    \brief The QMediaService class provides a common base class for media
    service implementations.
    \ingroup multimedia
    \ingroup multimedia_control
    \ingroup multimedia_core
    \inmodule QtMultimedia

    Media services provide implementations of the functionality promised
    by media objects, and allow multiple providers to implement a QMediaObject.

    To provide the functionality of a QMediaObject media services implement
    QMediaControl interfaces.  Services typically implement one core media
    control which provides the core feature of a media object, and some
    number of additional controls which provide either optional features of
    the media object, or features of a secondary media object or peripheral
    object.

    A pointer to media service's QMediaControl implementation can be obtained
    by passing the control's interface name to the requestControl() function.

    \snippet multimedia-snippets/media.cpp Request control

    Media objects can use services loaded dynamically from plug-ins or
    implemented statically within an applications.  Plug-in based services
    should also implement the QMediaServiceProviderPlugin interface.  Static
    services should implement the QMediaServiceProvider interface.  In general,
    implementing a QMediaService is outside of the scope of this documentation
    and support on the relevant mailing lists or IRC channels should be sought.

    \sa QMediaObject, QMediaControl
*/

/*!
    Construct a media service with the given \a parent. This class is meant as a
    base class for Multimedia services so this constructor is protected.
*/

QMediaService::QMediaService(QObject *parent)
    : QObject(parent)
    , d_ptr(new QMediaServicePrivate)
{
    d_ptr->q_ptr = this;
}

/*!
    \internal
*/
QMediaService::QMediaService(QMediaServicePrivate &dd, QObject *parent)
    : QObject(parent)
    , d_ptr(&dd)
{
    d_ptr->q_ptr = this;
}

/*!
    Destroys a media service.
*/

QMediaService::~QMediaService()
{
    delete d_ptr;
}

/*!
    \fn QMediaControl* QMediaService::requestControl(const char *interface)

    Returns a pointer to the media control implementing \a interface.

    If the service does not implement the control, or if it is unavailable a
    null pointer is returned instead.

    Controls must be returned to the service when no longer needed using the
    releaseControl() function.
*/

/*!
    \fn T QMediaService::requestControl()

    Returns a pointer to the media control of type T implemented by a media service.

    If the service does not implement the control, or if it is unavailable a
    null pointer is returned instead.

    Controls must be returned to the service when no longer needed using the
    releaseControl() function.
*/

/*!
    \fn void QMediaService::releaseControl(QMediaControl *control);

    Releases a \a control back to the service.
*/

#include "moc_qmediaservice.cpp"

QT_END_NAMESPACE

