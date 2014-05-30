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
#ifndef QV4DATEOBJECT_P_H
#define QV4DATEOBJECT_P_H

#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include <QtCore/qnumeric.h>

QT_BEGIN_NAMESPACE

class QDateTime;

namespace QV4 {

struct DateObject: Object {
    V4_OBJECT
    Q_MANAGED_TYPE(DateObject)
    Value value;
    DateObject(ExecutionEngine *engine, const ValueRef date): Object(engine->dateClass) {
        value = date;
    }
    DateObject(ExecutionEngine *engine, const QDateTime &value);

    QDateTime toQDateTime() const;

protected:
    DateObject(InternalClass *ic): Object(ic) {
        Q_ASSERT(internalClass->vtable == staticVTable());
        value = Primitive::fromDouble(qSNaN());
    }
};

struct DateCtor: FunctionObject
{
    V4_OBJECT
    DateCtor(ExecutionContext *scope);

    static ReturnedValue construct(Managed *, CallData *callData);
    static ReturnedValue call(Managed *that, CallData *);
};

struct DatePrototype: DateObject
{
    DatePrototype(InternalClass *ic): DateObject(ic) {}
    void init(ExecutionEngine *engine, ObjectRef ctor);

    static double getThisDate(ExecutionContext *ctx);

    static ReturnedValue method_parse(CallContext *ctx);
    static ReturnedValue method_UTC(CallContext *ctx);
    static ReturnedValue method_now(CallContext *ctx);

    static ReturnedValue method_toString(CallContext *ctx);
    static ReturnedValue method_toDateString(CallContext *ctx);
    static ReturnedValue method_toTimeString(CallContext *ctx);
    static ReturnedValue method_toLocaleString(CallContext *ctx);
    static ReturnedValue method_toLocaleDateString(CallContext *ctx);
    static ReturnedValue method_toLocaleTimeString(CallContext *ctx);
    static ReturnedValue method_valueOf(CallContext *ctx);
    static ReturnedValue method_getTime(CallContext *ctx);
    static ReturnedValue method_getYear(CallContext *ctx);
    static ReturnedValue method_getFullYear(CallContext *ctx);
    static ReturnedValue method_getUTCFullYear(CallContext *ctx);
    static ReturnedValue method_getMonth(CallContext *ctx);
    static ReturnedValue method_getUTCMonth(CallContext *ctx);
    static ReturnedValue method_getDate(CallContext *ctx);
    static ReturnedValue method_getUTCDate(CallContext *ctx);
    static ReturnedValue method_getDay(CallContext *ctx);
    static ReturnedValue method_getUTCDay(CallContext *ctx);
    static ReturnedValue method_getHours(CallContext *ctx);
    static ReturnedValue method_getUTCHours(CallContext *ctx);
    static ReturnedValue method_getMinutes(CallContext *ctx);
    static ReturnedValue method_getUTCMinutes(CallContext *ctx);
    static ReturnedValue method_getSeconds(CallContext *ctx);
    static ReturnedValue method_getUTCSeconds(CallContext *ctx);
    static ReturnedValue method_getMilliseconds(CallContext *ctx);
    static ReturnedValue method_getUTCMilliseconds(CallContext *ctx);
    static ReturnedValue method_getTimezoneOffset(CallContext *ctx);
    static ReturnedValue method_setTime(CallContext *ctx);
    static ReturnedValue method_setMilliseconds(CallContext *ctx);
    static ReturnedValue method_setUTCMilliseconds(CallContext *ctx);
    static ReturnedValue method_setSeconds(CallContext *ctx);
    static ReturnedValue method_setUTCSeconds(CallContext *ctx);
    static ReturnedValue method_setMinutes(CallContext *ctx);
    static ReturnedValue method_setUTCMinutes(CallContext *ctx);
    static ReturnedValue method_setHours(CallContext *ctx);
    static ReturnedValue method_setUTCHours(CallContext *ctx);
    static ReturnedValue method_setDate(CallContext *ctx);
    static ReturnedValue method_setUTCDate(CallContext *ctx);
    static ReturnedValue method_setMonth(CallContext *ctx);
    static ReturnedValue method_setUTCMonth(CallContext *ctx);
    static ReturnedValue method_setYear(CallContext *ctx);
    static ReturnedValue method_setFullYear(CallContext *ctx);
    static ReturnedValue method_setUTCFullYear(CallContext *ctx);
    static ReturnedValue method_toUTCString(CallContext *ctx);
    static ReturnedValue method_toISOString(CallContext *ctx);
    static ReturnedValue method_toJSON(CallContext *ctx);

    static void timezoneUpdated();
};

}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
