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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QtLocation/QPlaceEditorial>
#include <QtLocation/QPlaceSupplier>
#include <QtLocation/QPlaceUser>

#include "../utils/qlocationtestutils_p.h"

QT_USE_NAMESPACE

class tst_QPlaceEditorial : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceEditorial();

    //needed for QLocationTestUtils::testConversion
    QPlaceEditorial initialSubObject();
    bool checkType(const QPlaceContent &);
    void detach(QPlaceContent *);
    void setSubClassProperty(QPlaceEditorial *);

private Q_SLOTS:
    void constructorTest();
    void supplierTest();
    void textTest();
    void titleTest();
    void languageTest();
    void operatorsTest();
    void conversionTest();
};

tst_QPlaceEditorial::tst_QPlaceEditorial()
{
}

QPlaceEditorial tst_QPlaceEditorial::initialSubObject()
{
    QPlaceUser user;
    user.setName("user 1");
    user.setUserId("0001");

    QPlaceSupplier supplier;
    supplier.setName("supplier");
    supplier.setSupplierId("1");

    QPlaceEditorial editorial;
    editorial.setTitle("title");
    editorial.setText("text");
    editorial.setLanguage("en");
    editorial.setUser(user);
    editorial.setSupplier(supplier);
    editorial.setAttribution("attribution");

    return editorial;
}

bool tst_QPlaceEditorial::checkType(const QPlaceContent &content)
{
    return content.type() == QPlaceContent::EditorialType;
}

void tst_QPlaceEditorial::detach(QPlaceContent *content)
{
    content->setAttribution("attribution");
}

void tst_QPlaceEditorial::setSubClassProperty(QPlaceEditorial * editorial)
{
    editorial->setTitle("new title");
}
void tst_QPlaceEditorial::constructorTest()
{
    QPlaceEditorial testObj;
    testObj.setText("testId");
    QPlaceEditorial *testObjPtr = new QPlaceEditorial(testObj);
    QVERIFY2(testObjPtr != NULL, "Copy constructor - null");
    QVERIFY2(*testObjPtr == testObj, "Copy constructor - compare");
    delete testObjPtr;
}

void tst_QPlaceEditorial::supplierTest()
{
    QPlaceEditorial testObj;
    QVERIFY2(testObj.supplier().supplierId() == QString(), "Wrong default value");
    QPlaceSupplier sup;
    sup.setName("testName1");
    sup.setSupplierId("testId");
    testObj.setSupplier(sup);
    QVERIFY2(testObj.supplier() == sup, "Wrong value returned");
}

void tst_QPlaceEditorial::textTest()
{
    QPlaceEditorial testObj;
    QVERIFY2(testObj.text() == QString(), "Wrong default value");
    testObj.setText("testText");
    QVERIFY2(testObj.text() == "testText", "Wrong value returned");
}

void tst_QPlaceEditorial::titleTest()
{
    QPlaceEditorial testObj;
    QVERIFY2(testObj.title() == QString(), "Wrong default value");
    testObj.setTitle("testText");
    QVERIFY2(testObj.title() == "testText", "Wrong value returned");
}

void tst_QPlaceEditorial::languageTest()
{
    QPlaceEditorial testObj;
    QVERIFY2(testObj.language() == QString(), "Wrong default value");
    testObj.setLanguage("testText");
    QVERIFY2(testObj.language() == "testText", "Wrong value returned");
}

void tst_QPlaceEditorial::operatorsTest()
{
    QPlaceEditorial testObj;
    testObj.setLanguage("testValue");
    QPlaceEditorial testObj2;
    testObj2 = testObj;
    QVERIFY2(testObj == testObj2, "Not copied correctly");
    testObj2.setText("testValue2");
    QVERIFY2(testObj != testObj2, "Object should be different");
}

void tst_QPlaceEditorial::conversionTest()
{
    QLocationTestUtils::testConversion<tst_QPlaceEditorial,
                                       QPlaceContent,
                                       QPlaceEditorial>(this);
}
QTEST_APPLESS_MAIN(tst_QPlaceEditorial);

#include "tst_qplaceeditorial.moc"
