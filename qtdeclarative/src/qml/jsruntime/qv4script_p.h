/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QV4SCRIPT_H
#define QV4SCRIPT_H

#include "qv4global_p.h"
#include "qv4engine_p.h"
#include "qv4functionobject_p.h"

#include <QQmlError>

QT_BEGIN_NAMESPACE

class QQmlContextData;

namespace QV4 {

struct ExecutionContext;

struct ContextStateSaver {
    ExecutionContext *savedContext;
    bool strictMode;
    Lookup *lookups;
    CompiledData::CompilationUnit *compilationUnit;
    int lineNumber;

    ContextStateSaver(ExecutionContext *context)
        : savedContext(context)
        , strictMode(context->d()->strictMode)
        , lookups(context->d()->lookups)
        , compilationUnit(context->d()->compilationUnit)
        , lineNumber(context->d()->lineNumber)
    {}

    ~ContextStateSaver()
    {
        savedContext->d()->strictMode = strictMode;
        savedContext->d()->lookups = lookups;
        savedContext->d()->compilationUnit = compilationUnit;
        savedContext->d()->lineNumber = lineNumber;
    }
};

struct Q_QML_EXPORT QmlBindingWrapper : FunctionObject {
    struct Data : FunctionObject::Data {
        Data(ExecutionContext *scope, Function *f, Object *qml);
        // Constructor for QML functions and signal handlers, resulting binding wrapper is not callable!
        Data(ExecutionContext *scope, Object *qml);
        Object *qml;
        CallContext *qmlContext;
    };
    V4_OBJECT(FunctionObject)

    static ReturnedValue call(Managed *that, CallData *);
    static void markObjects(Managed *m, ExecutionEngine *e);

    CallContext *context() const { return d()->qmlContext; }

    static Returned<FunctionObject> *createQmlCallableForFunction(QQmlContextData *qmlContext, QObject *scopeObject, QV4::Function *runtimeFunction,
                                                                  const QList<QByteArray> &signalParameters = QList<QByteArray>(), QString *error = 0);

private:
};

struct Q_QML_EXPORT Script {
    Script(ExecutionContext *scope, const QString &sourceCode, const QString &source = QString(), int line = 1, int column = 0)
        : sourceFile(source), line(line), column(column), sourceCode(sourceCode)
        , scope(scope), strictMode(false), inheritContext(false), parsed(false)
        , vmFunction(0), parseAsBinding(false) {}
    Script(ExecutionEngine *engine, Object *qml, const QString &sourceCode, const QString &source = QString(), int line = 1, int column = 0)
        : sourceFile(source), line(line), column(column), sourceCode(sourceCode)
        , scope(engine->rootContext), strictMode(false), inheritContext(true), parsed(false)
        , qml(qml->asReturnedValue()), vmFunction(0), parseAsBinding(true) {}
    Script(ExecutionEngine *engine, Object *qml, CompiledData::CompilationUnit *compilationUnit);
    ~Script();
    QString sourceFile;
    int line;
    int column;
    QString sourceCode;
    ExecutionContext *scope;
    bool strictMode;
    bool inheritContext;
    bool parsed;
    QV4::PersistentValue qml;
    QV4::PersistentValue compilationUnitHolder;
    Function *vmFunction;
    bool parseAsBinding;

    void parse();
    ReturnedValue run();
    ReturnedValue qmlBinding();

    Function *function();

    static QQmlRefPointer<CompiledData::CompilationUnit> precompile(IR::Module *module, Compiler::JSUnitGenerator *unitGenerator, ExecutionEngine *engine, const QUrl &url, const QString &source, QList<QQmlError> *reportedErrors = 0);

    static ReturnedValue evaluate(ExecutionEngine *engine, const QString &script, Object *scopeObject);
};

}

QT_END_NAMESPACE

#endif
