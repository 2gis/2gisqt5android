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

#ifndef FOLLOWEMITTER_H
#define FOLLOWEMITTER_H
#include "qquickparticleemitter_p.h"
#include "qquickparticleaffector_p.h"

QT_BEGIN_NAMESPACE

class QQuickTrailEmitter : public QQuickParticleEmitter
{
    Q_OBJECT
    Q_PROPERTY(QString follow READ follow WRITE setFollow NOTIFY followChanged)
    Q_PROPERTY(int emitRatePerParticle READ particlesPerParticlePerSecond WRITE setParticlesPerParticlePerSecond NOTIFY particlesPerParticlePerSecondChanged)

    Q_PROPERTY(QQuickParticleExtruder* emitShape READ emissonShape WRITE setEmissionShape NOTIFY emissionShapeChanged)
    Q_PROPERTY(qreal emitHeight READ emitterYVariation WRITE setEmitterYVariation NOTIFY emitterYVariationChanged)
    Q_PROPERTY(qreal emitWidth READ emitterXVariation WRITE setEmitterXVariation NOTIFY emitterXVariationChanged)

    Q_ENUMS(EmitSize)
public:
    enum EmitSize {
        ParticleSize = -2//Anything less than 0 will do
    };
    explicit QQuickTrailEmitter(QQuickItem *parent = 0);
    virtual void emitWindow(int timeStamp);
    virtual void reset();

    int particlesPerParticlePerSecond() const
    {
        return m_particlesPerParticlePerSecond;
    }

    qreal emitterXVariation() const
    {
        return m_emitterXVariation;
    }

    qreal emitterYVariation() const
    {
        return m_emitterYVariation;
    }

    QString follow() const
    {
        return m_follow;
    }

    QQuickParticleExtruder* emissonShape() const
    {
        return m_emissionExtruder;
    }

Q_SIGNALS:
    void emitFollowParticles(QQmlV4Handle particles, QQmlV4Handle followed);

    void particlesPerParticlePerSecondChanged(int arg);

    void emitterXVariationChanged(qreal arg);

    void emitterYVariationChanged(qreal arg);

    void followChanged(QString arg);

    void emissionShapeChanged(QQuickParticleExtruder* arg);

public Q_SLOTS:

    void setParticlesPerParticlePerSecond(int arg)
    {
        if (m_particlesPerParticlePerSecond != arg) {
            m_particlesPerParticlePerSecond = arg;
            Q_EMIT particlesPerParticlePerSecondChanged(arg);
        }
    }
    void setEmitterXVariation(qreal arg)
    {
        if (m_emitterXVariation != arg) {
            m_emitterXVariation = arg;
            Q_EMIT emitterXVariationChanged(arg);
        }
    }

    void setEmitterYVariation(qreal arg)
    {
        if (m_emitterYVariation != arg) {
            m_emitterYVariation = arg;
            Q_EMIT emitterYVariationChanged(arg);
        }
    }

    void setFollow(QString arg)
    {
        if (m_follow != arg) {
            m_follow = arg;
            Q_EMIT followChanged(arg);
        }
    }

    void setEmissionShape(QQuickParticleExtruder* arg)
    {
        if (m_emissionExtruder != arg) {
            m_emissionExtruder = arg;
            Q_EMIT emissionShapeChanged(arg);
        }
    }

private Q_SLOTS:
    void recalcParticlesPerSecond();

private:
    QSet<QQuickParticleData*> m_pending;
    QVector<qreal> m_lastEmission;
    int m_particlesPerParticlePerSecond;
    qreal m_lastTimeStamp;
    qreal m_emitterXVariation;
    qreal m_emitterYVariation;
    QString m_follow;
    int m_followCount;
    QQuickParticleExtruder* m_emissionExtruder;
    QQuickParticleExtruder* m_defaultEmissionExtruder;
    bool isEmitFollowConnected();
};

QT_END_NAMESPACE
#endif // FOLLOWEMITTER_H
