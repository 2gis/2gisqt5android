/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "abstractformwindowtool.h"

QT_BEGIN_NAMESPACE

/*!
    \class QDesignerFormWindowToolInterface

    \brief The QDesignerFormWindowToolInterface class provides an
    interface that enables tools to be used on items in a form window.

    \inmodule QtDesigner

    \internal
*/

/*!
*/
QDesignerFormWindowToolInterface::QDesignerFormWindowToolInterface(QObject *parent)
    : QObject(parent)
{
}

/*!
*/
QDesignerFormWindowToolInterface::~QDesignerFormWindowToolInterface()
{
}

/*!
    \fn virtual QDesignerFormEditorInterface *QDesignerFormWindowToolInterface::core() const = 0
*/

/*!
    \fn virtual QDesignerFormWindowInterface *QDesignerFormWindowToolInterface::formWindow() const = 0
*/

/*!
    \fn virtual QWidget *QDesignerFormWindowToolInterface::editor() const = 0
*/

/*!
    \fn virtual QAction *QDesignerFormWindowToolInterface::action() const = 0
*/

/*!
    \fn virtual void QDesignerFormWindowToolInterface::activated() = 0
*/

/*!
    \fn virtual void QDesignerFormWindowToolInterface::deactivated() = 0
*/

/*!
    \fn virtual void QDesignerFormWindowToolInterface::saveToDom(DomUI*, QWidget*) {
*/

/*!
    \fn virtual void QDesignerFormWindowToolInterface::loadFromDom(DomUI*, QWidget*) {
*/

/*!
    \fn virtual bool QDesignerFormWindowToolInterface::handleEvent(QWidget *widget, QWidget *managedWidget, QEvent *event) = 0
*/

QT_END_NAMESPACE
