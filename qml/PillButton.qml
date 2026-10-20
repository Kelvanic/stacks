import QtQuick
import QtQuick.Controls
import "."

AbstractButton {
    id: button
    property string label: ""
    property string iconName: ""
    property bool primary: false
    property bool active: false
    horizontalPadding: 14
    implicitWidth: row.implicitWidth + horizontalPadding * 2
    implicitHeight: 36
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    Accessible.name: label
    Accessible.role: Accessible.Button
    Accessible.description: active ? "Selected" : ""
    scale: down ? 0.98 : 1
    Behavior on scale { NumberAnimation { duration: 90 } }

    background: Rectangle {
        radius: 8
        color: button.primary ? (button.hovered ? Theme.copperBright : Theme.copper)
             : button.active ? Theme.copperWash : button.hovered ? Theme.ink3 : Theme.ink1
        border.width: button.primary && !button.activeFocus ? 0 : 1
        border.color: button.activeFocus ? Theme.copper : button.active ? Theme.copperSoft : Theme.line
        opacity: button.enabled ? 1 : 0.4
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 7
            Icon {
                visible: button.iconName.length > 0
                name: button.iconName
                width: 15
                height: 15
                color: button.primary ? Theme.primaryText : button.active ? Theme.copper : Theme.textMuted
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                objectName: "buttonLabel"
                text: button.label
                color: button.primary ? Theme.primaryText : button.active ? Theme.copper : Theme.text
                font.family: Theme.fontUI
                font.pixelSize: 12
                font.weight: button.primary || button.active ? Font.DemiBold : Font.Medium
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; cursorShape: button.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
}
