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

#include "qandroidmediaplayercontrol.h"
#include "androidmediaplayer.h"
#include "qandroidvideooutput.h"

QT_BEGIN_NAMESPACE

QAndroidMediaPlayerControl::QAndroidMediaPlayerControl(QObject *parent)
    : QMediaPlayerControl(parent),
      mMediaPlayer(new AndroidMediaPlayer),
      mCurrentState(QMediaPlayer::StoppedState),
      mCurrentMediaStatus(QMediaPlayer::NoMedia),
      mMediaStream(0),
      mVideoOutput(0),
      mSeekable(true),
      mBufferPercent(-1),
      mBufferFilled(false),
      mAudioAvailable(false),
      mVideoAvailable(false),
      mBuffering(false),
      mState(AndroidMediaPlayer::Uninitialized),
      mPendingState(-1),
      mPendingPosition(-1),
      mPendingSetMedia(false),
      mPendingVolume(-1),
      mPendingMute(-1)
{
    connect(mMediaPlayer,SIGNAL(bufferingChanged(qint32)),
            this,SLOT(onBufferingChanged(qint32)));
    connect(mMediaPlayer,SIGNAL(info(qint32,qint32)),
            this,SLOT(onInfo(qint32,qint32)));
    connect(mMediaPlayer,SIGNAL(error(qint32,qint32)),
            this,SLOT(onError(qint32,qint32)));
    connect(mMediaPlayer,SIGNAL(stateChanged(qint32)),
            this,SLOT(onStateChanged(qint32)));
    connect(mMediaPlayer,SIGNAL(videoSizeChanged(qint32,qint32)),
            this,SLOT(onVideoSizeChanged(qint32,qint32)));
    connect(mMediaPlayer,SIGNAL(progressChanged(qint64)),
            this,SIGNAL(positionChanged(qint64)));
    connect(mMediaPlayer,SIGNAL(durationChanged(qint64)),
            this,SIGNAL(durationChanged(qint64)));
}

QAndroidMediaPlayerControl::~QAndroidMediaPlayerControl()
{
    mMediaPlayer->release();
    delete mMediaPlayer;
}

QMediaPlayer::State QAndroidMediaPlayerControl::state() const
{
    return mCurrentState;
}

QMediaPlayer::MediaStatus QAndroidMediaPlayerControl::mediaStatus() const
{
    return mCurrentMediaStatus;
}

qint64 QAndroidMediaPlayerControl::duration() const
{
    if ((mState & (AndroidMediaPlayer::Prepared
                   | AndroidMediaPlayer::Started
                   | AndroidMediaPlayer::Paused
                   | AndroidMediaPlayer::Stopped
                   | AndroidMediaPlayer::PlaybackCompleted)) == 0) {
        return 0;
    }

    return mMediaPlayer->getDuration();
}

qint64 QAndroidMediaPlayerControl::position() const
{
    if (mCurrentMediaStatus == QMediaPlayer::EndOfMedia)
        return duration();

    if ((mState & (AndroidMediaPlayer::Idle
                   | AndroidMediaPlayer::Initialized
                   | AndroidMediaPlayer::Prepared
                   | AndroidMediaPlayer::Started
                   | AndroidMediaPlayer::Paused
                   | AndroidMediaPlayer::Stopped
                   | AndroidMediaPlayer::PlaybackCompleted)) == 0) {
         return (mPendingPosition == -1) ? 0 : mPendingPosition;
    }

    return (mCurrentState == QMediaPlayer::StoppedState) ? 0 : mMediaPlayer->getCurrentPosition();
}

void QAndroidMediaPlayerControl::setPosition(qint64 position)
{
    if (!mSeekable)
        return;

    const int seekPosition = (position > INT_MAX) ? INT_MAX : position;

    if ((mState & (AndroidMediaPlayer::Prepared
                   | AndroidMediaPlayer::Started
                   | AndroidMediaPlayer::Paused
                   | AndroidMediaPlayer::PlaybackCompleted)) == 0) {
        if (mPendingPosition != seekPosition) {
            mPendingPosition = seekPosition;
            Q_EMIT positionChanged(seekPosition);
        }
        return;
    }

    if (mCurrentMediaStatus == QMediaPlayer::EndOfMedia)
        setMediaStatus(QMediaPlayer::LoadedMedia);

    mMediaPlayer->seekTo(seekPosition);

    if (mPendingPosition != -1) {
        mPendingPosition = -1;
    }

    Q_EMIT positionChanged(seekPosition);
}

int QAndroidMediaPlayerControl::volume() const
{
    return (mPendingVolume == -1) ? mMediaPlayer->volume() : mPendingVolume;
}

void QAndroidMediaPlayerControl::setVolume(int volume)
{
    if ((mState & (AndroidMediaPlayer::Idle
                   | AndroidMediaPlayer::Initialized
                   | AndroidMediaPlayer::Stopped
                   | AndroidMediaPlayer::Prepared
                   | AndroidMediaPlayer::Started
                   | AndroidMediaPlayer::Paused
                   | AndroidMediaPlayer::PlaybackCompleted)) == 0) {
        if (mPendingVolume != volume) {
            mPendingVolume = volume;
            Q_EMIT volumeChanged(volume);
        }
        return;
    }

    mMediaPlayer->setVolume(volume);

    if (mPendingVolume != -1) {
        mPendingVolume = -1;
        return;
    }

    Q_EMIT volumeChanged(volume);
}

bool QAndroidMediaPlayerControl::isMuted() const
{
    return (mPendingMute == -1) ? mMediaPlayer->isMuted() : (mPendingMute == 1);
}

void QAndroidMediaPlayerControl::setMuted(bool muted)
{
    if ((mState & (AndroidMediaPlayer::Idle
                   | AndroidMediaPlayer::Initialized
                   | AndroidMediaPlayer::Stopped
                   | AndroidMediaPlayer::Prepared
                   | AndroidMediaPlayer::Started
                   | AndroidMediaPlayer::Paused
                   | AndroidMediaPlayer::PlaybackCompleted)) == 0) {
        if (mPendingMute != muted) {
            mPendingMute = muted;
            Q_EMIT mutedChanged(muted);
        }
        return;
    }

    mMediaPlayer->setMuted(muted);

    if (mPendingMute != -1) {
        mPendingMute = -1;
        return;
    }

    Q_EMIT mutedChanged(muted);
}

int QAndroidMediaPlayerControl::bufferStatus() const
{
    return mBufferFilled ? 100 : 0;
}

bool QAndroidMediaPlayerControl::isAudioAvailable() const
{
    return mAudioAvailable;
}

bool QAndroidMediaPlayerControl::isVideoAvailable() const
{
    return mVideoAvailable;
}

bool QAndroidMediaPlayerControl::isSeekable() const
{
    return mSeekable;
}

QMediaTimeRange QAndroidMediaPlayerControl::availablePlaybackRanges() const
{
    return mAvailablePlaybackRange;
}

void QAndroidMediaPlayerControl::updateAvailablePlaybackRanges()
{
    if (mBuffering) {
        const qint64 pos = position();
        const qint64 end = (duration() / 100) * mBufferPercent;
        mAvailablePlaybackRange.addInterval(pos, end);
    } else if (mSeekable) {
        mAvailablePlaybackRange = QMediaTimeRange(0, duration());
    } else {
        mAvailablePlaybackRange = QMediaTimeRange();
    }

    Q_EMIT availablePlaybackRangesChanged(mAvailablePlaybackRange);
}

qreal QAndroidMediaPlayerControl::playbackRate() const
{
    return 1.0f;
}

void QAndroidMediaPlayerControl::setPlaybackRate(qreal rate)
{
    Q_UNUSED(rate);
}

QMediaContent QAndroidMediaPlayerControl::media() const
{
    return mMediaContent;
}

const QIODevice *QAndroidMediaPlayerControl::mediaStream() const
{
    return mMediaStream;
}

void QAndroidMediaPlayerControl::setMedia(const QMediaContent &mediaContent,
                                          QIODevice *stream)
{
    const bool reloading = (mMediaContent == mediaContent);

    if (!reloading) {
        mMediaContent = mediaContent;
        mMediaStream = stream;
    }

    // Release the mediaplayer if it's not in in Idle or Uninitialized state
    if ((mState & (AndroidMediaPlayer::Idle | AndroidMediaPlayer::Uninitialized)) == 0)
        mMediaPlayer->release();

    if (mediaContent.isNull()) {
        setMediaStatus(QMediaPlayer::NoMedia);
        return;
    }

    if (mVideoOutput && !mVideoOutput->isReady()) {
        // if a video output is set but the video texture is not ready, delay loading the media
        // since it can cause problems on some hardware
        mPendingSetMedia = true;
        return;
    }

    const QUrl url = mediaContent.canonicalUrl();
    QString mediaPath;
    if (url.scheme() == QLatin1String("qrc")) {
        const QString path = url.toString().mid(3);
        mTempFile.reset(QTemporaryFile::createNativeFile(path));
        if (!mTempFile.isNull())
            mediaPath = QStringLiteral("file://") + mTempFile->fileName();
    } else {
        mediaPath = url.toString();
    }

    if (mVideoSize.isValid() && mVideoOutput)
        mVideoOutput->setVideoSize(mVideoSize);

    if ((mMediaPlayer->display() == 0) && mVideoOutput)
        mMediaPlayer->setDisplay(mVideoOutput->surfaceTexture());
    mMediaPlayer->setDataSource(mediaPath);
    mMediaPlayer->prepareAsync();

    if (!reloading)
        Q_EMIT mediaChanged(mMediaContent);

    resetBufferingProgress();
}

void QAndroidMediaPlayerControl::setVideoOutput(QObject *videoOutput)
{
    if (mVideoOutput) {
        mMediaPlayer->setDisplay(0);
        mVideoOutput->stop();
        mVideoOutput->reset();
    }

    mVideoOutput = qobject_cast<QAndroidVideoOutput *>(videoOutput);

    if (!mVideoOutput)
        return;

    if (mVideoOutput->isReady())
        mMediaPlayer->setDisplay(mVideoOutput->surfaceTexture());

    connect(videoOutput, SIGNAL(readyChanged(bool)), this, SLOT(onVideoOutputReady(bool)));
}

void QAndroidMediaPlayerControl::play()
{
    // We need to prepare the mediaplayer again.
    if ((mState & AndroidMediaPlayer::Stopped) && !mMediaContent.isNull()) {
        setMedia(mMediaContent, mMediaStream);
    }

    setState(QMediaPlayer::PlayingState);

    if ((mState & (AndroidMediaPlayer::Prepared
                   | AndroidMediaPlayer::Started
                   | AndroidMediaPlayer::Paused
                   | AndroidMediaPlayer::PlaybackCompleted)) == 0) {
        mPendingState = QMediaPlayer::PlayingState;
        return;
    }

    mMediaPlayer->play();
}

void QAndroidMediaPlayerControl::pause()
{
    setState(QMediaPlayer::PausedState);

    if ((mState & (AndroidMediaPlayer::Started
                   | AndroidMediaPlayer::Paused
                   | AndroidMediaPlayer::PlaybackCompleted)) == 0) {
        mPendingState = QMediaPlayer::PausedState;
        return;
    }

    mMediaPlayer->pause();
}

void QAndroidMediaPlayerControl::stop()
{
    setState(QMediaPlayer::StoppedState);

    if ((mState & (AndroidMediaPlayer::Prepared
                   | AndroidMediaPlayer::Started
                   | AndroidMediaPlayer::Stopped
                   | AndroidMediaPlayer::Paused
                   | AndroidMediaPlayer::PlaybackCompleted)) == 0) {
        if ((mState & (AndroidMediaPlayer::Idle | AndroidMediaPlayer::Uninitialized | AndroidMediaPlayer::Error)) == 0)
            mPendingState = QMediaPlayer::StoppedState;
        return;
    }

    mMediaPlayer->stop();
}

void QAndroidMediaPlayerControl::onInfo(qint32 what, qint32 extra)
{
    Q_UNUSED(extra);
    switch (what) {
    case AndroidMediaPlayer::MEDIA_INFO_UNKNOWN:
        break;
    case AndroidMediaPlayer::MEDIA_INFO_VIDEO_TRACK_LAGGING:
        // IGNORE
        break;
    case AndroidMediaPlayer::MEDIA_INFO_VIDEO_RENDERING_START:
        break;
    case AndroidMediaPlayer::MEDIA_INFO_BUFFERING_START:
        mPendingState = mCurrentState;
        setState(QMediaPlayer::PausedState);
        setMediaStatus(QMediaPlayer::StalledMedia);
        break;
    case AndroidMediaPlayer::MEDIA_INFO_BUFFERING_END:
        if (mCurrentState != QMediaPlayer::StoppedState)
            flushPendingStates();
        break;
    case AndroidMediaPlayer::MEDIA_INFO_BAD_INTERLEAVING:
        break;
    case AndroidMediaPlayer::MEDIA_INFO_NOT_SEEKABLE:
        setSeekable(false);
        break;
    case AndroidMediaPlayer::MEDIA_INFO_METADATA_UPDATE:
        Q_EMIT metaDataUpdated();
        break;
    }
}

void QAndroidMediaPlayerControl::onError(qint32 what, qint32 extra)
{
    QString errorString;
    QMediaPlayer::Error error = QMediaPlayer::ResourceError;

    switch (what) {
    case AndroidMediaPlayer::MEDIA_ERROR_UNKNOWN:
        errorString = QLatin1String("Error:");
        break;
    case AndroidMediaPlayer::MEDIA_ERROR_SERVER_DIED:
        errorString = QLatin1String("Error: Server died");
        error = QMediaPlayer::ServiceMissingError;
        break;
    case AndroidMediaPlayer::MEDIA_ERROR_INVALID_STATE:
        errorString = QLatin1String("Error: Invalid state");
        error = QMediaPlayer::ServiceMissingError;
        break;
    }

    switch (extra) {
    case AndroidMediaPlayer::MEDIA_ERROR_IO: // Network OR file error
        errorString += QLatin1String(" (I/O operation failed)");
        error = QMediaPlayer::NetworkError;
        setMediaStatus(QMediaPlayer::InvalidMedia);
        break;
    case AndroidMediaPlayer::MEDIA_ERROR_MALFORMED:
        errorString += QLatin1String(" (Malformed bitstream)");
        error = QMediaPlayer::FormatError;
        setMediaStatus(QMediaPlayer::InvalidMedia);
        break;
    case AndroidMediaPlayer::MEDIA_ERROR_UNSUPPORTED:
        errorString += QLatin1String(" (Unsupported media)");
        error = QMediaPlayer::FormatError;
        setMediaStatus(QMediaPlayer::InvalidMedia);
        break;
    case AndroidMediaPlayer::MEDIA_ERROR_TIMED_OUT:
        errorString += QLatin1String(" (Timed out)");
        break;
    case AndroidMediaPlayer::MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
        errorString += QLatin1String(" (Unable to start progressive playback')");
        error = QMediaPlayer::FormatError;
        setMediaStatus(QMediaPlayer::InvalidMedia);
        break;
    case AndroidMediaPlayer::MEDIA_ERROR_BAD_THINGS_ARE_GOING_TO_HAPPEN:
        errorString += QLatin1String(" (Unknown error/Insufficient resources)");
        error = QMediaPlayer::ServiceMissingError;
        break;
    }

    Q_EMIT QMediaPlayerControl::error(error, errorString);
}

void QAndroidMediaPlayerControl::onBufferingChanged(qint32 percent)
{
    mBuffering = percent != 100;
    mBufferPercent = percent;

    updateAvailablePlaybackRanges();

    if (mCurrentState != QMediaPlayer::StoppedState)
        setMediaStatus(mBuffering ? QMediaPlayer::BufferingMedia : QMediaPlayer::BufferedMedia);
}

void QAndroidMediaPlayerControl::onVideoSizeChanged(qint32 width, qint32 height)
{
    QSize newSize(width, height);

    if (width == 0 || height == 0 || newSize == mVideoSize)
        return;

    setVideoAvailable(true);
    mVideoSize = newSize;

    if (mVideoOutput)
        mVideoOutput->setVideoSize(mVideoSize);
}

void QAndroidMediaPlayerControl::onStateChanged(qint32 state)
{
    // If reloading, don't report state changes unless the new state is Prepared or Error.
    if ((mState & AndroidMediaPlayer::Stopped)
        && (state & (AndroidMediaPlayer::Prepared | AndroidMediaPlayer::Error | AndroidMediaPlayer::Uninitialized)) == 0) {
        return;
    }

    mState = state;
    switch (mState) {
    case AndroidMediaPlayer::Idle:
        break;
    case AndroidMediaPlayer::Initialized:
        break;
    case AndroidMediaPlayer::Preparing:
        setMediaStatus(QMediaPlayer::LoadingMedia);
        break;
    case AndroidMediaPlayer::Prepared:
        setMediaStatus(QMediaPlayer::LoadedMedia);
        if (mBuffering) {
            setMediaStatus(mBufferPercent == 100 ? QMediaPlayer::BufferedMedia
                                                 : QMediaPlayer::BufferingMedia);
        } else {
            onBufferingChanged(100);
        }
        setAudioAvailable(true);
        flushPendingStates();
        break;
    case AndroidMediaPlayer::Started:
        setState(QMediaPlayer::PlayingState);
        if (mBuffering) {
            setMediaStatus(mBufferPercent == 100 ? QMediaPlayer::BufferedMedia
                                                 : QMediaPlayer::BufferingMedia);
        } else {
            setMediaStatus(QMediaPlayer::BufferedMedia);
        }
        break;
    case AndroidMediaPlayer::Paused:
        setState(QMediaPlayer::PausedState);
        break;
    case AndroidMediaPlayer::Error:
        setState(QMediaPlayer::StoppedState);
        setMediaStatus(QMediaPlayer::UnknownMediaStatus);
        mMediaPlayer->release();
        break;
    case AndroidMediaPlayer::Stopped:
        setState(QMediaPlayer::StoppedState);
        setMediaStatus(QMediaPlayer::LoadedMedia);
        setPosition(0);
        break;
    case AndroidMediaPlayer::PlaybackCompleted:
        setState(QMediaPlayer::StoppedState);
        setPosition(0);
        setMediaStatus(QMediaPlayer::EndOfMedia);
        break;
    case AndroidMediaPlayer::Uninitialized:
        // reset some properties
        resetBufferingProgress();
        mPendingPosition = -1;
        mPendingSetMedia = false;
        mPendingState = -1;

        setAudioAvailable(false);
        setVideoAvailable(false);
        setSeekable(true);
        break;
    default:
        break;
    }

    if ((mState & (AndroidMediaPlayer::Stopped | AndroidMediaPlayer::Uninitialized)) != 0) {
        mMediaPlayer->setDisplay(0);
        if (mVideoOutput) {
            mVideoOutput->stop();
            mVideoOutput->reset();
        }
    }
}

void QAndroidMediaPlayerControl::onVideoOutputReady(bool ready)
{
    if ((mMediaPlayer->display() == 0) && mVideoOutput && ready)
        mMediaPlayer->setDisplay(mVideoOutput->surfaceTexture());

    flushPendingStates();
}

void QAndroidMediaPlayerControl::setState(QMediaPlayer::State state)
{
    if (mCurrentState == state)
        return;

    if (mCurrentState == QMediaPlayer::StoppedState && state == QMediaPlayer::PausedState)
        return;

    mCurrentState = state;
    Q_EMIT stateChanged(mCurrentState);
}

void QAndroidMediaPlayerControl::setMediaStatus(QMediaPlayer::MediaStatus status)
{
    if (mCurrentMediaStatus == status)
        return;

    if (status == QMediaPlayer::NoMedia || status == QMediaPlayer::InvalidMedia)
        Q_EMIT durationChanged(0);

    if (status == QMediaPlayer::EndOfMedia)
        Q_EMIT durationChanged(duration());

    mCurrentMediaStatus = status;
    Q_EMIT mediaStatusChanged(mCurrentMediaStatus);

    updateBufferStatus();
}

void QAndroidMediaPlayerControl::setSeekable(bool seekable)
{
    if (mSeekable == seekable)
        return;

    mSeekable = seekable;
    Q_EMIT seekableChanged(mSeekable);
}

void QAndroidMediaPlayerControl::setAudioAvailable(bool available)
{
    if (mAudioAvailable == available)
        return;

    mAudioAvailable = available;
    Q_EMIT audioAvailableChanged(mAudioAvailable);
}

void QAndroidMediaPlayerControl::setVideoAvailable(bool available)
{
    if (mVideoAvailable == available)
        return;

    if (!available)
        mVideoSize = QSize();

    mVideoAvailable = available;
    Q_EMIT videoAvailableChanged(mVideoAvailable);
}

void QAndroidMediaPlayerControl::resetBufferingProgress()
{
    mBuffering = false;
    mBufferPercent = 0;
    mAvailablePlaybackRange = QMediaTimeRange();
}

void QAndroidMediaPlayerControl::flushPendingStates()
{
    if (mPendingSetMedia) {
        mPendingSetMedia = false;
        setMedia(mMediaContent, 0);
        return;
    }

    const int newState = mPendingState;
    mPendingState = -1;

    if (mPendingPosition != -1)
        setPosition(mPendingPosition);
    if (mPendingVolume != -1)
        setVolume(mPendingVolume);
    if (mPendingMute != -1)
        setMuted((mPendingMute == 1));

    switch (newState) {
    case QMediaPlayer::PlayingState:
        play();
        break;
    case QMediaPlayer::PausedState:
        pause();
        break;
    case QMediaPlayer::StoppedState:
        stop();
        break;
    default:
        break;
    }
}

void QAndroidMediaPlayerControl::updateBufferStatus()
{
    bool bufferFilled = (mCurrentMediaStatus == QMediaPlayer::BufferedMedia
                         || mCurrentMediaStatus == QMediaPlayer::BufferingMedia);

    if (mBufferFilled != bufferFilled) {
        mBufferFilled = bufferFilled;
        Q_EMIT bufferStatusChanged(bufferStatus());
    }
}

QT_END_NAMESPACE
