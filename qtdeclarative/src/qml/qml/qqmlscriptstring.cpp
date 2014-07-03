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

#include "qqmlscriptstring.h"
#include "qqmlscriptstring_p.h"

QT_BEGIN_NAMESPACE

/*!
\class QQmlScriptString
\brief The QQmlScriptString class encapsulates a script and its context.
\inmodule QtQml

QQmlScriptString is used to create QObject properties that accept a script "assignment" from QML.

Normally, the following QML would result in a binding being established for the \c script
property; i.e. \c script would be assigned the value obtained from running \c {myObj.value = Math.max(myValue, 100)}

\qml
MyType {
    script: myObj.value = Math.max(myValue, 100)
}
\endqml

If instead the property had a type of QQmlScriptString,
the script itself -- \e {myObj.value = Math.max(myValue, 100)} -- would be passed to the \c script property
and the class could choose how to handle it. Typically, the class will evaluate
the script at some later time using a QQmlExpression.

\code
QQmlExpression expr(scriptString);
expr.evaluate();
\endcode

\sa QQmlExpression
*/

const QQmlScriptStringPrivate* QQmlScriptStringPrivate::get(const QQmlScriptString &script)
{
    return script.d.constData();
}

/*!
Constructs an empty instance.
*/
QQmlScriptString::QQmlScriptString()
:  d(new QQmlScriptStringPrivate)
{
}

/*!
    \internal
*/
QQmlScriptString::QQmlScriptString(const QString &script, QQmlContext *context, QObject *scope)
:  d(new QQmlScriptStringPrivate)
{
    d->script = script;
    d->context = context;
    d->scope = scope;
}

/*!
Copies \a other.
*/
QQmlScriptString::QQmlScriptString(const QQmlScriptString &other)
: d(other.d)
{
}

/*!
\internal
*/
QQmlScriptString::~QQmlScriptString()
{
}

/*!
Assigns \a other to this.
*/
QQmlScriptString &QQmlScriptString::operator=(const QQmlScriptString &other)
{
    d = other.d;
    return *this;
}

/*!
Returns whether the QQmlScriptString is empty.
*/
bool QQmlScriptString::isEmpty() const
{
    if (!d->script.isEmpty())
        return false;
    return d->bindingId == -1;
}

/*!
Returns whether the content of the QQmlScriptString is the \c undefined literal.
*/
bool QQmlScriptString::isUndefinedLiteral() const
{
    return d->script == QStringLiteral("undefined");
}

/*!
Returns whether the content of the QQmlScriptString is the \c null literal.
*/
bool QQmlScriptString::isNullLiteral() const
{
    return d->script == QStringLiteral("null");
}

/*!
If the content of the QQmlScriptString is a string literal, returns that string.
Otherwise returns a null QString.
*/
QString QQmlScriptString::stringLiteral() const
{
    if (d->isStringLiteral)
        return d->script.mid(1, d->script.length()-2);
    return QString();
}

/*!
If the content of the QQmlScriptString is a number literal, returns that number and
sets \a ok to true. Otherwise returns 0.0 and sets \a ok to false.
*/
qreal QQmlScriptString::numberLiteral(bool *ok) const
{
    if (ok)
        *ok = d->isNumberLiteral;
    return d->isNumberLiteral ? d->numberValue : 0.;
}

/*!
If the content of the QQmlScriptString is a boolean literal, returns the boolean value and
sets \a ok to true. Otherwise returns false and sets \a ok to false.
*/
bool QQmlScriptString::booleanLiteral(bool *ok) const
{
    bool isTrue = d->script == QStringLiteral("true");
    bool isFalse = !isTrue && d->script == QStringLiteral("false");
    if (ok)
        *ok = isTrue || isFalse;
    return isTrue ? true : false;
}

QT_END_NAMESPACE

