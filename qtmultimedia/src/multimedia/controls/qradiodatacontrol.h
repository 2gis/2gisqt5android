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

#ifndef QRADIODATACONTROL_H
#define QRADIODATACONTROL_H

#include <QtMultimedia/qmediacontrol.h>
#include <QtMultimedia/qradiodata.h>

QT_BEGIN_NAMESPACE

// Required for QDoc workaround
class QString;

class Q_MULTIMEDIA_EXPORT QRadioDataControl : public QMediaControl
{
    Q_OBJECT

public:
    ~QRadioDataControl();

    virtual QString stationId() const = 0;
    virtual QRadioData::ProgramType programType() const = 0;
    virtual QString programTypeName() const = 0;
    virtual QString stationName() const = 0;
    virtual QString radioText() const = 0;
    virtual void setAlternativeFrequenciesEnabled(bool enabled) = 0;
    virtual bool isAlternativeFrequenciesEnabled() const = 0;

    virtual QRadioData::Error error() const = 0;
    virtual QString errorString() const = 0;

Q_SIGNALS:
    void stationIdChanged(QString stationId);
    void programTypeChanged(QRadioData::ProgramType programType);
    void programTypeNameChanged(QString programTypeName);
    void stationNameChanged(QString stationName);
    void radioTextChanged(QString radioText);
    void alternativeFrequenciesEnabledChanged(bool enabled);
    void error(QRadioData::Error err);

protected:
    QRadioDataControl(QObject *parent = 0);
};

#define QRadioDataControl_iid "org.qt-project.qt.radiodatacontrol/5.0"
Q_MEDIA_DECLARE_CONTROL(QRadioDataControl, QRadioDataControl_iid)

QT_END_NAMESPACE


#endif  // QRADIODATACONTROL_H
