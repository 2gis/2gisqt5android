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


#ifndef QGSTREAMERCAMERACONTROL_H
#define QGSTREAMERCAMERACONTROL_H

#include <QHash>
#include <qcameracontrol.h>
#include "qgstreamercapturesession.h"

QT_BEGIN_NAMESPACE

class QGstreamerCameraControl : public QCameraControl
{
    Q_OBJECT
public:
    QGstreamerCameraControl( QGstreamerCaptureSession *session );
    virtual ~QGstreamerCameraControl();

    bool isValid() const { return true; }

    QCamera::State state() const;
    void setState(QCamera::State state);

    QCamera::Status status() const { return m_status; }

    QCamera::CaptureModes captureMode() const { return m_captureMode; }
    void setCaptureMode(QCamera::CaptureModes mode);

    bool isCaptureModeSupported(QCamera::CaptureModes mode) const;

    QCamera::LockTypes supportedLocks() const
    {
        return QCamera::NoLock;
    }

    bool canChangeProperty(PropertyChangeType changeType, QCamera::Status status) const;

public slots:
    void reloadLater();

private slots:
    void updateStatus();
    void reloadPipeline();


private:
    QCamera::CaptureModes m_captureMode;
    QGstreamerCaptureSession *m_session;
    QCamera::State m_state;
    QCamera::Status m_status;
    bool m_reloadPending;
};

QT_END_NAMESPACE

#endif // QGSTREAMERCAMERACONTROL_H
