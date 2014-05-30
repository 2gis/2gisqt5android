/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#ifndef QSENSORGESTUREMANAGERPRIVATE_P_H
#define QSENSORGESTUREMANAGERPRIVATE_P_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QDebug>
#include <QSharedPointer>
#include <QPluginLoader>

#include "qsensorgesture.h"
#include "qsensorgesturerecognizer.h"

QT_BEGIN_NAMESPACE

class QFactoryLoader;

class QSensorGestureManagerPrivate : public QObject
{
    Q_OBJECT
public:
    explicit QSensorGestureManagerPrivate(QObject *parent = 0);
    ~QSensorGestureManagerPrivate();

    QMap<QString, QSensorGestureRecognizer *> registeredSensorGestures;

    QList <QObject *> plugins;

    QFactoryLoader *loader;
    void loadPlugins();
    bool loadRecognizer(const QString &id);

    QSensorGestureRecognizer *sensorGestureRecognizer(const QString &id);

    bool registerSensorGestureRecognizer(QSensorGestureRecognizer *recognizer);
    QStringList gestureIds();
    QStringList knownIds;
    void initPlugin(QObject *o);
#ifdef SIMULATOR_BUILD
    void recognizerStarted(const QSensorGestureRecognizer *);
    void recognizerStopped(const QSensorGestureRecognizer *);
#endif

    static QSensorGestureManagerPrivate * instance();
Q_SIGNALS:
        void newSensorGestureAvailable();

#ifdef SIMULATOR_BUILD
Q_SIGNALS:
    void newSensorGestures(QStringList);
    void removeSensorGestures(QStringList);

private slots:
    void sensorGestureDetected();

#endif
};

QT_END_NAMESPACE

#endif // QSENSORGESTUREMANAGERPRIVATE_P_H
