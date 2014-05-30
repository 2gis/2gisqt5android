/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include "qandroidmediastoragelocation.h"

#include "jmultimediautils.h"

QT_BEGIN_NAMESPACE

QAndroidMediaStorageLocation::QAndroidMediaStorageLocation()
{
}

QDir QAndroidMediaStorageLocation::defaultDir(CaptureSource source) const
{
    QStringList dirCandidates;

    if (source == Camera)
        dirCandidates << JMultimediaUtils::getDefaultMediaDirectory(JMultimediaUtils::DCIM);
    else
        dirCandidates << JMultimediaUtils::getDefaultMediaDirectory(JMultimediaUtils::Sounds);
    dirCandidates << QDir::homePath();
    dirCandidates << QDir::currentPath();
    dirCandidates << QDir::tempPath();

    Q_FOREACH (const QString &path, dirCandidates) {
        if (QFileInfo(path).isWritable())
            return QDir(path);
    }

    return QDir();
}

QString QAndroidMediaStorageLocation::generateFileName(const QString &requestedName,
                                                       CaptureSource source,
                                                       const QString &prefix,
                                                       const QString &extension) const
{
    if (requestedName.isEmpty())
        return generateFileName(prefix, defaultDir(source), extension);

    QString path = requestedName;

    if (QFileInfo(path).isRelative())
        path = defaultDir(source).absoluteFilePath(path);

    if (QFileInfo(path).isDir())
        return generateFileName(prefix, QDir(path), extension);

    if (!path.endsWith(extension))
        path.append(QString(".%1").arg(extension));

    return path;
}

QString QAndroidMediaStorageLocation::generateFileName(const QString &prefix,
                                                       const QDir &dir,
                                                       const QString &extension) const
{
    QMutexLocker lock(&m_mutex);

    const QString lastMediaKey = dir.absolutePath() + QLatin1Char(' ') + prefix + QLatin1Char(' ') + extension;
    qint64 lastMediaIndex = m_lastUsedIndex.value(lastMediaKey, 0);

    if (lastMediaIndex == 0) {
        // first run, find the maximum media number during the fist capture
        Q_FOREACH (const QString &fileName, dir.entryList(QStringList() << QString("%1*.%2").arg(prefix).arg(extension))) {
            const qint64 mediaIndex = fileName.midRef(prefix.length(), fileName.size() - prefix.length() - extension.length() - 1).toInt();
            lastMediaIndex = qMax(lastMediaIndex, mediaIndex);
        }
    }

    // don't just rely on cached lastMediaIndex value,
    // someone else may create a file after camera started
    while (true) {
        const QString name = QString("%1%2.%3").arg(prefix)
                                               .arg(lastMediaIndex + 1, 8, 10, QLatin1Char('0'))
                                               .arg(extension);

        const QString path = dir.absoluteFilePath(name);
        if (!QFileInfo(path).exists()) {
            m_lastUsedIndex[lastMediaKey] = lastMediaIndex + 1;
            return path;
        }

        lastMediaIndex++;
    }

    return QString();
}

QT_END_NAMESPACE
