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

#ifndef QV4EXECUTABLEALLOCATOR_H
#define QV4EXECUTABLEALLOCATOR_H

#include "qv4global_p.h"

#include <QMultiMap>
#include <QHash>
#include <QVector>
#include <QByteArray>
#include <QMutex>

namespace WTF {
class PageAllocation;
}

QT_BEGIN_NAMESPACE

namespace QV4 {

class Q_AUTOTEST_EXPORT ExecutableAllocator
{
public:
    struct ChunkOfPages;
    struct Allocation;

    ExecutableAllocator();
    ~ExecutableAllocator();

    Allocation *allocate(size_t size);
    void free(Allocation *allocation);

    struct Allocation
    {
        Allocation()
            : addr(0)
            , size(0)
            , free(true)
            , next(0)
            , prev(0)
        {}

        void *start() const;
        void invalidate() { addr = 0; }
        bool isValid() const { return addr != 0; }
        void deallocate(ExecutableAllocator *allocator);

    private:
        ~Allocation() {}

        friend class ExecutableAllocator;

        Allocation *split(size_t dividingSize);
        bool mergeNext(ExecutableAllocator *allocator);
        bool mergePrevious(ExecutableAllocator *allocator);

        quintptr addr;
        uint size : 31; // More than 2GB of function code? nah :)
        uint free : 1;
        Allocation *next;
        Allocation *prev;
    };

    // for debugging / unit-testing
    int freeAllocationCount() const { return freeAllocations.count(); }
    int chunkCount() const { return chunks.count(); }

    struct ChunkOfPages
    {
        ChunkOfPages()
            : pages(0)
            , firstAllocation(0)
        {}
        ~ChunkOfPages();

        WTF::PageAllocation *pages;
        Allocation *firstAllocation;

        bool contains(Allocation *alloc) const;
    };

    ChunkOfPages *chunkForAllocation(Allocation *allocation) const;

private:
    QMultiMap<size_t, Allocation*> freeAllocations;
    QMap<quintptr, ChunkOfPages*> chunks;
    mutable QMutex mutex;
};

}

QT_END_NAMESPACE

#endif // QV4EXECUTABLEALLOCATOR_H
