/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
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

/*
    The test verifies that batching does not interfere with overlapping
    regions.

    #samples: 8
                 PixelPos     R    G    B    Error-tolerance
    #final:       10  10     0.0  0.0  0.0        0.0
    #final:       20  10     1.0  1.0  1.0        0.0
    #final:       30  10     0.0  0.0  0.0        0.0
    #final:       40  10     0.5  0.5  0.5        0.1
    #final:       50  10     0.0  0.0  0.0        0.0
    #final:       60  10     1.0  1.0  1.0        0.0
    #final:       70  10     0.0  0.0  0.0        0.0
    #final:       80  10     0.5  0.5  0.5        0.1
*/

RenderTestBase
{
    Item {
        x: 10
        y: 10
        scale: 10
        Image { x: 0; source: "blacknwhite.png"; smooth: false }
        Image { x: 2; source: "blacknwhite.png"; smooth: true }
        Image { x: 4; source: "blacknwhite.png"; smooth: false }
        Image { x: 6; source: "blacknwhite.png"; smooth: true }
    }

    finalStageComplete: true
}
