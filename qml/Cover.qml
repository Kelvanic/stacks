import QtQuick
import "Theme.js" as Theme

// A book jacket, drawn rather than decorated: cloth gradient, a bound
// spine, a hairline plate and the title set in the book face.
Rectangle {
    id: cover

    property string title: ""
    property string author: ""
    property string accent: Theme.copper
    property string format: "pdf"
    property bool dim: false

    radius: 2
    clip: true
    color: accent

    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter(cover.accent, 1.22) }
        GradientStop { position: 0.55; color: cover.accent }
        GradientStop { position: 1.0; color: Qt.darker(cover.accent, 1.38) }
    }

    opacity: dim ? 0.55 : 1

    // The spine: a darker band with a highlight where the cloth folds.
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.max(4, cover.width * 0.085)
        color: Qt.darker(cover.accent, 1.6)
        opacity: 0.55
    }

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Math.max(4, cover.width * 0.085)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Qt.rgba(1, 1, 1, 0.22)
    }

    // Cloth sheen.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.10) }
            GradientStop { position: 0.45; color: Qt.rgba(1, 1, 1, 0.0) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.14) }
        }
    }

    // Below roughly thumb size the jacket is cloth and nothing else: text at
    // that size reads as noise.
    readonly property bool tiny: width < 64

    Column {
        anchors.fill: parent
        anchors.leftMargin: Math.max(6, cover.width * 0.14)
        anchors.rightMargin: Math.max(4, cover.width * 0.09)
        anchors.topMargin: Math.max(8, cover.height * 0.1)
        anchors.bottomMargin: Math.max(8, cover.height * 0.09)
        spacing: Math.max(4, cover.height * 0.022)
        clip: true

        Text {
            visible: !cover.tiny
            width: parent.width
            text: cover.title
            wrapMode: Text.WordWrap
            maximumLineCount: 4
            elide: Text.ElideRight
            color: "#FFF6E9"
            font.family: Theme.fontBook
            font.pixelSize: Math.max(11, cover.width * 0.135)
            font.weight: Font.Medium
            lineHeight: 1.02
            lineHeightMode: Text.ProportionalHeight
        }

        Rectangle {
            visible: !cover.tiny
            width: Math.max(16, cover.width * 0.2)
            height: 1
            color: Qt.rgba(1, 1, 1, 0.5)
        }

        Text {
            visible: !cover.tiny
            width: parent.width
            text: cover.author
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            color: Qt.rgba(1, 1, 1, 0.66)
            font.family: Theme.fontUI
            font.pixelSize: Math.max(8, cover.width * 0.072)
            font.letterSpacing: 0.2
        }
    }

    // Format stamp.
    Rectangle {
        visible: !cover.tiny
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Math.max(6, cover.width * 0.07)
        width: stamp.implicitWidth + 8
        height: 14
        radius: 3
        color: Qt.rgba(0, 0, 0, 0.22)

        Text {
            id: stamp
            anchors.centerIn: parent
            text: cover.format.toUpperCase()
            color: Qt.rgba(1, 1, 1, 0.7)
            font.family: Theme.fontMono
            font.pixelSize: Math.max(7, cover.width * 0.055)
            font.letterSpacing: 0.8
        }
    }
}
