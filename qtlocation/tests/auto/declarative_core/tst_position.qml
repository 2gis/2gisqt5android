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
import QtTest 1.0
import QtPositioning 5.3

TestCase {
    id: testCase

    name: "Position"

    Position { id: defaultPosition }

    SignalSpy { id: latitudeValidSpy; target: defaultPosition; signalName: "latitudeValidChanged" }
    SignalSpy { id: longitudeValidSpy; target: defaultPosition; signalName: "longitudeValidChanged" }
    SignalSpy { id: altitudeValidSpy; target: defaultPosition; signalName: "altitudeValidChanged" }
    SignalSpy { id: timestampSpy; target: defaultPosition; signalName: "timestampChanged" }
    SignalSpy { id: speedSpy; target: defaultPosition; signalName: "speedChanged" }
    SignalSpy { id: speedValidSpy; target: defaultPosition; signalName: "speedValidChanged" }
    SignalSpy { id: coordinateSpy; target: defaultPosition; signalName: "coordinateChanged" }
    SignalSpy { id: horizontalAccuracySpy; target: defaultPosition; signalName: "horizontalAccuracyChanged" }
    SignalSpy { id: horizontalAccuracyValidSpy; target: defaultPosition; signalName: "horizontalAccuracyValidChanged" }
    SignalSpy { id: verticalAccuracySpy; target: defaultPosition; signalName: "verticalAccuracyChanged" }
    SignalSpy { id: verticalAccuracyValidSpy; target: defaultPosition; signalName: "verticalAccuracyValidChanged" }
    SignalSpy { id: directionSpy; target: defaultPosition; signalName: "directionChanged" }
    SignalSpy { id: verticalSpeedSpy; target: defaultPosition; signalName: "verticalSpeedChanged" }

    function test_defaults() {
        compare(defaultPosition.latitudeValid, false);
        compare(defaultPosition.longitudeValid, false);
        compare(defaultPosition.altitudeValid, false);
        compare(defaultPosition.speedValid, false);
        compare(defaultPosition.horizontalAccuracyValid, false);
        compare(defaultPosition.verticalAccuracyValid, false);
        verify(!defaultPosition.directionValid);
        verify(isNaN(defaultPosition.direction));
        verify(!defaultPosition.verticalSpeedValid);
        verify(isNaN(defaultPosition.verticalSpeed));
    }

    function test_modifiers() {
        latitudeValidSpy.clear();
        longitudeValidSpy.clear();
        altitudeValidSpy.clear();
        timestampSpy.clear();
        speedSpy.clear();
        speedValidSpy.clear();
        coordinateSpy.clear();
        horizontalAccuracySpy.clear();
        horizontalAccuracyValidSpy.clear();
        verticalAccuracySpy.clear();
        verticalAccuracyValidSpy.clear();
        directionSpy.clear();
        verticalSpeedSpy.clear();

        defaultPosition.horizontalAccuracy = 10;
        compare(horizontalAccuracySpy.count, 1);
        compare(horizontalAccuracyValidSpy.count, 1);
        compare(defaultPosition.horizontalAccuracy, 10);
        compare(defaultPosition.horizontalAccuracyValid, true);

        defaultPosition.verticalAccuracy = 10;
        compare(verticalAccuracySpy.count, 1);
        compare(verticalAccuracyValidSpy.count, 1);
        compare(defaultPosition.verticalAccuracy, 10);
        compare(defaultPosition.verticalAccuracyValid, true);

        // some extra precautions
        compare(horizontalAccuracyValidSpy.count, 1);
        compare(speedSpy.count, 0);
        compare(speedValidSpy.count, 0);
    }
}
