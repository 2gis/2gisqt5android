/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include "qbuiltintypes_p.h"
#include "qitem_p.h"

#include "qanynodetype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

bool AnyNodeType::xdtTypeMatches(const ItemType::Ptr &other) const
{
    return other->isNodeType();
}

bool AnyNodeType::itemMatches(const Item &item) const
{
    return item.isNode();
}

ItemType::Ptr AnyNodeType::atomizedType() const
{
    return BuiltinTypes::xsAnyAtomicType;
}

QString AnyNodeType::displayName(const NamePool::Ptr &) const
{
    return QLatin1String("node()");
}

ItemType::Ptr AnyNodeType::xdtSuperType() const
{
    return BuiltinTypes::item;
}

bool AnyNodeType::isNodeType() const
{
    return true;
}

bool AnyNodeType::isAtomicType() const
{
    return false;
}

QXmlNodeModelIndex::NodeKind AnyNodeType::nodeKind() const
{
    /* node() is an abstract type, so we don't have a value for it in
     * QXmlNodeModelIndex::NodeKind. */
    return QXmlNodeModelIndex::NodeKind(0);
}

PatternPriority AnyNodeType::patternPriority() const
{
    return -0.5;
}

QT_END_NAMESPACE
