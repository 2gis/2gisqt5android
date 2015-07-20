/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmlpool_p.h"
#include <stdlib.h>

#ifdef Q_OS_QNX
#include <malloc.h>
#endif

// #define POOL_DEBUG

QT_BEGIN_NAMESPACE

void QQmlPool::newpage()
{
#ifdef POOL_DEBUG
    qWarning("QQmlPool: Allocating page");
#endif

    Page *page = (Page *)malloc(sizeof(Page));
    page->header.next = _page;
    page->header.free = page->memory;
    _page = page;
}

void QQmlPool::clear()
{
#ifdef POOL_DEBUG
    int count = 0;
#endif

    Class *c = _classList;
    while (c) {
        Class *n = c->_next;
        c->_destroy(c);
#ifdef POOL_DEBUG
        ++count;
#endif
        c = n;
    }

#ifdef POOL_DEBUG
    qWarning("QQmlPool: Destroyed %d objects", count);
#endif

    Page *p = _page;
    while (p) {
        Page *n = p->header.next;
        free(p);
        p = n;
    }

    _classList = 0;
    _page = 0;
}


QT_END_NAMESPACE
