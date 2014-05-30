/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QDECLARATIVECAMERA_H
#define QDECLARATIVECAMERA_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qdeclarativecameracapture_p.h"
#include "qdeclarativecamerarecorder_p.h"

#include <qcamera.h>
#include <qcameraimageprocessing.h>
#include <qcameraimagecapture.h>

#include <QtCore/qbasictimer.h>
#include <QtCore/qdatetime.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QDeclarativeCameraExposure;
class QDeclarativeCameraFocus;
class QDeclarativeCameraFlash;
class QDeclarativeCameraImageProcessing;

class QDeclarativeCamera : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(CaptureMode captureMode READ captureMode WRITE setCaptureMode NOTIFY captureModeChanged)
    Q_PROPERTY(State cameraState READ cameraState WRITE setCameraState NOTIFY cameraStateChanged)
    Q_PROPERTY(Status cameraStatus READ cameraStatus NOTIFY cameraStatusChanged)
    Q_PROPERTY(LockStatus lockStatus READ lockStatus NOTIFY lockStatusChanged)
    Q_PROPERTY(Error errorCode READ errorCode NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

    Q_PROPERTY(Availability availability READ availability NOTIFY availabilityChanged)

    Q_PROPERTY(qreal opticalZoom READ opticalZoom WRITE setOpticalZoom NOTIFY opticalZoomChanged)
    Q_PROPERTY(qreal maximumOpticalZoom READ maximumOpticalZoom NOTIFY maximumOpticalZoomChanged)
    Q_PROPERTY(qreal digitalZoom READ digitalZoom WRITE setDigitalZoom NOTIFY digitalZoomChanged)
    Q_PROPERTY(qreal maximumDigitalZoom READ maximumDigitalZoom NOTIFY maximumDigitalZoomChanged)

    Q_PROPERTY(QObject *mediaObject READ mediaObject NOTIFY mediaObjectChanged SCRIPTABLE false DESIGNABLE false)
    Q_PROPERTY(QDeclarativeCameraCapture* imageCapture READ imageCapture CONSTANT)
    Q_PROPERTY(QDeclarativeCameraRecorder* videoRecorder READ videoRecorder CONSTANT)
    Q_PROPERTY(QDeclarativeCameraExposure* exposure READ exposure CONSTANT)
    Q_PROPERTY(QDeclarativeCameraFlash* flash READ flash CONSTANT)
    Q_PROPERTY(QDeclarativeCameraFocus* focus READ focus CONSTANT)
    Q_PROPERTY(QDeclarativeCameraImageProcessing* imageProcessing READ imageProcessing CONSTANT)

    Q_ENUMS(CaptureMode)
    Q_ENUMS(State)
    Q_ENUMS(Status)
    Q_ENUMS(LockStatus)
    Q_ENUMS(Error)

    Q_ENUMS(FlashMode)
    Q_ENUMS(ExposureMode)
    Q_ENUMS(MeteringMode)

    Q_ENUMS(FocusMode)
    Q_ENUMS(FocusPointMode)
    Q_ENUMS(FocusAreaStatus)
    Q_ENUMS(Availability)

public:
    enum CaptureMode {
        CaptureViewfinder = QCamera::CaptureViewfinder,
        CaptureStillImage = QCamera::CaptureStillImage,
        CaptureVideo = QCamera::CaptureVideo
    };

    enum State
    {
        ActiveState = QCamera::ActiveState,
        LoadedState = QCamera::LoadedState,
        UnloadedState = QCamera::UnloadedState
    };

    enum Status
    {
        UnavailableStatus = QCamera::UnavailableStatus,
        UnloadedStatus = QCamera::UnloadedStatus,
        LoadingStatus = QCamera::LoadingStatus,
        UnloadingStatus = QCamera::UnloadingStatus,
        LoadedStatus = QCamera::LoadedStatus,
        StandbyStatus = QCamera::StandbyStatus,
        StartingStatus = QCamera::StartingStatus,
        StoppingStatus = QCamera::StoppingStatus,
        ActiveStatus = QCamera::ActiveStatus
    };

    enum LockStatus
    {
        Unlocked = QCamera::Unlocked,
        Searching = QCamera::Searching,
        Locked = QCamera::Locked
    };

    enum Error
    {
        NoError = QCamera::NoError,
        CameraError = QCamera::CameraError,
        InvalidRequestError = QCamera::InvalidRequestError,
        ServiceMissingError = QCamera::ServiceMissingError,
        NotSupportedFeatureError = QCamera::NotSupportedFeatureError
    };

    enum FlashMode {
        FlashAuto = QCameraExposure::FlashAuto,
        FlashOff = QCameraExposure::FlashOff,
        FlashOn = QCameraExposure::FlashOn,
        FlashRedEyeReduction = QCameraExposure::FlashRedEyeReduction,
        FlashFill = QCameraExposure::FlashFill,
        FlashTorch = QCameraExposure::FlashTorch,
        FlashVideoLight = QCameraExposure::FlashVideoLight,
        FlashSlowSyncFrontCurtain = QCameraExposure::FlashSlowSyncFrontCurtain,
        FlashSlowSyncRearCurtain = QCameraExposure::FlashSlowSyncRearCurtain,
        FlashManual = QCameraExposure::FlashManual
    };

    enum ExposureMode {
        ExposureAuto = QCameraExposure::ExposureAuto,
        ExposureManual = QCameraExposure::ExposureManual,
        ExposurePortrait = QCameraExposure::ExposurePortrait,
        ExposureNight = QCameraExposure::ExposureNight,
        ExposureBacklight = QCameraExposure::ExposureBacklight,
        ExposureSpotlight = QCameraExposure::ExposureSpotlight,
        ExposureSports = QCameraExposure::ExposureSports,
        ExposureSnow = QCameraExposure::ExposureSnow,
        ExposureBeach = QCameraExposure::ExposureBeach,
        ExposureLargeAperture = QCameraExposure::ExposureLargeAperture,
        ExposureSmallAperture = QCameraExposure::ExposureSmallAperture,
        ExposureModeVendor = QCameraExposure::ExposureModeVendor
    };

    enum MeteringMode {
        MeteringMatrix = QCameraExposure::MeteringMatrix,
        MeteringAverage = QCameraExposure::MeteringAverage,
        MeteringSpot = QCameraExposure::MeteringSpot
    };

    enum FocusMode {
        FocusManual = QCameraFocus::ManualFocus,
        FocusHyperfocal = QCameraFocus::HyperfocalFocus,
        FocusInfinity = QCameraFocus::InfinityFocus,
        FocusAuto = QCameraFocus::AutoFocus,
        FocusContinuous = QCameraFocus::ContinuousFocus,
        FocusMacro = QCameraFocus::MacroFocus
    };

    enum FocusPointMode {
        FocusPointAuto = QCameraFocus::FocusPointAuto,
        FocusPointCenter = QCameraFocus::FocusPointCenter,
        FocusPointFaceDetection = QCameraFocus::FocusPointFaceDetection,
        FocusPointCustom = QCameraFocus::FocusPointCustom
    };

    enum FocusAreaStatus {
        FocusAreaUnused = QCameraFocusZone::Unused,
        FocusAreaSelected = QCameraFocusZone::Selected,
        FocusAreaFocused = QCameraFocusZone::Focused
    };

    enum Availability {
        Available = QMultimedia::Available,
        Busy = QMultimedia::Busy,
        Unavailable = QMultimedia::ServiceMissing,
        ResourceMissing = QMultimedia::ResourceError
    };

    QDeclarativeCamera(QObject *parent = 0);
    ~QDeclarativeCamera();

    QObject *mediaObject() { return m_camera; }

    QDeclarativeCameraCapture *imageCapture() { return m_imageCapture; }
    QDeclarativeCameraRecorder *videoRecorder() { return m_videoRecorder; }
    QDeclarativeCameraExposure *exposure() { return m_exposure; }
    QDeclarativeCameraFlash *flash() { return m_flash; }
    QDeclarativeCameraFocus *focus() { return m_focus; }
    QDeclarativeCameraImageProcessing *imageProcessing() { return m_imageProcessing; }

    CaptureMode captureMode() const;
    State cameraState() const;
    Status cameraStatus() const;

    Error errorCode() const;
    QString errorString() const;

    LockStatus lockStatus() const;

    qreal maximumOpticalZoom() const;
    qreal maximumDigitalZoom() const;

    qreal opticalZoom() const;
    qreal digitalZoom() const;

    Availability availability() const;

public Q_SLOTS:
    void setCaptureMode(CaptureMode mode);

    void start();
    void stop();

    void setCameraState(State state);

    void searchAndLock();
    void unlock();

    void setOpticalZoom(qreal);
    void setDigitalZoom(qreal);

Q_SIGNALS:
    void errorChanged();
    void error(QDeclarativeCamera::Error errorCode, const QString &errorString);

    void captureModeChanged();
    void cameraStateChanged(QDeclarativeCamera::State);
    void cameraStatusChanged();

    void lockStatusChanged();

    void opticalZoomChanged(qreal);
    void digitalZoomChanged(qreal);
    void maximumOpticalZoomChanged(qreal);
    void maximumDigitalZoomChanged(qreal);

    void mediaObjectChanged();

    void availabilityChanged(Availability availability);

private Q_SLOTS:
    void _q_updateState(QCamera::State);
    void _q_error(QCamera::Error);
    void _q_availabilityChanged(QMultimedia::AvailabilityStatus);

protected:
    void classBegin();
    void componentComplete();

private:
    Q_DISABLE_COPY(QDeclarativeCamera)
    QCamera *m_camera;

    QDeclarativeCameraCapture *m_imageCapture;
    QDeclarativeCameraRecorder *m_videoRecorder;
    QDeclarativeCameraExposure *m_exposure;
    QDeclarativeCameraFlash *m_flash;
    QDeclarativeCameraFocus *m_focus;
    QDeclarativeCameraImageProcessing *m_imageProcessing;

    State m_pendingState;
    bool m_componentComplete;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativeCamera))

#endif
