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
#ifndef QV4BINOP_P_H
#define QV4BINOP_P_H

#include <qv4jsir_p.h>
#include <qv4isel_masm_p.h>
#include <qv4assembler_p.h>

QT_BEGIN_NAMESPACE

#if ENABLE(ASSEMBLER)

namespace QV4 {
namespace JIT {

struct Binop {
    Binop(Assembler *assembler, IR::AluOp operation)
        : as(assembler)
        , op(operation)
    {}

    void generate(IR::Expr *lhs, IR::Expr *rhs, IR::Temp *target);
    void doubleBinop(IR::Expr *lhs, IR::Expr *rhs, IR::Temp *target);
    bool int32Binop(IR::Expr *leftSource, IR::Expr *rightSource, IR::Temp *target);
    Assembler::Jump genInlineBinop(IR::Expr *leftSource, IR::Expr *rightSource, IR::Temp *target);

    typedef Assembler::Jump (Binop::*MemRegOp)(Assembler::Address, Assembler::RegisterID);
    typedef Assembler::Jump (Binop::*ImmRegOp)(Assembler::TrustedImm32, Assembler::RegisterID);

    struct OpInfo {
        const char *name;
        QV4::Runtime::BinaryOperation fallbackImplementation;
        QV4::Runtime::BinaryOperationContext contextImplementation;
        MemRegOp inlineMemRegOp;
        ImmRegOp inlineImmRegOp;
    };

    static const OpInfo operations[IR::LastAluOp + 1];
    static const OpInfo &operation(IR::AluOp operation)
    { return operations[operation]; }

    Assembler::Jump inline_add32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        return as->branchAdd32(Assembler::Overflow, addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        return as->branchAdd32(Assembler::Overflow, Assembler::ScratchRegister, reg);
#endif
    }

    Assembler::Jump inline_add32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        return as->branchAdd32(Assembler::Overflow, imm, reg);
    }

    Assembler::Jump inline_sub32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        return as->branchSub32(Assembler::Overflow, addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        return as->branchSub32(Assembler::Overflow, Assembler::ScratchRegister, reg);
#endif
    }

    Assembler::Jump inline_sub32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        return as->branchSub32(Assembler::Overflow, imm, reg);
    }

    Assembler::Jump inline_mul32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        return as->branchMul32(Assembler::Overflow, addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        return as->branchMul32(Assembler::Overflow, Assembler::ScratchRegister, reg);
#endif
    }

    Assembler::Jump inline_mul32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        return as->branchMul32(Assembler::Overflow, imm, reg, reg);
    }

    Assembler::Jump inline_shl32(Assembler::Address addr, Assembler::RegisterID reg)
    {
        as->load32(addr, Assembler::ScratchRegister);
        as->and32(Assembler::TrustedImm32(0x1f), Assembler::ScratchRegister);
        as->lshift32(Assembler::ScratchRegister, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_shl32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        imm.m_value &= 0x1f;
        as->lshift32(imm, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_shr32(Assembler::Address addr, Assembler::RegisterID reg)
    {
        as->load32(addr, Assembler::ScratchRegister);
        as->and32(Assembler::TrustedImm32(0x1f), Assembler::ScratchRegister);
        as->rshift32(Assembler::ScratchRegister, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_shr32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        imm.m_value &= 0x1f;
        as->rshift32(imm, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_ushr32(Assembler::Address addr, Assembler::RegisterID reg)
    {
        as->load32(addr, Assembler::ScratchRegister);
        as->and32(Assembler::TrustedImm32(0x1f), Assembler::ScratchRegister);
        as->urshift32(Assembler::ScratchRegister, reg);
        return as->branchTest32(Assembler::Signed, reg, reg);
    }

    Assembler::Jump inline_ushr32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        imm.m_value &= 0x1f;
        as->urshift32(imm, reg);
        return as->branchTest32(Assembler::Signed, reg, reg);
    }

    Assembler::Jump inline_and32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        as->and32(addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        as->and32(Assembler::ScratchRegister, reg);
#endif
        return Assembler::Jump();
    }

    Assembler::Jump inline_and32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        as->and32(imm, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_or32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        as->or32(addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        as->or32(Assembler::ScratchRegister, reg);
#endif
        return Assembler::Jump();
    }

    Assembler::Jump inline_or32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        as->or32(imm, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_xor32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        as->xor32(addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        as->xor32(Assembler::ScratchRegister, reg);
#endif
        return Assembler::Jump();
    }

    Assembler::Jump inline_xor32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        as->xor32(imm, reg);
        return Assembler::Jump();
    }



    Assembler *as;
    IR::AluOp op;
};

}
}

#endif

QT_END_NAMESPACE

#endif
