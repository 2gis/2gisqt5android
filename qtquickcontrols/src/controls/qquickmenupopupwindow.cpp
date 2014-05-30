/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickmenupopupwindow_p.h"

#include <qguiapplication.h>
#include <qpa/qwindowsysteminterface.h>
#include <qquickitem.h>
#include <QtGui/QScreen>
#include <QtQuick/private/qquickrendercontrol_p.h>

QT_BEGIN_NAMESPACE

QQuickMenuPopupWindow::QQuickMenuPopupWindow() :
    QQuickPopupWindow(), m_itemAt(0)
{ }

void QQuickMenuPopupWindow::show()
{
    QQuickPopupWindow::show();
    // show() will reposition the popup at the last moment,
    // so its initial position must be captured after the call.
    m_initialPos = position();
}

void QQuickMenuPopupWindow::setParentItem(QQuickItem *item)
{
    QQuickPopupWindow::setParentItem(item);
    if (item)
        setParentWindow(item->window());
}

void QQuickMenuPopupWindow::setItemAt(QQuickItem *menuItem)
{
    if (m_itemAt) {
        disconnect(m_itemAt, SIGNAL(xChanged()), this, SLOT(updatePosition()));
        disconnect(m_itemAt, SIGNAL(yChanged()), this, SLOT(updatePosition()));
    }

    m_itemAt = menuItem;
    if (menuItem) {
        m_oldItemPos = menuItem->position().toPoint();
        connect(menuItem, SIGNAL(xChanged()), this, SLOT(updatePosition()));
        connect(menuItem, SIGNAL(yChanged()), this, SLOT(updatePosition()));
    }
}

void QQuickMenuPopupWindow::setParentWindow(QQuickWindow *parentWindow)
{
    QWindow *proxyWindow = QQuickRenderControl::renderWindowFor(parentWindow);
    QWindow *renderWindow = proxyWindow ? proxyWindow : parentWindow;
    if (transientParent() != renderWindow)
        setTransientParent(renderWindow);
    if (parentWindow) {
        connect(parentWindow, SIGNAL(destroyed()), this, SLOT(dismissPopup()));
        if (QQuickMenuPopupWindow *pw = qobject_cast<QQuickMenuPopupWindow *>(parentWindow))
            connect(pw, SIGNAL(popupDismissed()), this, SLOT(dismissPopup()));
    }
}

void QQuickMenuPopupWindow::setGeometry(int posx, int posy, int w, int h)
{
    QWindow *pw = transientParent();
    if (!pw && parentItem())
        pw = parentItem()->window();
    if (!pw)
        pw = this;
    QRect g = pw->screen()->availableVirtualGeometry();

    if (posx + w > g.right()) {
        if (qobject_cast<QQuickMenuPopupWindow *>(transientParent())) {
            // reposition submenu window on the parent menu's left side
            int submenuOverlap = pw->x() + pw->width() - posx;
            posx -= pw->width() + w - 2 * submenuOverlap;
        } else {
            posx = g.right() - w;
        }
    } else {
        posx = qMax(posx, g.left());
    }

    posy = qBound(g.top(), posy, g.bottom() - h);

    QQuickWindow::setGeometry(posx, posy, w, h);
    emit geometryChanged();
}

void QQuickMenuPopupWindow::updateSize()
{
    QSize contentSize = popupContentItem()->childrenRect().size().toSize();
    qreal x = m_initialPos.x();
    qreal y = m_initialPos.y();
    if (qGuiApp->layoutDirection() == Qt::RightToLeft)
        x -= contentSize.width();
    setGeometry(x, y, contentSize.width(), contentSize.height());
}

void QQuickMenuPopupWindow::updatePosition()
{
    QPointF newPos = position() + m_oldItemPos - m_itemAt->position();
    m_initialPos += m_oldItemPos - m_itemAt->position();
    setGeometry(newPos.x(), newPos.y(), width(), height());
}

QT_END_NAMESPACE
