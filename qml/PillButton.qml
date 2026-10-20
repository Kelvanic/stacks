import QtQuick
import "Theme.js" as Theme

// The house button: a quiet plate that warms up on hover and turns solid
// when it is the primary action.
Item {
    id: button

    property string label: ""
    property string icon: ""
    property bool primary: false
    property bool enabled: true
    property bool active: false
    property int horizontalPadding: 14

    signal clicked()

    implicitWidth: row.implicitWidth + horizontalPadding * 2
    implicitHeight: 34

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: {
            if (!button.enabled)
                return "transparent"
            if (button.primary)
                return hover.hovered ? Theme.copperBright : Theme.copper
            if (button.active)
                return Theme.copperWash
            return hover.hovered ? Theme.ink3 : Theme.ink2
        }
        border.width: button.primary ? 0 : 1
        border.color: button.active ? Theme.copperSoft : Theme.line
        opacity: button.enabled ? 1 : 0.4

        Behavior on color { ColorAnimation { duration: 120 } }
    }

    HoverHandler {
        id: hover
        enabled: button.enabled
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 7

        Icon {
            visible: button.icon.length > 0
            name: button.icon
            width: 15
            height: 15
            color: button.primary ? "#1A130D"
                                : (button.active ? Theme.copper
                                                 : (button.enabled ? Theme.textMuted : Theme.textFaint))
        }

        Text {
            visible: button.label.length > 0
            text: button.label
            color: button.primary ? "#1A130D"
                                  : (button.active ? Theme.copper
                                                   : (button.enabled ? Theme.text : Theme.textFaint))
            font.family: Theme.fontUI
            font.pixelSize: 12
            font.weight: button.primary ? Font.DemiBold : Font.Medium
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: button.enabled
        cursorShape: button.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: button.clicked()
    }
}
