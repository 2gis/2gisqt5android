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
#ifndef QV4REGEXPOBJECT_H
#define QV4REGEXPOBJECT_H

#include "qv4runtime_p.h"
#include "qv4engine_p.h"
#include "qv4context_p.h"
#include "qv4functionobject_p.h"
#include "qv4string_p.h"
#include "qv4codegen_p.h"
#include "qv4isel_p.h"
#include "qv4managed_p.h"
#include "qv4property_p.h"
#include "qv4objectiterator_p.h"
#include "qv4regexp_p.h"

#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QScopedPointer>
#include <cstdio>
#include <cassert>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct RegExpObject: Object {
    struct Data : Object::Data {
        Data(ExecutionEngine *engine, RegExp *value, bool global);
        Data(ExecutionEngine *engine, const QRegExp &re);
        Data(InternalClass *ic);

        RegExp *value;
        bool global;
    };
    V4_OBJECT(Object)
    Q_MANAGED_TYPE(RegExpObject)

    // needs to be compatible with the flags in qv4jsir_p.h
    enum Flags {
        RegExp_Global     = 0x01,
        RegExp_IgnoreCase = 0x02,
        RegExp_Multiline  = 0x04
    };

    enum {
        Index_ArrayIndex = ArrayObject::LengthPropertyIndex + 1,
        Index_ArrayInput = Index_ArrayIndex + 1
    };

    RegExp *value() const { return d()->value; }
    bool global() const { return d()->global; }

    void init(ExecutionEngine *engine);

    Property *lastIndexProperty(ExecutionContext *ctx);
    QRegExp toQRegExp() const;
    QString toString() const;
    QString source() const;
    uint flags() const;

protected:
    static void markObjects(Managed *that, ExecutionEngine *e);
};

struct RegExpCtor: FunctionObject
{
    struct Data : FunctionObject::Data {
        Data(ExecutionContext *scope);
        Value lastMatch;
        StringValue lastInput;
        int lastMatchStart;
        int lastMatchEnd;
        void clearLastMatch();
    };
    V4_OBJECT(FunctionObject)

    Value lastMatch() { return d()->lastMatch; }
    StringValue lastInput() { return d()->lastInput; }
    int lastMatchStart() { return d()->lastMatchStart; }
    int lastMatchEnd() { return d()->lastMatchEnd; }

    static ReturnedValue construct(Managed *m, CallData *callData);
    static ReturnedValue call(Managed *that, CallData *callData);
    static void markObjects(Managed *that, ExecutionEngine *e);
};

struct RegExpPrototype: RegExpObject
{
    struct Data : RegExpObject::Data
    {
        Data(InternalClass *ic): RegExpObject::Data(ic)
        {
            setVTable(staticVTable());
        }
    };
    V4_OBJECT(RegExpObject)

    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_exec(CallContext *ctx);
    static ReturnedValue method_test(CallContext *ctx);
    static ReturnedValue method_toString(CallContext *ctx);
    static ReturnedValue method_compile(CallContext *ctx);

    template <int index>
    static ReturnedValue method_get_lastMatch_n(CallContext *ctx);
    static ReturnedValue method_get_lastParen(CallContext *ctx);
    static ReturnedValue method_get_input(CallContext *ctx);
    static ReturnedValue method_get_leftContext(CallContext *ctx);
    static ReturnedValue method_get_rightContext(CallContext *ctx);
};

}

QT_END_NAMESPACE

#endif // QMLJS_OBJECTS_H
