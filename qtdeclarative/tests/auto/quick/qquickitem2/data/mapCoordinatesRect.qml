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

import QtQuick 2.0

Item {
    id: root; objectName: "root"
    width: 200; height: 200

    Item { id: itemA; objectName: "itemA"; x: 50; y: 50; width: 25; height: 70 }

    Item {
        x: 50; y: 50
        rotation: 45
        Item { id: itemB; objectName: "itemB"; x: 100; y: 100; width: 30; height: 45 }
    }

    function mapAToB(x, y, w, h) {
        var pos = itemA.mapToItem(itemB, x, y, w, h)
        return Qt.rect(pos.x, pos.y, pos.width, pos.height)
    }

    function mapAFromB(x, y, w, h) {
        var pos = itemA.mapFromItem(itemB, x, y, w, h)
        return Qt.rect(pos.x, pos.y, pos.width, pos.height)
    }

    function mapAToNull(x, y, w, h) {
        var pos = itemA.mapToItem(null, x, y, w, h)
        return Qt.rect(pos.x, pos.y, pos.width, pos.height)
    }

    function mapAFromNull(x, y, w, h) {
        var pos = itemA.mapFromItem(null, x, y, w, h)
        return Qt.rect(pos.x, pos.y, pos.width, pos.height)
    }

    function checkMapAToInvalid(x, y, w, h) {
        var pos = itemA.mapToItem(1122, x, y, w, h)
        return pos == undefined;
    }

    function checkMapAFromInvalid(x, y, w, h) {
        var pos = itemA.mapFromItem(1122, x, y, w, h)
        return pos == undefined;
    }
}
