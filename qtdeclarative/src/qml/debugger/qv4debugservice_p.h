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

#ifndef QV4DEBUGSERVICE_P_H
#define QV4DEBUGSERVICE_P_H

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

#include "qqmlconfigurabledebugservice_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 { struct ExecutionEngine; }
class QQmlEngine;
class QV4DebugServicePrivate;

class QV4DebugService : public QQmlConfigurableDebugService
{
    Q_OBJECT
public:
    explicit QV4DebugService(QObject *parent = 0);
    ~QV4DebugService();

    static QV4DebugService *instance();
    void engineAboutToBeAdded(QQmlEngine *engine);
    void engineAboutToBeRemoved(QQmlEngine *engine);

    void signalEmitted(const QString &signal);

protected:
    void messageReceived(const QByteArray &);
    void sendSomethingToSomebody(const char *type, int magicNumber = 1);

private:
    void handleV8Request(const QByteArray &payload);

private:
    Q_DISABLE_COPY(QV4DebugService)
    Q_DECLARE_PRIVATE(QV4DebugService)
};

QT_END_NAMESPACE

#endif // QV4DEBUGSERVICE_P_H
