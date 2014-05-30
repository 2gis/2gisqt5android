/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qplaceuser.h"
#include "qplaceuser_p.h"

QT_USE_NAMESPACE

QPlaceUserPrivate::QPlaceUserPrivate()
    : QSharedData()
{
}

QPlaceUserPrivate::QPlaceUserPrivate(const QPlaceUserPrivate &other)
    : QSharedData(), userId(other.userId), name(other.name)
{
}

QPlaceUserPrivate::~QPlaceUserPrivate()
{
}

bool QPlaceUserPrivate::operator==(const QPlaceUserPrivate &other) const
{
    return userId == other.userId && name == other.name;
}

/*!
    \class QPlaceUser
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-data
    \since Qt Location 5.0

    \brief The QPlaceUser class represents an individual user.
*/

/*!
    Constructs a new user object.
*/
QPlaceUser::QPlaceUser()
    : d(new QPlaceUserPrivate)
{
}

/*!
    Constructs a copy of \a other.
*/
QPlaceUser::QPlaceUser(const QPlaceUser &other)
    :d(other.d)
{
}

/*!
    Destroys the user object.
*/
QPlaceUser::~QPlaceUser()
{
}

/*!
    Assigns \a other to this user and returns a reference to this user.
*/
QPlaceUser &QPlaceUser::operator=(const QPlaceUser &other)
{
    if (this == &other)
        return *this;

    d = other.d;
    return *this;
}

/*!
    \fn bool QPlaceUser::operator!=(const QPlaceUser &other) const

    Returns true if \a other is not equal to this user,
    otherwise returns false.
*/

/*!
    Returns true if this user is equal to \a other.
    Otherwise returns false.
*/
bool QPlaceUser::operator==(const QPlaceUser &other) const
{
    return (*d) == *(other.d);
}

/*!
    Returns the identifier of the user.
*/
QString QPlaceUser::userId() const
{
    return d->userId;
}

/*!
    Sets the \a identifier of the user.
*/
void QPlaceUser::setUserId(const QString &identifier)
{
    d->userId = identifier;
}

/*!
    Returns the name of the user.
*/
QString QPlaceUser::name() const
{
    return d->name;
}

/*!
    Sets the \a name of the user.
*/

void QPlaceUser::setName(const QString &name)
{
    d->name = name;
}
