/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.1
import org.qtproject.examples.calendar 1.0

ApplicationWindow {
    visible: true
    width: 640
    height: 400
    minimumWidth: 400
    minimumHeight: 300
    color: "#f4f4f4"

    title: "Calendar Example"

    SqlEventModel {
        id: eventModel
    }

    Row {
        id: row
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        Rectangle {
            width: row.width * 0.4 - row.spacing / 2
            height: calendar.height
            border.color: Qt.darker(color, 1.2)
            Column {
                id: eventsPane
                anchors.fill: parent
                anchors.margins: spacing / 2
                spacing: 10

                Row {
                    id: eventDateRow
                    width: parent.width
                    height: eventDayLabel.height
                    spacing: 10

                    Label {
                        id: eventDayLabel
                        text: calendar.selectedDate.getDate()
                        font.pointSize: 35
                    }

                    Column {
                        height: eventDayLabel.height

                        Label {
                            readonly property var options: { weekday: "long" }
                            text: Qt.locale().standaloneDayName(calendar.selectedDate.getDay(), Locale.LongFormat)
                            font.pointSize: 18
                        }
                        Label {
                            text: Qt.locale().standaloneMonthName(calendar.selectedDate.getMonth())
                                  + calendar.selectedDate.toLocaleDateString(Qt.locale(), " yyyy")
                            font.pointSize: 12
                        }
                    }
                }

                ListView {
                    id: eventsListView
                    width: parent.width
                    height: parent.height - eventDateRow.height
                    spacing: 4
                    clip: true

                    model: eventModel.eventsForDate(calendar.selectedDate)
                    delegate: Rectangle {
                        width: eventsListView.width
                        height: eventItemColumn.height

                        Image {
                            anchors.top: parent.top
                            anchors.topMargin: 4
                            width: 12
                            height: width
                            source: "qrc:/images/eventindicator.png"
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: "#eee"
                        }

                        Column {
                            id: eventItemColumn
                            anchors.left: parent.left
                            anchors.leftMargin: 20
                            anchors.right: parent.right
                            height: timeLabel.height + nameLabel.height + 8

                            Label {
                                id: nameLabel
                                width: parent.width
                                wrapMode: Text.Wrap
                                text: modelData.name
                            }
                            Label {
                                id: timeLabel
                                width: parent.width
                                wrapMode: Text.Wrap
                                text: modelData.startDate.toLocaleTimeString(calendar.locale, Locale.ShortFormat)
                                color: "#aaa"
                            }
                        }
                    }
                }
            }
        }

        Calendar {
            id: calendar
            width: parent.width * 0.6 - row.spacing / 2
            height: parent.height
            selectedDate: new Date(2014, 0, 1)
            focus: true

            style: CalendarStyle {
                dayDelegate: Item {
                    readonly property color sameMonthDateTextColor: "#444"
                    readonly property color selectedDateColor: Qt.platform.os === "osx" ? "#3778d0" : __syspal.highlight
                    readonly property color selectedDateTextColor: "white"
                    readonly property color differentMonthDateTextColor: "#bbb"
                    readonly property color invalidDatecolor: "#dddddd"

                    Rectangle {
                        anchors.fill: parent
                        border.color: "transparent"
                        color: styleData.date !== undefined && styleData.selected ? selectedDateColor : "transparent"
                        anchors.margins: styleData.selected ? -1 : 0
                    }

                    Image {
                        visible: eventModel.eventsForDate(styleData.date).length > 0
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: -1
                        width: 12
                        height: width
                        source: "qrc:/images/eventindicator.png"
                    }

                    Label {
                        id: dayDelegateText
                        text: styleData.date.getDate()
                        font.pixelSize: 14
                        anchors.centerIn: parent
                        color: {
                            var color = invalidDatecolor;
                            if (styleData.valid) {
                                // Date is within the valid range.
                                color = styleData.visibleMonth ? sameMonthDateTextColor : differentMonthDateTextColor;
                                if (styleData.selected) {
                                    color = selectedDateTextColor;
                                }
                            }
                            color;
                        }
                    }
                }
            }
        }
    }
}
