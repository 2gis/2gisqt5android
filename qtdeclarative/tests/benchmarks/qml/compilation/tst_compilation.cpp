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

#include <qtest.h>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/private/qqmljsengine_p.h>
#include <QtQml/private/qqmljsmemorypool_p.h>
#include <QtQml/private/qqmljsparser_p.h>
#include <QtQml/private/qqmljslexer_p.h>
#include <QtQml/private/qqmlscript_p.h>

#include <QFile>
#include <QDebug>
#include <QTextStream>

class tst_compilation : public QObject
{
    Q_OBJECT
public:
    tst_compilation();

private slots:
    void boomblock();

    void jsparser_data();
    void jsparser();

    void scriptparser_data();
    void scriptparser();

private:
    QQmlEngine engine;
};

tst_compilation::tst_compilation()
{
}

inline QUrl TEST_FILE(const QString &filename)
{
    return QUrl::fromLocalFile(QLatin1String(SRCDIR) + QLatin1String("/data/") + filename);
}

void tst_compilation::boomblock()
{
    QFile f(SRCDIR + QLatin1String("/data/BoomBlock.qml"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray data = f.readAll();

    //get rid of initialization effects
    {
        QQmlComponent c(&engine);
        c.setData(data, QUrl());
    }

    QBENCHMARK {
        QQmlComponent c(&engine);
        c.setData(data, QUrl());
//        QVERIFY(c.isReady());
    }
}

void tst_compilation::jsparser_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("boomblock") << QString(SRCDIR + QLatin1String("/data/BoomBlock.qml"));
}

void tst_compilation::jsparser()
{
    QFETCH(QString, file);

    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray data = f.readAll();

    QTextStream stream(data, QIODevice::ReadOnly);
    const QString code = stream.readAll();

    QBENCHMARK {
        QQmlJS::Engine engine;

        QQmlJS::Lexer lexer(&engine);
        lexer.setCode(code, -1);

        QQmlJS::Parser parser(&engine);
        parser.parse();
        parser.ast();
    }
}

void tst_compilation::scriptparser_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("boomblock") << QString(SRCDIR + QLatin1String("/data/BoomBlock.qml"));
}

void tst_compilation::scriptparser()
{
    QFETCH(QString, file);

    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray data = f.readAll();

    //TODO(pvarga): check preparseData
    QByteArray preparseData;
    QUrl url = QUrl::fromLocalFile(file);
    QString urlString = url.toString();

    QBENCHMARK {
        QQmlScript::Parser parser;
        parser.parse(data, preparseData, url, urlString);
        parser.tree();
    }
}

QTEST_MAIN(tst_compilation)

#include "tst_compilation.moc"
