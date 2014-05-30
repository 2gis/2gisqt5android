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


#include <QtTest/QtTest>

#include <QSimpleXmlNodeModel>
#include <QXmlNamePool>
#include <QXmlQuery>
#include <QXmlSerializer>
#include <QXmlStreamReader>

#include "TestSimpleNodeModel.h"

/*!
 \class tst_QSimpleXmlNodeModel
 \internal
 \since 4.4
 \brief Tests class QSimpleXmlNodeModel.
 */
class tst_QSimpleXmlNodeModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void namePool() const;
    void namePoolIsReference() const;
    void defaultConstructor() const;
    void objectSize() const;
    void constCorrectness() const;
    void stringValue() const;
};

void tst_QSimpleXmlNodeModel::namePool() const
{
    /* Check that the name pool we pass in, is what actually is returned. */
    QXmlNamePool np;
    const QXmlName name(np, QLatin1String("localName"),
                            QLatin1String("http://example.com/XYZ"),
                            QLatin1String("prefix432"));
    TestSimpleNodeModel model(np);
    const QXmlNamePool np2(model.namePool());

    /* If it's a bug, this will more or less crash. */
    QCOMPARE(name.namespaceUri(np2), QString::fromLatin1("http://example.com/XYZ"));
    QCOMPARE(name.localName(np2), QString::fromLatin1("localName"));
    QCOMPARE(name.prefix(np2), QString::fromLatin1("prefix432"));
}

void tst_QSimpleXmlNodeModel::namePoolIsReference() const
{
    /* Test is placed in TestSimpleNodeModel.h:~50. */
}

void tst_QSimpleXmlNodeModel::defaultConstructor() const
{
    QXmlNamePool np;
}

void tst_QSimpleXmlNodeModel::objectSize() const
{
    /* We shouldn't have added any members. */
    QCOMPARE(sizeof(QSimpleXmlNodeModel), sizeof(QAbstractXmlNodeModel));
}

void tst_QSimpleXmlNodeModel::constCorrectness() const
{
    QXmlNamePool np;
    const TestSimpleNodeModel instance(np);

    instance.nextFromSimpleAxis(QSimpleXmlNodeModel::Parent, QXmlNodeModelIndex());
    instance.attributes(QXmlNodeModelIndex());

    instance.namePool();
}

/*!
 Verify that if QAbstractXmlNodeModel::typedValue() return a null
 QVariant, it is treated as that the node has no typed value.

 This verifies the default implementation of QSimpleXmlNodeModel::stringValue().
 */
void tst_QSimpleXmlNodeModel::stringValue() const
{
    class TypedModel : public TestSimpleNodeModel
    {
    public:
        TypedModel(const QXmlNamePool &np) : TestSimpleNodeModel(np)
        {
        }

        virtual QXmlNodeModelIndex::NodeKind kind(const QXmlNodeModelIndex &) const
        {
            return QXmlNodeModelIndex::Element;
        }

        virtual QVariant typedValue(const QXmlNodeModelIndex &) const
        {
            return QVariant();
        }

        QXmlNodeModelIndex root() const
        {
            return createIndex(qint64(1));
        }
    };

    QXmlNamePool np;
    TypedModel model(np);

    QXmlQuery query(np);
    query.bindVariable(QLatin1String("node"), model.root());
    query.setQuery(QLatin1String("declare variable $node external;"
                                 "string($node), data($node)"));

    QByteArray output;
    QBuffer buffer(&output);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(query.isValid());

    QXmlSerializer serializer(query, &buffer);
    QVERIFY(query.evaluateTo(&serializer));

    QVERIFY(output.isEmpty());
}

QTEST_MAIN(tst_QSimpleXmlNodeModel)

#include "tst_qsimplexmlnodemodel.moc"
