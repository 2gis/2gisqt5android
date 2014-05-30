/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QV4SPARSEARRAY_H
#define QV4SPARSEARRAY_H

#include "qv4global_p.h"
#include <QtCore/qmap.h>
#include "qv4value_inl_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4property_p.h"
#include <assert.h>

//#define Q_MAP_DEBUG
#ifdef Q_MAP_DEBUG
#include <QtCore/qdebug.h>
#endif

#include <new>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct SparseArray;

struct SparseArrayNode
{
    quintptr p;
    SparseArrayNode *left;
    SparseArrayNode *right;
    uint size_left;
    uint value;

    enum Color { Red = 0, Black = 1 };
    enum { Mask = 3 }; // reserve the second bit as well

    const SparseArrayNode *nextNode() const;
    SparseArrayNode *nextNode() { return const_cast<SparseArrayNode *>(const_cast<const SparseArrayNode *>(this)->nextNode()); }
    const SparseArrayNode *previousNode() const;
    SparseArrayNode *previousNode() { return const_cast<SparseArrayNode *>(const_cast<const SparseArrayNode *>(this)->previousNode()); }

    Color color() const { return Color(p & 1); }
    void setColor(Color c) { if (c == Black) p |= Black; else p &= ~Black; }
    SparseArrayNode *parent() const { return reinterpret_cast<SparseArrayNode *>(p & ~Mask); }
    void setParent(SparseArrayNode *pp) { p = (p & Mask) | quintptr(pp); }

    uint key() const {
        uint k = size_left;
        const SparseArrayNode *n = this;
        while (SparseArrayNode *p = n->parent()) {
            if (p && p->right == n)
                k += p->size_left;
            n = p;
        }
        return k;
    }

    SparseArrayNode *copy(SparseArray *d) const;

    SparseArrayNode *lowerBound(uint key);
    SparseArrayNode *upperBound(uint key);
};


inline SparseArrayNode *SparseArrayNode::lowerBound(uint akey)
{
    SparseArrayNode *n = this;
    SparseArrayNode *last = 0;
    while (n) {
        if (akey <= n->size_left) {
            last = n;
            n = n->left;
        } else {
            akey -= n->size_left;
            n = n->right;
        }
    }
    return last;
}


inline SparseArrayNode *SparseArrayNode::upperBound(uint akey)
{
    SparseArrayNode *n = this;
    SparseArrayNode *last = 0;
    while (n) {
        if (akey < n->size_left) {
            last = n;
            n = n->left;
        } else {
            akey -= n->size_left;
            n = n->right;
        }
    }
    return last;
}



struct Q_QML_EXPORT SparseArray
{
    SparseArray();
    ~SparseArray() {
        if (root())
            freeTree(header.left, Q_ALIGNOF(SparseArrayNode));
    }

    SparseArray(const SparseArray &other);
private:
    SparseArray &operator=(const SparseArray &other);

    int numEntries;
    SparseArrayNode header;
    SparseArrayNode *mostLeftNode;

    void rotateLeft(SparseArrayNode *x);
    void rotateRight(SparseArrayNode *x);
    void rebalance(SparseArrayNode *x);
    void recalcMostLeftNode();

    SparseArrayNode *root() const { return header.left; }

    void deleteNode(SparseArrayNode *z);


public:
    SparseArrayNode *createNode(uint sl, SparseArrayNode *parent, bool left);
    void freeTree(SparseArrayNode *root, int alignment);

    SparseArrayNode *findNode(uint akey) const;

    uint nEntries() const { return numEntries; }

    uint pop_front();
    void push_front(uint at);
    uint pop_back(uint len);
    void push_back(uint at, uint len);

    QList<int> keys() const;

    const SparseArrayNode *end() const { return &header; }
    SparseArrayNode *end() { return &header; }
    const SparseArrayNode *begin() const { if (root()) return mostLeftNode; return end(); }
    SparseArrayNode *begin() { if (root()) return mostLeftNode; return end(); }

    SparseArrayNode *erase(SparseArrayNode *n);

    SparseArrayNode *lowerBound(uint key);
    const SparseArrayNode *lowerBound(uint key) const;
    SparseArrayNode *upperBound(uint key);
    const SparseArrayNode *upperBound(uint key) const;
    SparseArrayNode *insert(uint akey);

    // STL compatibility
    typedef uint key_type;
    typedef int mapped_type;
    typedef qptrdiff difference_type;
    typedef int size_type;

#ifndef QT_NO_DEBUG
    void dump() const;
#endif
};

inline SparseArrayNode *SparseArray::findNode(uint akey) const
{
    SparseArrayNode *n = root();

    while (n) {
        if (akey == n->size_left) {
            return n;
        } else if (akey < n->size_left) {
            n = n->left;
        } else {
            akey -= n->size_left;
            n = n->right;
        }
    }

    return 0;
}

inline uint SparseArray::pop_front()
{
    uint idx = UINT_MAX ;

    SparseArrayNode *n = findNode(0);
    if (n) {
        idx = n->value;
        deleteNode(n);
        // adjust all size_left indices on the path to leftmost item by 1
        SparseArrayNode *n = root();
        while (n) {
            n->size_left -= 1;
            n = n->left;
        }
    }
    return idx;
}

inline void SparseArray::push_front(uint value)
{
    // adjust all size_left indices on the path to leftmost item by 1
    SparseArrayNode *n = root();
    while (n) {
        n->size_left += 1;
        n = n->left;
    }
    n = insert(0);
    n->value = value;
}

inline uint SparseArray::pop_back(uint len)
{
    uint idx = UINT_MAX;
    if (!len)
        return idx;

    SparseArrayNode *n = findNode(len - 1);
    if (n) {
        idx = n->value;
        deleteNode(n);
    }
    return idx;
}

inline void SparseArray::push_back(uint index, uint len)
{
    SparseArrayNode *n = insert(len);
    n->value = index;
}

#ifndef QT_NO_DEBUG
inline void SparseArray::dump() const
{
    const SparseArrayNode *it = begin();
    qDebug() << "map dump:";
    while (it != end()) {
        const SparseArrayNode *n = it;
        int depth = 0;
        while (n && n != root()) {
            ++depth;
            n = n->parent();
        }
        QByteArray space(4*depth, ' ');
        qDebug() << space << (it->color() == SparseArrayNode::Red ? "Red  " : "Black") << it << it->size_left << it->left << it->right
                 << it->key() << it->value;
        it = it->nextNode();
    }
    qDebug() << "---------";
}
#endif


inline SparseArrayNode *SparseArray::erase(SparseArrayNode *n)
{
    if (n == end())
        return n;

    SparseArrayNode *next = n->nextNode();
    deleteNode(n);
    return next;
}

inline QList<int> SparseArray::keys() const
{
    QList<int> res;
    res.reserve(numEntries);
    SparseArrayNode *n = mostLeftNode;
    while (n != end()) {
        res.append(n->key());
        n = n->nextNode();
    }
    return res;
}

inline const SparseArrayNode *SparseArray::lowerBound(uint akey) const
{
    const SparseArrayNode *lb = root()->lowerBound(akey);
    if (!lb)
        lb = end();
    return lb;
}


inline SparseArrayNode *SparseArray::lowerBound(uint akey)
{
    SparseArrayNode *lb = root()->lowerBound(akey);
    if (!lb)
        lb = end();
    return lb;
}


inline const SparseArrayNode *SparseArray::upperBound(uint akey) const
{
    const SparseArrayNode *ub = root()->upperBound(akey);
    if (!ub)
        ub = end();
    return ub;
}


inline SparseArrayNode *SparseArray::upperBound(uint akey)
{
    SparseArrayNode *ub = root()->upperBound(akey);
    if (!ub)
        ub = end();
    return ub;
}

}

QT_END_NAMESPACE

#endif // QMAP_H
