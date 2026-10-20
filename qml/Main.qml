import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import "Theme.js" as Theme

ApplicationWindow {
    id: root
    width: 1440
    height: 900
    minimumWidth: 1120
    minimumHeight: 720
    visible: true
    color: Theme.ink0
    title: "Stacks"

    property bool paletteOpen: false
    property bool diagramOpen: false
    property bool focusMode: false

    FileDialog {
        id: importDialog
        title: "Add a book to Stacks"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Books (*.pdf *.epub)", "PDF (*.pdf)", "EPUB (*.epub)"]
        onAccepted: controller.importBook(selectedFile)
    }

    FileDialog {
        id: kindleFileDialog
        title: "Choose a file to send to Kindle"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Documents (*.pdf *.epub *.txt *.md *.htm *.html *.doc *.docx *.rtf *.png *.jpg *.jpeg *.gif *.bmp)"]
        onAccepted: controller.convertFileForKindle(selectedFile)
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: commandPalette.open()
    }
    Shortcut {
        sequence: "Ctrl+Shift+K"
        onActivated: kindle.open()
    }
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: importDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+Shift+D"
        onActivated: {
            if (controller.readerOpen)
                diagram.open()
        }
    }
    Shortcut {
        sequence: "Ctrl+\\"
        onActivated: root.focusMode = !root.focusMode
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (commandPalette.visible)
                commandPalette.close()
            else if (diagram.visible)
                diagram.close()
            else if (kindle.visible)
                kindle.close()
        }
    }

    Row {
        anchors.fill: parent
        spacing: 0

        // ── Shelf ────────────────────────────────────────────────────────────
        LibraryPane {
            id: libraryPane
            width: root.focusMode ? 0 : Theme.railLeft
            height: parent.height
            clip: true
            opacity: root.focusMode ? 0 : 1
            visible: width > 0
            onSearchRequested: commandPalette.open()
            onImportRequested: importDialog.open()
            onFocusToggled: root.focusMode = !root.focusMode

            Behavior on width { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 160 } }
        }

        // ── Centre ───────────────────────────────────────────────────────────
        Item {
            width: parent.width - libraryPane.width - marginPane.width
            height: parent.height

            ReaderPane {
                id: readerPane
                anchors.fill: parent
                visible: controller.readerOpen
                opacity: controller.readerOpen ? 1 : 0
                onSearchRequested: commandPalette.open()
                onDiagramRequested: diagram.open()
                onFocusToggled: root.focusMode = !root.focusMode
                onKindleRequested: kindle.open()

                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
            }

            DeskView {
                id: deskView
                anchors.fill: parent
                visible: !controller.readerOpen
                opacity: controller.readerOpen ? 0 : 1
                onSearchRequested: commandPalette.open()
                onImportRequested: importDialog.open()
                onKindleRequested: kindle.open()

                Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
            }

            // Film grain over the whole window.
            Image {
                anchors.fill: parent
                source: assetsDir + "/textures/grain.png"
                fillMode: Image.Tile
                opacity: 0.45
            }
        }

        // ── Margin ───────────────────────────────────────────────────────────
        MarginPane {
            id: marginPane
            width: controller.readerOpen && !root.focusMode ? Theme.railRight : 0
            height: parent.height
            visible: width > 0
            opacity: controller.readerOpen ? 1 : 0
            clip: true
            onDiagramRequested: diagram.open()

            Behavior on width { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 160 } }
        }
    }

    CommandPalette {
        id: commandPalette
        anchors.fill: parent
    }

    DiagramCanvas {
        id: diagram
        anchors.fill: parent
    }

    KindleSheet {
        id: kindle
        anchors.fill: parent
    }

    // ── Toast ────────────────────────────────────────────────────────────────
    Rectangle {
        id: toast
        visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 26
        width: toastRow.implicitWidth + 34
        height: 42
        radius: 12
        color: Theme.ink3
        border.width: 1
        border.color: Theme.lineStrong
        z: 200

        opacity: toast.visible ? 1 : 0
        transform: Translate { y: toast.visible ? 0 : 10 }
        Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        Row {
            id: toastRow
            anchors.centerIn: parent
            spacing: 10

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 6
                height: 6
                radius: 3
                color: Theme.copper
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                id: toastText
                color: Theme.text
                font.family: Theme.fontUI
                font.pixelSize: 12
            }
        }

        Timer {
            id: toastTimer
            interval: 2400
            onTriggered: toast.visible = false
        }
    }

    Connections {
        target: controller
        function onToast(message) {
            toastText.text = message
            toast.visible = true
            toastTimer.restart()
        }
        function onDiagramRequested() {
            diagram.open()
        }
    }
}
