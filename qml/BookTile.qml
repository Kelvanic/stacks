import QtQuick
import QtQuick.Controls
import "."

AbstractButton {
    id: tile
    property var book: ({})
    property bool compact: false
    readonly property real progress: Math.max(0, Math.min(1, book.progress || 0))
    readonly property string statusText: progress >= 0.999 ? "Finished" : progress > 0.001 ? "In progress" : "Not started"
    readonly property color statusColor: progress >= 0.999 ? Theme.moss : progress > 0.001 ? Theme.copper : Theme.textFaint
    implicitHeight: compact ? 88 : 330
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    Accessible.name: (book.title || "Untitled") + " by " + (book.author || "Unknown author")
    Accessible.description: Math.round(progress * 100) + "% read. Open book."
    onClicked: controller.openBook(book.id)

    background: Rectangle {
        radius: 12
        color: Theme.ink1
        border.width: 1
        border.color: tile.activeFocus ? Theme.copper : tile.hovered ? Theme.lineStrong : Theme.lineSoft
        Behavior on border.color { ColorAnimation { duration: 130 } }
    }
    contentItem: Loader {
        sourceComponent: tile.compact ? listContent : gridContent
    }

    Component {
        id: gridContent
        Item {
            Rectangle {
                id: stage
                x: 8; y: 8; width: parent.width - 16; height: 204; radius: 8
                color: Theme.stage
                Rectangle {
                    anchors.fill: parent; radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.alpha(tile.book.accent || Theme.copper, Theme.dark ? 0.07 : 0.045) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                }
                // A small format label and a progress mark, not extra actions.
                Text { x: 12; y: 11; text: (tile.book.format || "book").toUpperCase(); color: Theme.textFaint; font.family: Theme.fontMono; font.pixelSize: 8; font.letterSpacing: 1 }
                Icon { visible: tile.progress >= 0.999; anchors.right: parent.right; anchors.rightMargin: 11; y: 10; name: "check"; color: Theme.moss; width: 13; height: 13 }
                Rectangle {
                    anchors.horizontalCenter: jacket.horizontalCenter
                    y: jacket.y + 7
                    width: jacket.width + 6; height: jacket.height; radius: 3
                    color: Theme.deepShadow
                    opacity: tile.hovered ? 0.7 : 0.45
                }
                Cover {
                    id: jacket
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 4
                    width: 108; height: 158
                    title: tile.book.title || "Untitled"
                    author: tile.book.author || ""
                    accent: tile.book.accent || Theme.copper
                    format: tile.book.format || "pdf"
                    transform: Translate { y: tile.hovered ? -5 : 0; Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } } }
                    rotation: tile.hovered ? -3 : 0
                    Behavior on rotation { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    opacity: tile.hovered || tile.activeFocus ? 1 : 0
                    anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10
                    width: 30; height: 30; radius: 15
                    color: Theme.copper
                    Behavior on opacity { NumberAnimation { duration: 130 } }
                    Icon { anchors.centerIn: parent; width: 15; height: 15; name: "arrow-right"; color: Theme.primaryText }
                }
            }
            Column {
                x: 16; y: 226; width: parent.width - 32; spacing: 6
                Text {
                    width: parent.width
                    height: 34
                    text: tile.book.title || "Untitled"
                    color: Theme.text; font.family: Theme.fontUI; font.pixelSize: 13; font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                }
                Text { width: parent.width; text: tile.book.author || "Unknown author"; color: Theme.textFaint; font.family: Theme.fontUI; font.pixelSize: 11; elide: Text.ElideRight }
            }
            Item {
                x: 16; y: parent.height - 36; width: parent.width - 32; height: 15
                Row {
                    spacing: 5
                    Rectangle { width: 4; height: 4; radius: 2; anchors.verticalCenter: parent.verticalCenter; color: tile.statusColor }
                    Text { text: tile.statusText; color: tile.statusColor; font.family: Theme.fontUI; font.pixelSize: 10 }
                }
                Text { visible: tile.progress > 0.001; anchors.right: parent.right; text: Math.round(tile.progress * 100) + "%"; color: tile.statusColor; font.family: Theme.fontMono; font.pixelSize: 9 }
            }
            Rectangle {
                x: 16; y: parent.height - 15; width: parent.width - 32; height: 2; radius: 1; color: Theme.lineSoft
                Rectangle { width: parent.width * tile.progress; height: 2; radius: 1; color: tile.statusColor }
            }
        }
    }
    Component {
        id: listContent
        Item {
            Cover {
                x: 16; anchors.verticalCenter: parent.verticalCenter
                width: 38; height: 56
                title: tile.book.title || ""; author: tile.book.author || ""
                accent: tile.book.accent || Theme.copper; format: tile.book.format || "pdf"
            }
            Column {
                x: 72; anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.49 - 72; spacing: 7
                Text { width: parent.width; text: tile.book.title || "Untitled"; color: Theme.text; font.family: Theme.fontUI; font.pixelSize: 13; font.weight: Font.Medium; elide: Text.ElideRight }
                Text { width: parent.width; text: tile.book.author || "Unknown author"; color: Theme.textFaint; font.family: Theme.fontUI; font.pixelSize: 11; elide: Text.ElideRight }
            }
            Text { x: parent.width * 0.54; anchors.verticalCenter: parent.verticalCenter; text: (tile.book.format || "").toUpperCase(); color: Theme.textFaint; font.family: Theme.fontMono; font.pixelSize: 10 }
            Column {
                x: parent.width * 0.68; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.23; spacing: 9
                Text { text: tile.statusText + (tile.progress > 0.001 ? " · " + Math.round(tile.progress * 100) + "%" : ""); color: tile.statusColor; font.family: Theme.fontUI; font.pixelSize: 10 }
                Rectangle { width: parent.width; height: 2; radius: 1; color: Theme.lineSoft; Rectangle { width: parent.width * tile.progress; height: 2; radius: 1; color: tile.statusColor } }
            }
            Icon { anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; width: 15; height: 15; name: "arrow-right"; color: tile.hovered ? Theme.copper : Theme.textFaint }
        }
    }
}
