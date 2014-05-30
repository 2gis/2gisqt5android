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

#ifndef QANDROIDCAMERALOCKSCONTROL_H
#define QANDROIDCAMERALOCKSCONTROL_H

#include <qcameralockscontrol.h>

QT_BEGIN_NAMESPACE

class QAndroidCameraSession;
class QTimer;

class QAndroidCameraLocksControl : public QCameraLocksControl
{
    Q_OBJECT
public:
    explicit QAndroidCameraLocksControl(QAndroidCameraSession *session);

    QCamera::LockTypes supportedLocks() const Q_DECL_OVERRIDE;
    QCamera::LockStatus lockStatus(QCamera::LockType lock) const Q_DECL_OVERRIDE;
    void searchAndLock(QCamera::LockTypes locks) Q_DECL_OVERRIDE;
    void unlock(QCamera::LockTypes locks) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void onCameraOpened();
    void onCameraAutoFocusComplete(bool success);
    void onRecalculateTimeOut();
    void onWhiteBalanceChanged();

private:
    void setFocusLockStatus(QCamera::LockStatus status, QCamera::LockChangeReason reason);
    void setWhiteBalanceLockStatus(QCamera::LockStatus status, QCamera::LockChangeReason reason);
    void setExposureLockStatus(QCamera::LockStatus status, QCamera::LockChangeReason reason);

    QAndroidCameraSession *m_session;

    QTimer *m_recalculateTimer;

    QCamera::LockTypes m_supportedLocks;

    QCamera::LockStatus m_focusLockStatus;
    QCamera::LockStatus m_exposureLockStatus;
    QCamera::LockStatus m_whiteBalanceLockStatus;
};

QT_END_NAMESPACE

#endif // QANDROIDCAMERALOCKSCONTROL_H
