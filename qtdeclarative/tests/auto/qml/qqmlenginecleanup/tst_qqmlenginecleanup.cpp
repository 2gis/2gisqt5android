/****************************************************************************
**
** Copyright (C) 2013 Research In Motion.
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

#include "../../shared/util.h"
#include <QtCore/QObject>
#include <QtQml/qqml.h>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>

//Separate test, because if engine cleanup attempts fail they can easily break unrelated tests
class tst_qqmlenginecleanup : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlenginecleanup() {}

private slots:
    void test_qmlClearTypeRegistrations();
};

void tst_qqmlenginecleanup::test_qmlClearTypeRegistrations()
{
    //Test for preventing memory leaks is in tests/manual/qmltypememory
    QQmlEngine* engine;
    QQmlComponent* component;
    QUrl testFile = testFileUrl("types.qml");

    qmlRegisterType<QObject>("Test", 2, 0, "TestTypeCpp");
    engine = new QQmlEngine;
    component = new QQmlComponent(engine, testFile);
    QVERIFY(component->isReady());

    delete engine;
    delete component;
    qmlClearTypeRegistrations();

    //2nd run verifies that types can reload after a qmlClearTypeRegistrations
    qmlRegisterType<QObject>("Test", 2, 0, "TestTypeCpp");
    engine = new QQmlEngine;
    component = new QQmlComponent(engine, testFile);
    QVERIFY(component->isReady());

    delete engine;
    delete component;
    qmlClearTypeRegistrations();

    //3nd run verifies that TestTypeCpp is no longer registered
    engine = new QQmlEngine;
    component = new QQmlComponent(engine, testFile);
    QVERIFY(component->isError());
    QCOMPARE(component->errorString(),
            testFile.toString() +":46 module \"Test\" is not installed\n");

    delete engine;
    delete component;
}

QTEST_MAIN(tst_qqmlenginecleanup)

#include "tst_qqmlenginecleanup.moc"
