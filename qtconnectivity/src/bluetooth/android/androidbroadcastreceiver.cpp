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

#include <android/log.h>
#include "android/androidbroadcastreceiver_p.h"
#include <QtCore/QLoggingCategory>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtGui/QGuiApplication>
#include <QtAndroidExtras/QAndroidJniEnvironment>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)


AndroidBroadcastReceiver::AndroidBroadcastReceiver(QObject* parent)
    : QObject(parent), valid(false)
{
    //get QtActivity
    activityObject = QAndroidJniObject(QtAndroidPrivate::activity());

    broadcastReceiverObject = QAndroidJniObject("org/qtproject/qt5/android/bluetooth/QtBluetoothBroadcastReceiver");
    if (!broadcastReceiverObject.isValid())
        return;
    broadcastReceiverObject.setField<jlong>("qtObject", reinterpret_cast<long>(this));

    intentFilterObject = QAndroidJniObject("android/content/IntentFilter");
    if (!intentFilterObject.isValid())
        return;

    valid = true;
}

AndroidBroadcastReceiver::~AndroidBroadcastReceiver()
{
}

bool AndroidBroadcastReceiver::isValid() const
{
    return valid;
}

void AndroidBroadcastReceiver::unregisterReceiver()
{
    if (!valid)
        return;

    broadcastReceiverObject.callMethod<void>("unregisterReceiver");
}

void AndroidBroadcastReceiver::addAction(const QAndroidJniObject &action)
{
    if (!valid || !action.isValid())
        return;

    intentFilterObject.callMethod<void>("addAction", "(Ljava/lang/String;)V", action.object<jstring>());

    activityObject.callObjectMethod(
                "registerReceiver",
                "(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;",
                broadcastReceiverObject.object<jobject>(),
                intentFilterObject.object<jobject>());
}

QT_END_NAMESPACE
