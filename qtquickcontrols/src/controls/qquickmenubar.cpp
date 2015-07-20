/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickmenubar_p.h"

#include <private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/qpa/qplatformmenu.h>
#include <qquickwindow.h>

QT_BEGIN_NAMESPACE


/*!
  \class QQuickMenuBar
  \internal
 */

/*!
  \qmltype MenuBarPrivate
  \instantiates QQuickMenuBar
  \internal
  \inqmlmodule QtQuick.Controls
 */

/*!
    \qmlproperty list<Menu> MenuBar::menus
    \default

    The list of menus in the menubar.

    \sa Menu
*/

QQuickMenuBar::QQuickMenuBar(QObject *parent)
    : QObject(parent), m_platformMenuBar(0), m_contentItem(0), m_parentWindow(0)
{
}

QQuickMenuBar::~QQuickMenuBar()
{
}

QQmlListProperty<QQuickMenu> QQuickMenuBar::menus()
{
    return QQmlListProperty<QQuickMenu>(this, 0, &QQuickMenuBar::append_menu, &QQuickMenuBar::count_menu, &QQuickMenuBar::at_menu, 0);
}

bool QQuickMenuBar::isNative() const
{
    return m_platformMenuBar != 0;
}

void QQuickMenuBar::setNative(bool native)
{
    bool wasNative = isNative();
    if (native) {
        if (!m_platformMenuBar) {
            m_platformMenuBar = QGuiApplicationPrivate::platformTheme()->createPlatformMenuBar();
            if (m_platformMenuBar) {
                m_platformMenuBar->handleReparent(m_parentWindow);
                foreach (QQuickMenu *menu, m_menus)
                    m_platformMenuBar->insertMenu(menu->platformMenu(), 0 /* append */);
            }
        }
    } else {
        if (m_platformMenuBar) {
            foreach (QQuickMenu *menu, m_menus)
                m_platformMenuBar->removeMenu(menu->platformMenu());
        }
        delete m_platformMenuBar;
        m_platformMenuBar = 0;
    }
    if (isNative() != wasNative)
        emit nativeChanged();
}

void QQuickMenuBar::setContentItem(QQuickItem *item)
{
    if (item != m_contentItem) {
        m_contentItem = item;
        emit contentItemChanged();
    }
}

void QQuickMenuBar::setParentWindow(QQuickWindow *newParentWindow)
{
    if (newParentWindow != m_parentWindow) {
        m_parentWindow = newParentWindow;
        if (m_platformMenuBar)
            m_platformMenuBar->handleReparent(m_parentWindow);
    }
}

void QQuickMenuBar::append_menu(QQmlListProperty<QQuickMenu> *list, QQuickMenu *menu)
{
    if (QQuickMenuBar *menuBar = qobject_cast<QQuickMenuBar *>(list->object)) {
        menu->setParent(menuBar);
        menuBar->m_menus.append(menu);

        if (menuBar->m_platformMenuBar)
            menuBar->m_platformMenuBar->insertMenu(menu->platformMenu(), 0 /* append */);

        emit menuBar->menusChanged();
    }
}

int QQuickMenuBar::count_menu(QQmlListProperty<QQuickMenu> *list)
{
    if (QQuickMenuBar *menuBar = qobject_cast<QQuickMenuBar *>(list->object))
        return menuBar->m_menus.size();
    return 0;
}

QQuickMenu *QQuickMenuBar::at_menu(QQmlListProperty<QQuickMenu> *list, int index)
{
    QQuickMenuBar *menuBar = qobject_cast<QQuickMenuBar *>(list->object);
    if (menuBar &&  0 <= index && index < menuBar->m_menus.size())
        return menuBar->m_menus[index];
    return 0;
}

QT_END_NAMESPACE
