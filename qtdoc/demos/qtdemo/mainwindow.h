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

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QtWidgets>
#include <QPixmap>

class DemoTextItem;
class ImageItem;

class MainWindow : public QGraphicsView
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void enableMask(bool enable);
    void toggleFullscreen();
    int performBenchmark();
    void switchTimerOnOff(bool on);
    void start();

    QGraphicsScene *scene;
    QGraphicsWidget* mainSceneRoot;

    bool loop;

    // FPS stuff:
    QList<QTime> frameTimeList;
    QList<float> fpsHistory;
    float currentFps;
    float fpsMedian;
    DemoTextItem *fpsLabel;

protected:
    // Overridden methods:
    void showEvent(QShowEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void resizeEvent(QResizeEvent *event);
    void drawBackground(QPainter *painter, const QRectF &rect);
    void drawItems(QPainter *painter, int numItems, QGraphicsItem ** items, const QStyleOptionGraphicsItem* options);
    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);

private slots:
    void tick();

private:
    void setupWidget();
    void setupSceneItems();
    void drawBackgroundToPixmap();
    void setupScene();
    bool measureFps();
    void forceFpsMedianCalculation();
    void checkAdapt();
    void setRenderingSystem();

    QTimer updateTimer;
    QTime demoStartTime;
    QTime fpsTime;
    QPixmap background;
    ImageItem *companyLogo;
    ImageItem *qtLogo;
    bool doneAdapt;
    bool useTimer;
    DemoTextItem *pausedLabel;
};

#endif // MAIN_WINDOW_H

