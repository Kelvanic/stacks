import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

Rectangle {
    id: pane

    color: Theme.ink1

    signal searchRequested()
    signal importRequested()
    signal focusToggled()

    function heroBook() {
        const books = controller.books
        if (!books || books.length === 0)
            return null
        // The book you are in wins; otherwise the one left part-way.
        if (controller.readerOpen && controller.currentBook.id > 0)
            return controller.currentBook
        for (let i = 0; i < books.length; ++i) {
            const p = books[i].progress || 0
            if (p > 0.001 && p < 0.999)
                return books[i]
        }
        return books[0]
    }

    // ── Wordmark ────────────────────────────────────────────────────────────
    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 22
        anchors.rightMargin: 16
        anchors.topMargin: 20
        height: 46

        Item {
            id: wordmark
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: wordRow.implicitWidth
            height: wordRow.implicitHeight

            Row {
                id: wordRow
                anchors.fill: parent
                spacing: 9

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7
                    height: 7
                    radius: 1.5
                    rotation: 45
                    color: Theme.copper
                    Behavior on rotation { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                }

                Column {
                    spacing: 3
                    Text {
                        text: "Stacks"
                        color: Theme.text
                        font.family: Theme.fontBook
                        font.pixelSize: 25
                        font.weight: Font.Medium
                    }
                    Text {
                        text: wordHover.hovered ? "BACK TO THE DESK" : "A QUIET READING DESK"
                        color: wordHover.hovered ? Theme.copper : Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 1.5
                        Behavior on color { ColorAnimation { duration: 130 } }
                    }
                }
            }

            HoverHandler { id: wordHover }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                cursorShape: Qt.PointingHandCursor
                onClicked: controller.closeBook()
            }
        }

        Item {
            id: focusButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28

            Rectangle {
                anchors.fill: parent
                radius: 7
                color: focusHover.hovered ? Theme.ink3 : "transparent"
            }
            Icon {
                anchors.centerIn: parent
                width: 15
                height: 15
                name: "panel-left"
                color: focusHover.hovered ? Theme.textMuted : Theme.textGhost
            }
            HoverHandler { id: focusHover }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: pane.focusToggled()
            }
        }
    }

    // ── Search ──────────────────────────────────────────────────────────────
    Rectangle {
        id: searchField
        anchors.top: header.bottom
        anchors.topMargin: 14
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 40
        radius: 10
        color: searchHover.hovered ? Theme.ink3 : Theme.ink2
        border.width: 1
        border.color: searchHover.hovered ? Theme.lineStrong : Theme.line
        Behavior on color { ColorAnimation { duration: 130 } }
        Behavior on border.color { ColorAnimation { duration: 130 } }

        Icon {
            id: magnifier
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 15
            height: 15
            name: "search"
            color: Theme.textFaint
        }

        Text {
            anchors.left: magnifier.right
            anchors.leftMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            text: "Search every page"
            color: Theme.textMuted
            font.family: Theme.fontUI
            font.pixelSize: 12
        }

        KeyCap {
            anchors.right: parent.right
            anchors.rightMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            label: "⌘K"
        }

        HoverHandler { id: searchHover }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: pane.searchRequested()
        }
    }

    // ── Now reading ─────────────────────────────────────────────────────────
    Item {
        id: nowReading
        anchors.top: searchField.bottom
        anchors.topMargin: 22
        anchors.left: parent.left
        anchors.leftMargin: 22
        height: 14

        Text {
            text: "NOW READING"
            color: Theme.textGhost
            font.family: Theme.fontMono
            font.pixelSize: 8
            font.letterSpacing: 1.6
        }
    }

    Rectangle {
        id: hero
        visible: pane.heroBook() !== null
        anchors.top: nowReading.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 178
        radius: 14
        clip: true

        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.ink3 }
            GradientStop { position: 1.0; color: Theme.ink2 }
        }

        Rectangle {
            anchors.fill: parent
            radius: 14
            color: "transparent"
            border.width: 1
            border.color: Theme.line
        }

        // Warm light from the top-left, like a lamp on a desk.
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.copperWash }
                GradientStop { position: 0.6; color: "transparent" }
            }
        }

        Cover {
            id: heroCover
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.top: parent.top
            anchors.topMargin: 20
            width: 84
            height: 118
            title: (pane.heroBook() && pane.heroBook().title) || ""
            author: (pane.heroBook() && pane.heroBook().author) || ""
            accent: (pane.heroBook() && pane.heroBook().accent) || Theme.copper
            format: (pane.heroBook() && pane.heroBook().format) || "pdf"

            // A jacket sitting on a desk casts a shadow.
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 4
                radius: 3
                color: "#33000000"
                z: -1
            }
        }

        Column {
            anchors.left: heroCover.right
            anchors.leftMargin: 16
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.top: parent.top
            anchors.topMargin: 22
            spacing: 0

            Text {
                width: parent.width
                text: (pane.heroBook() && pane.heroBook().title) || ""
                color: Theme.text
                font.family: Theme.fontBook
                font.pixelSize: 19
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                lineHeight: 1.05
                lineHeightMode: Text.ProportionalHeight
            }

            Item { width: 1; height: 7 }

            Text {
                width: parent.width
                text: ((pane.heroBook() && pane.heroBook().author) || "").toUpperCase()
                color: Theme.textFaint
                font.family: Theme.fontMono
                font.pixelSize: 8
                font.letterSpacing: 1.1
                elide: Text.ElideRight
            }

            Item { width: 1; height: 14 }

            Item {
                width: parent.width
                height: 3
                Rectangle {
                    anchors.fill: parent
                    radius: 1.5
                    color: Theme.lineSoft
                }
                Rectangle {
                    width: parent.width * Math.max(0.02, Math.min(1, (pane.heroBook() ? (pane.heroBook().progress || 0) : 0)))
                    height: 3
                    radius: 1.5
                    color: Theme.copper
                    Behavior on width { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
                }
            }

            Item { width: 1; height: 8 }

            Text {
                text: {
                    const p = pane.heroBook() ? (pane.heroBook().progress || 0) : 0
                    return p > 0.001 ? Math.round(p * 100) + "% READ" : "NOT STARTED"
                }
                color: Theme.textFaint
                font.family: Theme.fontMono
                font.pixelSize: 8
                font.letterSpacing: 1
            }
        }

        // Resume action, revealed on hover.
        Row {
            id: resumeRow
            anchors.left: heroCover.right
            anchors.leftMargin: 16
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            spacing: 7
            opacity: heroHover.hovered ? 1 : 0.55
            Behavior on opacity { NumberAnimation { duration: 150 } }

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: 13
                height: 13
                name: "arrow-right"
                color: Theme.copper
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Continue reading"
                color: Theme.copper
                font.family: Theme.fontUI
                font.pixelSize: 12
                font.weight: Font.Medium
            }
        }

        HoverHandler { id: heroHover }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                const book = pane.heroBook()
                if (book)
                    controller.openBook(book.id)
            }
        }
    }

    // ── Shelf ───────────────────────────────────────────────────────────────
    Item {
        id: shelf
        anchors.top: hero.bottom
        anchors.topMargin: 24
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.right: parent.right
        anchors.rightMargin: 22
        height: 14

        Text {
            text: "SHELF"
            color: Theme.textGhost
            font.family: Theme.fontMono
            font.pixelSize: 8
            font.letterSpacing: 1.6
        }
        Text {
            anchors.right: parent.right
            text: controller.books.length
            color: Theme.textGhost
            font.family: Theme.fontMono
            font.pixelSize: 8
        }
    }

    ListView {
        id: bookList
        anchors.top: shelf.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: addRow.top
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        clip: true
        spacing: 1
        boundsBehavior: Flickable.StopAtBounds
        model: controller.books

        delegate: BookRow {
            width: bookList.width - 12
            book: modelData
            selected: controller.readerOpen && controller.currentBook.id === modelData.id
            onActivated: controller.openBook(modelData.id)
        }
    }

    // ── Footer ──────────────────────────────────────────────────────────────
    Item {
        id: addRow
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 52

        Item {
            id: addButton
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 138
            height: 38

            Rectangle {
                anchors.fill: parent
                radius: 9
                color: addHover.hovered ? Theme.ink3 : "transparent"
                border.width: 1
                border.color: addHover.hovered ? Theme.lineStrong : Theme.line
                Behavior on color { ColorAnimation { duration: 130 } }
            }

            Row {
                id: addButtonRow
                anchors.centerIn: parent
                spacing: 8
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 13
                    height: 13
                    name: "plus"
                    color: Theme.copper
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Add a book"
                    color: Theme.text
                    font.family: Theme.fontUI
                    font.pixelSize: 12
                }
            }

            HoverHandler { id: addHover }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: pane.importRequested()
            }
        }

        Text {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 150
            horizontalAlignment: Text.AlignRight
            text: controller.ftsAvailable ? "LOCAL · FULL-TEXT" : "LOCAL · FALLBACK"
            color: Theme.textGhost
            font.family: Theme.fontMono
            font.pixelSize: 8
            font.letterSpacing: 1
        }
    }
}
