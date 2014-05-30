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
#ifndef QV4INTERNALCLASS_H
#define QV4INTERNALCLASS_H

#include <QHash>
#include <QVector>
#include "qv4global_p.h"
#include <private/qqmljsmemorypool_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct String;
struct ExecutionEngine;
struct Object;
struct Identifier;
struct ManagedVTable;

struct PropertyHashData;
struct PropertyHash
{
    struct Entry {
        const Identifier *identifier;
        uint index;
    };

    PropertyHashData *d;

    inline PropertyHash();
    inline PropertyHash(const PropertyHash &other);
    inline ~PropertyHash();

    void addEntry(const Entry &entry, int classSize);
    uint lookup(const Identifier *identifier) const;

private:
    PropertyHash &operator=(const PropertyHash &other);
};

struct PropertyHashData
{
    PropertyHashData(int numBits);
    ~PropertyHashData() {
        free(entries);
    }

    int refCount;
    int alloc;
    int size;
    int numBits;
    PropertyHash::Entry *entries;
};

inline PropertyHash::PropertyHash()
{
    d = new PropertyHashData(3);
}

inline PropertyHash::PropertyHash(const PropertyHash &other)
{
    d = other.d;
    ++d->refCount;
}

inline PropertyHash::~PropertyHash()
{
    if (!--d->refCount)
        delete d;
}


template <typename T>
struct SharedInternalClassData {
    struct Private {
        Private(int alloc)
            : refcount(1),
              alloc(alloc),
              size(0)
        { data = new T  [alloc]; }
        ~Private() { delete [] data; }

        int refcount;
        uint alloc;
        uint size;
        T *data;
    };
    Private *d;

    inline SharedInternalClassData() {
        d = new Private(8);
    }

    inline SharedInternalClassData(const SharedInternalClassData &other)
        : d(other.d)
    {
        ++d->refcount;
    }
    inline ~SharedInternalClassData() {
        if (!--d->refcount)
            delete d;
    }

    void add(uint pos, T value) {
        if (pos < d->size) {
            Q_ASSERT(d->refcount > 1);
            // need to detach
            Private *dd = new Private(pos + 8);
            memcpy(dd->data, d->data, pos*sizeof(T));
            dd->size = pos + 1;
            dd->data[pos] = value;
            if (!--d->refcount)
                delete d;
            d = dd;
            return;
        }
        Q_ASSERT(pos == d->size);
        if (pos == d->alloc) {
            T *n = new T[d->alloc * 2];
            memcpy(n, d->data, d->alloc*sizeof(T));
            delete [] d->data;
            d->data = n;
            d->alloc *= 2;
        }
        d->data[pos] = value;
        ++d->size;
    }

    void set(uint pos, T value) {
        Q_ASSERT(pos < d->size);
        if (d->refcount > 1) {
            // need to detach
            Private *dd = new Private(d->alloc);
            memcpy(dd->data, d->data, d->size*sizeof(T));
            dd->size = d->size;
            if (!--d->refcount)
                delete d;
            d = dd;
        }
        d->data[pos] = value;
    }

    T *constData() const {
        return d->data;
    }
    T at(uint i) const {
        Q_ASSERT(i < d->size);
        return d->data[i];
    }
    T operator[] (uint i) {
        Q_ASSERT(i < d->size);
        return d->data[i];
    }

private:
    SharedInternalClassData &operator=(const SharedInternalClassData &other);
};

struct InternalClassTransition
{
    union {
        Identifier *id;
        Object *prototype;
        const ManagedVTable *vtable;
    };
    int flags;
    enum {
        // range 0-0xff is reserved for attribute changes
        ProtoChange = 0x100,
        VTableChange = 0x200
    };

    bool operator==(const InternalClassTransition &other) const
    { return id == other.id && flags == other.flags; }
};
uint qHash(const QV4::InternalClassTransition &t, uint = 0);

struct InternalClass : public QQmlJS::Managed {
    ExecutionEngine *engine;
    Object *prototype;
    const ManagedVTable *vtable;

    PropertyHash propertyTable; // id to valueIndex
    SharedInternalClassData<String *> nameMap;
    SharedInternalClassData<PropertyAttributes> propertyData;

    typedef InternalClassTransition Transition;
    QHash<Transition, InternalClass *> transitions; // id to next class, positive means add, negative delete

    InternalClass *m_sealed;
    InternalClass *m_frozen;

    uint size;

    static InternalClass *create(ExecutionEngine *engine, const ManagedVTable *vtable, Object *proto);
    InternalClass *changePrototype(Object *proto);
    InternalClass *changeVTable(const ManagedVTable *vt);
    static void addMember(Object *object, StringRef string, PropertyAttributes data, uint *index);
    static void addMember(Object *object, String *string, PropertyAttributes data, uint *index);
    InternalClass *addMember(StringRef string, PropertyAttributes data, uint *index = 0);
    InternalClass *addMember(String *string, PropertyAttributes data, uint *index = 0);
    InternalClass *changeMember(String *string, PropertyAttributes data, uint *index = 0);
    static void changeMember(Object *object, String *string, PropertyAttributes data, uint *index = 0);
    static void removeMember(Object *object, Identifier *id);
    uint find(const StringRef string);
    uint find(const String *s);

    InternalClass *sealed();
    InternalClass *frozen();

    void destroy();

private:
    InternalClass *addMemberImpl(String *string, PropertyAttributes data, uint *index);
    friend struct ExecutionEngine;
    InternalClass(ExecutionEngine *engine);
    InternalClass(const InternalClass &other);
};

struct InternalClassPool : public QQmlJS::MemoryPool
{
    void markObjects(ExecutionEngine *engine);
};

}

QT_END_NAMESPACE

#endif
