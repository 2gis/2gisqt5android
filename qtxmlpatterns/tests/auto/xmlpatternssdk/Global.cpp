/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QXmlNamePool>

#include "Global.h"

using namespace QPatternistSDK;

const QString Global::xqtsCatalogNS     (QLatin1String("http://www.w3.org/2005/02/query-test-XQTSCatalog"));
const QString Global::xqtsResultNS      (QLatin1String("http://www.w3.org/2005/02/query-test-XQTSResult"));
const QString Global::xsltsCatalogNS    (QLatin1String("http://www.w3.org/2005/05/xslt20-test-catalog"));
const QString Global::organizationName  (QLatin1String("Patternist Team"));
const qint16  Global::versionNumber     (0x01);

static QXmlNamePool s_namePool;

QPatternist::NamePool::Ptr Global::namePool()
{
    return s_namePool.d;
}

QXmlNamePool Global::namePoolAsPublic()
{
    return s_namePool;
}

bool Global::readBoolean(const QString &value)
{
    const QString normd(value.simplified());

    if(normd == QLatin1String("true") ||
       normd == QLatin1String("1"))
        return true;
    else if(normd.isEmpty() ||
            normd == QLatin1String("false") ||
            normd == QLatin1String("0"))
        return false;

    Q_ASSERT_X(false, Q_FUNC_INFO,
               "The lexical representation of xs:boolean was invalid");
    return false;
}

// vim: et:ts=4:sw=4:sts=4
