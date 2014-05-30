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
    color: "black"

    Rectangle {
        id: rect;
        anchors.centerIn: parent
        width: 1
        height: 10

        gradient: Gradient {
            GradientStop { position: 0; color: "#ff0000" }
            GradientStop { position: 0.5; color: "#00ff00" }
            GradientStop { position: 1; color: "#0000ff" }
        }
    }

    Text {
        id: text
        anchors.centerIn: parent
        font.pixelSize:  80
        text: "Shaderz!"
    }

    ShaderEffectSource {
        id: maskSource
        sourceItem: text
        hideSource: true
    }

    ShaderEffectSource {
        id: colorSource
        sourceItem: rect;
        hideSource: true
    }

    ShaderEffectItem {
        anchors.fill: text;

        property variant colorSource: colorSource
        property variant maskSource: maskSource;

        fragmentShader: "
        uniform lowp sampler2D maskSource;
        uniform lowp sampler2D colorSource;
        varying highp vec2 qt_TexCoord0;
        void main() {
            gl_FragColor = texture2D(maskSource, qt_TexCoord0).a * texture2D(colorSource, qt_TexCoord0.yx);
        }
        "
    }
}
