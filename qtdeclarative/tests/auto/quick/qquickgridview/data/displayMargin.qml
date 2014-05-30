/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd.
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

import QtQuick 2.3

Item {
    width: 400; height: 400

    GridView {
        id: view
        anchors.top: header.bottom
        anchors.bottom: footer.top
        width: parent.width

        cellWidth: 50
        cellHeight: 25

        cacheBuffer: 0
        displayMarginBeginning: 60
        displayMarginEnd: 60

        model: 200
        delegate: Rectangle {
            objectName: "delegate"
            width: 50
            height: 25
            color: index % 2 ? "steelblue" : "lightsteelblue"
            Text {
                anchors.centerIn: parent
                text: index
            }
        }
    }

    Rectangle {
        id: header
        width: parent.width; height: 60
        color: "#80FF0000"
    }

    Rectangle {
        id: footer
        anchors.bottom: parent.bottom
        width: parent.width; height: 60
        color: "#80FF0000"
    }
}
