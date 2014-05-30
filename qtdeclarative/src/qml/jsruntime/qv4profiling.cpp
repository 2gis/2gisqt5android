/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "qv4profiling_p.h"

QT_BEGIN_NAMESPACE

using namespace QV4;
using namespace QV4::Profiling;

FunctionCallProperties FunctionCall::resolve() const
{
    FunctionCallProperties props = {
        m_start,
        m_end,
        m_function->name()->toQString(),
        m_function->compilationUnit->fileName(),
        m_function->compiledFunction->location.line,
        m_function->compiledFunction->location.column
    };
    return props;
}


Profiler::Profiler() : enabled(false)
{
    static int metatype = qRegisterMetaType<QList<QV4::Profiling::FunctionCallProperties> >();
    Q_UNUSED(metatype);
    m_timer.start();
}

struct FunctionCallComparator {
    bool operator()(const FunctionCallProperties &p1, const FunctionCallProperties &p2)
    { return p1.start < p2.start; }
};

void Profiler::stopProfiling()
{
    enabled = false;
    reportData();
}

void Profiler::reportData()
{
    QList<FunctionCallProperties> resolved;
    resolved.reserve(m_data.size());
    FunctionCallComparator comp;
    foreach (const FunctionCall &call, m_data) {
        FunctionCallProperties props = call.resolve();
        resolved.insert(std::upper_bound(resolved.begin(), resolved.end(), props, comp), props);
    }
    emit dataReady(resolved);
}

void Profiler::startProfiling()
{
    if (!enabled) {
        m_data.clear();
        enabled = true;
    }
}

QT_END_NAMESPACE
