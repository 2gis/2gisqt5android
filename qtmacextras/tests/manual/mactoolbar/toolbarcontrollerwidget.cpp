/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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

#include "toolbarcontrollerwidget.h"

ToolBarControllerWidget::ToolBarControllerWidget()
    :QWidget(0)
{

    setWindowTitle("QMacToolBar Test");
    resize(400, 200);

    QMacToolBar *toolBar = new QMacToolBar(this);

    QIcon qtIcon(QStringLiteral(":qtlogo.png"));
    fooItem = toolBar->addItem(qtIcon, QStringLiteral("Foo"));
    fooItem->setText("foo");

    connect(fooItem, SIGNAL(activated()), this, SLOT(activated()));

    QMacToolBarItem *item5 = toolBar->addAllowedItem(qtIcon, QStringLiteral("AllowedFoo"));
    connect(item5, SIGNAL(activated()), this, SLOT(activated()));

    QLineEdit *fooItemText = new QLineEdit(this);
    fooItemText->setText("Foo");
    fooItemText->move(10, 10);
    connect(fooItemText, SIGNAL(textChanged(QString)), this, SLOT(changeItemText(QString)));

    winId();
    toolBar->attachToWindow(windowHandle());
}

void ToolBarControllerWidget::activated()
{
    qDebug() << "activated" << sender();
}

void ToolBarControllerWidget::changeItemText(const QString &text)
{
    fooItem->setText(text);
}
