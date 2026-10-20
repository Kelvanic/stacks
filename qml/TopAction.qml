import QtQuick
import "Theme.js" as Theme

// An icon button that explains itself: the label rises out of the button
// on hover instead of sitting there permanently taking up the top bar.
Item {
    id: action

    property string icon: ""
    property string label: ""
    property string shortcut: ""
    property bool active: false
    signal clicked()

    implicitWidth: 34
    implicitHeight: 34

    Rectangle {
        anchors.fill: parent
        radius: 9
        color: action.active ? Theme.copperWash
                             : (hover.hovered && action.enabled ? Theme.ink3 : "transparent")
        border.width: 1
        border.color: action.active ? Theme.copperSoft : "transparent"
        Behavior on color { ColorAnimation { duration: 130 } }
    }

    Icon {
        anchors.centerIn: parent
        width: 17
        height: 17
        name: action.icon
        color: action.active ? Theme.copper
                            : (action.enabled ? (hover.hovered ? Theme.text : Theme.textMuted) : Theme.textGhost)
        Behavior on color { ColorAnimation { duration: 130 } }
    }

    // Hover label.
    Rectangle {
        id: tip
        visible: hover.hovered && action.label.length > 0 && action.enabled
        opacity: visible ? 1 : 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        anchors.bottomMargin: 7
        width: tipRow.implicitWidth + 18
        height: 26
        radius: 7
        color: Theme.ink3
        border.width: 1
        border.color: Theme.lineStrong
        z: 50

        Behavior on opacity { NumberAnimation { duration: 120 } }

        Row {
            id: tipRow
            anchors.centerIn: parent
            spacing: 7

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: action.label
                color: Theme.text
                font.family: Theme.fontUI
                font.pixelSize: 11
            }

            KeyCap {
                anchors.verticalCenter: parent.verticalCenter
                visible: action.shortcut.length > 0
                label: action.shortcut
            }
        }
    }

    HoverHandler { id: hover }

    MouseArea {
        anchors.fill: parent
        enabled: action.enabled
        cursorShape: action.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: action.clicked()
    }
}
