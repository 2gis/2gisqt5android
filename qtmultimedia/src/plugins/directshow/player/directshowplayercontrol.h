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

#ifndef DIRECTSHOWPLAYERCONTROL_H
#define DIRECTSHOWPLAYERCONTROL_H

#include "qmediacontent.h"
#include "qmediaplayercontrol.h"

#include <QtCore/qcoreevent.h>

#include "directshowplayerservice.h"

QT_USE_NAMESPACE

class DirectShowPlayerControl : public QMediaPlayerControl
{
    Q_OBJECT
public:
    DirectShowPlayerControl(DirectShowPlayerService *service, QObject *parent = 0);
    ~DirectShowPlayerControl();

    QMediaPlayer::State state() const;

    QMediaPlayer::MediaStatus mediaStatus() const;

    qint64 duration() const;

    qint64 position() const;
    void setPosition(qint64 position);

    int volume() const;
    void setVolume(int volume);

    bool isMuted() const;
    void setMuted(bool muted);

    int bufferStatus() const;

    bool isAudioAvailable() const;
    bool isVideoAvailable() const;

    bool isSeekable() const;

    QMediaTimeRange availablePlaybackRanges() const;

    qreal playbackRate() const;
    void setPlaybackRate(qreal rate);

    QMediaContent media() const;
    const QIODevice *mediaStream() const;
    void setMedia(const QMediaContent &media, QIODevice *stream);

    void play();
    void pause();
    void stop();

    void updateState(QMediaPlayer::State state);
    void updateStatus(QMediaPlayer::MediaStatus status);
    void updateMediaInfo(qint64 duration, int streamTypes, bool seekable);
    void updatePlaybackRate(qreal rate);
    void updateAudioOutput(IBaseFilter *filter);
    void updateError(QMediaPlayer::Error error, const QString &errorString);
    void updatePosition(qint64 position);

protected:
    void customEvent(QEvent *event);

private:
    enum Properties
    {
        StateProperty        = 0x01,
        StatusProperty       = 0x02,
        StreamTypesProperty  = 0x04,
        DurationProperty     = 0x08,
        PlaybackRateProperty = 0x10,
        SeekableProperty     = 0x20,
        ErrorProperty        = 0x40,
        PositionProperty     = 0x80
    };

    enum Event
    {
        PropertiesChanged = QEvent::User
    };

    void scheduleUpdate(int properties);
    void emitPropertyChanges();

    DirectShowPlayerService *m_service;
    IBasicAudio *m_audio;
    QIODevice *m_stream;
    int m_updateProperties;
    QMediaPlayer::State m_state;
    QMediaPlayer::MediaStatus m_status;
    QMediaPlayer::Error m_error;
    int m_streamTypes;
    int m_muteVolume;
    qint64 m_position;
    qint64 m_pendingPosition;
    qint64 m_duration;
    qreal m_playbackRate;
    bool m_seekable;
    QMediaContent m_media;
    QString m_errorString;

};

#endif
