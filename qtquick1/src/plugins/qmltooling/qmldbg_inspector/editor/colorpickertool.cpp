/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "colorpickertool.h"

#include "qdeclarativeviewinspector.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtCore/QRectF>
#include <QtGui/QRgb>
#include <QtGui/QImage>
#include <QtWidgets/QApplication>
#include <QtGui/QPalette>

namespace QmlJSDebugger {
namespace QtQuick1 {

ColorPickerTool::ColorPickerTool(QDeclarativeViewInspector *view) :
    AbstractLiveEditTool(view)
{
    m_selectedColor.setRgb(0,0,0);
}

ColorPickerTool::~ColorPickerTool()
{
}

void ColorPickerTool::mousePressEvent(QMouseEvent *event)
{
    pickColor(event->pos());
}

void ColorPickerTool::mouseMoveEvent(QMouseEvent *event)
{
    pickColor(event->pos());
}

void ColorPickerTool::clear()
{
    view()->setCursor(Qt::CrossCursor);
}

void ColorPickerTool::pickColor(const QPoint &pos)
{
    QRgb fillColor = view()->backgroundBrush().color().rgb();
    if (view()->backgroundBrush().style() == Qt::NoBrush)
        fillColor = view()->palette().color(QPalette::Base).rgb();

    QRectF target(0,0, 1, 1);
    QRect source(pos.x(), pos.y(), 1, 1);
    QImage img(1, 1, QImage::Format_ARGB32);
    img.fill(fillColor);
    QPainter painter(&img);
    view()->render(&painter, target, source);
    m_selectedColor = QColor::fromRgb(img.pixel(0, 0));

    emit selectedColorChanged(m_selectedColor);
}

} // namespace QtQuick1
} // namespace QmlJSDebugger
