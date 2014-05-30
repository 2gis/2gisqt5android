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
#ifndef QMLJS_MANAGED_H
#define QMLJS_MANAGED_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QDebug>
#include "qv4global_p.h"
#include "qv4value_p.h"
#include "qv4internalclass_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

#define Q_MANAGED_CHECK \
    template <typename T> inline void qt_check_for_QMANAGED_macro(const T *_q_argument) const \
    { int i = qYouForgotTheQ_MANAGED_Macro(this, _q_argument); i = i + 1; }

template <typename T>
inline int qYouForgotTheQ_MANAGED_Macro(T, T) { return 0; }

template <typename T1, typename T2>
inline void qYouForgotTheQ_MANAGED_Macro(T1, T2) {}

#define V4_MANAGED \
    public: \
        Q_MANAGED_CHECK \
        static const QV4::ManagedVTable static_vtbl; \
        static inline const QV4::ManagedVTable *staticVTable() { return &static_vtbl; } \
        template <typename T> \
        QV4::Returned<T> *asReturned() { return QV4::Returned<T>::create(this); } \

#define V4_OBJECT \
    public: \
        Q_MANAGED_CHECK \
        static const QV4::ObjectVTable static_vtbl; \
        static inline const QV4::ManagedVTable *staticVTable() { return &static_vtbl.managedVTable; } \
        template <typename T> \
        QV4::Returned<T> *asReturned() { return QV4::Returned<T>::create(this); } \

#define Q_MANAGED_TYPE(type) \
    public: \
        enum { MyType = Type_##type };

struct GCDeletable
{
    GCDeletable() : next(0), lastCall(false) {}
    virtual ~GCDeletable() {}
    GCDeletable *next;
    bool lastCall;
};

struct ManagedVTable
{
    uint isExecutionContext : 1;
    uint isString : 1;
    uint isObject : 1;
    uint isFunctionObject : 1;
    uint isErrorObject : 1;
    uint isArrayData : 1;
    uint unused : 18;
    uint type : 8;
    const char *className;
    void (*destroy)(Managed *);
    void (*markObjects)(Managed *, ExecutionEngine *e);
    bool (*isEqualTo)(Managed *m, Managed *other);
};

struct ObjectVTable
{
    ManagedVTable managedVTable;
    ReturnedValue (*call)(Managed *, CallData *data);
    ReturnedValue (*construct)(Managed *, CallData *data);
    ReturnedValue (*get)(Managed *, const StringRef name, bool *hasProperty);
    ReturnedValue (*getIndexed)(Managed *, uint index, bool *hasProperty);
    void (*put)(Managed *, const StringRef name, const ValueRef value);
    void (*putIndexed)(Managed *, uint index, const ValueRef value);
    PropertyAttributes (*query)(const Managed *, StringRef name);
    PropertyAttributes (*queryIndexed)(const Managed *, uint index);
    bool (*deleteProperty)(Managed *m, const StringRef name);
    bool (*deleteIndexedProperty)(Managed *m, uint index);
    ReturnedValue (*getLookup)(Managed *m, Lookup *l);
    void (*setLookup)(Managed *m, Lookup *l, const ValueRef v);
    uint (*getLength)(const Managed *m);
    void (*advanceIterator)(Managed *m, ObjectIterator *it, StringRef name, uint *index, Property *p, PropertyAttributes *attributes);
};

#define DEFINE_MANAGED_VTABLE_INT(classname) \
{     \
    classname::IsExecutionContext,   \
    classname::IsString,   \
    classname::IsObject,   \
    classname::IsFunctionObject,   \
    classname::IsErrorObject,   \
    classname::IsArrayData,   \
    0,                                          \
    classname::MyType,                          \
    #classname,                                 \
    destroy,                                    \
    markObjects,                                \
    isEqualTo                                  \
}


#define DEFINE_MANAGED_VTABLE(classname) \
const QV4::ManagedVTable classname::static_vtbl = DEFINE_MANAGED_VTABLE_INT(classname)


#define DEFINE_OBJECT_VTABLE(classname) \
const QV4::ObjectVTable classname::static_vtbl =    \
{     \
    DEFINE_MANAGED_VTABLE_INT(classname), \
    call,                                       \
    construct,                                  \
    get,                                        \
    getIndexed,                                 \
    put,                                        \
    putIndexed,                                 \
    query,                                      \
    queryIndexed,                               \
    deleteProperty,                             \
    deleteIndexedProperty,                      \
    getLookup,                                  \
    setLookup,                                  \
    getLength,                                  \
    advanceIterator                            \
}

#define DEFINE_MANAGED_VTABLE_WITH_NAME(classname, name) \
const QV4::ObjectVTable classname::static_vtbl =    \
{                                               \
    DEFINE_MANAGED_VTABLE_INT(name), \
    call,                                       \
    construct,                                  \
    get,                                        \
    getIndexed,                                 \
    put,                                        \
    putIndexed,                                 \
    query,                                      \
    queryIndexed,                               \
    deleteProperty,                             \
    deleteIndexedProperty,                      \
    getLookup,                                  \
    setLookup,                                  \
    getLength,                                  \
    advanceIterator                            \
}


struct Q_QML_PRIVATE_EXPORT Managed
{
    V4_MANAGED
    enum {
        IsExecutionContext = false,
        IsString = false,
        IsObject = false,
        IsFunctionObject = false,
        IsErrorObject = false,
        IsArrayData = false
    };
private:
    void *operator new(size_t);
    Managed(const Managed &other);
    void operator = (const Managed &other);

protected:
    Managed(InternalClass *internal)
        : internalClass(internal), _data(0)
    {
        Q_ASSERT(internalClass && internalClass->vtable);
        inUse = 1; extensible = 1;
    }

public:
    void *operator new(size_t size, MemoryManager *mm);
    void *operator new(size_t, Managed *m) { return m; }
    void operator delete(void *ptr);
    void operator delete(void *ptr, MemoryManager *mm);

    inline void mark(QV4::ExecutionEngine *engine);

    enum Type {
        Type_Invalid,
        Type_String,
        Type_Object,
        Type_ArrayObject,
        Type_FunctionObject,
        Type_BooleanObject,
        Type_NumberObject,
        Type_StringObject,
        Type_DateObject,
        Type_RegExpObject,
        Type_ErrorObject,
        Type_ArgumentsObject,
        Type_JsonObject,
        Type_MathObject,

        Type_ExecutionContext,
        Type_ForeachIteratorObject,
        Type_RegExp,

        Type_QmlSequence
    };
    Q_MANAGED_TYPE(Invalid)

    ExecutionEngine *engine() const;

    template <typename T>
    T *as() {
        // ### FIXME:
        if (!this || !internalClass)
            return 0;
#if !defined(QT_NO_QOBJECT_CHECK)
        static_cast<T *>(this)->qt_check_for_QMANAGED_macro(static_cast<T *>(this));
#endif
        return internalClass->vtable == T::staticVTable() ? static_cast<T *>(this) : 0;
    }
    template <typename T>
    const T *as() const {
        // ### FIXME:
        if (!this)
            return 0;
#if !defined(QT_NO_QOBJECT_CHECK)
        static_cast<T *>(this)->qt_check_for_QMANAGED_macro(static_cast<T *>(const_cast<Managed *>(this)));
#endif
        return internalClass->vtable == T::staticVTable() ? static_cast<const T *>(this) : 0;
    }

    String *asString() { return internalClass->vtable->isString ? reinterpret_cast<String *>(this) : 0; }
    Object *asObject() { return internalClass->vtable->isObject ? reinterpret_cast<Object *>(this) : 0; }
    ArrayObject *asArrayObject() { return internalClass->vtable->type == Type_ArrayObject ? reinterpret_cast<ArrayObject *>(this) : 0; }
    FunctionObject *asFunctionObject() { return internalClass->vtable->isFunctionObject ? reinterpret_cast<FunctionObject *>(this) : 0; }
    BooleanObject *asBooleanObject() { return internalClass->vtable->type == Type_BooleanObject ? reinterpret_cast<BooleanObject *>(this) : 0; }
    NumberObject *asNumberObject() { return internalClass->vtable->type == Type_NumberObject ? reinterpret_cast<NumberObject *>(this) : 0; }
    StringObject *asStringObject() { return internalClass->vtable->type == Type_StringObject ? reinterpret_cast<StringObject *>(this) : 0; }
    DateObject *asDateObject() { return internalClass->vtable->type == Type_DateObject ? reinterpret_cast<DateObject *>(this) : 0; }
    ErrorObject *asErrorObject() { return internalClass->vtable->isErrorObject ? reinterpret_cast<ErrorObject *>(this) : 0; }
    ArgumentsObject *asArgumentsObject() { return internalClass->vtable->type == Type_ArgumentsObject ? reinterpret_cast<ArgumentsObject *>(this) : 0; }

    bool isListType() const { return internalClass->vtable->type == Type_QmlSequence; }

    bool isArrayObject() const { return internalClass->vtable->type == Type_ArrayObject; }
    bool isStringObject() const { return internalClass->vtable->type == Type_StringObject; }

    QString className() const;

    Managed **nextFreeRef() {
        return reinterpret_cast<Managed **>(this);
    }
    Managed *nextFree() {
        return *reinterpret_cast<Managed **>(this);
    }
    void setNextFree(Managed *m) {
        *reinterpret_cast<Managed **>(this) = m;
    }

    void setVTable(const ManagedVTable *vt);

    bool isEqualTo(Managed *other)
    { return internalClass->vtable->isEqualTo(this, other); }

    static void destroy(Managed *that) { that->_data = 0; }
    static bool isEqualTo(Managed *m, Managed *other);

    ReturnedValue asReturnedValue() { return Value::fromManaged(this).asReturnedValue(); }


    InternalClass *internalClass;

    union {
        uint _data;
        struct {
            uchar markBit :  1;
            uchar inUse   :  1;
            uchar extensible : 1; // used by Object
            uchar _unused : 1;
            uchar needsActivation : 1; // used by FunctionObject
            uchar strictMode : 1; // used by FunctionObject
            uchar bindingKeyFlag : 1;
            uchar hasAccessorProperty : 1;
            uchar _type;
            mutable uchar subtype;
            uchar _flags;
        };
    };

private:
    friend class MemoryManager;
    friend struct Identifiers;
    friend struct ObjectIterator;
};


template<>
inline Managed *value_cast(const Value &v) {
    return v.asManaged();
}

template<typename T>
inline T *managed_cast(Managed *m)
{
    return m->as<T>();
}

template<>
inline String *managed_cast(Managed *m)
{
    return m->asString();
}
template<>
inline Object *managed_cast(Managed *m)
{
    return m->asObject();
}
template<>
inline FunctionObject *managed_cast(Managed *m)
{
    return m->asFunctionObject();
}


Value *extractValuePointer(const ScopedValue &);
template<typename T>
Value *extractValuePointer(const Scoped<T> &);

#define DEFINE_REF_METHODS(Class, Base) \
    Class##Ref(const QV4::ScopedValue &v) \
    { QV4::Value *val = extractValuePointer(v); ptr = QV4::value_cast<Class>(*val) ? val : 0; } \
    Class##Ref(const QV4::Scoped<Class> &v) { ptr = extractValuePointer(v); } \
    Class##Ref(QV4::TypedValue<Class> &v) { ptr = &v; } \
    Class##Ref(QV4::Value &v) { ptr = QV4::value_cast<Class>(v) ? &v : 0; } \
    Class##Ref &operator=(Class *t) { \
        if (sizeof(void *) == 4) \
            ptr->tag = QV4::Value::Managed_Type; \
        ptr->m = t; \
        return *this; \
    } \
    Class##Ref &operator=(QV4::Returned<Class> *t) { \
        if (sizeof(void *) == 4) \
            ptr->tag = QV4::Value::Managed_Type; \
        ptr->m = t->getPointer(); \
        return *this; \
    } \
    operator const Class *() const { return ptr ? static_cast<Class*>(ptr->managed()) : 0; } \
    const Class *operator->() const { return static_cast<Class*>(ptr->managed()); } \
    operator Class *() { return ptr ? static_cast<Class*>(ptr->managed()) : 0; } \
    Class *operator->() { return static_cast<Class*>(ptr->managed()); } \
    Class *getPointer() const { return static_cast<Class *>(ptr->managed()); } \
    operator QV4::Returned<Class> *() const { return ptr ? QV4::Returned<Class>::create(getPointer()) : 0; } \
    static Class##Ref null() { Class##Ref c; c.ptr = 0; return c; } \
protected: \
    Class##Ref() {} \
public: \

#define DEFINE_REF(Class, Base) \
struct Class##Ref : public Base##Ref \
{ DEFINE_REF_METHODS(Class, Base) } \


struct ManagedRef {
    // Important: Do NOT add a copy constructor to this class or any derived class
    // adding a copy constructor actually changes the calling convention, ie.
    // is not even binary compatible. Adding it would break assumptions made
    // in the jit'ed code.
    DEFINE_REF_METHODS(Managed, Managed);

    bool operator==(const ManagedRef &other) {
        if (ptr == other.ptr)
            return true;
        return ptr && other.ptr && ptr->m == other.ptr->m;
    }
    bool operator!=(const ManagedRef &other) {
        return !operator==(other);
    }
    bool operator!() const { return !ptr || !ptr->managed(); }

    bool isNull() const { return !ptr; }
    ReturnedValue asReturnedValue() const { return ptr ? ptr->val : Primitive::undefinedValue().asReturnedValue(); }

public:
    Value *ptr;
};

}


QT_END_NAMESPACE

#endif
