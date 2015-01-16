/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QV4MATHOBJECT_H
#define QV4MATHOBJECT_H

#include "qv4object_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct MathObject: Object
{
    struct Data : Object::Data {
        Data(InternalClass *ic);
    };

    V4_OBJECT(Object)
    Q_MANAGED_TYPE(MathObject)

    static ReturnedValue method_abs(CallContext *context);
    static ReturnedValue method_acos(CallContext *context);
    static ReturnedValue method_asin(CallContext *context);
    static ReturnedValue method_atan(CallContext *context);
    static ReturnedValue method_atan2(CallContext *context);
    static ReturnedValue method_ceil(CallContext *context);
    static ReturnedValue method_cos(CallContext *context);
    static ReturnedValue method_exp(CallContext *context);
    static ReturnedValue method_floor(CallContext *context);
    static ReturnedValue method_log(CallContext *context);
    static ReturnedValue method_max(CallContext *context);
    static ReturnedValue method_min(CallContext *context);
    static ReturnedValue method_pow(CallContext *context);
    static ReturnedValue method_random(CallContext *context);
    static ReturnedValue method_round(CallContext *context);
    static ReturnedValue method_sin(CallContext *context);
    static ReturnedValue method_sqrt(CallContext *context);
    static ReturnedValue method_tan(CallContext *context);
};

}

QT_END_NAMESPACE

#endif // QMLJS_OBJECTS_H
