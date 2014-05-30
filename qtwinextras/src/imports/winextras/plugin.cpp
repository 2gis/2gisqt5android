/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qquickdwmfeatures_p.h"
#include "qquicktaskbarbutton_p.h"
#include "qquickjumplist_p.h"
#include "qquickjumplistitem_p.h"
#include "qquickjumplistcategory_p.h"
#include "qquickthumbnailtoolbar_p.h"
#include "qquickthumbnailtoolbutton_p.h"
#include "qquickwin_p.h"

#include <QtQml/QtQml>

QT_BEGIN_NAMESPACE

class QWinExtrasQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0")

public:
    void registerTypes(const char *uri) Q_DECL_OVERRIDE
    {
        Q_ASSERT(uri == QLatin1String("QtWinExtras"));
        qmlRegisterUncreatableType<QQuickWin>(uri, 1, 0, "QtWin", "Cannot create an instance of the QtWin namespace.");
        qmlRegisterType<QQuickDwmFeatures>(uri, 1, 0, "DwmFeatures");
        qmlRegisterType<QQuickTaskbarButton>(uri, 1, 0, "TaskbarButton");
        qmlRegisterUncreatableType<QWinTaskbarProgress>(uri, 1, 0, "TaskbarProgress", "Cannot create TaskbarProgress - use TaskbarButton.progress instead.");
        qmlRegisterUncreatableType<QQuickTaskbarOverlay>(uri, 1, 0, "TaskbarOverlay", "Cannot create TaskbarOverlay - use TaskbarButton.overlay instead.");
        qmlRegisterType<QQuickJumpList>(uri, 1, 0, "JumpList");
        qmlRegisterType<QQuickJumpListItem>(uri, 1, 0, "JumpListItem");
        qmlRegisterType<QQuickJumpListCategory>(uri, 1, 0, "JumpListCategory");
        qmlRegisterType<QQuickThumbnailToolBar>(uri, 1, 0, "ThumbnailToolBar");
        qmlRegisterType<QQuickThumbnailToolButton>(uri, 1, 0, "ThumbnailToolButton");
    }
};

QT_END_NAMESPACE

#include "plugin.moc"
