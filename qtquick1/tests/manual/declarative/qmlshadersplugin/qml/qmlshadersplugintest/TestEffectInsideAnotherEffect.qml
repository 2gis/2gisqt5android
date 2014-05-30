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
    color: "green"

    Timer {
        running: true
        interval: 2000
        repeat: true
        onTriggered: {

        }
    }

    Rectangle {
        id: theSource
        color: "red"
        anchors.centerIn: parent;
        width: parent.width/2
        height: parent.height/2
    }

    ShaderEffectItem {
        id: effect1
        anchors.fill: theSource;
        property variant source: ShaderEffectSource{ sourceItem: theSource; hideSource: true }
    }

    ShaderEffectItem {
        id: effect2
        anchors.fill: effect1;
        property variant source: effect1

        fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            uniform sampler2D source;
            void main(void)
            {
                gl_FragColor = vec4(texture2D(source, qt_TexCoord0.st).rgb, 1.0);
            }
        "
    }

    ShaderEffectItem {
         id: effect3
         x: effect2.x
         y: effect2.y
         width: effect2.width
         height: effect2.height

         property variant source: ShaderEffectSource { sourceItem: effect2 ; hideSource: false }

         fragmentShader:
             "
              varying highp vec2 qt_TexCoord0;
              uniform sampler2D source;
              void main(void)
                  {
                      gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
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
             text: "Red rect inside green fullscreen rect."
             color: "white"
             font.bold: true
        }
    }
}
