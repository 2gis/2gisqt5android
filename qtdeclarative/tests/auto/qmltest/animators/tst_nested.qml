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

import QtQuick 2.2
import QtTest 1.0

Item {
    id: root;
    width: 200
    height: 200

    TestCase {
        id: testCase
        name: "animators-nested"
        when: !animation.running
        function test_endresult() {
            compare(box.before, 2);
            compare(box.after, 2);
        }
    }

    Box {
        id: box

        anchors.centerIn: undefined

        property int before: 0;
        property int after: 0;

        SequentialAnimation {
            id: animation;
            ScriptAction { script: box.before++; }
            ParallelAnimation {
                ScaleAnimator { target: box; from: 2.0; to: 1; duration: 100; }
                OpacityAnimator { target: box; from: 0; to: 1; duration: 100; }
            }
            PauseAnimation { duration: 100 }
            SequentialAnimation {
                ParallelAnimation {
                    XAnimator { target: box; from: 0; to: 100; duration: 100 }
                    RotationAnimator { target: box; from: 0; to: 90; duration: 100 }
                }
                ParallelAnimation {
                    XAnimator { target: box; from: 100; to: 0; duration: 100 }
                    RotationAnimator { target: box; from: 90; to: 0; duration: 100 }
                }
            }
            ScriptAction { script: box.after++; }
            running: true
            loops: 2
        }
    }

}
