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

#ifndef QMEDIAPLAYLIST_H
#define QMEDIAPLAYLIST_H

#include <QtCore/qobject.h>

#include <QtMultimedia/qmediacontent.h>
#include <QtMultimedia/qmediaobject.h>
#include <QtMultimedia/qmediabindableinterface.h>
#include <QtMultimedia/qmediaenumdebug.h>


QT_BEGIN_NAMESPACE


class QMediaPlaylistProvider;

class QMediaPlaylistPrivate;
class Q_MULTIMEDIA_EXPORT QMediaPlaylist : public QObject, public QMediaBindableInterface
{
    Q_OBJECT
    Q_INTERFACES(QMediaBindableInterface)
    Q_PROPERTY(QMediaPlaylist::PlaybackMode playbackMode READ playbackMode WRITE setPlaybackMode NOTIFY playbackModeChanged)
    Q_PROPERTY(QMediaContent currentMedia READ currentMedia NOTIFY currentMediaChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_ENUMS(PlaybackMode Error)

public:
    enum PlaybackMode { CurrentItemOnce, CurrentItemInLoop, Sequential, Loop, Random };
    enum Error { NoError, FormatError, FormatNotSupportedError, NetworkError, AccessDeniedError };

    QMediaPlaylist(QObject *parent = 0);
    virtual ~QMediaPlaylist();

    QMediaObject *mediaObject() const;

    PlaybackMode playbackMode() const;
    void setPlaybackMode(PlaybackMode mode);

    int currentIndex() const;
    QMediaContent currentMedia() const;

    int nextIndex(int steps = 1) const;
    int previousIndex(int steps = 1) const;

    QMediaContent media(int index) const;

    int mediaCount() const;
    bool isEmpty() const;
    bool isReadOnly() const;

    bool addMedia(const QMediaContent &content);
    bool addMedia(const QList<QMediaContent> &items);
    bool insertMedia(int index, const QMediaContent &content);
    bool insertMedia(int index, const QList<QMediaContent> &items);
    bool removeMedia(int pos);
    bool removeMedia(int start, int end);
    bool clear();

    void load(const QNetworkRequest &request, const char *format = 0);
    void load(const QUrl &location, const char *format = 0);
    void load(QIODevice * device, const char *format = 0);

    bool save(const QUrl &location, const char *format = 0);
    bool save(QIODevice * device, const char *format);

    Error error() const;
    QString errorString() const;

public Q_SLOTS:
    void shuffle();

    void next();
    void previous();

    void setCurrentIndex(int index);

Q_SIGNALS:
    void currentIndexChanged(int index);
    void playbackModeChanged(QMediaPlaylist::PlaybackMode mode);
    void currentMediaChanged(const QMediaContent&);

    void mediaAboutToBeInserted(int start, int end);
    void mediaInserted(int start, int end);
    void mediaAboutToBeRemoved(int start, int end);
    void mediaRemoved(int start, int end);
    void mediaChanged(int start, int end);

    void loaded();
    void loadFailed();

protected:
    bool setMediaObject(QMediaObject *object);
    QMediaPlaylistPrivate *d_ptr;

private:
    Q_DECLARE_PRIVATE(QMediaPlaylist)
    Q_PRIVATE_SLOT(d_func(), void _q_loadFailed(QMediaPlaylist::Error, const QString &))
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QMediaPlaylist::PlaybackMode)
Q_DECLARE_METATYPE(QMediaPlaylist::Error)

Q_MEDIA_ENUM_DEBUG(QMediaPlaylist, PlaybackMode)
Q_MEDIA_ENUM_DEBUG(QMediaPlaylist, Error)

#endif  // QMEDIAPLAYLIST_H
