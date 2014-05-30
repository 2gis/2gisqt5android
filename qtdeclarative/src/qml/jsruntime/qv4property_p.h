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
#ifndef QV4PROPERTYDESCRIPTOR_H
#define QV4PROPERTYDESCRIPTOR_H

#include "qv4global_p.h"
#include "qv4value_inl_p.h"
#include "qv4internalclass_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct FunctionObject;

struct Property {
    Value value;
    Value set;

    // Section 8.10
    inline void fullyPopulated(PropertyAttributes *attrs) {
        if (!attrs->hasType()) {
            value = Primitive::undefinedValue();
        }
        if (attrs->type() == PropertyAttributes::Accessor) {
            attrs->clearWritable();
            if (value.isEmpty())
                value = Primitive::undefinedValue();
            if (set.isEmpty())
                set = Primitive::undefinedValue();
        }
        attrs->resolve();
    }

    static Property genericDescriptor() {
        Property pd;
        pd.value = Primitive::emptyValue();
        return pd;
    }

    inline bool isSubset(const PropertyAttributes &attrs, const Property &other, PropertyAttributes otherAttrs) const;
    inline void merge(PropertyAttributes &attrs, const Property &other, PropertyAttributes otherAttrs);

    inline FunctionObject *getter() const { return reinterpret_cast<FunctionObject *>(value.asManaged()); }
    inline FunctionObject *setter() const { return reinterpret_cast<FunctionObject *>(set.asManaged()); }
    inline void setGetter(FunctionObject *g) { value = Primitive::fromManaged(reinterpret_cast<Managed *>(g)); }
    inline void setSetter(FunctionObject *s) { set = Primitive::fromManaged(reinterpret_cast<Managed *>(s)); }

    void copy(const Property &other, PropertyAttributes attrs) {
        value = other.value;
        if (attrs.isAccessor())
            set = other.set;
    }

    explicit Property()  { value = Encode::undefined(); set = Encode::undefined(); }
    explicit Property(Value v) : value(v) { set = Encode::undefined(); }
    Property(FunctionObject *getter, FunctionObject *setter) {
        value = Primitive::fromManaged(reinterpret_cast<Managed *>(getter));
        set = Primitive::fromManaged(reinterpret_cast<Managed *>(setter));
    }
    Property &operator=(Value v) { value = v; return *this; }
private:
    Property(const Property &);
    Property &operator=(const Property &);
};

inline bool Property::isSubset(const PropertyAttributes &attrs, const Property &other, PropertyAttributes otherAttrs) const
{
    if (attrs.type() != PropertyAttributes::Generic && attrs.type() != otherAttrs.type())
        return false;
    if (attrs.hasEnumerable() && attrs.isEnumerable() != otherAttrs.isEnumerable())
        return false;
    if (attrs.hasConfigurable() && attrs.isConfigurable() != otherAttrs.isConfigurable())
        return false;
    if (attrs.hasWritable() && attrs.isWritable() != otherAttrs.isWritable())
        return false;
    if (attrs.type() == PropertyAttributes::Data && !value.sameValue(other.value))
        return false;
    if (attrs.type() == PropertyAttributes::Accessor) {
        if (value.asManaged() != other.value.asManaged())
            return false;
        if (set.asManaged() != other.set.asManaged())
            return false;
    }
    return true;
}

inline void Property::merge(PropertyAttributes &attrs, const Property &other, PropertyAttributes otherAttrs)
{
    if (otherAttrs.hasEnumerable())
        attrs.setEnumerable(otherAttrs.isEnumerable());
    if (otherAttrs.hasConfigurable())
        attrs.setConfigurable(otherAttrs.isConfigurable());
    if (otherAttrs.hasWritable())
        attrs.setWritable(otherAttrs.isWritable());
    if (otherAttrs.type() == PropertyAttributes::Accessor) {
        attrs.setType(PropertyAttributes::Accessor);
        if (!other.value.isEmpty())
            value = other.value;
        if (!other.set.isEmpty())
            set = other.set;
    } else if (otherAttrs.type() == PropertyAttributes::Data){
        attrs.setType(PropertyAttributes::Data);
        value = other.value;
    }
}

}

Q_DECLARE_TYPEINFO(QV4::Property, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
