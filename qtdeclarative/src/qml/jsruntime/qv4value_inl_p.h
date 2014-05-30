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
#ifndef QV4VALUE_INL_H
#define QV4VALUE_INL_H

#include <cmath> // this HAS to come

#include <QtCore/QString>
#include <QtCore/qnumeric.h>
#include "qv4global_p.h"
#include "qv4string_p.h"
#include <QtCore/QDebug>
#include "qv4managed_p.h"
#include "qv4engine_p.h"
#include <private/qtqmlglobal_p.h>

//#include <wtf/MathExtras.h>

#include "qv4value_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

inline bool Value::isString() const
{
    if (!isManaged())
        return false;
    return managed() && managed()->internalClass->vtable->isString;
}
inline bool Value::isObject() const
{
    if (!isManaged())
        return false;
    return managed() && managed()->internalClass->vtable->isObject;
}

inline bool Value::isPrimitive() const
{
    return !isObject();
}

inline ExecutionEngine *Value::engine() const
{
    Managed *m = asManaged();
    return m ? m->engine() : 0;
}

inline void Value::mark(ExecutionEngine *e) const
{
    if (!val)
        return;
    Managed *m = asManaged();
    if (m)
        m->mark(e);
}

inline Primitive Primitive::nullValue()
{
    Primitive v;
#if QT_POINTER_SIZE == 8
    v.val = quint64(_Null_Type) << Tag_Shift;
#else
    v.tag = _Null_Type;
    v.int_32 = 0;
#endif
    return v;
}

inline Primitive Primitive::fromBoolean(bool b)
{
    Primitive v;
    v.tag = _Boolean_Type;
    v.int_32 = (bool)b;
    return v;
}

inline Primitive Primitive::fromDouble(double d)
{
    Primitive v;
    v.setDouble(d);
    return v;
}

inline Primitive Primitive::fromInt32(int i)
{
    Primitive v;
    v.tag = _Integer_Type;
    v.int_32 = i;
    return v;
}

inline Primitive Primitive::fromUInt32(uint i)
{
    Primitive v;
    if (i < INT_MAX) {
        v.tag = _Integer_Type;
        v.int_32 = (int)i;
    } else {
        v.setDouble(i);
    }
    return v;
}

inline double Value::toNumber() const
{
    if (isInteger())
        return int_32;
    if (isDouble())
        return doubleValue();
    return toNumberImpl();
}

inline int Value::toInt32() const
{
    if (isInteger())
        return int_32;
    double d = isNumber() ? doubleValue() : toNumberImpl();

    const double D32 = 4294967296.0;
    const double D31 = D32 / 2.0;

    if ((d >= -D31 && d < D31))
        return static_cast<int>(d);

    return Primitive::toInt32(d);
}

inline unsigned int Value::toUInt32() const
{
    return (unsigned int)toInt32();
}


inline bool Value::toBoolean() const
{
    switch (type()) {
    case Value::Undefined_Type:
    case Value::Null_Type:
        return false;
    case Value::Boolean_Type:
    case Value::Integer_Type:
        return (bool)int_32;
    case Value::Managed_Type:
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        if (isString())
            return stringValue()->toQString().length() > 0;
#endif
        return true;
    default: // double
        return doubleValue() && !std::isnan(doubleValue());
    }
}

#ifndef V4_BOOTSTRAP
inline uint Value::asArrayIndex() const
{
#if QT_POINTER_SIZE == 8
    if (!isNumber())
        return UINT_MAX;
    if (isInteger())
        return int_32 >= 0 ? (uint)int_32 : UINT_MAX;
#else
    if (isInteger() && int_32 >= 0)
        return (uint)int_32;
    if (!isDouble())
        return UINT_MAX;
#endif
    double d = doubleValue();
    uint idx = (uint)d;
    if (idx != d)
        return UINT_MAX;
    return idx;
}

inline uint Value::asArrayLength(bool *ok) const
{
    *ok = true;
    if (isInteger()) {
        if (int_32 >= 0) {
            return (uint)int_32;
        } else {
            *ok = false;
            return UINT_MAX;
        }
    }
    if (isNumber()) {
        double d = doubleValue();
        uint idx = (uint)d;
        if (idx != d) {
            *ok = false;
            return UINT_MAX;
        }
        return idx;
    }
    if (isString())
        return stringValue()->toUInt(ok);

    uint idx = toUInt32();
    double d = toNumber();
    if (d != idx) {
        *ok = false;
        return UINT_MAX;
    }
    return idx;
}

inline Object *Value::asObject() const
{
    return isObject() ? objectValue() : 0;
}

inline FunctionObject *Value::asFunctionObject() const
{
    return isObject() ? managed()->asFunctionObject() : 0;
}

inline NumberObject *Value::asNumberObject() const
{
    return isObject() ? managed()->asNumberObject() : 0;
}

inline StringObject *Value::asStringObject() const
{
    return isObject() ? managed()->asStringObject() : 0;
}

inline DateObject *Value::asDateObject() const
{
    return isObject() ? managed()->asDateObject() : 0;
}

inline ArrayObject *Value::asArrayObject() const
{
    return isObject() ? managed()->asArrayObject() : 0;
}

inline ErrorObject *Value::asErrorObject() const
{
    return isObject() ? managed()->asErrorObject() : 0;
}

template<typename T>
inline T *Value::as() const { Managed *m = isObject() ? managed() : 0; return m ? m->as<T>() : 0; }

#endif

} // namespace QV4

QT_END_NAMESPACE

#endif
