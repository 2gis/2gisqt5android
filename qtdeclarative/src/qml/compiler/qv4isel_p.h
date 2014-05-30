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

#ifndef QV4ISEL_P_H
#define QV4ISEL_P_H

#include "private/qv4global_p.h"
#include "qv4jsir_p.h"
#include <private/qv4compileddata_p.h>
#include <private/qv4compiler_p.h>

#include <qglobal.h>
#include <QHash>

QT_BEGIN_NAMESPACE

class QQmlEnginePrivate;

namespace QV4 {

class ExecutableAllocator;
struct Function;

class Q_QML_PRIVATE_EXPORT EvalInstructionSelection
{
public:
    EvalInstructionSelection(QV4::ExecutableAllocator *execAllocator, IR::Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator);
    virtual ~EvalInstructionSelection() = 0;

    QV4::CompiledData::CompilationUnit *compile(bool generateUnitData = true);

    void setUseFastLookups(bool b) { useFastLookups = b; }

    int registerString(const QString &str) { return jsGenerator->registerString(str); }
    uint registerIndexedGetterLookup() { return jsGenerator->registerIndexedGetterLookup(); }
    uint registerIndexedSetterLookup() { return jsGenerator->registerIndexedSetterLookup(); }
    uint registerGetterLookup(const QString &name) { return jsGenerator->registerGetterLookup(name); }
    uint registerSetterLookup(const QString &name) { return jsGenerator->registerSetterLookup(name); }
    uint registerGlobalGetterLookup(const QString &name) { return jsGenerator->registerGlobalGetterLookup(name); }
    int registerRegExp(IR::RegExp *regexp) { return jsGenerator->registerRegExp(regexp); }
    int registerJSClass(int count, IR::ExprList *args) { return jsGenerator->registerJSClass(count, args); }
    QV4::Compiler::JSUnitGenerator *jsUnitGenerator() const { return jsGenerator; }

protected:
    virtual void run(int functionIndex) = 0;
    virtual QV4::CompiledData::CompilationUnit *backendCompileStep() = 0;

    bool useFastLookups;
    QV4::ExecutableAllocator *executableAllocator;
    QV4::Compiler::JSUnitGenerator *jsGenerator;
    QScopedPointer<QV4::Compiler::JSUnitGenerator> ownJSGenerator;
    IR::Module *irModule;
};

class Q_QML_PRIVATE_EXPORT EvalISelFactory
{
public:
    virtual ~EvalISelFactory() = 0;
    virtual EvalInstructionSelection *create(QQmlEnginePrivate *qmlEngine, QV4::ExecutableAllocator *execAllocator, IR::Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator) = 0;
    virtual bool jitCompileRegexps() const = 0;
};

namespace IR {
class Q_QML_PRIVATE_EXPORT IRDecoder: protected IR::StmtVisitor
{
public:
    IRDecoder() : _function(0) {}
    virtual ~IRDecoder() = 0;

    virtual void visitPhi(IR::Phi *) {}

public: // visitor methods for StmtVisitor:
    virtual void visitMove(IR::Move *s);
    virtual void visitExp(IR::Exp *s);

public: // to implement by subclasses:
    virtual void callBuiltinInvalid(IR::Name *func, IR::ExprList *args, IR::Temp *result) = 0;
    virtual void callBuiltinTypeofMember(IR::Expr *base, const QString &name, IR::Temp *result) = 0;
    virtual void callBuiltinTypeofSubscript(IR::Expr *base, IR::Expr *index, IR::Temp *result) = 0;
    virtual void callBuiltinTypeofName(const QString &name, IR::Temp *result) = 0;
    virtual void callBuiltinTypeofValue(IR::Expr *value, IR::Temp *result) = 0;
    virtual void callBuiltinDeleteMember(IR::Temp *base, const QString &name, IR::Temp *result) = 0;
    virtual void callBuiltinDeleteSubscript(IR::Temp *base, IR::Expr *index, IR::Temp *result) = 0;
    virtual void callBuiltinDeleteName(const QString &name, IR::Temp *result) = 0;
    virtual void callBuiltinDeleteValue(IR::Temp *result) = 0;
    virtual void callBuiltinThrow(IR::Expr *arg) = 0;
    virtual void callBuiltinReThrow() = 0;
    virtual void callBuiltinUnwindException(IR::Temp *) = 0;
    virtual void callBuiltinPushCatchScope(const QString &exceptionName) = 0;
    virtual void callBuiltinForeachIteratorObject(IR::Expr *arg, IR::Temp *result) = 0;
    virtual void callBuiltinForeachNextPropertyname(IR::Temp *arg, IR::Temp *result) = 0;
    virtual void callBuiltinPushWithScope(IR::Temp *arg) = 0;
    virtual void callBuiltinPopScope() = 0;
    virtual void callBuiltinDeclareVar(bool deletable, const QString &name) = 0;
    virtual void callBuiltinDefineArray(IR::Temp *result, IR::ExprList *args) = 0;
    virtual void callBuiltinDefineObjectLiteral(IR::Temp *result, int keyValuePairCount, IR::ExprList *keyValuePairs, IR::ExprList *arrayEntries, bool needSparseArray) = 0;
    virtual void callBuiltinSetupArgumentObject(IR::Temp *result) = 0;
    virtual void callBuiltinConvertThisToObject() = 0;
    virtual void callValue(IR::Temp *value, IR::ExprList *args, IR::Temp *result) = 0;
    virtual void callProperty(IR::Expr *base, const QString &name, IR::ExprList *args, IR::Temp *result) = 0;
    virtual void callSubscript(IR::Expr *base, IR::Expr *index, IR::ExprList *args, IR::Temp *result) = 0;
    virtual void convertType(IR::Temp *source, IR::Temp *target) = 0;
    virtual void constructActivationProperty(IR::Name *func, IR::ExprList *args, IR::Temp *result) = 0;
    virtual void constructProperty(IR::Temp *base, const QString &name, IR::ExprList *args, IR::Temp *result) = 0;
    virtual void constructValue(IR::Temp *value, IR::ExprList *args, IR::Temp *result) = 0;
    virtual void loadThisObject(IR::Temp *temp) = 0;
    virtual void loadQmlIdArray(IR::Temp *temp) = 0;
    virtual void loadQmlImportedScripts(IR::Temp *temp) = 0;
    virtual void loadQmlContextObject(IR::Temp *temp) = 0;
    virtual void loadQmlScopeObject(IR::Temp *temp) = 0;
    virtual void loadQmlSingleton(const QString &name, IR::Temp *temp) = 0;
    virtual void loadConst(IR::Const *sourceConst, IR::Temp *targetTemp) = 0;
    virtual void loadString(const QString &str, IR::Temp *targetTemp) = 0;
    virtual void loadRegexp(IR::RegExp *sourceRegexp, IR::Temp *targetTemp) = 0;
    virtual void getActivationProperty(const IR::Name *name, IR::Temp *temp) = 0;
    virtual void setActivationProperty(IR::Expr *source, const QString &targetName) = 0;
    virtual void initClosure(IR::Closure *closure, IR::Temp *target) = 0;
    virtual void getProperty(IR::Expr *base, const QString &name, IR::Temp *target) = 0;
    virtual void getQObjectProperty(IR::Expr *base, int propertyIndex, bool captureRequired, int attachedPropertiesId, IR::Temp *targetTemp) = 0;
    virtual void setProperty(IR::Expr *source, IR::Expr *targetBase, const QString &targetName) = 0;
    virtual void setQObjectProperty(IR::Expr *source, IR::Expr *targetBase, int propertyIndex) = 0;
    virtual void getElement(IR::Expr *base, IR::Expr *index, IR::Temp *target) = 0;
    virtual void setElement(IR::Expr *source, IR::Expr *targetBase, IR::Expr *targetIndex) = 0;
    virtual void copyValue(IR::Temp *sourceTemp, IR::Temp *targetTemp) = 0;
    virtual void swapValues(IR::Temp *sourceTemp, IR::Temp *targetTemp) = 0;
    virtual void unop(IR::AluOp oper, IR::Temp *sourceTemp, IR::Temp *targetTemp) = 0;
    virtual void binop(IR::AluOp oper, IR::Expr *leftSource, IR::Expr *rightSource, IR::Temp *target) = 0;

protected:
    virtual void callBuiltin(IR::Call *c, IR::Temp *result);

    IR::Function *_function; // subclass needs to set
};
} // namespace IR

} // namespace QV4

QT_END_NAMESPACE

#endif // QV4ISEL_P_H
