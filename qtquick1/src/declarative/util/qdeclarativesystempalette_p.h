/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#ifndef QDECLARATIVESYSTEMPALETTE_H
#define QDECLARATIVESYSTEMPALETTE_H

#include <qdeclarative.h>

#include <QtCore/qobject.h>
#include <QPalette>

QT_BEGIN_NAMESPACE

QT_MODULE(Declarative)

class QDeclarativeSystemPalettePrivate;
class Q_AUTOTEST_EXPORT QDeclarativeSystemPalette : public QObject
{
    Q_OBJECT
    Q_ENUMS(ColorGroup)
    Q_DECLARE_PRIVATE(QDeclarativeSystemPalette)

    Q_PROPERTY(QDeclarativeSystemPalette::ColorGroup colorGroup READ colorGroup WRITE setColorGroup NOTIFY paletteChanged)
    Q_PROPERTY(QColor window READ window NOTIFY paletteChanged)
    Q_PROPERTY(QColor windowText READ windowText NOTIFY paletteChanged)
    Q_PROPERTY(QColor base READ base NOTIFY paletteChanged)
    Q_PROPERTY(QColor text READ text NOTIFY paletteChanged)
    Q_PROPERTY(QColor alternateBase READ alternateBase NOTIFY paletteChanged)
    Q_PROPERTY(QColor button READ button NOTIFY paletteChanged)
    Q_PROPERTY(QColor buttonText READ buttonText NOTIFY paletteChanged)
    Q_PROPERTY(QColor light READ light NOTIFY paletteChanged)
    Q_PROPERTY(QColor midlight READ midlight NOTIFY paletteChanged)
    Q_PROPERTY(QColor dark READ dark NOTIFY paletteChanged)
    Q_PROPERTY(QColor mid READ mid NOTIFY paletteChanged)
    Q_PROPERTY(QColor shadow READ shadow NOTIFY paletteChanged)
    Q_PROPERTY(QColor highlight READ highlight NOTIFY paletteChanged)
    Q_PROPERTY(QColor highlightedText READ highlightedText NOTIFY paletteChanged)

public:
    QDeclarativeSystemPalette(QObject *parent=0);
    ~QDeclarativeSystemPalette();

    enum ColorGroup { Active = QPalette::Active, Inactive = QPalette::Inactive, Disabled = QPalette::Disabled };

    QColor window() const;
    QColor windowText() const;

    QColor base() const;
    QColor text() const;
    QColor alternateBase() const;

    QColor button() const;
    QColor buttonText() const;

    QColor light() const;
    QColor midlight() const;
    QColor dark() const;
    QColor mid() const;
    QColor shadow() const;

    QColor highlight() const;
    QColor highlightedText() const;

    QDeclarativeSystemPalette::ColorGroup colorGroup() const;
    void setColorGroup(QDeclarativeSystemPalette::ColorGroup);

Q_SIGNALS:
    void paletteChanged();

private:
    bool eventFilter(QObject *watched, QEvent *event);
    bool event(QEvent *event);

};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativeSystemPalette)

#endif // QDECLARATIVESYSTEMPALETTE_H
