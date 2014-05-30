/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#ifndef DEMO_ITEM_ANIMATION_H
#define DEMO_ITEM_ANIMATION_H

#include <QtCore>
#include <QtWidgets>

class DemoItem;

class DemoItemAnimation : public QGraphicsItemAnimation
{
    Q_OBJECT

public:
    enum INOROUT {ANIM_IN, ANIM_OUT, ANIM_UNSPECIFIED};

    DemoItemAnimation(DemoItem *item, INOROUT inOrOut = ANIM_UNSPECIFIED);
    virtual ~DemoItemAnimation();

    virtual void play(bool fromStart = true, bool force = false);
    virtual void playReverse();
    virtual void stop(bool reset = true);
    virtual void setRepeat(int nr = 0);

    void setDuration(int duration);
    void setDuration(float duration){ setDuration(int(duration)); };
    void setOpacityAt0(qreal opacity);
    void setOpacityAt1(qreal opacity);
    void setOpacity(qreal step);
    void setCurrentTime(int ms);
    void setStartPos(const QPointF &pos);
    bool notOwnerOfItem();

    bool running();
    bool runningOrItemLocked();
    void lockItem(bool state);
    void prepare();

    DemoItem *demoItem();

    virtual void afterAnimationStep(qreal step); // overridden

    QTimeLine *timeline;
    qreal opacityAt0;
    qreal opacityAt1;
    int startDelay;
    QPointF startPos;
    bool hideOnFinished;
    bool moveOnPlay;
    bool forcePlay;
    bool fromStart;
    INOROUT inOrOut;

private slots:
    virtual void playWithoutDelay();
};

#endif // DEMO_ITEM_ANIMATION_H



