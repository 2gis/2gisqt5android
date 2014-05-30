/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlconfigurabledebugservice_p.h"
#include "qqmlconfigurabledebugservice_p_p.h"

QT_BEGIN_NAMESPACE

QQmlConfigurableDebugService::QQmlConfigurableDebugService(const QString &name, float version,
                                                           QObject *parent) :
    QQmlDebugService((*new QQmlConfigurableDebugServicePrivate), name, version, parent) { init(); }

QQmlConfigurableDebugService::QQmlConfigurableDebugService(QQmlDebugServicePrivate &dd,
                                                           const QString &name, float version,
                                                           QObject *parent) :
    QQmlDebugService(dd, name, version, parent) { init(); }

QMutex *QQmlConfigurableDebugService::configMutex()
{
    Q_D(QQmlConfigurableDebugService);
    return &d->configMutex;
}

void QQmlConfigurableDebugService::init()
{
    Q_D(QQmlConfigurableDebugService);
    QMutexLocker lock(&d->configMutex);
    // If we're not enabled or not blocking, don't wait for configuration
    d->waitingForConfiguration = (registerService() == Enabled && blockingMode());
}

void QQmlConfigurableDebugService::stopWaiting()
{
    Q_D(QQmlConfigurableDebugService);
    QMutexLocker lock(&d->configMutex);
    d->waitingForConfiguration = false;
    foreach (QQmlEngine *engine, d->waitingEngines)
        emit attachedToEngine(engine);
    d->waitingEngines.clear();
}

void QQmlConfigurableDebugService::stateChanged(QQmlDebugService::State newState)
{
    if (newState != Enabled)
        stopWaiting();
}

void QQmlConfigurableDebugService::engineAboutToBeAdded(QQmlEngine *engine)
{
    Q_D(QQmlConfigurableDebugService);
    QMutexLocker lock(&d->configMutex);
    if (d->waitingForConfiguration)
        d->waitingEngines.append(engine);
    else
        emit attachedToEngine(engine);
}

QT_END_NAMESPACE
