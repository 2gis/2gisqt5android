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


#include "qexpressionvariablereference_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ExpressionVariableReference::ExpressionVariableReference(const VariableSlotID slotP,
                                                         const VariableDeclaration::Ptr &varDecl) : VariableReference(slotP)
                                                                                                  , m_varDecl(varDecl)
{
}

bool ExpressionVariableReference::evaluateEBV(const DynamicContext::Ptr &context) const
{
    return context->expressionVariable(slot())->evaluateEBV(context);
}

Item ExpressionVariableReference::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    return context->expressionVariable(slot())->evaluateSingleton(context);
}

Item::Iterator::Ptr ExpressionVariableReference::evaluateSequence(const DynamicContext::Ptr &context) const
{
    return context->expressionVariable(slot())->evaluateSequence(context);
}
Expression::Ptr ExpressionVariableReference::typeCheck(const StaticContext::Ptr &context,
                                                       const SequenceType::Ptr &reqType)
{
    if(m_varDecl->canSourceRewrite)
        return m_varDecl->expression()->typeCheck(context, reqType);
    else
        return VariableReference::typeCheck(context, reqType);
}

Expression::ID ExpressionVariableReference::id() const
{
    return IDExpressionVariableReference;
}

ExpressionVisitorResult::Ptr ExpressionVariableReference::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

SequenceType::Ptr ExpressionVariableReference::staticType() const
{
    return m_varDecl->expression()->staticType();
}

QT_END_NAMESPACE
