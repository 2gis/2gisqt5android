/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#ifndef QDECLARATIVEPLACEATTRIBUTE_P_H
#define QDECLARATIVEPLACEATTRIBUTE_P_H

#include <QObject>
#include <QtQml/qqml.h>
#include <QString>

#include <qplaceattribute.h>

QT_BEGIN_NAMESPACE

class QDeclarativePlaceAttribute : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QPlaceAttribute attribute READ attribute WRITE setAttribute)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)

public:
    explicit QDeclarativePlaceAttribute(QObject *parent = 0);
    explicit QDeclarativePlaceAttribute(const QPlaceAttribute &src, QObject *parent = 0);
    ~QDeclarativePlaceAttribute();

    QPlaceAttribute attribute() const;
    void setAttribute(const QPlaceAttribute &place);

    QString text() const;
    void setText(const QString &text);


    QString label() const;
    void setLabel(const QString &label);

Q_SIGNALS:
    void labelChanged();
    void textChanged();

private:
    QPlaceAttribute m_attribute;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativePlaceAttribute)

#endif
