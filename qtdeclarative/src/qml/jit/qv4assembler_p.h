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
#ifndef QV4ASSEMBLER_P_H
#define QV4ASSEMBLER_P_H

#include "private/qv4global_p.h"
#include "private/qv4jsir_p.h"
#include "private/qv4isel_p.h"
#include "private/qv4isel_util_p.h"
#include "private/qv4value_p.h"
#include "private/qv4lookup_p.h"

#include <QtCore/QHash>
#include <QtCore/QStack>
#include <config.h>
#include <wtf/Vector.h>

#if ENABLE(ASSEMBLER)

#include <assembler/MacroAssembler.h>
#include <assembler/MacroAssemblerCodeRef.h>

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace JIT {

#define OP(op) \
    { isel_stringIfy(op), op, 0, 0, 0 }
#define OPCONTEXT(op) \
    { isel_stringIfy(op), 0, op, 0, 0 }

class InstructionSelection;

struct CompilationUnit : public QV4::CompiledData::CompilationUnit
{
    virtual ~CompilationUnit();

    virtual void linkBackendToEngine(QV4::ExecutionEngine *engine);

    virtual QV4::ExecutableAllocator::ChunkOfPages *chunkForFunction(int functionIndex);

    // Coderef + execution engine

    QVector<JSC::MacroAssemblerCodeRef> codeRefs;
    QList<QVector<QV4::Primitive> > constantValues;
};

struct RelativeCall {
    JSC::MacroAssembler::Address addr;

    explicit RelativeCall(const JSC::MacroAssembler::Address &addr)
        : addr(addr)
    {}
};


template <typename T>
struct ExceptionCheck {
    enum { NeedsCheck = 1 };
};
// push_catch and pop context methods shouldn't check for exceptions
template <>
struct ExceptionCheck<QV4::ExecutionContext *(*)(QV4::ExecutionContext *)> {
    enum { NeedsCheck = 0 };
};
template <typename A>
struct ExceptionCheck<QV4::ExecutionContext *(*)(QV4::ExecutionContext *, A)> {
    enum { NeedsCheck = 0 };
};
template <>
struct ExceptionCheck<QV4::ReturnedValue (*)(QV4::NoThrowContext *)> {
    enum { NeedsCheck = 0 };
};
template <typename A>
struct ExceptionCheck<QV4::ReturnedValue (*)(QV4::NoThrowContext *, A)> {
    enum { NeedsCheck = 0 };
};
template <typename A, typename B>
struct ExceptionCheck<QV4::ReturnedValue (*)(QV4::NoThrowContext *, A, B)> {
    enum { NeedsCheck = 0 };
};
template <typename A, typename B, typename C>
struct ExceptionCheck<void (*)(QV4::NoThrowContext *, A, B, C)> {
    enum { NeedsCheck = 0 };
};

class Assembler : public JSC::MacroAssembler
{
public:
    Assembler(InstructionSelection *isel, IR::Function* function, QV4::ExecutableAllocator *executableAllocator,
              int maxArgCountForBuiltins);

#if CPU(X86)

#undef VALUE_FITS_IN_REGISTER
#undef ARGUMENTS_IN_REGISTERS

#define HAVE_ALU_OPS_WITH_MEM_OPERAND 1

    static const RegisterID StackFrameRegister = JSC::X86Registers::ebp;
    static const RegisterID StackPointerRegister = JSC::X86Registers::esp;
    static const RegisterID LocalsRegister = JSC::X86Registers::edi;
    static const RegisterID ContextRegister = JSC::X86Registers::esi;
    static const RegisterID ReturnValueRegister = JSC::X86Registers::eax;
    static const RegisterID ScratchRegister = JSC::X86Registers::ecx;
    static const FPRegisterID FPGpr0 = JSC::X86Registers::xmm0;
    static const FPRegisterID FPGpr1 = JSC::X86Registers::xmm1;

    static const int RegisterSize = 4;

    static const int RegisterArgumentCount = 0;
    static RegisterID registerForArgument(int)
    {
        Q_ASSERT(false);
        // Not reached.
        return JSC::X86Registers::eax;
    }

    // Return address is pushed onto stack by the CPU.
    static const int StackSpaceAllocatedUponFunctionEntry = RegisterSize;
    static const int StackShadowSpace = 0;
    inline void platformEnterStandardStackFrame() {}
    inline void platformLeaveStandardStackFrame() {}
#elif CPU(X86_64)

#define VALUE_FITS_IN_REGISTER
#define ARGUMENTS_IN_REGISTERS
#define HAVE_ALU_OPS_WITH_MEM_OPERAND 1

    static const RegisterID StackFrameRegister = JSC::X86Registers::ebp;
    static const RegisterID StackPointerRegister = JSC::X86Registers::esp;
    static const RegisterID LocalsRegister = JSC::X86Registers::r12;
    static const RegisterID ContextRegister = JSC::X86Registers::r14;
    static const RegisterID ReturnValueRegister = JSC::X86Registers::eax;
    static const RegisterID ScratchRegister = JSC::X86Registers::r10;
    static const FPRegisterID FPGpr0 = JSC::X86Registers::xmm0;
    static const FPRegisterID FPGpr1 = JSC::X86Registers::xmm1;

    static const int RegisterSize = 8;

#if OS(WINDOWS)
    static const int RegisterArgumentCount = 4;
    static RegisterID registerForArgument(int index)
    {
        static RegisterID regs[RegisterArgumentCount] = {
            JSC::X86Registers::ecx,
            JSC::X86Registers::edx,
            JSC::X86Registers::r8,
            JSC::X86Registers::r9
        };
        Q_ASSERT(index >= 0 && index < RegisterArgumentCount);
        return regs[index];
    };
    static const int StackShadowSpace = 32;
#else // Unix
    static const int RegisterArgumentCount = 6;
    static RegisterID registerForArgument(int index)
    {
        static RegisterID regs[RegisterArgumentCount] = {
            JSC::X86Registers::edi,
            JSC::X86Registers::esi,
            JSC::X86Registers::edx,
            JSC::X86Registers::ecx,
            JSC::X86Registers::r8,
            JSC::X86Registers::r9
        };
        Q_ASSERT(index >= 0 && index < RegisterArgumentCount);
        return regs[index];
    };
    static const int StackShadowSpace = 0;
#endif

    // Return address is pushed onto stack by the CPU.
    static const int StackSpaceAllocatedUponFunctionEntry = RegisterSize;
    inline void platformEnterStandardStackFrame() {}
    inline void platformLeaveStandardStackFrame() {}
#elif CPU(ARM)

#undef VALUE_FITS_IN_REGISTER
#define ARGUMENTS_IN_REGISTERS
#undef HAVE_ALU_OPS_WITH_MEM_OPERAND

    static const RegisterID StackPointerRegister = JSC::ARMRegisters::sp; // r13
    static const RegisterID StackFrameRegister = JSC::ARMRegisters::fp; // r11
    static const RegisterID LocalsRegister = JSC::ARMRegisters::r7;
    static const RegisterID ScratchRegister = JSC::ARMRegisters::r6;
    static const RegisterID ContextRegister = JSC::ARMRegisters::r5;
    static const RegisterID ReturnValueRegister = JSC::ARMRegisters::r0;
    static const FPRegisterID FPGpr0 = JSC::ARMRegisters::d0;
    static const FPRegisterID FPGpr1 = JSC::ARMRegisters::d1;

    static const int RegisterSize = 4;

    static const RegisterID RegisterArgument1 = JSC::ARMRegisters::r0;
    static const RegisterID RegisterArgument2 = JSC::ARMRegisters::r1;
    static const RegisterID RegisterArgument3 = JSC::ARMRegisters::r2;
    static const RegisterID RegisterArgument4 = JSC::ARMRegisters::r3;

    static const int RegisterArgumentCount = 4;
    static RegisterID registerForArgument(int index)
    {
        Q_ASSERT(index >= 0 && index < RegisterArgumentCount);
        return static_cast<RegisterID>(JSC::ARMRegisters::r0 + index);
    };

    // Registers saved in platformEnterStandardStackFrame below.
    static const int StackSpaceAllocatedUponFunctionEntry = 5 * RegisterSize;
    static const int StackShadowSpace = 0;
    inline void platformEnterStandardStackFrame()
    {
        // Move the register arguments onto the stack as if they were
        // pushed by the caller, just like on ia32. This gives us consistent
        // access to the parameters if we need to.
        push(JSC::ARMRegisters::r3);
        push(JSC::ARMRegisters::r2);
        push(JSC::ARMRegisters::r1);
        push(JSC::ARMRegisters::r0);
        push(JSC::ARMRegisters::lr);
    }
    inline void platformLeaveStandardStackFrame()
    {
        pop(JSC::ARMRegisters::lr);
        addPtr(TrustedImm32(4 * RegisterSize), StackPointerRegister);
    }
#else
#error The JIT needs to be ported to this platform.
#endif
    static const int calleeSavedRegisterCount;

#if CPU(X86) || CPU(X86_64)
    static const int StackAlignment = 16;
#elif CPU(ARM)
    // Per AAPCS
    static const int StackAlignment = 8;
#else
#error Stack alignment unknown for this platform.
#endif

    // Explicit type to allow distinguishing between
    // pushing an address itself or the value it points
    // to onto the stack when calling functions.
    struct Pointer : public Address
    {
        explicit Pointer(const Address& addr)
            : Address(addr)
        {}
        explicit Pointer(RegisterID reg, int32_t offset)
            : Address(reg, offset)
        {}
    };

    // V4 uses two stacks: one stack with QV4::Value items, which is checked by the garbage
    // collector, and one stack used by the native C/C++/ABI code. This C++ stack is not scanned
    // by the garbage collector, so if any JS object needs to be retained, it should be put on the
    // JS stack.
    //
    // The "saved reg arg X" are on the C++ stack is used to store values in registers that need to
    // be passed by reference to native functions. It is fine to use the C++ stack, because only
    // non-object values can be stored in registers.
    //
    // Stack layout for the C++ stack:
    //   return address
    //   old FP                     <- FP
    //   callee saved reg n
    //   ...
    //   callee saved reg 0
    //   saved reg arg 0
    //   ...
    //   saved reg arg n            <- SP
    //
    // Stack layout for the JS stack:
    //   function call argument n   <- LocalsRegister
    //   ...
    //   function call argument 0
    //   local 0
    //   ...
    //   local n
    class StackLayout
    {
    public:
        StackLayout(IR::Function *function, int maxArgCountForBuiltins)
            : calleeSavedRegCount(Assembler::calleeSavedRegisterCount + 1)
            , maxOutgoingArgumentCount(function->maxNumberOfArguments)
            , localCount(function->tempCount)
            , savedRegCount(maxArgCountForBuiltins)
        {
#if 0 // debug code
            qDebug("calleeSavedRegCount.....: %d",calleeSavedRegCount);
            qDebug("maxOutgoingArgumentCount: %d",maxOutgoingArgumentCount);
            qDebug("localCount..............: %d",localCount);
            qDebug("savedConstCount.........: %d",savedRegCount);
            for (int i = 0; i < maxOutgoingArgumentCount; ++i)
                qDebug("argumentAddressForCall(%d) = 0x%x / -0x%x", i,
                       argumentAddressForCall(i).offset, -argumentAddressForCall(i).offset);
            for (int i = 0; i < localCount; ++i)
                qDebug("local(%d) = 0x%x / -0x%x", i, stackSlotPointer(i).offset,
                       -stackSlotPointer(i).offset);
            qDebug("savedReg(0) = 0x%x / -0x%x", savedRegPointer(0).offset, -savedRegPointer(0).offset);
            qDebug("savedReg(1) = 0x%x / -0x%x", savedRegPointer(1).offset, -savedRegPointer(1).offset);
            qDebug("savedReg(2) = 0x%x / -0x%x", savedRegPointer(2).offset, -savedRegPointer(2).offset);
            qDebug("savedReg(3) = 0x%x / -0x%x", savedRegPointer(3).offset, -savedRegPointer(3).offset);
            qDebug("savedReg(4) = 0x%x / -0x%x", savedRegPointer(4).offset, -savedRegPointer(4).offset);
            qDebug("savedReg(5) = 0x%x / -0x%x", savedRegPointer(5).offset, -savedRegPointer(5).offset);

            qDebug("callDataAddress(0) = 0x%x", callDataAddress(0).offset);
#endif
        }

        int calculateStackFrameSize() const
        {
            const int stackSpaceAllocatedOtherwise = StackSpaceAllocatedUponFunctionEntry
                                                     + RegisterSize; // saved StackFrameRegister

            // space for the callee saved registers
            int frameSize = RegisterSize * calleeSavedRegisterCount;
            frameSize += savedRegCount * sizeof(QV4::Value); // these get written out as Values, not as native registers

            Q_ASSERT(frameSize + stackSpaceAllocatedOtherwise < INT_MAX);
            frameSize = static_cast<int>(WTF::roundUpToMultipleOf(StackAlignment, frameSize + stackSpaceAllocatedOtherwise));
            frameSize -= stackSpaceAllocatedOtherwise;

            return frameSize;
        }

        int calculateJSStackFrameSize() const
        {
            const int locals = (localCount + sizeof(QV4::CallData)/sizeof(QV4::Value) - 1 + maxOutgoingArgumentCount) + 1;
            int frameSize = locals * sizeof(QV4::Value);
            return frameSize;
        }

        Address stackSlotPointer(int idx) const
        {
            Q_ASSERT(idx >= 0);
            Q_ASSERT(idx < localCount);

            Pointer addr = callDataAddress(0);
            addr.offset -= sizeof(QV4::Value) * (idx + 1);
            return addr;
        }

        // Some run-time functions take (Value* args, int argc). This function is for populating
        // the args.
        Pointer argumentAddressForCall(int argument) const
        {
            Q_ASSERT(argument >= 0);
            Q_ASSERT(argument < maxOutgoingArgumentCount);

            const int index = maxOutgoingArgumentCount - argument;
            return Pointer(Assembler::LocalsRegister, sizeof(QV4::Value) * (-index));
        }

        Pointer callDataAddress(int offset = 0) const {
            return Pointer(Assembler::LocalsRegister, offset - (sizeof(QV4::CallData) + sizeof(QV4::Value) * (maxOutgoingArgumentCount - 1)));
        }

        Address savedRegPointer(int offset) const
        {
            Q_ASSERT(offset >= 0);
            Q_ASSERT(offset < savedRegCount);

            const int off = offset * sizeof(QV4::Value);
            return Address(Assembler::StackFrameRegister, - calleeSavedRegisterSpace() - off);
        }

        int calleeSavedRegisterSpace() const
        {
            // plus 1 for the old FP
            return RegisterSize * (calleeSavedRegCount + 1);
        }

    private:
        int calleeSavedRegCount;

        /// arg count for calls to JS functions
        int maxOutgoingArgumentCount;

        /// the number of spill slots needed by this function
        int localCount;

        /// used by built-ins to save arguments (e.g. constants) to the stack when they need to be
        /// passed by reference.
        int savedRegCount;
    };

    class ConstantTable
    {
    public:
        ConstantTable(Assembler *as): _as(as) {}

        int add(const QV4::Primitive &v);
        ImplicitAddress loadValueAddress(IR::Const *c, RegisterID baseReg);
        ImplicitAddress loadValueAddress(const QV4::Primitive &v, RegisterID baseReg);
        void finalize(JSC::LinkBuffer &linkBuffer, InstructionSelection *isel);

    private:
        Assembler *_as;
        QVector<QV4::Primitive> _values;
        QVector<DataLabelPtr> _toPatch;
    };

    struct VoidType { VoidType() {} };
    static const VoidType Void;

    typedef JSC::FunctionPtr FunctionPtr;

    struct CallToLink {
        Call call;
        FunctionPtr externalFunction;
        const char* functionName;
    };
    struct PointerToValue {
        PointerToValue(IR::Expr *value)
            : value(value)
        {}
        IR::Expr *value;
    };
    struct PointerToString {
        explicit PointerToString(const QString &string) : string(string) {}
        QString string;
    };
    struct Reference {
        Reference(IR::Temp *value) : value(value) {}
        IR::Temp *value;
    };

    struct ReentryBlock {
        ReentryBlock(IR::BasicBlock *b) : block(b) {}
        IR::BasicBlock *block;
    };

    void callAbsolute(const char* functionName, FunctionPtr function) {
        CallToLink ctl;
        ctl.call = call();
        ctl.externalFunction = function;
        ctl.functionName = functionName;
        _callsToLink.append(ctl);
    }

    void callAbsolute(const char* /*functionName*/, Address addr) {
        call(addr);
    }

    void callAbsolute(const char* /*functionName*/, const RelativeCall &relativeCall)
    {
        call(relativeCall.addr);
    }

    void registerBlock(IR::BasicBlock*, IR::BasicBlock *nextBlock);
    IR::BasicBlock *nextBlock() const { return _nextBlock; }
    void jumpToBlock(IR::BasicBlock* current, IR::BasicBlock *target);
    void addPatch(IR::BasicBlock* targetBlock, Jump targetJump);
    void addPatch(DataLabelPtr patch, Label target);
    void addPatch(DataLabelPtr patch, IR::BasicBlock *target);
    void generateCJumpOnNonZero(RegisterID reg, IR::BasicBlock *currentBlock,
                             IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock);
    void generateCJumpOnCompare(RelationalCondition cond, RegisterID left, TrustedImm32 right,
                                IR::BasicBlock *currentBlock, IR::BasicBlock *trueBlock,
                                IR::BasicBlock *falseBlock);
    void generateCJumpOnCompare(RelationalCondition cond, RegisterID left, RegisterID right,
                                IR::BasicBlock *currentBlock, IR::BasicBlock *trueBlock,
                                IR::BasicBlock *falseBlock);
    Jump genTryDoubleConversion(IR::Expr *src, Assembler::FPRegisterID dest);
    Assembler::Jump branchDouble(bool invertCondition, IR::AluOp op, IR::Expr *left, IR::Expr *right);

    Pointer loadTempAddress(RegisterID baseReg, IR::Temp *t);
    Pointer loadStringAddress(RegisterID reg, const QString &string);
    void loadStringRef(RegisterID reg, const QString &string);
    Pointer stackSlotPointer(IR::Temp *t) const
    {
        Q_ASSERT(t->kind == IR::Temp::StackSlot);
        Q_ASSERT(t->scope == 0);

        return Pointer(_stackLayout.stackSlotPointer(t->index));
    }

    template <int argumentNumber>
    void saveOutRegister(PointerToValue arg)
    {
        if (!arg.value)
            return;
        if (IR::Temp *t = arg.value->asTemp()) {
            if (t->kind == IR::Temp::PhysicalRegister) {
                Pointer addr(_stackLayout.savedRegPointer(argumentNumber));
                switch (t->type) {
                case IR::BoolType:
                    storeBool((RegisterID) t->index, addr);
                    break;
                case IR::SInt32Type:
                    storeInt32((RegisterID) t->index, addr);
                    break;
                case IR::UInt32Type:
                    storeUInt32((RegisterID) t->index, addr);
                    break;
                case IR::DoubleType:
                    storeDouble((FPRegisterID) t->index, addr);
                    break;
                default:
                    Q_UNIMPLEMENTED();
                }
            }
        }
    }

    template <int, typename ArgType>
    void saveOutRegister(ArgType)
    {}

    void loadArgumentInRegister(RegisterID source, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        move(source, dest);
    }

    void loadArgumentInRegister(TrustedImmPtr ptr, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        move(TrustedImmPtr(ptr), dest);
    }

    void loadArgumentInRegister(const Pointer& ptr, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);
        addPtr(TrustedImm32(ptr.offset), ptr.base, dest);
    }

    void loadArgumentInRegister(PointerToValue temp, RegisterID dest, int argumentNumber)
    {
        if (!temp.value) {
            loadArgumentInRegister(TrustedImmPtr(0), dest, argumentNumber);
        } else {
            Pointer addr = toAddress(dest, temp.value, argumentNumber);
            loadArgumentInRegister(addr, dest, argumentNumber);
        }
    }
    void loadArgumentInRegister(PointerToString temp, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);
        loadStringRef(dest, temp.string);
    }

    void loadArgumentInRegister(Reference temp, RegisterID dest, int argumentNumber)
    {
        Q_ASSERT(temp.value);
        Pointer addr = loadTempAddress(dest, temp.value);
        loadArgumentInRegister(addr, dest, argumentNumber);
    }

    void loadArgumentInRegister(ReentryBlock block, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        Q_ASSERT(block.block);
        DataLabelPtr patch = moveWithPatch(TrustedImmPtr(0), dest);
        addPatch(patch, block.block);
    }

#ifdef VALUE_FITS_IN_REGISTER
    void loadArgumentInRegister(IR::Temp* temp, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        if (!temp) {
            QV4::Value undefined = QV4::Primitive::undefinedValue();
            move(TrustedImm64(undefined.val), dest);
        } else {
            Pointer addr = loadTempAddress(dest, temp);
            load64(addr, dest);
        }
    }

    void loadArgumentInRegister(IR::Const* c, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        QV4::Value v = convertToValue(c);
        move(TrustedImm64(v.val), dest);
    }

    void loadArgumentInRegister(IR::Expr* expr, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        if (!expr) {
            QV4::Value undefined = QV4::Primitive::undefinedValue();
            move(TrustedImm64(undefined.val), dest);
        } else if (expr->asTemp()){
            loadArgumentInRegister(expr->asTemp(), dest, argumentNumber);
        } else if (expr->asConst()) {
            loadArgumentInRegister(expr->asConst(), dest, argumentNumber);
        } else {
            Q_ASSERT(!"unimplemented expression type in loadArgument");
        }
    }
#else
    void loadArgumentInRegister(IR::Expr*, RegisterID)
    {
        Q_ASSERT(!"unimplemented: expression in loadArgument");
    }
#endif

    void loadArgumentInRegister(QV4::String* string, RegisterID dest, int argumentNumber)
    {
        loadArgumentInRegister(TrustedImmPtr(string), dest, argumentNumber);
    }

    void loadArgumentInRegister(TrustedImm32 imm32, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        xorPtr(dest, dest);
        if (imm32.m_value)
            move(imm32, dest);
    }

    void storeReturnValue(RegisterID dest)
    {
        move(ReturnValueRegister, dest);
    }

    void storeUInt32ReturnValue(RegisterID dest)
    {
        Pointer tmp(StackPointerRegister, -int(sizeof(QV4::Value)));
        storeReturnValue(tmp);
        toUInt32Register(tmp, dest);
    }

    void storeReturnValue(FPRegisterID dest)
    {
#ifdef VALUE_FITS_IN_REGISTER
        move(TrustedImm64(QV4::Value::NaNEncodeMask), ScratchRegister);
        xor64(ScratchRegister, ReturnValueRegister);
        move64ToDouble(ReturnValueRegister, dest);
#else
        Pointer tmp(StackPointerRegister, -int(sizeof(QV4::Value)));
        storeReturnValue(tmp);
        loadDouble(tmp, dest);
#endif
    }

#ifdef VALUE_FITS_IN_REGISTER
    void storeReturnValue(const Pointer &dest)
    {
        store64(ReturnValueRegister, dest);
    }
#elif defined(Q_PROCESSOR_X86)
    void storeReturnValue(const Pointer &dest)
    {
        Pointer destination = dest;
        store32(JSC::X86Registers::eax, destination);
        destination.offset += 4;
        store32(JSC::X86Registers::edx, destination);
    }
#elif defined(Q_PROCESSOR_ARM)
    void storeReturnValue(const Pointer &dest)
    {
        Pointer destination = dest;
        store32(JSC::ARMRegisters::r0, destination);
        destination.offset += 4;
        store32(JSC::ARMRegisters::r1, destination);
    }
#endif

    void storeReturnValue(IR::Temp *temp)
    {
        if (!temp)
            return;

        if (temp->kind == IR::Temp::PhysicalRegister) {
            if (temp->type == IR::DoubleType)
                storeReturnValue((FPRegisterID) temp->index);
            else if (temp->type == IR::UInt32Type)
                storeUInt32ReturnValue((RegisterID) temp->index);
            else
                storeReturnValue((RegisterID) temp->index);
        } else {
            Pointer addr = loadTempAddress(ScratchRegister, temp);
            storeReturnValue(addr);
        }
    }

    void storeReturnValue(VoidType)
    {
    }

    template <int StackSlot>
    void loadArgumentOnStack(RegisterID reg, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        poke(reg, StackSlot);
    }

    template <int StackSlot>
    void loadArgumentOnStack(TrustedImm32 value, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        poke(value, StackSlot);
    }

    template <int StackSlot>
    void loadArgumentOnStack(const Pointer& ptr, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        addPtr(TrustedImm32(ptr.offset), ptr.base, ScratchRegister);
        poke(ScratchRegister, StackSlot);
    }

    template <int StackSlot>
    void loadArgumentOnStack(PointerToValue temp, int argumentNumber)
    {
        if (temp.value) {
            Pointer ptr = toAddress(ScratchRegister, temp.value, argumentNumber);
            loadArgumentOnStack<StackSlot>(ptr, argumentNumber);
        } else {
            poke(TrustedImmPtr(0), StackSlot);
        }
    }

    template <int StackSlot>
    void loadArgumentOnStack(PointerToString temp, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);
        loadStringRef(ScratchRegister, temp.string);
        poke(ScratchRegister, StackSlot);
    }

    template <int StackSlot>
    void loadArgumentOnStack(Reference temp, int argumentNumber)
    {
        Q_ASSERT (temp.value);

        Pointer ptr = loadTempAddress(ScratchRegister, temp.value);
        loadArgumentOnStack<StackSlot>(ptr, argumentNumber);
    }

    template <int StackSlot>
    void loadArgumentOnStack(ReentryBlock block, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        Q_ASSERT(block.block);
        DataLabelPtr patch = moveWithPatch(TrustedImmPtr(0), ScratchRegister);
        poke(ScratchRegister, StackSlot);
        addPatch(patch, block.block);
    }

    template <int StackSlot>
    void loadArgumentOnStack(TrustedImmPtr ptr, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        move(TrustedImmPtr(ptr), ScratchRegister);
        poke(ScratchRegister, StackSlot);
    }

    template <int StackSlot>
    void loadArgumentOnStack(QV4::String* name, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        poke(TrustedImmPtr(name), StackSlot);
    }

    void loadDouble(IR::Temp* temp, FPRegisterID dest)
    {
        if (temp->kind == IR::Temp::PhysicalRegister) {
            moveDouble((FPRegisterID) temp->index, dest);
            return;
        }
        Pointer ptr = loadTempAddress(ScratchRegister, temp);
        loadDouble(ptr, dest);
    }

    void storeDouble(FPRegisterID source, IR::Temp* temp)
    {
        if (temp->kind == IR::Temp::PhysicalRegister) {
            moveDouble(source, (FPRegisterID) temp->index);
            return;
        }
#if QT_POINTER_SIZE == 8
        moveDoubleTo64(source, ReturnValueRegister);
        move(TrustedImm64(QV4::Value::NaNEncodeMask), ScratchRegister);
        xor64(ScratchRegister, ReturnValueRegister);
        Pointer ptr = loadTempAddress(ScratchRegister, temp);
        store64(ReturnValueRegister, ptr);
#else
        Pointer ptr = loadTempAddress(ScratchRegister, temp);
        storeDouble(source, ptr);
#endif
    }
#if QT_POINTER_SIZE == 8
    // We need to (de)mangle the double
    void loadDouble(Address addr, FPRegisterID dest)
    {
        load64(addr, ReturnValueRegister);
        move(TrustedImm64(QV4::Value::NaNEncodeMask), ScratchRegister);
        xor64(ScratchRegister, ReturnValueRegister);
        move64ToDouble(ReturnValueRegister, dest);
    }

    void storeDouble(FPRegisterID source, Address addr)
    {
        moveDoubleTo64(source, ReturnValueRegister);
        move(TrustedImm64(QV4::Value::NaNEncodeMask), ScratchRegister);
        xor64(ScratchRegister, ReturnValueRegister);
        store64(ReturnValueRegister, addr);
    }
#else
    using JSC::MacroAssembler::loadDouble;
    using JSC::MacroAssembler::storeDouble;
#endif

    template <typename Result, typename Source>
    void copyValue(Result result, Source source);
    template <typename Result>
    void copyValue(Result result, IR::Expr* source);

    // The scratch register is used to calculate the temp address for the source.
    void memcopyValue(Pointer target, IR::Temp *sourceTemp, RegisterID scratchRegister)
    {
        Q_ASSERT(sourceTemp->kind != IR::Temp::PhysicalRegister);
        Q_ASSERT(target.base != scratchRegister);
        JSC::MacroAssembler::loadDouble(loadTempAddress(scratchRegister, sourceTemp), FPGpr0);
        JSC::MacroAssembler::storeDouble(FPGpr0, target);
    }

    void storeValue(QV4::Primitive value, RegisterID destination)
    {
        Q_UNUSED(value);
        Q_UNUSED(destination);
        Q_UNREACHABLE();
    }

    void storeValue(QV4::Primitive value, Address destination)
    {
#ifdef VALUE_FITS_IN_REGISTER
        store64(TrustedImm64(value.val), destination);
#else
        store32(TrustedImm32(value.int_32), destination);
        destination.offset += 4;
        store32(TrustedImm32(value.tag), destination);
#endif
    }

    void storeValue(QV4::Primitive value, IR::Temp* temp);

    void enterStandardStackFrame();
    void leaveStandardStackFrame();

    void checkException() {
        loadPtr(Address(ContextRegister, qOffsetOf(QV4::ExecutionContext, engine)), ScratchRegister);
        load32(Address(ScratchRegister, qOffsetOf(QV4::ExecutionEngine, hasException)), ScratchRegister);
        Jump exceptionThrown = branch32(NotEqual, ScratchRegister, TrustedImm32(0));
        if (catchBlock)
            addPatch(catchBlock, exceptionThrown);
        else
            exceptionPropagationJumps.append(exceptionThrown);
    }
    void jumpToExceptionHandler() {
        Jump exceptionThrown = jump();
        if (catchBlock)
            addPatch(catchBlock, exceptionThrown);
        else
            exceptionPropagationJumps.append(exceptionThrown);
    }

    template <int argumentNumber, typename T>
    void loadArgumentOnStackOrRegister(const T &value)
    {
        if (argumentNumber < RegisterArgumentCount)
            loadArgumentInRegister(value, registerForArgument(argumentNumber), argumentNumber);
        else
#if OS(WINDOWS) && CPU(X86_64)
            loadArgumentOnStack<argumentNumber>(value, argumentNumber);
#else // Sanity:
            loadArgumentOnStack<argumentNumber - RegisterArgumentCount>(value, argumentNumber);
#endif
    }

    template <int argumentNumber>
    void loadArgumentOnStackOrRegister(const VoidType &value)
    {
        Q_UNUSED(value);
    }

    template <bool selectFirst, int First, int Second>
    struct Select
    {
        enum { Chosen = First };
    };

    template <int First, int Second>
    struct Select<false, First, Second>
    {
        enum { Chosen = Second };
    };

    template <int ArgumentIndex, typename Parameter>
    struct SizeOnStack
    {
        enum { Size = Select<ArgumentIndex >= RegisterArgumentCount, QT_POINTER_SIZE, 0>::Chosen };
    };

    template <int ArgumentIndex>
    struct SizeOnStack<ArgumentIndex, VoidType>
    {
        enum { Size = 0 };
    };

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
    void generateFunctionCallImp(ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
    {
        int stackSpaceNeeded =   SizeOnStack<0, Arg1>::Size
                               + SizeOnStack<1, Arg2>::Size
                               + SizeOnStack<2, Arg3>::Size
                               + SizeOnStack<3, Arg4>::Size
                               + SizeOnStack<4, Arg5>::Size
                               + SizeOnStack<5, Arg6>::Size
                               + StackShadowSpace;

        if (stackSpaceNeeded) {
            Q_ASSERT(stackSpaceNeeded < (INT_MAX - StackAlignment));
            stackSpaceNeeded = static_cast<int>(WTF::roundUpToMultipleOf(StackAlignment, stackSpaceNeeded));
            subPtr(TrustedImm32(stackSpaceNeeded), StackPointerRegister);
        }

        // First save any arguments that reside in registers, because they could be overwritten
        // if that register is also used to pass arguments.
        saveOutRegister<5>(arg6);
        saveOutRegister<4>(arg5);
        saveOutRegister<3>(arg4);
        saveOutRegister<2>(arg3);
        saveOutRegister<1>(arg2);
        saveOutRegister<0>(arg1);

        loadArgumentOnStackOrRegister<5>(arg6);
        loadArgumentOnStackOrRegister<4>(arg5);
        loadArgumentOnStackOrRegister<3>(arg4);
        loadArgumentOnStackOrRegister<2>(arg3);
        loadArgumentOnStackOrRegister<1>(arg2);

        prepareRelativeCall(function, this);
        loadArgumentOnStackOrRegister<0>(arg1);

#if (OS(LINUX) && CPU(X86) && (defined(__PIC__) || defined(__PIE__))) || \
    (OS(WINDOWS) && CPU(X86))
        load32(Address(StackFrameRegister, -int(sizeof(void*))),
               JSC::X86Registers::ebx); // restore the GOT ptr
#endif

        callAbsolute(functionName, function);

        if (stackSpaceNeeded)
            addPtr(TrustedImm32(stackSpaceNeeded), StackPointerRegister);

        if (ExceptionCheck<Callable>::NeedsCheck) {
            checkException();
        }

        storeReturnValue(r);

    }

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    void generateFunctionCallImp(ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    {
        generateFunctionCallImp(r, functionName, function, arg1, arg2, arg3, arg4, arg5, VoidType());
    }

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    void generateFunctionCallImp(ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        generateFunctionCallImp(r, functionName, function, arg1, arg2, arg3, arg4, VoidType());
    }

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2, typename Arg3>
    void generateFunctionCallImp(ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        generateFunctionCallImp(r, functionName, function, arg1, arg2, arg3, VoidType(), VoidType());
    }

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2>
    void generateFunctionCallImp(ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2)
    {
        generateFunctionCallImp(r, functionName, function, arg1, arg2, VoidType(), VoidType(), VoidType());
    }

    template <typename ArgRet, typename Callable, typename Arg1>
    void generateFunctionCallImp(ArgRet r, const char* functionName, Callable function, Arg1 arg1)
    {
        generateFunctionCallImp(r, functionName, function, arg1, VoidType(), VoidType(), VoidType(), VoidType());
    }

    Pointer toAddress(RegisterID tmpReg, IR::Expr *e, int offset)
    {
        if (IR::Const *c = e->asConst()) {
            Address addr = _stackLayout.savedRegPointer(offset);
            Address tagAddr = addr;
            tagAddr.offset += 4;

            QV4::Primitive v = convertToValue(c);
            store32(TrustedImm32(v.int_32), addr);
            store32(TrustedImm32(v.tag), tagAddr);
            return Pointer(addr);
        }

        IR::Temp *t = e->asTemp();
        Q_ASSERT(t);
        if (t->kind != IR::Temp::PhysicalRegister)
            return loadTempAddress(tmpReg, t);


        return Pointer(_stackLayout.savedRegPointer(offset));
    }

    void storeBool(RegisterID reg, Pointer addr)
    {
        store32(reg, addr);
        addr.offset += 4;
        store32(TrustedImm32(QV4::Primitive::fromBoolean(0).tag), addr);
    }

    void storeBool(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    void storeBool(RegisterID reg, IR::Temp *target)
    {
        if (target->kind == IR::Temp::PhysicalRegister) {
            move(reg, (RegisterID) target->index);
        } else {
            Pointer addr = loadTempAddress(ScratchRegister, target);
            storeBool(reg, addr);
        }
    }

    void storeBool(bool value, IR::Temp *target) {
        TrustedImm32 trustedValue(value ? 1 : 0);
        if (target->kind == IR::Temp::PhysicalRegister) {
            move(trustedValue, (RegisterID) target->index);
        } else {
            move(trustedValue, ScratchRegister);
            storeBool(ScratchRegister, target);
        }
    }

    void storeInt32(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    void storeInt32(RegisterID reg, Pointer addr)
    {
        store32(reg, addr);
        addr.offset += 4;
        store32(TrustedImm32(QV4::Primitive::fromInt32(0).tag), addr);
    }

    void storeInt32(RegisterID reg, IR::Temp *target)
    {
        if (target->kind == IR::Temp::PhysicalRegister) {
            move(reg, (RegisterID) target->index);
        } else {
            Pointer addr = loadTempAddress(ScratchRegister, target);
            storeInt32(reg, addr);
        }
    }

    void storeUInt32(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    void storeUInt32(RegisterID reg, Pointer addr)
    {
        // The UInt32 representation in QV4::Value is really convoluted. See also toUInt32Register.
        Jump intRange = branch32(GreaterThanOrEqual, reg, TrustedImm32(0));
        convertUInt32ToDouble(reg, FPGpr0, ReturnValueRegister);
        storeDouble(FPGpr0, addr);
        Jump done = jump();
        intRange.link(this);
        storeInt32(reg, addr);
        done.link(this);
    }

    void storeUInt32(RegisterID reg, IR::Temp *target)
    {
        if (target->kind == IR::Temp::PhysicalRegister) {
            move(reg, (RegisterID) target->index);
        } else {
            Pointer addr = loadTempAddress(ScratchRegister, target);
            storeUInt32(reg, addr);
        }
    }

    FPRegisterID toDoubleRegister(IR::Expr *e, FPRegisterID target = FPGpr0)
    {
        if (IR::Const *c = e->asConst()) {
#if QT_POINTER_SIZE == 8
            union {
                double d;
                int64_t i;
            } u;
            u.d = c->value;
            move(TrustedImm64(u.i), ReturnValueRegister);
            move64ToDouble(ReturnValueRegister, target);
#else
            JSC::MacroAssembler::loadDouble(constantTable().loadValueAddress(c, ScratchRegister), target);
#endif
            return target;
        }

        IR::Temp *t = e->asTemp();
        Q_ASSERT(t);
        if (t->kind == IR::Temp::PhysicalRegister)
            return (FPRegisterID) t->index;

        loadDouble(t, target);
        return target;
    }

    RegisterID toBoolRegister(IR::Expr *e, RegisterID scratchReg)
    {
        return toInt32Register(e, scratchReg);
    }

    RegisterID toInt32Register(IR::Expr *e, RegisterID scratchReg)
    {
        if (IR::Const *c = e->asConst()) {
            move(TrustedImm32(convertToValue(c).int_32), scratchReg);
            return scratchReg;
        }

        IR::Temp *t = e->asTemp();
        Q_ASSERT(t);
        if (t->kind == IR::Temp::PhysicalRegister)
            return (RegisterID) t->index;

        return toInt32Register(loadTempAddress(scratchReg, t), scratchReg);
    }

    RegisterID toInt32Register(Pointer addr, RegisterID scratchReg)
    {
        load32(addr, scratchReg);
        return scratchReg;
    }

    RegisterID toUInt32Register(IR::Expr *e, RegisterID scratchReg)
    {
        if (IR::Const *c = e->asConst()) {
            move(TrustedImm32(unsigned(c->value)), scratchReg);
            return scratchReg;
        }

        IR::Temp *t = e->asTemp();
        Q_ASSERT(t);
        if (t->kind == IR::Temp::PhysicalRegister)
            return (RegisterID) t->index;

        return toUInt32Register(loadTempAddress(scratchReg, t), scratchReg);
    }

    RegisterID toUInt32Register(Pointer addr, RegisterID scratchReg)
    {
        Q_ASSERT(addr.base != scratchReg);

        // The UInt32 representation in QV4::Value is really convoluted. See also storeUInt32.
        Pointer tagAddr = addr;
        tagAddr.offset += 4;
        load32(tagAddr, scratchReg);
        Jump inIntRange = branch32(Equal, scratchReg, TrustedImm32(QV4::Value::_Integer_Type));

        // it's not in signed int range, so load it as a double, and truncate it down
        loadDouble(addr, FPGpr0);
        static const double magic = double(INT_MAX) + 1;
        move(TrustedImmPtr(&magic), scratchReg);
        subDouble(Address(scratchReg, 0), FPGpr0);
        Jump canNeverHappen = branchTruncateDoubleToUint32(FPGpr0, scratchReg);
        canNeverHappen.link(this);
        or32(TrustedImm32(1 << 31), scratchReg);
        Jump done = jump();

        inIntRange.link(this);
        load32(addr, scratchReg);

        done.link(this);
        return scratchReg;
    }

    JSC::MacroAssemblerCodeRef link(int *codeSize);

    const StackLayout stackLayout() const { return _stackLayout; }
    ConstantTable &constantTable() { return _constTable; }

    Label exceptionReturnLabel;
    IR::BasicBlock * catchBlock;
    QVector<Jump> exceptionPropagationJumps;
private:
    const StackLayout _stackLayout;
    ConstantTable _constTable;
    IR::Function *_function;
    QHash<IR::BasicBlock *, Label> _addrs;
    QHash<IR::BasicBlock *, QVector<Jump> > _patches;
    QList<CallToLink> _callsToLink;

    struct DataLabelPatch {
        DataLabelPtr dataLabel;
        Label target;
    };
    QList<DataLabelPatch> _dataLabelPatches;

    QHash<IR::BasicBlock *, QVector<DataLabelPtr> > _labelPatches;
    IR::BasicBlock *_nextBlock;

    QV4::ExecutableAllocator *_executableAllocator;
    InstructionSelection *_isel;
};

template <typename Result, typename Source>
void Assembler::copyValue(Result result, Source source)
{
#ifdef VALUE_FITS_IN_REGISTER
    // Use ReturnValueRegister as "scratch" register because loadArgument
    // and storeArgument are functions that may need a scratch register themselves.
    loadArgumentInRegister(source, ReturnValueRegister, 0);
    storeReturnValue(result);
#else
    loadDouble(source, FPGpr0);
    storeDouble(FPGpr0, result);
#endif
}

template <typename Result>
void Assembler::copyValue(Result result, IR::Expr* source)
{
    if (source->type == IR::BoolType) {
        RegisterID reg = toInt32Register(source, ScratchRegister);
        storeBool(reg, result);
    } else if (source->type == IR::SInt32Type) {
        RegisterID reg = toInt32Register(source, ScratchRegister);
        storeInt32(reg, result);
    } else if (source->type == IR::UInt32Type) {
        RegisterID reg = toUInt32Register(source, ScratchRegister);
        storeUInt32(reg, result);
    } else if (source->type == IR::DoubleType) {
        storeDouble(toDoubleRegister(source), result);
    } else if (IR::Temp *temp = source->asTemp()) {
#ifdef VALUE_FITS_IN_REGISTER
        Q_UNUSED(temp);

        // Use ReturnValueRegister as "scratch" register because loadArgument
        // and storeArgument are functions that may need a scratch register themselves.
        loadArgumentInRegister(source, ReturnValueRegister, 0);
        storeReturnValue(result);
#else
        loadDouble(temp, FPGpr0);
        storeDouble(FPGpr0, result);
#endif
    } else if (IR::Const *c = source->asConst()) {
        QV4::Primitive v = convertToValue(c);
        storeValue(v, result);
    } else {
        Q_UNREACHABLE();
    }
}



template <typename T> inline void prepareRelativeCall(const T &, Assembler *){}
template <> inline void prepareRelativeCall(const RelativeCall &relativeCall, Assembler *as)
{
    as->loadPtr(Assembler::Address(Assembler::ContextRegister, qOffsetOf(QV4::ExecutionContext, lookups)),
                relativeCall.addr.base);
}

} // end of namespace JIT
} // end of namespace QV4

QT_END_NAMESPACE

#endif // ENABLE(ASSEMBLER)

#endif // QV4ISEL_MASM_P_H
