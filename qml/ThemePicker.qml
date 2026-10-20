import QtQuick
import QtQuick.Controls
import "."

Item {
    id: picker
    implicitHeight: 60
    height: implicitHeight

    Text {
        text: "APPEARANCE"
        color: Theme.textFaint
        font.family: Theme.fontMono
        font.pixelSize: 10
        font.letterSpacing: 1
    }

    Text {
        anchors.right: parent.right
        text: controller.themeMode === "system" ? "AUTO · " + (Theme.dark ? "DARK" : "LIGHT") : ""
        color: Theme.textGhost
        font.family: Theme.fontMono
        font.pixelSize: 10
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 36
        radius: 9
        color: Theme.ink2
        border.width: 1
        border.color: Theme.line

        Row {
            anchors.fill: parent
            anchors.margins: 3
            spacing: 3

            Repeater {
                model: [
                    { key: "system", icon: "monitor", label: "System", hint: "Follow your computer’s appearance" },
                    { key: "light", icon: "sun", label: "Light", hint: "Always use the light theme" },
                    { key: "dark", icon: "moon", label: "Dark", hint: "Always use the dark theme" }
                ]
                delegate: AbstractButton {
                    id: option
                    objectName: "theme-" + modelData.key
                    width: (parent.width - 6) / 3
                    height: parent.height
                    checkable: true
                    autoExclusive: true
                    checked: controller.themeMode === modelData.key
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: modelData.label + " theme"
                    Accessible.description: modelData.hint
                    ToolTip.visible: hovered
                    ToolTip.text: modelData.hint
                    ToolTip.delay: 500
                    onClicked: controller.setThemeMode(modelData.key)
                    background: Rectangle {
                        radius: 6
                        color: option.checked ? Theme.ink1 : (option.hovered ? Theme.ink3 : "transparent")
                        border.width: option.activeFocus ? 1 : 0
                        border.color: Theme.copper
                    }
                    contentItem: Item {
                        Row {
                            anchors.centerIn: parent
                            spacing: 5
                            Icon {
                                name: modelData.icon
                                width: 13
                                height: 13
                                color: option.checked ? Theme.copper : Theme.textMuted
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.label
                                color: option.checked ? Theme.copper : Theme.textMuted
                                font.family: Theme.fontUI
                                font.pixelSize: 11
                                font.weight: option.checked ? Font.DemiBold : Font.Normal
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }
}
