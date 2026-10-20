import QtQuick
import QtQuick.Controls
import "."

AbstractButton {
    id: control
    property string label: ""
    property string iconName: "library"
    property int count: -1
    property bool selected: false
    implicitHeight: 42
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: label
    Accessible.description: (selected ? "Selected. " : "") + (count >= 0 ? count + " books" : "")
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    background: Rectangle {
        radius: 9
        color: control.selected ? Theme.copperWash : control.hovered ? Theme.ink2 : "transparent"
        border.width: control.activeFocus ? 1 : 0
        border.color: Theme.copper
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    contentItem: Item {
        Icon {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            width: 18; height: 18
            color: control.selected ? Theme.copper : Theme.textFaint
        }
        Text {
            x: 42
            anchors.verticalCenter: parent.verticalCenter
            text: control.label
            color: control.selected ? Theme.copper : Theme.textMuted
            font.family: Theme.fontUI
            font.pixelSize: 13
            font.weight: control.selected ? Font.DemiBold : Font.Medium
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 13
            anchors.verticalCenter: parent.verticalCenter
            visible: control.count >= 0
            text: control.count
            color: control.selected ? Theme.copper : Theme.textGhost
            font.family: Theme.fontMono
            font.pixelSize: 11
        }
    }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; cursorShape: Qt.PointingHandCursor }
}
