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

#ifndef QQMLCOMPONENTATTACHED_P_H
#define QQMLCOMPONENTATTACHED_P_H

#include <QtQml/qqml.h>
#include <private/qtqmlglobal_p.h>
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE


class Q_QML_PRIVATE_EXPORT QQmlComponentAttached : public QObject
{
    Q_OBJECT
public:
    QQmlComponentAttached(QObject *parent = 0);
    ~QQmlComponentAttached();

    void add(QQmlComponentAttached **a) {
        prev = a; next = *a; *a = this;
        if (next) next->prev = &next;
    }
    void rem() {
        if (next) next->prev = prev;
        *prev = next;
        next = 0; prev = 0;
    }
    QQmlComponentAttached **prev;
    QQmlComponentAttached *next;

Q_SIGNALS:
    void completed();
    void destruction();

private:
    friend class QQmlContextData;
};

QT_END_NAMESPACE

#endif // QQMLCOMPONENTATTACHED_P_H
