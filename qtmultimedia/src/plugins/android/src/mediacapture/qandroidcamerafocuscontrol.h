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

#ifndef QANDROIDCAMERAFOCUSCONTROL_H
#define QANDROIDCAMERAFOCUSCONTROL_H

#include <qcamerafocuscontrol.h>

QT_BEGIN_NAMESPACE

class QAndroidCameraSession;

class QAndroidCameraFocusControl : public QCameraFocusControl
{
    Q_OBJECT
public:
    explicit QAndroidCameraFocusControl(QAndroidCameraSession *session);

    QCameraFocus::FocusModes focusMode() const Q_DECL_OVERRIDE;
    void setFocusMode(QCameraFocus::FocusModes mode) Q_DECL_OVERRIDE;
    bool isFocusModeSupported(QCameraFocus::FocusModes mode) const Q_DECL_OVERRIDE;
    QCameraFocus::FocusPointMode focusPointMode() const Q_DECL_OVERRIDE;
    void setFocusPointMode(QCameraFocus::FocusPointMode mode) Q_DECL_OVERRIDE;
    bool isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const Q_DECL_OVERRIDE;
    QPointF customFocusPoint() const Q_DECL_OVERRIDE;
    void setCustomFocusPoint(const QPointF &point) Q_DECL_OVERRIDE;
    QCameraFocusZoneList focusZones() const Q_DECL_OVERRIDE;

private Q_SLOTS:
    void onCameraOpened();
    void onViewportSizeChanged();
    void onCameraCaptureModeChanged();
    void onAutoFocusStarted();
    void onAutoFocusComplete(bool success);

private:
    void updateFocusZones(QCameraFocusZone::FocusZoneStatus status = QCameraFocusZone::Selected);
    void setCameraFocusArea();

    QAndroidCameraSession *m_session;

    QCameraFocus::FocusModes m_focusMode;
    QCameraFocus::FocusPointMode m_focusPointMode;
    QPointF m_actualFocusPoint;
    QPointF m_customFocusPoint;
    QCameraFocusZoneList m_focusZones;

    QList<QCameraFocus::FocusModes> m_supportedFocusModes;
    bool m_continuousPictureFocusSupported;
    bool m_continuousVideoFocusSupported;

    QList<QCameraFocus::FocusPointMode> m_supportedFocusPointModes;
};

QT_END_NAMESPACE

#endif // QANDROIDCAMERAFOCUSCONTROL_H
