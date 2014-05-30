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

#ifndef QMEDIAPLAYERCONTROL_H
#define QMEDIAPLAYERCONTROL_H

#include <QtMultimedia/qmediacontrol.h>
#include <QtMultimedia/qmediaplayer.h>
#include <QtMultimedia/qmediatimerange.h>

#include <QtCore/qpair.h>

QT_BEGIN_NAMESPACE


class QMediaPlaylist;

class Q_MULTIMEDIA_EXPORT QMediaPlayerControl : public QMediaControl
{
    Q_OBJECT

public:
    ~QMediaPlayerControl();

    virtual QMediaPlayer::State state() const = 0;

    virtual QMediaPlayer::MediaStatus mediaStatus() const = 0;

    virtual qint64 duration() const = 0;

    virtual qint64 position() const = 0;
    virtual void setPosition(qint64 position) = 0;

    virtual int volume() const = 0;
    virtual void setVolume(int volume) = 0;

    virtual bool isMuted() const = 0;
    virtual void setMuted(bool muted) = 0;

    virtual int bufferStatus() const = 0;

    virtual bool isAudioAvailable() const = 0;
    virtual bool isVideoAvailable() const = 0;

    virtual bool isSeekable() const = 0;

    virtual QMediaTimeRange availablePlaybackRanges() const = 0;

    virtual qreal playbackRate() const = 0;
    virtual void setPlaybackRate(qreal rate) = 0;

    virtual QMediaContent media() const = 0;
    virtual const QIODevice *mediaStream() const = 0;
    virtual void setMedia(const QMediaContent &media, QIODevice *stream) = 0;

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

Q_SIGNALS:
    void mediaChanged(const QMediaContent& content);
    void durationChanged(qint64 duration);
    void positionChanged(qint64 position);
    void stateChanged(QMediaPlayer::State newState);
    void mediaStatusChanged(QMediaPlayer::MediaStatus status);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void audioAvailableChanged(bool audioAvailable);
    void videoAvailableChanged(bool videoAvailable);
    void bufferStatusChanged(int percentFilled);
    void seekableChanged(bool);
    void availablePlaybackRangesChanged(const QMediaTimeRange&);
    void playbackRateChanged(qreal rate);
    void error(int error, const QString &errorString);

protected:
    QMediaPlayerControl(QObject* parent = 0);
};

#define QMediaPlayerControl_iid "org.qt-project.qt.mediaplayercontrol/5.0"
Q_MEDIA_DECLARE_CONTROL(QMediaPlayerControl, QMediaPlayerControl_iid)

QT_END_NAMESPACE


#endif  // QMEDIAPLAYERCONTROL_H

