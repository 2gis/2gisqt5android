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
            effect.fragmentShader == effect.redFragmentShader ?  effect.fragmentShader = effect.greenFragmentShader :  effect.fragmentShader = effect.redFragmentShader
        }
    }

    ShaderEffectItem {
        id: effect
        anchors.fill: parent;

        property string redFragmentShader: "
            varying highp vec2 qt_TexCoord0;
            void main() {
                gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            }
        "

        property string greenFragmentShader: "
            varying highp vec2 qt_TexCoord0;
            void main() {
                gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
            }
        "

        fragmentShader: redFragmentShader
    }

    Rectangle {
        width: parent.width
        height: 40
        color: "#cc000000"

        Text {
             id: label
             anchors.centerIn:  parent
             text: effect.fragmentShader == effect.redFragmentShader ? "Effect (display should be red)" : "Effect (display should be green)"
             color: "white"
             font.bold: true
        }
    }
}
