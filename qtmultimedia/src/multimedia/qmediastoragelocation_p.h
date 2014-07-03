/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QMEDIASTORAGELOCATION_H
#define QMEDIASTORAGELOCATION_H

#include <qtmultimediadefs.h>
#include <QDir>
#include <QMap>
#include <QHash>
#include <QMutex>

QT_BEGIN_NAMESPACE

class Q_MULTIMEDIA_EXPORT QMediaStorageLocation
{
public:
    enum MediaType {
        Movies,
        Music,
        Pictures,
        Sounds
    };

    QMediaStorageLocation();

    void addStorageLocation(MediaType type, const QString &location);

    QDir defaultLocation(MediaType type) const;

    QString generateFileName(const QString &requestedName, MediaType type, const QString &prefix, const QString &extension) const;
    QString generateFileName(const QString &prefix, const QDir &dir, const QString &extension) const;

private:
    mutable QMutex m_mutex;
    mutable QHash<QString, qint64> m_lastUsedIndex;
    QMap<MediaType, QStringList> m_customLocations;
};

QT_END_NAMESPACE

#endif // QMEDIASTORAGELOCATION_H
