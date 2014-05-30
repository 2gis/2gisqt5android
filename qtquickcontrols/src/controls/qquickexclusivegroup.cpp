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

#include "qquickexclusivegroup_p.h"

#include <qvariant.h>
#include <qdebug.h>
#include "qquickaction_p.h"

#define CHECKED_PROPERTY "checked"

QT_BEGIN_NAMESPACE

static const char *checkableSignals[] = {
    CHECKED_PROPERTY"Changed()",
    "toggled(bool)",
    "toggled()",
    0
};

static bool isChecked(const QObject *o)
{
    if (!o) return false;
    QVariant checkedVariant = o->property(CHECKED_PROPERTY);
    return checkedVariant.isValid() && checkedVariant.toBool();
}

/*!
    \qmltype ExclusiveGroup
    \instantiates QQuickExclusiveGroup
    \ingroup controls
    \inqmlmodule QtQuick.Controls
    \brief ExclusiveGroup provides a way to declare several checkable controls as mutually exclusive.

    ExclusiveGroup can contain several \l Action items, and those will automatically get their
    \l Action::exclusiveGroup property assigned.

    \code
    ExclusiveGroup {
        id: radioInputGroup

        Action {
            id: dabRadioInput
            text: "DAB"
            checkable: true
        }

        Action {
            id: fmRadioInput
            text: "FM"
            checkable: true
        }

        Action {
            id: amRadioInput
            text: "AM"
            checkable: true
        }
    }
    \endcode

    Several controls already support ExclusiveGroup, e.g. \l Action, \l MenuItem, \l {QtQuick.Controls::}{Button}, and \l RadioButton.

    Since ExclusiveGroup only supports \l Action as child items, we need to manually assign the \c exclusiveGroup
    property for other objects.

    \code
    ExclusiveGroup { id: textAlignmentGroup }

    Menu {
        MenuItem {
            text: "Alignt Left"
            checkable: true
            exclusiveGroup: textAlignmentGroup
        }
        MenuItem {
            text: "Alignt Right"
            checkable: true
            exclusiveGroup: textAlignmentGroup
        }
        MenuItem {
            text: "Center"
            checkable: true
            exclusiveGroup: textAlignmentGroup
        }
        MenuItem {
            text: "Justify"
            checkable: true
            exclusiveGroup: textAlignmentGroup
        }
    }
    \endcode

    \section1 Adding support to ExclusiveGroup

    It is possible to add support for ExclusiveGroup for an object or control. It should have a \c checked
    property, and either a \c checkedChanged, \c toggled(), or \c toggled(bool) signal. It also needs
    to be bound with \l ExclusiveGroup::bindCheckable() when its ExclusiveGroup typed property is set.

    \code
    Item {
        id: myItem

        property bool checked: false
        property ExclusiveGroup exclusiveGroup: null

        onExclusiveGroupChanged: {
            if (exclusiveGroup)
                exclusiveGroup.bindCheckable(myItem)
        }
    }
    \endcode

    The example above shows the minimum code necessary to add ExclusiveGroup support to any item.
*/

/*!
    \qmlproperty object ExclusiveGroup::current

    The currently selected object. Defaults to the first checked object bound to the ExclusiveGroup.
    If there is none, then it defaults to \c null.
*/

/*!
    \qmlmethod void ExclusiveGroup::bindCheckable(object)

    Register \c object to the exclusive group.

    You should only need to call this function when creating a component you want to be compatible with \c ExclusiveGroup.

    \sa ExclusiveGroup::unbindCheckable()
*/

/*!
    \qmlmethod void ExclusiveGroup::unbindCheckable(object)

    Unregister \c object from the exclusive group.

    You should only need to call this function when creating a component you want to be compatible with \c ExclusiveGroup.

    \sa ExclusiveGroup::bindCheckable()
*/

QQuickExclusiveGroup::QQuickExclusiveGroup(QObject *parent)
    : QObject(parent), m_current(0)
{
    int index = metaObject()->indexOfMethod("updateCurrent()");
    m_updateCurrentMethod = metaObject()->method(index);
}

QQmlListProperty<QQuickAction> QQuickExclusiveGroup::actions()
{
    return QQmlListProperty<QQuickAction>(this, 0, &QQuickExclusiveGroup::append_actions, 0, 0, 0);
}

void QQuickExclusiveGroup::append_actions(QQmlListProperty<QQuickAction> *list, QQuickAction *action)
{
    if (QQuickExclusiveGroup *eg = qobject_cast<QQuickExclusiveGroup *>(list->object))
        action->setExclusiveGroup(eg);
}

void QQuickExclusiveGroup::setCurrent(QObject * o)
{
    if (m_current == o)
        return;

    if (m_current)
        m_current->setProperty(CHECKED_PROPERTY, QVariant(false));
    m_current = o;
    if (m_current)
        m_current->setProperty(CHECKED_PROPERTY, QVariant(true));
    emit currentChanged();
}

void QQuickExclusiveGroup::updateCurrent()
{
    QObject *checkable = sender();
    if (isChecked(checkable))
        setCurrent(checkable);
}

void QQuickExclusiveGroup::bindCheckable(QObject *o)
{
    for (const char **signalName = checkableSignals; *signalName; signalName++) {
        int signalIndex = o->metaObject()->indexOfSignal(*signalName);
        if (signalIndex != -1) {
            QMetaMethod signalMethod = o->metaObject()->method(signalIndex);
            connect(o, signalMethod, this, m_updateCurrentMethod, Qt::UniqueConnection);
            connect(o, SIGNAL(destroyed(QObject*)), this, SLOT(unbindCheckable(QObject*)), Qt::UniqueConnection);
            if (!m_current && isChecked(o))
                setCurrent(o);
            return;
        }
    }

    qWarning() << "QQuickExclusiveGroup::bindCheckable(): Cannot bind to" << o;
}

void QQuickExclusiveGroup::unbindCheckable(QObject *o)
{
    if (m_current == o)
        setCurrent(0);
    for (const char **signalName = checkableSignals; *signalName; signalName++) {
        int signalIndex = o->metaObject()->indexOfSignal(*signalName);
        if (signalIndex != -1) {
            QMetaMethod signalMethod = o->metaObject()->method(signalIndex);
            if (disconnect(o, signalMethod, this, m_updateCurrentMethod)) {
                disconnect(o, SIGNAL(destroyed(QObject*)), this, SLOT(unbindCheckable(QObject*)));
                break;
            }
        }
    }
}

QT_END_NAMESPACE
