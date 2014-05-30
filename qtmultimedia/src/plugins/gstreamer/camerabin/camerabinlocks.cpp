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

#include "camerabinlocks.h"
#include "camerabinsession.h"
#include "camerabinfocus.h"

#include <gst/interfaces/photography.h>

#include <QDebug>

QT_BEGIN_NAMESPACE

CameraBinLocks::CameraBinLocks(CameraBinSession *session)
    :QCameraLocksControl(session),
     m_session(session),
     m_focus(m_session->cameraFocusControl())
{
    connect(m_focus, SIGNAL(_q_focusStatusChanged(QCamera::LockStatus,QCamera::LockChangeReason)),
            this, SLOT(updateFocusStatus(QCamera::LockStatus,QCamera::LockChangeReason)));
}

CameraBinLocks::~CameraBinLocks()
{
}

QCamera::LockTypes CameraBinLocks::supportedLocks() const
{
    return QCamera::LockFocus;
}

QCamera::LockStatus CameraBinLocks::lockStatus(QCamera::LockType lock) const
{
    return lock == QCamera::LockFocus ? m_focus->focusStatus() : QCamera::Unlocked;
}

void CameraBinLocks::searchAndLock(QCamera::LockTypes locks)
{
    if (locks & QCamera::LockFocus)
        m_focus->_q_startFocusing();
}

void CameraBinLocks::unlock(QCamera::LockTypes locks)
{
    if (locks & QCamera::LockFocus)
        m_focus->_q_stopFocusing();
}

void CameraBinLocks::updateFocusStatus(QCamera::LockStatus status, QCamera::LockChangeReason reason)
{
    emit lockStatusChanged(QCamera::LockFocus, status, reason);
}

QT_END_NAMESPACE
