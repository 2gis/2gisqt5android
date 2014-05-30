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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of Qt Designer.  This header
// file may change from version to version without notice, or even be removed.
//
// We mean it.
//

#ifndef QDESIGNER_COMMAND2_H
#define QDESIGNER_COMMAND2_H

#include "shared_global_p.h"
#include "qdesigner_formwindowcommand_p.h"

#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

class LayoutCommand;
class BreakLayoutCommand;

/* This command changes the type of a managed layout on a widget (including
 * red layouts of type 'QLayoutWidget') into another type, maintaining the
 * applicable properties. It does this by chaining BreakLayoutCommand and
 * LayoutCommand, parametrizing them not to actually delete/reparent
 * QLayoutWidget's. */

class QDESIGNER_SHARED_EXPORT MorphLayoutCommand : public QDesignerFormWindowCommand {
    Q_DISABLE_COPY(MorphLayoutCommand)
public:
    explicit MorphLayoutCommand(QDesignerFormWindowInterface *formWindow);
    virtual ~MorphLayoutCommand();

    bool init(QWidget *w, int newType);

    static bool canMorph(const QDesignerFormWindowInterface *formWindow, QWidget *w, int *ptrToCurrentType = 0);

    virtual void redo();
    virtual void undo();

private:
    static QString formatDescription(QDesignerFormEditorInterface *core, const QWidget *w, int oldType, int newType);

    BreakLayoutCommand *m_breakLayoutCommand;
    LayoutCommand *m_layoutCommand;
    int m_newType;
    QWidgetList m_widgets;
    QWidget *m_layoutBase;
};

// Change the alignment of a widget in a managed grid/box layout cell.
class LayoutAlignmentCommand : public QDesignerFormWindowCommand {
    Q_DISABLE_COPY(LayoutAlignmentCommand)
public:
    explicit LayoutAlignmentCommand(QDesignerFormWindowInterface *formWindow);

    bool init(QWidget *w, Qt::Alignment alignment);

    virtual void redo();
    virtual void undo();

    // Find out alignment and return whether command is enabled.
    static Qt::Alignment alignmentOf(const QDesignerFormEditorInterface *core, QWidget *w, bool *enabled = 0);

private:
    static void applyAlignment(const QDesignerFormEditorInterface *core, QWidget *w, Qt::Alignment a);

    Qt::Alignment m_newAlignment;
    Qt::Alignment m_oldAlignment;
    QWidget *m_widget;
};

} // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // QDESIGNER_COMMAND2_H
