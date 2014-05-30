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

#include "qcommonsequencetypes_p.h"
#include "qgenericsequencetype_p.h"

#include "qfirstitempredicate_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

FirstItemPredicate::FirstItemPredicate(const Expression::Ptr &source) : SingleContainer(source)
{
}

Item FirstItemPredicate::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    /* If our operand allows the empty sequence, this function can return Item(), otherwise
     * it returns the first item. As simple as that. */
    return m_operand->evaluateSequence(context)->next();
}

SequenceType::Ptr FirstItemPredicate::staticType() const
{
    const SequenceType::Ptr t(m_operand->staticType());
    return makeGenericSequenceType(t->itemType(), t->cardinality().toWithoutMany());
}

Expression::Ptr FirstItemPredicate::compress(const StaticContext::Ptr &context)
{
    const Expression::Ptr me(SingleContainer::compress(context));
    if(me != this)
        return me;

    if(m_operand->is(IDFirstItemPredicate))
        m_operand = m_operand->operands().first();

    return me;
}

SequenceType::List FirstItemPredicate::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

ExpressionVisitorResult::Ptr FirstItemPredicate::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

Expression::ID FirstItemPredicate::id() const
{
    return IDFirstItemPredicate;
}

QT_END_NAMESPACE
