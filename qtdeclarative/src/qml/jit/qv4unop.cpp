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
#include <qv4unop_p.h>
#include <qv4assembler_p.h>

#if ENABLE(ASSEMBLER)

using namespace QV4;
using namespace JIT;

#define stringIfyx(s) #s
#define stringIfy(s) stringIfyx(s)
#define setOp(operation) \
    do { call = operation; name = stringIfy(operation); } while (0)

void Unop::generate(IR::Temp *source, IR::Temp *target)
{
    Runtime::UnaryOperation call = 0;
    const char *name = 0;
    switch (op) {
    case IR::OpNot:
        generateNot(source, target);
        return;
    case IR::OpUMinus:
        generateUMinus(source, target);
        return;
    case IR::OpUPlus: setOp(Runtime::uPlus); break;
    case IR::OpCompl:
        generateCompl(source, target);
        return;
    case IR::OpIncrement: setOp(Runtime::increment); break;
    case IR::OpDecrement: setOp(Runtime::decrement); break;
    default:
        Q_UNREACHABLE();
    } // switch

    if (call) {
        as->generateFunctionCallImp(target, name, call, Assembler::PointerToValue(source));
    }
}

void Unop::generateUMinus(IR::Temp *source, IR::Temp *target)
{
    if (source->type == IR::SInt32Type) {
        Assembler::RegisterID tReg = Assembler::ScratchRegister;
        if (target->kind == IR::Temp::PhysicalRegister)
            tReg = (Assembler::RegisterID) target->index;
        Assembler::RegisterID sReg = as->toInt32Register(source, tReg);
        as->move(sReg, tReg);
        as->neg32(tReg);
        if (target->kind != IR::Temp::PhysicalRegister)
            as->storeInt32(tReg, target);
        return;
    }

    as->generateFunctionCallImp(target, "Runtime::uMinus", Runtime::uMinus, Assembler::PointerToValue(source));
}

void Unop::generateNot(IR::Temp *source, IR::Temp *target)
{
    if (source->type == IR::BoolType) {
        Assembler::RegisterID tReg = Assembler::ScratchRegister;
        if (target->kind == IR::Temp::PhysicalRegister)
            tReg = (Assembler::RegisterID) target->index;
        as->xor32(Assembler::TrustedImm32(0x1), as->toInt32Register(source, tReg), tReg);
        if (target->kind != IR::Temp::PhysicalRegister)
            as->storeBool(tReg, target);
        return;
    } else if (source->type == IR::SInt32Type) {
        Assembler::RegisterID tReg = Assembler::ScratchRegister;
        if (target->kind == IR::Temp::PhysicalRegister)
            tReg = (Assembler::RegisterID) target->index;
        as->compare32(Assembler::Equal,
                      as->toInt32Register(source, Assembler::ScratchRegister), Assembler::TrustedImm32(0),
                      tReg);
        if (target->kind != IR::Temp::PhysicalRegister)
            as->storeBool(tReg, target);
        return;
    } else if (source->type == IR::DoubleType) {
        // ###
    }
    // ## generic implementation testing for int/bool

    as->generateFunctionCallImp(target, "Runtime::uNot", Runtime::uNot, Assembler::PointerToValue(source));
}

void Unop::generateCompl(IR::Temp *source, IR::Temp *target)
{
    if (source->type == IR::SInt32Type) {
        Assembler::RegisterID tReg = Assembler::ScratchRegister;
        if (target->kind == IR::Temp::PhysicalRegister)
            tReg = (Assembler::RegisterID) target->index;
        as->xor32(Assembler::TrustedImm32(0xffffffff), as->toInt32Register(source, tReg), tReg);
        if (target->kind != IR::Temp::PhysicalRegister)
            as->storeInt32(tReg, target);
        return;
    }
    as->generateFunctionCallImp(target, "Runtime::complement", Runtime::complement, Assembler::PointerToValue(source));
}

#endif
