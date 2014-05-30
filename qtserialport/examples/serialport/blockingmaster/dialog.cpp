/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
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

#include "dialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QGridLayout>

#include <QtSerialPort/QSerialPortInfo>

QT_USE_NAMESPACE

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , transactionCount(0)
    , serialPortLabel(new QLabel(tr("Serial port:")))
    , serialPortComboBox(new QComboBox())
    , waitResponseLabel(new QLabel(tr("Wait response, msec:")))
    , waitResponseSpinBox(new QSpinBox())
    , requestLabel(new QLabel(tr("Request:")))
    , requestLineEdit(new QLineEdit(tr("Who are you?")))
    , trafficLabel(new QLabel(tr("No traffic.")))
    , statusLabel(new QLabel(tr("Status: Not running.")))
    , runButton(new QPushButton(tr("Start")))
{
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
        serialPortComboBox->addItem(info.portName());

    waitResponseSpinBox->setRange(0, 10000);
    waitResponseSpinBox->setValue(1000);

    QGridLayout *mainLayout = new QGridLayout;
    mainLayout->addWidget(serialPortLabel, 0, 0);
    mainLayout->addWidget(serialPortComboBox, 0, 1);
    mainLayout->addWidget(waitResponseLabel, 1, 0);
    mainLayout->addWidget(waitResponseSpinBox, 1, 1);
    mainLayout->addWidget(runButton, 0, 2, 2, 1);
    mainLayout->addWidget(requestLabel, 2, 0);
    mainLayout->addWidget(requestLineEdit, 2, 1, 1, 3);
    mainLayout->addWidget(trafficLabel, 3, 0, 1, 4);
    mainLayout->addWidget(statusLabel, 4, 0, 1, 5);
    setLayout(mainLayout);

    setWindowTitle(tr("Blocking Master"));
    serialPortComboBox->setFocus();

    connect(runButton, SIGNAL(clicked()),
            this, SLOT(transaction()));
    connect(&thread, SIGNAL(response(QString)),
            this, SLOT(showResponse(QString)));
    connect(&thread, SIGNAL(error(QString)),
            this, SLOT(processError(QString)));
    connect(&thread, SIGNAL(timeout(QString)),
            this, SLOT(processTimeout(QString)));
}

void Dialog::transaction()
{
    setControlsEnabled(false);
    statusLabel->setText(tr("Status: Running, connected to port %1.")
                         .arg(serialPortComboBox->currentText()));
    thread.transaction(serialPortComboBox->currentText(),
                       waitResponseSpinBox->value(),
                       requestLineEdit->text());
}

void Dialog::showResponse(const QString &s)
{
    setControlsEnabled(true);
    trafficLabel->setText(tr("Traffic, transaction #%1:"
                             "\n\r-request: %2"
                             "\n\r-response: %3")
                          .arg(++transactionCount).arg(requestLineEdit->text()).arg(s));
}

void Dialog::processError(const QString &s)
{
    setControlsEnabled(true);
    statusLabel->setText(tr("Status: Not running, %1.").arg(s));
    trafficLabel->setText(tr("No traffic."));
}

void Dialog::processTimeout(const QString &s)
{
    setControlsEnabled(true);
    statusLabel->setText(tr("Status: Running, %1.").arg(s));
    trafficLabel->setText(tr("No traffic."));
}

void Dialog::setControlsEnabled(bool enable)
{
    runButton->setEnabled(enable);
    serialPortComboBox->setEnabled(enable);
    waitResponseSpinBox->setEnabled(enable);
    requestLineEdit->setEnabled(enable);
}
