import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// The desk you come back to: a headline, the shelf, and a few quiet facts.
Item {
    id: desk

    signal searchRequested()
    signal importRequested()
    signal kindleRequested()

    function totalWords() {
        const books = controller.books || []
        let n = 0
        for (let i = 0; i < books.length; ++i)
            n += books[i].wordCount || 0
        return n
    }

    function wordLabel() {
        const n = desk.totalWords()
        if (n >= 1000000)
            return (n / 1000000).toFixed(1) + "M"
        if (n >= 1000)
            return Math.round(n / 1000) + "k"
        return String(n)
    }

    Canvas {
        id: deskGlow
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            if (typeof ctx.reset === "function")
                ctx.reset()
            else
                ctx.clearRect(0, 0, width, height)
            const g = ctx.createRadialGradient(width * 0.72, height * 0.46, 20,
                                               width * 0.72, height * 0.46, Math.max(width, height) * 0.6)
            g.addColorStop(0, "rgba(201,112,63,0.14)")
            g.addColorStop(0.5, "rgba(150,105,70,0.05)")
            g.addColorStop(1, "rgba(0,0,0,0)")
            ctx.fillStyle = g
            ctx.fillRect(0, 0, width, height)
        }
        Connections {
            target: desk
            function onWidthChanged() { deskGlow.requestPaint() }
            function onHeightChanged() { deskGlow.requestPaint() }
        }
    }

    // ── Left ────────────────────────────────────────────────────────────────
    Column {
        id: left
        anchors.left: parent.left
        anchors.leftMargin: 74
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(520, parent.width - 120)
        spacing: 0

        Text {
            text: "STACKS  ·  A READING DESK"
            color: Theme.textGhost
            font.family: Theme.fontMono
            font.pixelSize: 9
            font.letterSpacing: 2.2
        }

        Item { width: 1; height: 26 }

        Text {
            width: parent.width
            text: "A quiet place\nto read."
            color: Theme.text
            font.family: Theme.fontBook
            font.pixelSize: 56
            font.weight: Font.Medium
            lineHeight: 1.02
            lineHeightMode: Text.ProportionalHeight
        }

        Item { width: 1; height: 24 }

        Text {
            width: Math.min(440, parent.width)
            text: "Your books, your marks, your voice — indexed on this machine, in a fraction of a second."
            color: Theme.textMuted
            font.family: Theme.fontUI
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            lineHeight: 1.55
        }

        Item { width: 1; height: 30 }

        Row {
            spacing: 10

            PillButton {
                label: "Add a book"
                icon: "plus"
                primary: true
                onClicked: desk.importRequested()
            }

            PillButton {
                label: "Search everything"
                icon: "search"
                onClicked: desk.searchRequested()
            }

            PillButton {
                label: "Send to Kindle"
                icon: "send"
                onClicked: desk.kindleRequested()
            }
        }

        Item { width: 1; height: 34 }

        Row {
            spacing: 20

            Repeater {
                model: [
                    { key: "⌘K", label: "Search" },
                    { key: "⌘O", label: "Import" },
                    { key: "⌘⇧D", label: "Diagram" },
                    { key: "⌘\\", label: "Focus" }
                ]
                delegate: Row {
                    spacing: 7
                    KeyCap { label: modelData.key; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: Theme.textFaint
                        font.family: Theme.fontUI
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    // Books fanned out on the desk.
    property var shelfModel: {
        const books = controller.books || []
        const out = []
        for (let i = 0; i < Math.min(4, books.length); ++i) {
            out.push({
                book: books[i],
                x: 66 + i * 12,
                y: 62 + Math.abs(i - 1.5) * 12,
                rot: -6 + i * 4
            })
        }
        return out
    }

    // ── Right: the shelf ────────────────────────────────────────────────────
    Item {
        anchors.right: parent.right
        anchors.rightMargin: 88
        anchors.verticalCenter: parent.verticalCenter
        width: 300
        height: 360

        Repeater {
            id: shelfCovers
            model: desk.shelfModel

            delegate: Cover {
                id: shelfBook
                property var entry: modelData
                property real baseY: entry.y

                width: 148
                height: 208
                title: entry.book.title || ""
                author: entry.book.author || ""
                accent: entry.book.accent || Theme.copper
                format: entry.book.format || "pdf"

                // A fan of books leaning against each other.
                x: entry.x
                y: baseY
                rotation: entry.rot
                transformOrigin: Item.BottomLeft

                opacity: 0
                Component.onCompleted: fadeIn.start()
                SequentialAnimation {
                    id: fadeIn
                    NumberAnimation {
                        target: shelfBook
                        property: "opacity"
                        to: 1
                        duration: 420
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: shelfBook
                        property: "y"
                        to: shelfBook.baseY - 8
                        duration: 420
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: shelfBook
                        property: "y"
                        to: shelfBook.baseY
                        duration: 180
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    radius: 3
                    color: "#50000000"
                    z: -1
                }
            }
        }

        // Facts about the desk.
        Row {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            spacing: 42

            Repeater {
                model: [
                    { value: String(controller.books.length), label: "BOOKS" },
                    { value: desk.wordLabel(), label: "WORDS" },
                    { value: String(controller.annotations.length), label: "MARKS" }
                ]
                delegate: Column {
                    spacing: 6
                    Text {
                        text: modelData.value
                        color: Theme.text
                        font.family: Theme.fontBook
                        font.pixelSize: 30
                        font.weight: Font.Medium
                    }
                    Text {
                        text: modelData.label
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 1.8
                    }
                }
            }
        }
    }
}
