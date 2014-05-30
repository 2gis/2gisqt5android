/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.1
import QtQuick.Window 2.0

Item {
    id: root
    width: 580
    height: 400
    SystemPalette { id: palette }
    clip: true

    //! [dialog]
    Dialog {
        id: helloDialog
        modality: dialogModal.checked ? Qt.WindowModal : Qt.NonModal
        title: customizeTitle.checked ? windowTitleField.text : "Hello"
        onButtonClicked: console.log("clicked button " + clickedButton)
        onAccepted: lastChosen.text = "Accepted " +
            (clickedButton == StandardButton.Ok ? "(OK)" : (clickedButton == StandardButton.Retry ? "(Retry)" : "(Ignore)"))
        onRejected: lastChosen.text = "Rejected " +
            (clickedButton == StandardButton.Close ? "(Close)" : (clickedButton == StandardButton.Abort ? "(Abort)" : "(Cancel)"))
        onHelp: lastChosen.text = "Yelped for help!"
        onYes: lastChosen.text = (clickedButton == StandardButton.Yes ? "Yeessss!!" : "Yes, now and always")
        onNo: lastChosen.text = (clickedButton == StandardButton.No ? "Oh No." : "No, no, a thousand times no!")
        onApply: lastChosen.text = "Apply"
        onReset: lastChosen.text = "Reset"

        Text {
            text: "Hello world!"
        }
    }
    //! [dialog]

    Dialog {
        id: spinboxDialog
        modality: dialogModal.checked ? Qt.WindowModal : Qt.NonModal
        title: customizeTitle.checked ? windowTitleField.text : "Spinbox"
        onHelp: {
            lastChosen.text = "No help available, sorry.  Please answer the question."
            visible = true
        }
        onButtonClicked: {
            if (clickedButton === StandardButton.Ok && answer.value == 11.0)
                lastChosen.text = "You are correct!"
            else
                lastChosen.text = "Having failed to give the correct answer, you shall not pass!"
        }

        ColumnLayout {
            id: column
            width: parent ? parent.width : 100
            Text {
                text: "<b>What</b> is the average airspeed velocity of an unladen European swallow?"
                textFormat: Text.StyledText
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                SpinBox {
                    id: answer
                    onEditingFinished: spinboxDialog.click(StandardButton.Ok)
                }
                Text {
                    text: "m/s"
                    Layout.alignment: Qt.AlignBaseline | Qt.AlignLeft
                }
            }
        }
    }

    Dialog {
        id: dateDialog
        modality: dialogModal.checked ? Qt.WindowModal : Qt.NonModal
        title: customizeTitle.checked ? windowTitleField.text : "Choose a date"
        onButtonClicked: console.log("clicked button " + clickedButton)
        onAccepted: {
            if (clickedButton == StandardButton.Ok)
                lastChosen.text = "Accepted " + calendar.selectedDate.toLocaleDateString()
            else
                lastChosen.text = (clickedButton == StandardButton.Retry ? "(Retry)" : "(Ignore)")
        }

        Calendar {
            id: calendar
            width: parent ? parent.width : implicitWidth
            onDoubleClicked: dateDialog.click(StandardButton.Ok)
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        Text {
            color: palette.windowText
            font.bold: true
            text: "Message dialog properties:"
        }
        CheckBox {
            id: dialogModal
            text: "Modal"
            checked: true
            Binding on checked { value: helloDialog.modality != Qt.NonModal }
        }
        CheckBox {
            id: customizeTitle
            text: "Window Title"
            checked: true
            width: parent.width
            TextField {
                id: windowTitleField
                anchors.right: parent.right
                text: "Custom Dialog"
                width: root.width - customizeTitle.implicitWidth - 30
            }
        }

        Flow {
            spacing: 8
            width: parent.width
            property bool updating: false
            function updateButtons(button, checked) {
                if (updating) return
                updating = true
                var buttons = 0
                for (var i = 0; i < children.length; ++i)
                    if (children[i].checked)
                        buttons |= children[i].button
                if (!buttons)
                    buttons = StandardButton.Ok
                helloDialog.standardButtons = buttons
                spinboxDialog.standardButtons = buttons
                dateDialog.standardButtons = buttons
                updating = false
            }

            CheckBox {
                text: "Help"
                property int button: StandardButton.Help
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "Abort"
                property int button: StandardButton.Abort
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "Close"
                property int button: StandardButton.Close
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "Cancel"
                property int button: StandardButton.Cancel
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "NoToAll"
                property int button: StandardButton.NoToAll
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "No"
                property int button: StandardButton.No
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "YesToAll"
                property int button: StandardButton.YesToAll
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "Yes"
                property int button: StandardButton.Yes
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "Ignore"
                property int button: StandardButton.Ignore
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "Retry"
                property int button: StandardButton.Retry
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            CheckBox {
                text: "OK"
                checked: true
                property int button: StandardButton.Ok
                onCheckedChanged: parent.updateButtons(button, checked)
            }

            Component.onCompleted: updateButtons()
        }
        Text {
            id: lastChosen
        }
    }

    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: buttonRow.height * 1.2
        color: Qt.darker(palette.window, 1.1)
        border.color: Qt.darker(palette.window, 1.3)
        Row {
            id: buttonRow
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            width: parent.width
            Button {
                text: "Hello World dialog"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: helloDialog.open()
            }
            Button {
                text: "Input dialog"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: spinboxDialog.open()
            }
            Button {
                text: "Date picker"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: dateDialog.open()
            }
        }
    }
}
