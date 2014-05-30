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

#include "qmediaavailabilitycontrol.h"
#include "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QMediaAvailabilityControl

    \brief The QMediaAvailabilityControl class supplies a control for reporting availability of a service.

    \inmodule QtMultimedia


    \ingroup multimedia_control

    An instance of QMediaObject (or its derived classes) can report any changes
    in availability via this control.

    The interface name of QMediaAvailabilityControl is \c org.qt-project.qt.mediaavailabilitycontrol/5.0 as
    defined in QMediaAvailabilityControl_iid.

    \sa QMediaService::requestControl(), QMediaObject
*/

/*!
    \macro QMediaAvailabilityControl_iid

    \c org.qt-project.qt.mediaavailabilitycontrol/5.0

    Defines the interface name of the QMediaAvailabilityControl class.

    \relates QMediaAvailabilityControl
*/

/*!
    Constructs an availability control object with \a parent.
*/
QMediaAvailabilityControl::QMediaAvailabilityControl(QObject *parent)
    : QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destruct the availability control object.
*/
QMediaAvailabilityControl::~QMediaAvailabilityControl()
{
}


/*!
  \fn QMultimedia::AvailabilityStatus QMediaAvailabilityControl::availability() const

  Returns the current availability of this instance of the media service.
  If the availability changes at run time (for example, some other media
  client takes all media resources) the availabilityChanges() signal
  should be emitted.
*/


/*!
    \fn void QMediaAvailabilityControl::availabilityChanged(QMultimedia::AvailabilityStatus availability)

    Signal emitted when the current \a availability value changed.
*/

#include "moc_qmediaavailabilitycontrol.cpp"
QT_END_NAMESPACE
