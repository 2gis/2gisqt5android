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

#ifndef QQMLBUILTINFUNCTIONS_P_H
#define QQMLBUILTINFUNCTIONS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qqmlglobal_p.h>
#include <private/qv4functionobject_p.h>

QT_BEGIN_NAMESPACE

class QQmlEngine;
class QV8Engine;

namespace QV4 {

struct QtObject : Object
{
    V4_OBJECT
    QtObject(ExecutionEngine *v4, QQmlEngine *qmlEngine);

    static ReturnedValue method_isQtObject(CallContext *ctx);
    static ReturnedValue method_rgba(CallContext *ctx);
    static ReturnedValue method_hsla(CallContext *ctx);
    static ReturnedValue method_colorEqual(CallContext *ctx);
    static ReturnedValue method_font(CallContext *ctx);
    static ReturnedValue method_rect(CallContext *ctx);
    static ReturnedValue method_point(CallContext *ctx);
    static ReturnedValue method_size(CallContext *ctx);
    static ReturnedValue method_vector2d(CallContext *ctx);
    static ReturnedValue method_vector3d(CallContext *ctx);
    static ReturnedValue method_vector4d(CallContext *ctx);
    static ReturnedValue method_quaternion(CallContext *ctx);
    static ReturnedValue method_matrix4x4(CallContext *ctx);
    static ReturnedValue method_lighter(CallContext *ctx);
    static ReturnedValue method_darker(CallContext *ctx);
    static ReturnedValue method_tint(CallContext *ctx);
    static ReturnedValue method_formatDate(CallContext *ctx);
    static ReturnedValue method_formatTime(CallContext *ctx);
    static ReturnedValue method_formatDateTime(CallContext *ctx);
    static ReturnedValue method_openUrlExternally(CallContext *ctx);
    static ReturnedValue method_fontFamilies(CallContext *ctx);
    static ReturnedValue method_md5(CallContext *ctx);
    static ReturnedValue method_btoa(CallContext *ctx);
    static ReturnedValue method_atob(CallContext *ctx);
    static ReturnedValue method_quit(CallContext *ctx);
    static ReturnedValue method_resolvedUrl(CallContext *ctx);
    static ReturnedValue method_createQmlObject(CallContext *ctx);
    static ReturnedValue method_createComponent(CallContext *ctx);
    static ReturnedValue method_locale(CallContext *ctx);
    static ReturnedValue method_binding(CallContext *ctx);

    static ReturnedValue method_get_platform(CallContext *ctx);
    static ReturnedValue method_get_application(CallContext *ctx);
#ifndef QT_NO_IM
    static ReturnedValue method_get_inputMethod(CallContext *ctx);
#endif

    QObject *m_platform;
    QObject *m_application;
};

struct ConsoleObject : Object
{
    ConsoleObject(ExecutionEngine *v4);

    static ReturnedValue method_error(CallContext *ctx);
    static ReturnedValue method_log(CallContext *ctx);
    static ReturnedValue method_profile(CallContext *ctx);
    static ReturnedValue method_profileEnd(CallContext *ctx);
    static ReturnedValue method_time(CallContext *ctx);
    static ReturnedValue method_timeEnd(CallContext *ctx);
    static ReturnedValue method_count(CallContext *ctx);
    static ReturnedValue method_trace(CallContext *ctx);
    static ReturnedValue method_warn(CallContext *ctx);
    static ReturnedValue method_assert(CallContext *ctx);
    static ReturnedValue method_exception(CallContext *ctx);

};

struct GlobalExtensions {
    static void init(QQmlEngine *qmlEngine, Object *globalObject);

#ifndef QT_NO_TRANSLATION
    static ReturnedValue method_qsTranslate(CallContext *ctx);
    static ReturnedValue method_qsTranslateNoOp(CallContext *ctx);
    static ReturnedValue method_qsTr(CallContext *ctx);
    static ReturnedValue method_qsTrNoOp(CallContext *ctx);
    static ReturnedValue method_qsTrId(CallContext *ctx);
    static ReturnedValue method_qsTrIdNoOp(CallContext *ctx);
#endif
    static ReturnedValue method_gc(CallContext *ctx);

    // on String:prototype
    static ReturnedValue method_string_arg(CallContext *ctx);

};

struct QQmlBindingFunction : public QV4::FunctionObject
{
    V4_OBJECT
    QQmlBindingFunction(FunctionObject *originalFunction);

    void initBindingLocation(); // from caller stack trace

    static ReturnedValue call(Managed *that, CallData *callData);

    static void markObjects(Managed *that, ExecutionEngine *e);

    QV4::FunctionObject *originalFunction;
    // Set when the binding is created later
    QQmlSourceLocation bindingLocation;
};

}

QT_END_NAMESPACE

#endif // QQMLBUILTINFUNCTIONS_P_H
