/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QQUICKTRANSITION_H
#define QQUICKTRANSITION_H

#include "qquickstate_p.h"
#include <private/qabstractanimationjob_p.h>
#include <qqml.h>

#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class QQuickAbstractAnimation;
class QQuickTransitionPrivate;
class QQuickTransitionManager;
class QQuickTransition;

class QQuickTransitionInstance
{
public:
    QQuickTransitionInstance(QQuickTransitionPrivate *transition, QAbstractAnimationJob *anim);
    ~QQuickTransitionInstance();

    void start();
    void stop();

    bool isRunning() const;

private:
    QQuickTransitionPrivate *m_transition;
    QAbstractAnimationJob *m_anim;
    friend class QQuickTransition;
};

class Q_QUICK_PRIVATE_EXPORT QQuickTransition : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickTransition)

    Q_PROPERTY(QString from READ fromState WRITE setFromState NOTIFY fromChanged)
    Q_PROPERTY(QString to READ toState WRITE setToState NOTIFY toChanged)
    Q_PROPERTY(bool reversible READ reversible WRITE setReversible NOTIFY reversibleChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QQmlListProperty<QQuickAbstractAnimation> animations READ animations)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_CLASSINFO("DefaultProperty", "animations")
    Q_CLASSINFO("DeferredPropertyNames", "animations")

public:
    QQuickTransition(QObject *parent=0);
    ~QQuickTransition();

    QString fromState() const;
    void setFromState(const QString &);

    QString toState() const;
    void setToState(const QString &);

    bool reversible() const;
    void setReversible(bool);

    bool enabled() const;
    void setEnabled(bool enabled);

    bool running() const;

    QQmlListProperty<QQuickAbstractAnimation> animations();

    QQuickTransitionInstance *prepare(QQuickStateOperation::ActionList &actions,
                 QList<QQmlProperty> &after,
                 QQuickTransitionManager *end,
                 QObject *defaultTarget);

    void setReversed(bool r);

Q_SIGNALS:
    void fromChanged();
    void toChanged();
    void reversibleChanged();
    void enabledChanged();
    void runningChanged();
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickTransition)

#endif // QQUICKTRANSITION_H
