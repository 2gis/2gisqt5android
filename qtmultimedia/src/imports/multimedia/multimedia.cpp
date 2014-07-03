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

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include "qsoundeffect.h"

#include <private/qdeclarativevideooutput_p.h>

#include "qdeclarativemediametadata_p.h"
#include "qdeclarativeaudio_p.h"
#include "qdeclarativeradio_p.h"
#include "qdeclarativeradiodata_p.h"
#include "qdeclarativecamera_p.h"
#include "qdeclarativecamerapreviewprovider_p.h"
#include "qdeclarativecameraexposure_p.h"
#include "qdeclarativecameraflash_p.h"
#include "qdeclarativecamerafocus_p.h"
#include "qdeclarativecameraimageprocessing_p.h"
#include "qdeclarativetorch_p.h"

QML_DECLARE_TYPE(QSoundEffect)

QT_BEGIN_NAMESPACE

class QMultimediaDeclarativeModule : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0")

public:
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtMultimedia"));

        qmlRegisterType<QSoundEffect>(uri, 5, 0, "SoundEffect");
        qmlRegisterType<QDeclarativeAudio>(uri, 5, 0, "Audio");
        qmlRegisterType<QDeclarativeAudio>(uri, 5, 0, "MediaPlayer");
        qmlRegisterType<QDeclarativeVideoOutput>(uri, 5, 0, "VideoOutput");
        qmlRegisterType<QDeclarativeVideoOutput, 2>(uri, 5, 2, "VideoOutput");
        qmlRegisterType<QDeclarativeRadio>(uri, 5, 0, "Radio");
        qmlRegisterType<QDeclarativeRadioData>(uri, 5, 0, "RadioData");
        qmlRegisterType<QDeclarativeCamera>(uri, 5, 0, "Camera");
        qmlRegisterType<QDeclarativeTorch>(uri, 5, 0, "Torch");
        qmlRegisterUncreatableType<QDeclarativeCameraCapture>(uri, 5, 0, "CameraCapture",
                                trUtf8("CameraCapture is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraRecorder>(uri, 5, 0, "CameraRecorder",
                                trUtf8("CameraRecorder is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraExposure>(uri, 5, 0, "CameraExposure",
                                trUtf8("CameraExposure is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraFocus>(uri, 5, 0, "CameraFocus",
                                trUtf8("CameraFocus is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraFlash>(uri, 5, 0, "CameraFlash",
                                trUtf8("CameraFlash is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraImageProcessing>(uri, 5, 0, "CameraImageProcessing",
                                trUtf8("CameraImageProcessing is provided by Camera"));

        // Make types available for the 5.3 version
        // Adding "import QtMultimedia 5.3" in QML will fail unless at least one type is registered
        // for that version.
        qmlRegisterType<QSoundEffect>(uri, 5, 3, "SoundEffect");

        qmlRegisterType<QDeclarativeMediaMetaData>();
    }

    void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        Q_UNUSED(uri);
        engine->addImageProvider("camera", new QDeclarativeCameraPreviewProvider);
    }
};

QT_END_NAMESPACE

#include "multimedia.moc"

