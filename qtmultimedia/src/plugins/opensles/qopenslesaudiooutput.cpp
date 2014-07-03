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

#include "qopenslesaudiooutput.h"
#include "qopenslesengine.h"
#include <QDebug>
#include <qmath.h>

#ifdef ANDROID
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>
#endif // ANDROID

#define BUFFER_COUNT 2
#define DEFAULT_PERIOD_TIME_MS 50
#define MINIMUM_PERIOD_TIME_MS 5
#define EBASE 2.302585093
#define LOG10(x) qLn(x)/qreal(EBASE)

QT_BEGIN_NAMESPACE

QMap<QString, qint32> QOpenSLESAudioOutput::m_categories;

QOpenSLESAudioOutput::QOpenSLESAudioOutput(const QByteArray &device)
    : m_deviceName(device),
      m_state(QAudio::StoppedState),
      m_error(QAudio::NoError),
      m_outputMixObject(Q_NULLPTR),
      m_playerObject(Q_NULLPTR),
      m_playItf(Q_NULLPTR),
      m_volumeItf(Q_NULLPTR),
      m_bufferQueueItf(Q_NULLPTR),
      m_audioSource(Q_NULLPTR),
      m_buffers(Q_NULLPTR),
      m_volume(1.0),
      m_pullMode(false),
      m_nextBuffer(0),
      m_bufferSize(0),
      m_notifyInterval(1000),
      m_periodSize(0),
      m_elapsedTime(0),
      m_processedBytes(0),
      m_availableBuffers(BUFFER_COUNT)
{
#ifndef ANDROID
      m_streamType = -1;
#else
      m_streamType = SL_ANDROID_STREAM_MEDIA;
      m_category = QLatin1String("media");
#endif // ANDROID
}

QOpenSLESAudioOutput::~QOpenSLESAudioOutput()
{
    destroyPlayer();
}

QAudio::Error QOpenSLESAudioOutput::error() const
{
    return m_error;
}

QAudio::State QOpenSLESAudioOutput::state() const
{
    return m_state;
}

void QOpenSLESAudioOutput::start(QIODevice *device)
{
    Q_ASSERT(device);
    destroyPlayer();

    m_pullMode = true;

    if (!preparePlayer())
        return;

    m_audioSource = device;
    setState(QAudio::ActiveState);
    setError(QAudio::NoError);

    // Attempt to fill buffers first.
    for (int i = 0; i != BUFFER_COUNT; ++i) {
        const int index = i * m_bufferSize;
        const qint64 readSize = m_audioSource->read(m_buffers + index, m_bufferSize);
        if (readSize && SL_RESULT_SUCCESS != (*m_bufferQueueItf)->Enqueue(m_bufferQueueItf,
                                                                          m_buffers + index,
                                                                          readSize)) {
            setError(QAudio::FatalError);
            destroyPlayer();
            return;
        }
        m_processedBytes += readSize;
    }

    // Change the state to playing.
    // We need to do this after filling the buffers or processedBytes might get corrupted.
    if (SL_RESULT_SUCCESS != (*m_playItf)->SetPlayState(m_playItf, SL_PLAYSTATE_PLAYING)) {
        setError(QAudio::FatalError);
        destroyPlayer();
    }
}

QIODevice *QOpenSLESAudioOutput::start()
{
    destroyPlayer();

    m_pullMode = false;

    if (!preparePlayer())
        return Q_NULLPTR;

    m_audioSource = new SLIODevicePrivate(this);
    m_audioSource->open(QIODevice::WriteOnly | QIODevice::Unbuffered);

    // Change the state to playing
    if (SL_RESULT_SUCCESS != (*m_playItf)->SetPlayState(m_playItf, SL_PLAYSTATE_PLAYING)) {
        setError(QAudio::FatalError);
        destroyPlayer();
    }

    setState(QAudio::IdleState);
    return m_audioSource;
}

void QOpenSLESAudioOutput::stop()
{
    if (m_state == QAudio::StoppedState)
        return;

    destroyPlayer();
    setError(QAudio::NoError);
}

int QOpenSLESAudioOutput::bytesFree() const
{
    if (m_state != QAudio::ActiveState && m_state != QAudio::IdleState)
        return 0;

    return m_availableBuffers.load() ? m_bufferSize : 0;
}

int QOpenSLESAudioOutput::periodSize() const
{
    return m_periodSize;
}

void QOpenSLESAudioOutput::setBufferSize(int value)
{
    if (m_state != QAudio::StoppedState)
        return;

    m_bufferSize = value;
}

int QOpenSLESAudioOutput::bufferSize() const
{
    return m_bufferSize;
}

void QOpenSLESAudioOutput::setNotifyInterval(int ms)
{
    m_notifyInterval = ms > 0 ? ms : 0;
}

int QOpenSLESAudioOutput::notifyInterval() const
{
    return m_notifyInterval;
}

qint64 QOpenSLESAudioOutput::processedUSecs() const
{
    if (m_state == QAudio::IdleState || m_state == QAudio::SuspendedState)
        return m_format.durationForBytes(m_processedBytes);

    SLmillisecond processMSec = 0;
    if (m_playItf)
        (*m_playItf)->GetPosition(m_playItf, &processMSec);

    return processMSec * 1000;
}

void QOpenSLESAudioOutput::resume()
{
    if (m_state != QAudio::SuspendedState)
        return;

    if (SL_RESULT_SUCCESS != (*m_playItf)->SetPlayState(m_playItf, SL_PLAYSTATE_PLAYING)) {
        setError(QAudio::FatalError);
        destroyPlayer();
        return;
    }

    setState(QAudio::ActiveState);
    setError(QAudio::NoError);
}

void QOpenSLESAudioOutput::setFormat(const QAudioFormat &format)
{
    m_format = format;
}

QAudioFormat QOpenSLESAudioOutput::format() const
{
    return m_format;
}

void QOpenSLESAudioOutput::suspend()
{
    if (m_state != QAudio::ActiveState && m_state != QAudio::IdleState)
        return;

    if (SL_RESULT_SUCCESS != (*m_playItf)->SetPlayState(m_playItf, SL_PLAYSTATE_PAUSED)) {
        setError(QAudio::FatalError);
        destroyPlayer();
        return;
    }

    setState(QAudio::SuspendedState);
    setError(QAudio::NoError);
}

qint64 QOpenSLESAudioOutput::elapsedUSecs() const
{
    if (m_state == QAudio::StoppedState)
        return 0;

    return m_clockStamp.elapsed() * 1000;
}

void QOpenSLESAudioOutput::reset()
{
    destroyPlayer();
}

void QOpenSLESAudioOutput::setVolume(qreal vol)
{
    m_volume = qBound(qreal(0.0), vol, qreal(1.0));
    const SLmillibel newVolume = adjustVolume(m_volume);
    if (m_volumeItf && SL_RESULT_SUCCESS != (*m_volumeItf)->SetVolumeLevel(m_volumeItf, newVolume))
        qWarning() << "Unable to change volume";
}

qreal QOpenSLESAudioOutput::volume() const
{
    return m_volume;
}

void QOpenSLESAudioOutput::setCategory(const QString &category)
{
#ifndef ANDROID
    Q_UNUSED(category);
#else
    if (m_categories.isEmpty()) {
        m_categories.insert(QLatin1String("voice"), SL_ANDROID_STREAM_VOICE);
        m_categories.insert(QLatin1String("system"), SL_ANDROID_STREAM_SYSTEM);
        m_categories.insert(QLatin1String("ring"), SL_ANDROID_STREAM_RING);
        m_categories.insert(QLatin1String("media"), SL_ANDROID_STREAM_MEDIA);
        m_categories.insert(QLatin1String("alarm"), SL_ANDROID_STREAM_ALARM);
        m_categories.insert(QLatin1String("notification"), SL_ANDROID_STREAM_NOTIFICATION);
    }

    const SLint32 streamType = m_categories.value(category, -1);
    if (streamType == -1) {
        qWarning() << "Unknown category" << category
                   << ", available categories are:" << m_categories.keys()
                   << ". Defaulting to category \"media\"";
        return;
    }

    m_streamType = streamType;
    m_category = category;
#endif // ANDROID
}

QString QOpenSLESAudioOutput::category() const
{
    return m_category;
}

void QOpenSLESAudioOutput::onEOSEvent()
{
    if (m_state != QAudio::ActiveState)
        return;

    SLBufferQueueState state;
    if (SL_RESULT_SUCCESS != (*m_bufferQueueItf)->GetState(m_bufferQueueItf, &state))
        return;

    if (state.count > 0)
        return;

    setState(QAudio::IdleState);
    setError(QAudio::UnderrunError);
}

void QOpenSLESAudioOutput::bufferAvailable(quint32 count, quint32 playIndex)
{
    Q_UNUSED(count);
    Q_UNUSED(playIndex);

    if (m_state == QAudio::StoppedState)
        return;

    if (!m_pullMode) {
        m_availableBuffers.fetchAndAddRelaxed(1);
        return;
    }

    const int index = m_nextBuffer * m_bufferSize;
    const qint64 readSize = m_audioSource->read(m_buffers + index, m_bufferSize);

    if (1 > readSize)
        return;

    if (SL_RESULT_SUCCESS != (*m_bufferQueueItf)->Enqueue(m_bufferQueueItf,
                                                          m_buffers + index,
                                                          readSize)) {
        setError(QAudio::FatalError);
        destroyPlayer();
        return;
    }

    m_processedBytes += readSize;
    m_nextBuffer = (m_nextBuffer + 1) % BUFFER_COUNT;
}

void QOpenSLESAudioOutput::playCallback(SLPlayItf player, void *ctx, SLuint32 event)
{
    Q_UNUSED(player);
    QOpenSLESAudioOutput *audioOutput = reinterpret_cast<QOpenSLESAudioOutput *>(ctx);
    if (event & SL_PLAYEVENT_HEADATEND)
        QMetaObject::invokeMethod(audioOutput, "onEOSEvent", Qt::QueuedConnection);
    if (event & SL_PLAYEVENT_HEADATNEWPOS)
        Q_EMIT audioOutput->notify();

}

void QOpenSLESAudioOutput::bufferQueueCallback(SLBufferQueueItf bufferQueue, void *ctx)
{
    SLBufferQueueState state;
    (*bufferQueue)->GetState(bufferQueue, &state);
    QOpenSLESAudioOutput *audioOutput = reinterpret_cast<QOpenSLESAudioOutput *>(ctx);
    audioOutput->bufferAvailable(state.count, state.playIndex);
}

bool QOpenSLESAudioOutput::preparePlayer()
{
    SLEngineItf engine = QOpenSLESEngine::instance()->slEngine();
    if (!engine) {
        qWarning() << "No engine";
        setError(QAudio::FatalError);
        return false;
    }

    SLDataLocator_BufferQueue bufferQueueLocator = { SL_DATALOCATOR_BUFFERQUEUE, BUFFER_COUNT };
    SLDataFormat_PCM pcmFormat = QOpenSLESEngine::audioFormatToSLFormatPCM(m_format);

    SLDataSource audioSrc = { &bufferQueueLocator, &pcmFormat };

    // OutputMix
    if (SL_RESULT_SUCCESS != (*engine)->CreateOutputMix(engine,
                                                        &m_outputMixObject,
                                                        0,
                                                        Q_NULLPTR,
                                                        Q_NULLPTR)) {
        qWarning() << "Unable to create output mix";
        setError(QAudio::FatalError);
        return false;
    }

    if (SL_RESULT_SUCCESS != (*m_outputMixObject)->Realize(m_outputMixObject, SL_BOOLEAN_FALSE)) {
        qWarning() << "Unable to initialize output mix";
        setError(QAudio::FatalError);
        return false;
    }

    SLDataLocator_OutputMix outputMixLocator = { SL_DATALOCATOR_OUTPUTMIX, m_outputMixObject };
    SLDataSink audioSink = { &outputMixLocator, Q_NULLPTR };

#ifndef ANDROID
    const int iids = 2;
    const SLInterfaceID ids[iids] = { SL_IID_BUFFERQUEUE, SL_IID_VOLUME };
    const SLboolean req[iids] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };
#else
    const int iids = 3;
    const SLInterfaceID ids[iids] = { SL_IID_BUFFERQUEUE,
                                      SL_IID_VOLUME,
                                      SL_IID_ANDROIDCONFIGURATION };
    const SLboolean req[iids] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };
#endif // ANDROID

    // AudioPlayer
    if (SL_RESULT_SUCCESS != (*engine)->CreateAudioPlayer(engine,
                                                          &m_playerObject,
                                                          &audioSrc,
                                                          &audioSink,
                                                          iids,
                                                          ids,
                                                          req)) {
        qWarning() << "Unable to create AudioPlayer";
        setError(QAudio::OpenError);
        return false;
    }

#ifdef ANDROID
    // Set profile/category
    SLAndroidConfigurationItf playerConfig;
    if (SL_RESULT_SUCCESS == (*m_playerObject)->GetInterface(m_playerObject,
                                                             SL_IID_ANDROIDCONFIGURATION,
                                                             &playerConfig)) {
        (*playerConfig)->SetConfiguration(playerConfig,
                                          SL_ANDROID_KEY_STREAM_TYPE,
                                          &m_streamType,
                                          sizeof(SLint32));
    }
#endif // ANDROID

    if (SL_RESULT_SUCCESS != (*m_playerObject)->Realize(m_playerObject, SL_BOOLEAN_FALSE)) {
        qWarning() << "Unable to initialize AudioPlayer";
        setError(QAudio::OpenError);
        return false;
    }

    // Buffer interface
    if (SL_RESULT_SUCCESS != (*m_playerObject)->GetInterface(m_playerObject,
                                                             SL_IID_BUFFERQUEUE,
                                                             &m_bufferQueueItf)) {
        setError(QAudio::FatalError);
        return false;
    }

    if (SL_RESULT_SUCCESS != (*m_bufferQueueItf)->RegisterCallback(m_bufferQueueItf,
                                                                   bufferQueueCallback,
                                                                   this)) {
        setError(QAudio::FatalError);
        return false;
    }

    // Play interface
    if (SL_RESULT_SUCCESS != (*m_playerObject)->GetInterface(m_playerObject,
                                                             SL_IID_PLAY,
                                                             &m_playItf)) {
        setError(QAudio::FatalError);
        return false;
    }

    if (SL_RESULT_SUCCESS != (*m_playItf)->RegisterCallback(m_playItf, playCallback, this)) {
        setError(QAudio::FatalError);
        return false;
    }

    SLuint32 mask = SL_PLAYEVENT_HEADATEND;
    if (m_notifyInterval && SL_RESULT_SUCCESS == (*m_playItf)->SetPositionUpdatePeriod(m_playItf,
                                                                                       m_notifyInterval)) {
        mask |= SL_PLAYEVENT_HEADATNEWPOS;
    }

    if (SL_RESULT_SUCCESS != (*m_playItf)->SetCallbackEventsMask(m_playItf, mask)) {
        setError(QAudio::FatalError);
        return false;
    }

    // Volume interface
    if (SL_RESULT_SUCCESS != (*m_playerObject)->GetInterface(m_playerObject,
                                                             SL_IID_VOLUME,
                                                             &m_volumeItf)) {
        setError(QAudio::FatalError);
        return false;
    }

    setVolume(m_volume);

    // Buffer size
    if (m_bufferSize <= 0) {
        m_bufferSize = m_format.bytesForDuration(DEFAULT_PERIOD_TIME_MS * 1000);
    } else {
        const int minimumBufSize = m_format.bytesForDuration(MINIMUM_PERIOD_TIME_MS * 1000);
        if (m_bufferSize < minimumBufSize)
            m_bufferSize = minimumBufSize;
    }

    m_periodSize = m_bufferSize;

    if (!m_buffers)
        m_buffers = new char[BUFFER_COUNT * m_bufferSize];

    m_clockStamp.restart();
    setError(QAudio::NoError);

    return true;
}

void QOpenSLESAudioOutput::destroyPlayer()
{
    setState(QAudio::StoppedState);

    // We need to change the state manually...
    if (m_playItf)
        (*m_playItf)->SetPlayState(m_playItf, SL_PLAYSTATE_STOPPED);

    if (m_bufferQueueItf && SL_RESULT_SUCCESS != (*m_bufferQueueItf)->Clear(m_bufferQueueItf))
        qWarning() << "Unable to clear buffer";

    if (m_playerObject) {
        (*m_playerObject)->Destroy(m_playerObject);
        m_playerObject = Q_NULLPTR;
    }

    if (m_outputMixObject) {
        (*m_outputMixObject)->Destroy(m_outputMixObject);
        m_outputMixObject = Q_NULLPTR;
    }

    if (!m_pullMode && m_audioSource) {
        m_audioSource->close();
        delete m_audioSource;
        m_audioSource = Q_NULLPTR;
    }

    delete [] m_buffers;
    m_buffers = Q_NULLPTR;
    m_processedBytes = 0;
    m_nextBuffer = 0;
    m_availableBuffers = BUFFER_COUNT;
    m_playItf = Q_NULLPTR;
    m_volumeItf = Q_NULLPTR;
    m_bufferQueueItf = Q_NULLPTR;
}

qint64 QOpenSLESAudioOutput::writeData(const char *data, qint64 len)
{
    if (!len || !m_availableBuffers.load())
        return 0;

    if (len > m_bufferSize)
        len = m_bufferSize;

    const int index = m_nextBuffer * m_bufferSize;
    ::memcpy(m_buffers + index, data, len);
    const SLuint32 res = (*m_bufferQueueItf)->Enqueue(m_bufferQueueItf,
                                                      m_buffers + index,
                                                      len);

    if (res == SL_RESULT_BUFFER_INSUFFICIENT)
        return 0;

    if (res != SL_RESULT_SUCCESS) {
        setError(QAudio::FatalError);
        destroyPlayer();
        return -1;
    }

    m_processedBytes += len;
    m_availableBuffers.fetchAndAddRelaxed(-1);
    setState(QAudio::ActiveState);
    setError(QAudio::NoError);
    m_nextBuffer = (m_nextBuffer + 1) % BUFFER_COUNT;

    return len;
}

inline void QOpenSLESAudioOutput::setState(QAudio::State state)
{
    if (m_state == state)
        return;

    m_state = state;
    Q_EMIT stateChanged(m_state);
}

inline void QOpenSLESAudioOutput::setError(QAudio::Error error)
{
    if (m_error == error)
        return;

    m_error = error;
    Q_EMIT errorChanged(m_error);
}

inline SLmillibel QOpenSLESAudioOutput::adjustVolume(qreal vol)
{
    if (qFuzzyIsNull(vol))
        return SL_MILLIBEL_MIN;

    if (qFuzzyCompare(vol, qreal(1.0)))
        return 0;

    return 20 * LOG10(vol) * 100; // I.e., 20 * LOG10(SL_MILLIBEL_MAX * vol / SL_MILLIBEL_MAX)
}

QT_END_NAMESPACE
