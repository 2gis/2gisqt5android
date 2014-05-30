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

#ifndef QQMLBUNDLE_P_H
#define QQMLBUNDLE_P_H

#include <QtCore/qfile.h>
#include <QtCore/qstring.h>
#include <private/qtqmlglobal_p.h>

#ifdef Q_CC_MSVC
// nonstandard extension used : zero-sized array in struct/union.
#  pragma warning( disable : 4200 )
#endif

QT_BEGIN_NAMESPACE

class Q_QML_PRIVATE_EXPORT QQmlBundle
{
    Q_DISABLE_COPY(QQmlBundle)
public:
    struct Q_QML_PRIVATE_EXPORT Entry
    {
        enum Kind {
            File = 123, // Normal file
            Skip,       // Empty space
            Link        // A meta data linked file

            // ### add entries for qmldir, index, ...
        };

        int kind;
        quint32 size;
    };

    struct Q_QML_PRIVATE_EXPORT RawEntry : public Entry
    {
        char data[]; // trailing data
    };

    struct Q_QML_PRIVATE_EXPORT FileEntry : public Entry
    {
        quint32 link;
        int fileNameLength;
        char data[]; // trailing data

        QString fileName() const;
        bool isFileName(const QString &) const;

        quint32 fileSize() const;
        const char *contents() const;
    };

    QQmlBundle(const QString &fileName);
    ~QQmlBundle();

    bool open(QIODevice::OpenMode mode = QIODevice::ReadWrite);
    void close();

    QList<const FileEntry *> files() const;
    void remove(const FileEntry *entry);
    bool add(const QString &fileName);
    bool add(const QString &name, const QString &fileName);

    bool addMetaLink(const QString &fileName,
                     const QString &linkName,
                     const QByteArray &data);

    const FileEntry *find(const QString &fileName) const;
    const FileEntry *find(const QChar *fileName, int length) const;

    const FileEntry *link(const FileEntry *, const QString &linkName) const;

    static int bundleHeaderLength();
    static bool isBundleHeader(const char *, int size);
private:
    const Entry *findInsertPoint(quint32 size, qint32 *offset);

private:
    QFile file;
    uchar *buffer;
    quint32 bufferSize;
    bool opened:1;
    bool headerWritten:1;
};

QT_END_NAMESPACE

#endif // QQMLBUNDLE_P_H
