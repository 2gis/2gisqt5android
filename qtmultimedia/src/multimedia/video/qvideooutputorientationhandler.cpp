/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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

#include "qvideooutputorientationhandler_p.h"

#include <QGuiApplication>
#include <QScreen>
#include <qpa/qplatformscreen.h>

QT_BEGIN_NAMESPACE

QVideoOutputOrientationHandler::QVideoOutputOrientationHandler(QObject *parent)
    : QObject(parent)
    , m_currentOrientation(0)
{
    QScreen *screen = QGuiApplication::primaryScreen();

    // we want to be informed about all orientation changes
    screen->setOrientationUpdateMask(Qt::PortraitOrientation|Qt::LandscapeOrientation
                                     |Qt::InvertedPortraitOrientation|Qt::InvertedLandscapeOrientation);

    connect(screen, SIGNAL(orientationChanged(Qt::ScreenOrientation)),
            this, SLOT(screenOrientationChanged(Qt::ScreenOrientation)));

    screenOrientationChanged(screen->orientation());
}

int QVideoOutputOrientationHandler::currentOrientation() const
{
    return m_currentOrientation;
}

void QVideoOutputOrientationHandler::screenOrientationChanged(Qt::ScreenOrientation orientation)
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    const QPlatformScreen *platformScreen = screen->handle();

    const int angle = (360 - screen->angleBetween(platformScreen->nativeOrientation(), orientation)) % 360;

    if (angle == m_currentOrientation)
        return;

    m_currentOrientation = angle;
    emit orientationChanged(m_currentOrientation);
}

QT_END_NAMESPACE
