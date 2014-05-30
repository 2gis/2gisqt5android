import QtQuick 1.0

Rectangle {
    color: "white"
    width: 120
    height: 240
    id: page
    ListModel {
        id: model
        ListElement {
            name: "palegoldenrod"
        }
        ListElement {
            name: "lightsteelblue"
        }
    }
    Component {
        id: delegate
        Rectangle {
            color: "thistle"
            width: 100
            height: 100
            Rectangle {
                anchors.centerIn: parent
                width: 60
                height: 60
                color: name 
            }
        }
    }
    Column{
        Repeater{
            model: model
            delegate: delegate
        }
    }
}
