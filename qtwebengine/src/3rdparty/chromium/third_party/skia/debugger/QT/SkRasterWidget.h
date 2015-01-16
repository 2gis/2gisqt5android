/*
 * SkRasterWidget.h
 *
 *  Created on: Jul 28, 2012
 *      Author: chudy
 */


#ifndef SKRASTERWIDGET_H_
#define SKRASTERWIDGET_H_

#if SK_SUPPORT_GPU
#include "SkGpuDevice.h"
#endif

#include "SkBitmapDevice.h"
#include "SkDebugger.h"

#include <QApplication>
#include <QtGui>
#include <QWidget>

class  SkRasterWidget : public QWidget {
    Q_OBJECT

public:
    SkRasterWidget(SkDebugger* debugger);

    ~SkRasterWidget();

    void draw() {
        this->update();
    }

signals:
    void drawComplete();

protected:
    void paintEvent(QPaintEvent* event);

    void resizeEvent(QResizeEvent* event);

private:
    SkBitmap fBitmap;
    SkDebugger* fDebugger;
    SkCanvas* fCanvas;
    SkBaseDevice* fDevice;
};

#endif /* SKRASTERWIDGET_H_ */
