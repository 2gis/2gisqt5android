/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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
#ifndef BBCAMERAEXPOSURECONTROL_H
#define BBCAMERAEXPOSURECONTROL_H

#include <qcameraexposurecontrol.h>

QT_BEGIN_NAMESPACE

class BbCameraSession;

class BbCameraExposureControl : public QCameraExposureControl
{
    Q_OBJECT
public:
    explicit BbCameraExposureControl(BbCameraSession *session, QObject *parent = 0);

    virtual bool isParameterSupported(ExposureParameter parameter) const Q_DECL_OVERRIDE;
    virtual QVariantList supportedParameterRange(ExposureParameter parameter, bool *continuous) const Q_DECL_OVERRIDE;

    virtual QVariant requestedValue(ExposureParameter parameter) const Q_DECL_OVERRIDE;
    virtual QVariant actualValue(ExposureParameter parameter) const Q_DECL_OVERRIDE;
    virtual bool setValue(ExposureParameter parameter, const QVariant& value) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void statusChanged(QCamera::Status status);

private:
    BbCameraSession *m_session;
    QCameraExposure::ExposureMode m_requestedExposureMode;
};

QT_END_NAMESPACE

#endif
