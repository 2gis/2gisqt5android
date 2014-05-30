/***************************************************************************
 **
 ** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtNfc module of the Qt Toolkit.
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

#ifndef QNEARFIELDSHAREMANAGER_H
#define QNEARFIELDSHAREMANAGER_H

#include <QtCore/QObject>
#include <QtNfc/qnfcglobal.h>

QT_BEGIN_NAMESPACE

class QNearFieldShareManagerPrivate;
class QNearFieldShareTarget;

class Q_NFC_EXPORT QNearFieldShareManager : public QObject
{
    Q_OBJECT
    Q_ENUMS(ShareError)
    Q_ENUMS(ShareMode)

public:
    explicit QNearFieldShareManager(QObject *parent = 0);
    ~QNearFieldShareManager();

    enum ShareError {
        NoError,
        UnknownError,
        InvalidShareContentError,
        ShareCanceledError,
        ShareInterruptedError,
        ShareRejectedError,
        UnsupportedShareModeError,
        ShareAlreadyInProgressError,
        SharePermissionDeniedError
    };

    enum ShareMode {
        NoShare = 0x00,
        NdefShare = 0x01,
        FileShare = 0x02
    };
    Q_DECLARE_FLAGS(ShareModes, ShareMode)

public:
    static QNearFieldShareManager::ShareModes supportedShareModes();
    void setShareModes(ShareModes modes);
    QNearFieldShareManager::ShareModes shareModes() const;
    QNearFieldShareManager::ShareError shareError() const;

Q_SIGNALS:
    void targetDetected(QNearFieldShareTarget* shareTarget);
    void shareModesChanged(QNearFieldShareManager::ShareModes modes);
    void error(QNearFieldShareManager::ShareError error);

private:
    QScopedPointer<QNearFieldShareManagerPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QNearFieldShareManager)
    Q_DISABLE_COPY(QNearFieldShareManager)

    friend class QNearFieldShareManagerPrivateImpl;
    friend class QNearFieldShareTargetPrivateImpl;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QNearFieldShareManager::ShareModes)

QT_END_NAMESPACE

#endif /* QNEARFIELDSHAREMANAGER_H */
