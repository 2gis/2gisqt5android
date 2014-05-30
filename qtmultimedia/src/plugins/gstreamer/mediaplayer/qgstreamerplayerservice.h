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

#ifndef QGSTREAMERPLAYERSERVICE_H
#define QGSTREAMERPLAYERSERVICE_H

#include <QtCore/qobject.h>
#include <QtCore/qiodevice.h>

#include <qmediaservice.h>

QT_BEGIN_NAMESPACE
class QMediaPlayerControl;
class QMediaPlaylist;
class QMediaPlaylistNavigator;

class QGstreamerMetaData;
class QGstreamerPlayerControl;
class QGstreamerPlayerSession;
class QGstreamerMetaDataProvider;
class QGstreamerStreamsControl;
class QGstreamerVideoRenderer;
class QGstreamerVideoWidgetControl;
class QGStreamerAvailabilityControl;

class QGstreamerPlayerService : public QMediaService
{
    Q_OBJECT
public:
    QGstreamerPlayerService(QObject *parent = 0);
    ~QGstreamerPlayerService();

    QMediaControl *requestControl(const char *name);
    void releaseControl(QMediaControl *control);

private:
    QGstreamerPlayerControl *m_control;
    QGstreamerPlayerSession *m_session;
    QGstreamerMetaDataProvider *m_metaData;
    QGstreamerStreamsControl *m_streamsControl;
    QGStreamerAvailabilityControl *m_availabilityControl;

    QMediaControl *m_videoOutput;
    QMediaControl *m_videoRenderer;
    QMediaControl *m_videoWindow;
#if defined(HAVE_WIDGETS)
    QMediaControl *m_videoWidget;
#endif

    void increaseVideoRef();
    void decreaseVideoRef();
    int m_videoReferenceCount;
};

QT_END_NAMESPACE

#endif
