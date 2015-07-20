/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
import QtQuick 1.0
import Qt.labs.shaders 1.0

Rectangle {
    anchors.fill: parent;
    color: "white"

    Timer {
        running: true
        interval: 2000
        repeat: true
        onTriggered: {
            theSource.wrapMode == ShaderEffectSource.RepeatVertically ? theSource.wrapMode = ShaderEffectSource.ClampToEdge : theSource.wrapMode = ShaderEffectSource.RepeatVertically
        }
    }

    ShaderEffectSource {
        id: theSource
        sourceItem: Image { source: "image_small.png" }
        live: false
        hideSource: true
    }

    ShaderEffectItem {
        id: effect
        anchors.fill: parent;
        property variant source: theSource
        fragmentShader: "
        uniform lowp sampler2D source;
        varying highp vec2 qt_TexCoord0;
        void main() {
            highp vec2 tex = qt_TexCoord0 * 4.0;
            gl_FragColor = texture2D(source, tex);
        }
        "

    }

    Rectangle {
        width: parent.width
        height: 40
        color: "#cc000000"

        Text {
             id: label
             anchors.centerIn:  parent
             text: theSource.wrapMode == ShaderEffectSource.RepeatVertically ? "Wrap RepeatVertically" : "VerticalWrap ClampToEdge"
             color: "white"
             font.bold: true
        }
    }
}
