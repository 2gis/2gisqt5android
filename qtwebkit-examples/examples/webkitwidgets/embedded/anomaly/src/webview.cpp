/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

#include "webview.h"

#include <QPaintEvent>
#include <QWebFrame>

WebView::WebView(QWidget *parent)
    : QWebView(parent)
    , inLoading(false)
{
    connect(this, SIGNAL(loadStarted()), this, SLOT(newPageLoading()));
    connect(this, SIGNAL(loadFinished(bool)), this, SLOT(pageLoaded(bool)));
    page()->setPreferredContentsSize(QSize(1024, 768));
}

void WebView::paintEvent(QPaintEvent *event)
{
    if (inLoading && loadingTime.elapsed() < 750) {
        QPainter painter(this);
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        foreach (const QRect &rect, event->region().rects()) {
            painter.drawRect(rect);
        }
    } else {
        QWebView::paintEvent(event);
    }
}

void WebView::newPageLoading()
{
    inLoading = true;
    loadingTime.start();
}

void WebView::pageLoaded(bool)
{
    inLoading = false;
    update();
}
