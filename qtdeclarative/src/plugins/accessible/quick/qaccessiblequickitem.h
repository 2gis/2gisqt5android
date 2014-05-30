/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QACCESSIBLEQUICKITEM_H
#define QACCESSIBLEQUICKITEM_H

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include "qqmlaccessible.h"

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

class QTextDocument;

class QAccessibleQuickItem : public QQmlAccessible, public QAccessibleValueInterface, public QAccessibleTextInterface
{
public:
    QAccessibleQuickItem(QQuickItem *item);

    QRect rect() const;
    QRect viewRect() const;

    bool clipsChildren() const;

    QAccessibleInterface *parent() const;
    QAccessibleInterface *child(int index) const;
    int childCount() const;
    int indexOfChild(const QAccessibleInterface *iface) const;
    QList<QQuickItem *> childItems() const;

    QAccessible::State state() const;
    QAccessible::Role role() const;
    QString text(QAccessible::Text) const;

    bool isAccessible() const;

    // Value Interface
    QVariant currentValue() const;
    void setCurrentValue(const QVariant &value);
    QVariant maximumValue() const;
    QVariant minimumValue() const;
    QVariant minimumStepSize() const;


    // Text Interface
    void selection(int selectionIndex, int *startOffset, int *endOffset) const;
    int selectionCount() const;
    void addSelection(int startOffset, int endOffset);
    void removeSelection(int selectionIndex);
    void setSelection(int selectionIndex, int startOffset, int endOffset);

    // cursor
    int cursorPosition() const;
    void setCursorPosition(int position);

    // text
    QString text(int startOffset, int endOffset) const;
    QString textBeforeOffset(int offset, QAccessible::TextBoundaryType boundaryType,
                                     int *startOffset, int *endOffset) const;
    QString textAfterOffset(int offset, QAccessible::TextBoundaryType boundaryType,
                                    int *startOffset, int *endOffset) const;
    QString textAtOffset(int offset, QAccessible::TextBoundaryType boundaryType,
                                 int *startOffset, int *endOffset) const;
    int characterCount() const;

    // character <-> geometry
    QRect characterRect(int /* offset */) const { return QRect(); }
    int offsetAtPoint(const QPoint & /* point */) const { return -1; }

    void scrollToSubstring(int /* startIndex */, int /* endIndex */) {}
    QString attributes(int /* offset */, int *startOffset, int *endOffset) const { *startOffset = 0; *endOffset = 0; return QString(); }

    QTextDocument *textDocument() const;

protected:
    QQuickItem *item() const { return static_cast<QQuickItem*>(object()); }
    void *interface_cast(QAccessible::InterfaceType t);

private:
    QTextDocument *m_doc;
};

QRect itemScreenRect(QQuickItem *item);


#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // QACCESSIBLEQUICKITEM_H
