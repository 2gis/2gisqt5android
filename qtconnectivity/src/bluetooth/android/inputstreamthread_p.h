/****************************************************************************
**
** Copyright (C) 2013 Lauri Laanmets (Proekspert AS) <lauri.laanmets@eesti.ee>
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#ifndef INPUTSTREAMTHREAD_H
#define INPUTSTREAMTHREAD_H

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtAndroidExtras/QAndroidJniObject>
#include <jni.h>

QT_BEGIN_NAMESPACE

class QBluetoothSocketPrivate;

class InputStreamThread : public QObject
{
    Q_OBJECT
public:
    explicit InputStreamThread(QBluetoothSocketPrivate *socket_p);

    bool bytesAvailable() const;
    bool run();

    qint64 readData(char *data, qint64 maxSize);
    void javaThreadErrorOccurred(int errorCode);
    void javaReadyRead(jbyteArray buffer, int bufferLength);

    void prepareForClosure();

signals:
    void dataAvailable();
    void error(int errorCode);

private:
    QBluetoothSocketPrivate *m_socket_p;
    QAndroidJniObject javaInputStreamThread;
    mutable QMutex m_mutex;
    bool expectClosure;
};

QT_END_NAMESPACE

#endif // INPUTSTREAMTHREAD_H
