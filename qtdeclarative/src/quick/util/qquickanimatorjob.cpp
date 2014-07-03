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

#include "qquickanimatorcontroller_p.h"
#include "qquickanimatorjob_p.h"
#include "qquickanimator_p.h"
#include "qquickanimator_p_p.h"
#include <private/qquickwindow_p.h>
#include <private/qquickitem_p.h>
#include <private/qquickshadereffectnode_p.h>

#include <private/qanimationgroupjob_p.h>

#include <qcoreapplication.h>

QT_BEGIN_NAMESPACE

QQuickAnimatorProxyJob::QQuickAnimatorProxyJob(QAbstractAnimationJob *job, QObject *item)
    : m_controller(0)
    , m_job(job)
    , m_internalState(State_Stopped)
{
    m_isRenderThreadProxy = true;
    m_animation = qobject_cast<QQuickAbstractAnimation *>(item);

    setLoopCount(job->loopCount());

    // Instead of setting duration to job->duration() we need to set it to -1 so that
    // it runs as long as the job is running on the render thread. If we gave it
    // an explicit duration, it would be stopped, potentially stopping the RT animation
    // prematurely.
    // This means that the animation driver will tick on the GUI thread as long
    // as the animation is running on the render thread, but this overhead will
    // be negligiblie compared to animating and re-rendering the scene on the render thread.
    m_duration = -1;

    QObject *ctx = findAnimationContext(m_animation);
    if (!ctx) {
        qWarning("QtQuick: unable to find animation context for RT animation...");
        return;
    }

    QQuickWindow *window = qobject_cast<QQuickWindow *>(ctx);
    if (window) {
        setWindow(window);
    } else {
        QQuickItem *item = qobject_cast<QQuickItem *>(ctx);
        if (item->window())
            setWindow(item->window());
        connect(item, SIGNAL(windowChanged(QQuickWindow*)), this, SLOT(windowChanged(QQuickWindow*)));
    }
}

QQuickAnimatorProxyJob::~QQuickAnimatorProxyJob()
{
    deleteJob();
}

void QQuickAnimatorProxyJob::deleteJob()
{
    if (m_job) {
        if (m_controller && m_internalState != State_Starting)
            m_controller->deleteJob(m_job);
        else if (m_internalState == State_Starting)
            delete m_job;
        m_job = 0;
    }
}

QObject *QQuickAnimatorProxyJob::findAnimationContext(QQuickAbstractAnimation *a)
{
    QObject *p = a->parent();
    while (p != 0 && qobject_cast<QQuickWindow *>(p) == 0 && qobject_cast<QQuickItem *>(p) == 0)
        p = p->parent();
    return p;
}

void QQuickAnimatorProxyJob::updateCurrentTime(int)
{
}

void QQuickAnimatorProxyJob::updateState(QAbstractAnimationJob::State newState, QAbstractAnimationJob::State)
{
    if (m_state == Running) {
        m_internalState = State_Starting;
        if (m_controller)
            m_controller->startJob(this, m_job);
    } else if (newState == Stopped) {
        syncBackCurrentValues();
        if (m_internalState == State_Starting)
            m_internalState = State_Stopped;
        else if (m_controller) {
            m_controller->stopJob(this, m_job);
        }
    }
}

void QQuickAnimatorProxyJob::windowChanged(QQuickWindow *window)
{
    setWindow(window);
}

void QQuickAnimatorProxyJob::controllerWasDeleted()
{
    m_controller = 0;
    m_job = 0;
}

void QQuickAnimatorProxyJob::setWindow(QQuickWindow *window)
{
    if (!window) {
        // Stop will trigger syncBackCurrentValues so best to do it before
        // we delete m_job.
        stop();
        deleteJob();
        return;

    } else if (!m_controller && m_job) {
        m_controller = QQuickWindowPrivate::get(window)->animationController;
        if (window->openglContext())
            readyToAnimate();
        else
            connect(window, SIGNAL(sceneGraphInitialized()), this, SLOT(sceneGraphInitialized()));
    }
}

void QQuickAnimatorProxyJob::sceneGraphInitialized()
{
    readyToAnimate();
    disconnect(this, SLOT(sceneGraphInitialized()));
}

void QQuickAnimatorProxyJob::readyToAnimate()
{
    if (m_internalState == State_Starting)
        m_controller->startJob(this, m_job);
}

void QQuickAnimatorProxyJob::startedByController()
{
    m_internalState = State_Running;
}

bool QQuickAnimatorProxyJob::event(QEvent *e)
{
    if (e->type() == QEvent::User) {
        stop();
        return true;
    }
    return QObject::event(e);
}

static void qquick_syncback_helper(QAbstractAnimationJob *job)
{
    if (job->isRenderThreadJob()) {
        QQuickAnimatorJob *a = static_cast<QQuickAnimatorJob *>(job);
        // Sync back only those jobs that actually have been running
        if (a->controller() && a->hasBeenRunning())
            a->writeBack();
    } else if (job->isGroup()) {
        QAnimationGroupJob *g = static_cast<QAnimationGroupJob *>(job);
        for (QAbstractAnimationJob *a = g->firstChild(); a; a = a->nextSibling())
            qquick_syncback_helper(a);
    }
}

void QQuickAnimatorProxyJob::syncBackCurrentValues()
{
    if (m_job)
        qquick_syncback_helper(m_job);
}

QQuickAnimatorJob::QQuickAnimatorJob()
    : m_target(0)
    , m_controller(0)
    , m_from(0)
    , m_to(0)
    , m_value(0)
    , m_duration(0)
    , m_isTransform(false)
    , m_isUniform(false)
    , m_hasBeenRunning(false)
{
    m_isRenderThreadJob = true;
}

qreal QQuickAnimatorJob::value() const
{
    qreal v;
    m_controller->lock();
    v = m_value;
    m_controller->unlock();
    return v;
}

void QQuickAnimatorJob::setTarget(QQuickItem *target)
{
    m_target = target;
}

void QQuickAnimatorJob::initialize(QQuickAnimatorController *controller)
{
    m_controller = controller;
}

void QQuickAnimatorJob::targetWasDeleted()
{
    m_target = 0;
    m_controller = 0;
}

QQuickTransformAnimatorJob::QQuickTransformAnimatorJob()
    : m_helper(0)
{
    m_isTransform = true;
}

QQuickTransformAnimatorJob::~QQuickTransformAnimatorJob()
{
    if (m_helper && --m_helper->ref == 0) {
        m_controller->m_transforms.remove(m_helper->item);
        delete m_helper;
    }
}

void QQuickTransformAnimatorJob::initialize(QQuickAnimatorController *controller)
{
    QQuickAnimatorJob::initialize(controller);

    if (m_controller) {
        m_helper = m_controller->m_transforms.value(m_target);
        if (!m_helper) {
            m_helper = new Helper();
            m_helper->item = m_target;
            m_controller->m_transforms.insert(m_target, m_helper);
            QObject::connect(m_target, SIGNAL(destroyed(QObject*)), m_controller, SLOT(itemDestroyed(QObject*)), Qt::DirectConnection);
        } else {
            ++m_helper->ref;
            // Make sure leftovers from previous runs are being used...
            m_helper->wasSynced = false;
        }
        m_helper->sync();
    }
}

void QQuickTransformAnimatorJob::nodeWasDestroyed()
{
    if (m_helper)
        m_helper->node = 0;
}

void QQuickTransformAnimatorJob::Helper::sync()
{
    const quint32 mask = QQuickItemPrivate::Position
            | QQuickItemPrivate::BasicTransform
            | QQuickItemPrivate::TransformOrigin
            | QQuickItemPrivate::Size;

    QQuickItemPrivate *d = QQuickItemPrivate::get(item);
    if (d->extra.isAllocated()
            && d->extra->layer
            && d->extra->layer->enabled()) {
        d = QQuickItemPrivate::get(d->extra->layer->m_effectSource);
    }

    quint32 dirty = mask & d->dirtyAttributes;

    if (!wasSynced) {
        dirty = 0xffffffffu;
        wasSynced = true;
    }

    if (dirty == 0)
        return;

    node = d->itemNode();

    if (dirty & QQuickItemPrivate::Position) {
        dx = item->x();
        dy = item->y();
    }

    if (dirty & QQuickItemPrivate::BasicTransform) {
        scale = item->scale();
        rotation = item->rotation();
    }

    if (dirty & (QQuickItemPrivate::TransformOrigin | QQuickItemPrivate::Size)) {
        QPointF o = item->transformOriginPoint();
        ox = o.x();
        oy = o.y();
    }
}

void QQuickTransformAnimatorJob::Helper::apply()
{
    if (!wasChanged || !node)
        return;

    QMatrix4x4 m;
    m.translate(dx, dy);
    m.translate(ox, oy);
    m.scale(scale);
    m.rotate(rotation, 0, 0, 1);
    m.translate(-ox, -oy);
    node->setMatrix(m);

    wasChanged = false;
}



void QQuickXAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setX(value());
}

void QQuickXAnimatorJob::updateCurrentTime(int time)
{
    if (!m_controller)
        return;
    Q_ASSERT(m_controller->m_window->openglContext()->thread() == QThread::currentThread());

    m_value = m_from + (m_to - m_from) * m_easing.valueForProgress(time / (qreal) m_duration);
    m_helper->dx = m_value;
    m_helper->wasChanged = true;
}

void QQuickYAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setY(value());
}

void QQuickYAnimatorJob::updateCurrentTime(int time)
{
    if (!m_controller)
        return;
    Q_ASSERT(m_controller->m_window->openglContext()->thread() == QThread::currentThread());

    m_value = m_from + (m_to - m_from) * m_easing.valueForProgress(time / (qreal) m_duration);
    m_helper->dy = m_value;
    m_helper->wasChanged = true;
}

QQuickOpacityAnimatorJob::QQuickOpacityAnimatorJob()
    : m_opacityNode(0)
{
}

void QQuickOpacityAnimatorJob::initialize(QQuickAnimatorController *controller)
{
    QQuickAnimatorJob::initialize(controller);
    QQuickItemPrivate *d = QQuickItemPrivate::get(m_target);
    if (d->extra.isAllocated()
            && d->extra->layer
            && d->extra->layer->enabled()) {
        d = QQuickItemPrivate::get(d->extra->layer->m_effectSource);
    }

    m_opacityNode = d->opacityNode();
    if (!m_opacityNode) {
        m_opacityNode = new QSGOpacityNode();
        d->extra.value().opacityNode = m_opacityNode;

        QSGNode *child = d->clipNode();
        if (!child)
            child = d->rootNode();
        if (!child)
            child = d->groupNode;

        if (child) {
            if (child->parent())
                child->parent()->removeChildNode(child);
            m_opacityNode->appendChildNode(child);
        }

        d->itemNode()->appendChildNode(m_opacityNode);
    }
}

void QQuickOpacityAnimatorJob::nodeWasDestroyed()
{
    m_opacityNode = 0;
}

void QQuickOpacityAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setOpacity(value());
}

void QQuickOpacityAnimatorJob::updateCurrentTime(int time)
{
    if (!m_controller || !m_opacityNode)
        return;
    Q_ASSERT(m_controller->m_window->openglContext()->thread() == QThread::currentThread());

    m_value = m_from + (m_to - m_from) * m_easing.valueForProgress(time / (qreal) m_duration);
    m_opacityNode->setOpacity(m_value);
}

void QQuickScaleAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setScale(value());
}

void QQuickScaleAnimatorJob::updateCurrentTime(int time)
{
    if (!m_controller)
        return;
    Q_ASSERT(m_controller->m_window->openglContext()->thread() == QThread::currentThread());

    m_value = m_from + (m_to - m_from) * m_easing.valueForProgress(time / (qreal) m_duration);
    m_helper->scale = m_value;
    m_helper->wasChanged = true;
}

QQuickRotationAnimatorJob::QQuickRotationAnimatorJob()
    : m_direction(QQuickRotationAnimator::Numerical)
{
}

extern QVariant _q_interpolateShortestRotation(qreal &f, qreal &t, qreal progress);
extern QVariant _q_interpolateClockwiseRotation(qreal &f, qreal &t, qreal progress);
extern QVariant _q_interpolateCounterclockwiseRotation(qreal &f, qreal &t, qreal progress);

void QQuickRotationAnimatorJob::updateCurrentTime(int time)
{
    if (!m_controller)
        return;
    Q_ASSERT(m_controller->m_window->openglContext()->thread() == QThread::currentThread());

    float t =  m_easing.valueForProgress(time / (qreal) m_duration);
    switch (m_direction) {
    case QQuickRotationAnimator::Clockwise:
        m_value = _q_interpolateClockwiseRotation(m_from, m_to, t).toFloat();
        // The logic in _q_interpolateClockwise comes out a bit wrong
        // for the case of X->0 where 0<X<360. It ends on 360 which it
        // shouldn't.
        if (t == 1)
            m_value = m_to;
        break;
    case QQuickRotationAnimator::Counterclockwise:
        m_value = _q_interpolateCounterclockwiseRotation(m_from, m_to, t).toFloat();
        break;
    case QQuickRotationAnimator::Shortest:
        m_value = _q_interpolateShortestRotation(m_from, m_to, t).toFloat();
        break;
    case QQuickRotationAnimator::Numerical:
        m_value = m_from + (m_to - m_from) * t;
        break;
    }
    m_helper->rotation = m_value;
    m_helper->wasChanged = true;
}

void QQuickRotationAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setRotation(value());
}

QQuickUniformAnimatorJob::QQuickUniformAnimatorJob()
    : m_node(0)
    , m_uniformIndex(-1)
    , m_uniformType(-1)
{
    m_isUniform = true;
}

void QQuickUniformAnimatorJob::setTarget(QQuickItem *target)
{
    if (qobject_cast<QQuickShaderEffect *>(target) != 0)
        m_target = target;
}

void QQuickUniformAnimatorJob::nodeWasDestroyed()
{
    m_node = 0;
    m_uniformIndex = -1;
    m_uniformType = -1;
}

void QQuickUniformAnimatorJob::afterNodeSync()
{
    m_node = static_cast<QQuickShaderEffectNode *>(QQuickItemPrivate::get(m_target)->paintNode);

    if (m_node && m_uniformIndex == -1 && m_uniformType == -1) {
        QQuickShaderEffectMaterial *material =
                static_cast<QQuickShaderEffectMaterial *>(m_node->material());
        bool found = false;
        for (int t=0; !found && t<QQuickShaderEffectMaterialKey::ShaderTypeCount; ++t) {
            const QVector<QQuickShaderEffectMaterial::UniformData> &uniforms = material->uniforms[t];
            for (int i=0; i<uniforms.size(); ++i) {
                if (uniforms.at(i).name == m_uniform) {
                    m_uniformIndex = i;
                    m_uniformType = t;
                    found = true;
                    break;
                }
            }
        }
    }

}

void QQuickUniformAnimatorJob::updateCurrentTime(int time)
{
    if (!m_controller)
        return;
    Q_ASSERT(m_controller->m_window->openglContext()->thread() == QThread::currentThread());

    if (!m_node || m_uniformIndex == -1 || m_uniformType == -1)
        return;

    m_value = m_from + (m_to - m_from) * m_easing.valueForProgress(time / (qreal) m_duration);

    QQuickShaderEffectMaterial *material =
            static_cast<QQuickShaderEffectMaterial *>(m_node->material());
    material->uniforms[m_uniformType][m_uniformIndex].value = m_value;
    // As we're not touching the nodes, we need to explicitly mark it dirty.
    // Otherwise, the renderer will abort repainting if this was the only
    // change in the graph currently rendering.
    m_node->markDirty(QSGNode::DirtyMaterial);
}

void QQuickUniformAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setProperty(m_uniform, value());
}

QT_END_NAMESPACE
