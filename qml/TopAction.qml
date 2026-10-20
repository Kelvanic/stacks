import QtQuick
import QtQuick.Controls
import "."

AbstractButton {
    id: action
    property string iconName: ""
    property string label: ""
    property string shortcut: ""
    property bool active: false
    implicitWidth: 34
    implicitHeight: 34
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    Accessible.name: label
    Accessible.description: shortcut
    ToolTip.visible: hovered || activeFocus
    ToolTip.text: label + (shortcut.length ? "  " + shortcut : "")
    ToolTip.delay: 400
    ToolTip.timeout: 4000

    background: Rectangle {
        radius: 8
        color: action.active ? Theme.copperWash : action.hovered && action.enabled ? Theme.ink3 : "transparent"
        border.width: 1
        border.color: action.activeFocus ? Theme.copper : action.active ? Theme.copperSoft : "transparent"
        Behavior on color { ColorAnimation { duration: 130 } }
    }
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            width: 18
            height: 18
            name: action.iconName
            color: !action.enabled ? Theme.textGhost : action.active ? Theme.copper : action.hovered ? Theme.text : Theme.textMuted
            opacity: action.enabled ? 1 : 0.4
        }
    }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; cursorShape: action.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
}
