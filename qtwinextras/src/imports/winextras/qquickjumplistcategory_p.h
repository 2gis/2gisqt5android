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

#ifndef QQUICKJUMPLISTCATEGORY_P_H
#define QQUICKJUMPLISTCATEGORY_P_H

#include "qquickjumplistitem_p.h"

#include <QObject>
#include <QQmlListProperty>
#include <QWinJumpListCategory>
#include <QWinJumpListItem>

QT_BEGIN_NAMESPACE

class QQuickJumpListCategory : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QObject> data READ data)
    Q_PROPERTY(QQmlListProperty<QQuickJumpListItem> items READ items)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibilityChanged)
    Q_CLASSINFO("DefaultProperty", "data")

public:
    explicit QQuickJumpListCategory(QObject *parent = 0);
    ~QQuickJumpListCategory();

    QString title() const;
    void setTitle(const QString &title);

    bool isVisible() const;
    void setVisible(bool visible);

    QQmlListProperty<QObject> data();
    QQmlListProperty<QQuickJumpListItem> items();

    QList<QWinJumpListItem *> toItemList() const;

Q_SIGNALS:
    void itemsChanged();
    void titleChanged();
    void visibilityChanged();

private:
    static void data_append(QQmlListProperty<QObject> *property, QObject *object);
    static int items_count(QQmlListProperty<QQuickJumpListItem> *property);
    static QQuickJumpListItem *items_at(QQmlListProperty<QQuickJumpListItem> *property, int index);

    bool m_visible;
    QString m_title;
    QList<QQuickJumpListItem *> m_items;
};

QT_END_NAMESPACE

#endif // QQUICKJUMPLISTCATEGORY_P_H
