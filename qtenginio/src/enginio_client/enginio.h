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

#ifndef ENGINIO_H
#define ENGINIO_H

#include <Enginio/enginioclient_global.h>
#include <QObject>

QT_BEGIN_NAMESPACE

#ifndef Q_QDOC
class ENGINIOCLIENT_EXPORT Enginio
{
    Q_GADGET
#else
namespace Enginio {
#endif
    Q_ENUMS(AuthenticationState)
    Q_ENUMS(Operation)
    Q_ENUMS(ErrorType)
    Q_ENUMS(Role)

#ifndef Q_QDOC
public:
#endif
    enum AuthenticationState {
        NotAuthenticated,
        Authenticating,
        Authenticated,
        AuthenticationFailure
    };

    enum Operation {
        ObjectOperation,
        AccessControlOperation,
        UserOperation,
        UsergroupOperation,
        UsergroupMembersOperation,
        FileOperation,

        // private
        SessionOperation,
        SearchOperation,
        FileChunkUploadOperation,
        FileGetDownloadUrlOperation
    };

    enum Role {
        InvalidRole = -1,
        SyncedRole = Qt::UserRole + 1,
        CreatedAtRole,
        UpdatedAtRole,
        IdRole,
        ObjectTypeRole,
        CustomPropertyRole = Qt::UserRole + 10 // the first fully dynamic role
    };

    enum ErrorType {
        NoError,
        NetworkError,
        BackendError
    };
};

Q_DECLARE_TYPEINFO(Enginio::Operation, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(Enginio::AuthenticationState, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(Enginio::Role, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(Enginio::ErrorType, Q_PRIMITIVE_TYPE);

QT_END_NAMESPACE

Q_DECLARE_METATYPE(Enginio::Operation)
Q_DECLARE_METATYPE(Enginio::AuthenticationState)
Q_DECLARE_METATYPE(Enginio::Role)
Q_DECLARE_METATYPE(Enginio::ErrorType)

#endif // ENGINIO_H
