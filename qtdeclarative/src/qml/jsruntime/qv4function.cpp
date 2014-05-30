/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qv4function_p.h"
#include "qv4managed_p.h"
#include "qv4string_p.h"
#include "qv4value_inl_p.h"
#include "qv4engine_p.h"
#include "qv4lookup_p.h"

QT_BEGIN_NAMESPACE

using namespace QV4;

Function::Function(ExecutionEngine *engine, CompiledData::CompilationUnit *unit, const CompiledData::Function *function,
                   ReturnedValue (*codePtr)(ExecutionContext *, const uchar *))
        : compiledFunction(function)
        , compilationUnit(unit)
        , code(codePtr)
        , codeData(0)
{
    Q_UNUSED(engine);

    internalClass = engine->emptyClass;
    const quint32 *formalsIndices = compiledFunction->formalsTable();
    // iterate backwards, so we get the right ordering for duplicate names
    for (int i = static_cast<int>(compiledFunction->nFormals - 1); i >= 0; --i) {
        String *arg = compilationUnit->runtimeStrings[formalsIndices[i]].asString();
        while (1) {
            InternalClass *newClass = internalClass->addMember(arg, Attr_NotConfigurable);
            if (newClass != internalClass) {
                internalClass = newClass;
                break;
            }
            // duplicate arguments, need some trick to store them
            arg = new (engine->memoryManager) String(engine, arg, engine->newString(QString(0xfffe))->getPointer());
        }
    }

    const quint32 *localsIndices = compiledFunction->localsTable();
    for (quint32 i = 0; i < compiledFunction->nLocals; ++i) {
        String *local = compilationUnit->runtimeStrings[localsIndices[i]].asString();
        internalClass = internalClass->addMember(local, Attr_NotConfigurable);
    }
}

Function::~Function()
{
}

QT_END_NAMESPACE
