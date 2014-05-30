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

#ifndef PARTICLEAFFECTOR_H
#define PARTICLEAFFECTOR_H

#include <QObject>
#include "qquickparticlesystem_p.h"
#include "qquickparticleextruder_p.h"

QT_BEGIN_NAMESPACE

class QQuickParticleAffector : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QQuickParticleSystem* system READ system WRITE setSystem NOTIFY systemChanged)
    Q_PROPERTY(QStringList groups READ groups WRITE setGroups NOTIFY groupsChanged)
    Q_PROPERTY(QStringList whenCollidingWith READ whenCollidingWith WRITE setWhenCollidingWith NOTIFY whenCollidingWithChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool once READ onceOff WRITE setOnceOff NOTIFY onceChanged)
    Q_PROPERTY(QQuickParticleExtruder* shape READ shape WRITE setShape NOTIFY shapeChanged)

public:
    explicit QQuickParticleAffector(QQuickItem *parent = 0);
    virtual void affectSystem(qreal dt);
    virtual void reset(QQuickParticleData*);//As some store their own data per particle?
    QQuickParticleSystem* system() const
    {
        return m_system;
    }

    QStringList groups() const
    {
        return m_groups;
    }

    bool enabled() const
    {
        return m_enabled;
    }

    bool onceOff() const
    {
        return m_onceOff;
    }

    QQuickParticleExtruder* shape() const
    {
        return m_shape;
    }

    QStringList whenCollidingWith() const
    {
        return m_whenCollidingWith;
    }

Q_SIGNALS:

    void systemChanged(QQuickParticleSystem* arg);

    void groupsChanged(QStringList arg);

    void enabledChanged(bool arg);

    void onceChanged(bool arg);

    void shapeChanged(QQuickParticleExtruder* arg);

    void affected(qreal x, qreal y);

    void whenCollidingWithChanged(QStringList arg);

public Q_SLOTS:
void setSystem(QQuickParticleSystem* arg)
{
    if (m_system != arg) {
        m_system = arg;
        m_system->registerParticleAffector(this);
        Q_EMIT systemChanged(arg);
    }
}

void setGroups(QStringList arg)
{
    if (m_groups != arg) {
        m_groups = arg;
        m_updateIntSet = true;
        Q_EMIT groupsChanged(arg);
    }
}

void setEnabled(bool arg)
{
    if (m_enabled != arg) {
        m_enabled = arg;
        Q_EMIT enabledChanged(arg);
    }
}

void setOnceOff(bool arg)
{
    if (m_onceOff != arg) {
        m_onceOff = arg;
        m_needsReset = true;
        Q_EMIT onceChanged(arg);
    }
}

void setShape(QQuickParticleExtruder* arg)
{
    if (m_shape != arg) {
        m_shape = arg;
        Q_EMIT shapeChanged(arg);
    }
}

void setWhenCollidingWith(QStringList arg)
{
    if (m_whenCollidingWith != arg) {
        m_whenCollidingWith = arg;
        Q_EMIT whenCollidingWithChanged(arg);
    }
}
public Q_SLOTS:
    void updateOffsets();

protected:
    friend class QQuickParticleSystem;
    virtual bool affectParticle(QQuickParticleData *d, qreal dt);
    bool m_needsReset:1;//### What is this really saving?
    bool m_ignoresTime:1;
    bool m_onceOff:1;
    bool m_enabled:1;

    QQuickParticleSystem* m_system;
    QStringList m_groups;
    bool activeGroup(int g);
    bool shouldAffect(QQuickParticleData* datum);//Call to do the logic on whether it is affecting that datum
    void postAffect(QQuickParticleData* datum);//Call to do the post-affect logic on particles which WERE affected(once off, needs reset, affected signal)
    virtual void componentComplete();
    bool isAffectedConnected();
    static const qreal simulationDelta;
    static const qreal simulationCutoff;

    QPointF m_offset;
    QSet<QPair<int, int> > m_onceOffed;
private:
    QSet<int> m_groupIds;
    bool m_updateIntSet;

    QQuickParticleExtruder* m_shape;

    QStringList m_whenCollidingWith;

    bool isColliding(QQuickParticleData* d);
};

QT_END_NAMESPACE
#endif // PARTICLEAFFECTOR_H
