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

#include "qqmlvme_p.h"

#include "qqmlcompiler_p.h"
#include "qqmlboundsignal_p.h"
#include "qqmlstringconverters_p.h"
#include <private/qmetaobjectbuilder_p.h>
#include "qqmldata_p.h"
#include "qqml.h"
#include "qqmlinfo.h"
#include "qqmlcustomparser_p.h"
#include "qqmlengine.h"
#include "qqmlcontext.h"
#include "qqmlcomponent.h"
#include "qqmlcomponentattached_p.h"
#include "qqmlbinding_p.h"
#include "qqmlengine_p.h"
#include "qqmlcomponent_p.h"
#include "qqmlvmemetaobject_p.h"
#include "qqmlcontext_p.h"
#include "qqmlglobal_p.h"
#include <private/qfinitestack_p.h>
#include "qqmlscriptstring.h"
#include "qqmlscriptstring_p.h"
#include "qqmlpropertyvalueinterceptor_p.h"
#include "qqmlvaluetypeproxybinding_p.h"
#include "qqmlexpression_p.h"
#include "qqmlcontextwrapper_p.h"

#include <QStack>
#include <QPointF>
#include <QSizeF>
#include <QRectF>
#include <QtCore/qdebug.h>
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qvarlengtharray.h>
#include <QtQml/qjsvalue.h>

QT_BEGIN_NAMESPACE

using namespace QQmlVMETypes;

bool QQmlVME::s_enableComponentComplete = true;

void QQmlVME::enableComponentComplete()
{
    s_enableComponentComplete = true;
}

void QQmlVME::disableComponentComplete()
{
    s_enableComponentComplete = false;
}

bool QQmlVME::componentCompleteEnabled()
{
    return s_enableComponentComplete;
}

QQmlVMEGuard::QQmlVMEGuard()
: m_objectCount(0), m_objects(0), m_contextCount(0), m_contexts(0)
{
}

QQmlVMEGuard::~QQmlVMEGuard()
{
    clear();
}

void QQmlVMEGuard::guard(QQmlObjectCreator *creator)
{
    clear();

    QFiniteStack<QObject*> &objects = creator->allCreatedObjects();
    m_objectCount = objects.count();
    m_objects = new QPointer<QObject>[m_objectCount];
    for (int ii = 0; ii < m_objectCount; ++ii)
        m_objects[ii] = objects[ii];

    m_contextCount = 1;
    m_contexts = new QQmlGuardedContextData[m_contextCount];
    m_contexts[0] = creator->parentContextData();
}

void QQmlVMEGuard::clear()
{
    delete [] m_objects;
    delete [] m_contexts;

    m_objectCount = 0;
    m_objects = 0;
    m_contextCount = 0;
    m_contexts = 0;
}

bool QQmlVMEGuard::isOK() const
{
    for (int ii = 0; ii < m_objectCount; ++ii)
        if (m_objects[ii].isNull())
            return false;

    for (int ii = 0; ii < m_contextCount; ++ii)
        if (m_contexts[ii].isNull() || !m_contexts[ii]->engine)
            return false;

    return true;
}

QT_END_NAMESPACE
