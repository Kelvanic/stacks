import QtQuick
import "Theme.js" as Theme

Item {
    id: row

    property var book: ({})
    property bool selected: false
    signal activated()

    implicitHeight: 70

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        radius: 10
        color: row.selected ? Theme.ink2 : (hover.hovered ? Theme.ink2 : "transparent")
        Behavior on color { ColorAnimation { duration: 130 } }
    }

    // A copper spine marks the book you are in.
    Rectangle {
        visible: row.selected
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 2
        height: 34
        radius: 1
        color: Theme.copper
    }

    HoverHandler { id: hover }

    Cover {
        id: jacket
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 36
        height: 50
        title: row.book.title || ""
        author: row.book.author || ""
        accent: row.book.accent || Theme.copper
        format: row.book.format || "pdf"
        dim: !row.selected && row.book.progress > 0 && row.book.progress < 1

        Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
    }

    Column {
        anchors.left: jacket.right
        anchors.leftMargin: 13
        anchors.right: parent.right
        anchors.rightMargin: 30
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5

        Text {
            width: parent.width
            text: row.book.title || "Untitled"
            color: Theme.text
            font.family: Theme.fontUI
            font.pixelSize: 14
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        Row {
            spacing: 7
            Text {
                text: row.book.author || "Unknown author"
                color: Theme.textMuted
                font.family: Theme.fontUI
                font.pixelSize: 11
                elide: Text.ElideRight
                width: Math.min(implicitWidth, row.width - 150)
            }
            Text {
                text: "·"
                color: Theme.textGhost
                font.pixelSize: 11
            }
            Text {
                text: (row.book.format || "").toUpperCase()
                color: Theme.textFaint
                font.family: Theme.fontMono
                font.pixelSize: 9
                font.letterSpacing: 0.8
            }
        }

        // Reading progress, hairline.
        Item {
            width: parent.width
            height: 2
            Rectangle {
                anchors.fill: parent
                radius: 1
                color: Theme.lineSoft
            }
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, row.book.progress || 0))
                height: 2
                radius: 1
                color: Theme.copper
            }
        }
    }

    Icon {
        id: chevron
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        width: 14
        height: 14
        name: "chevron-right"
        color: Theme.textFaint
        opacity: hover.hovered ? 0.9 : 0
        Behavior on opacity { NumberAnimation { duration: 140 } }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: row.activated()
    }
}
