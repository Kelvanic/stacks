import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Stacks.Native 1.0
import "."

Rectangle {
    id: pane

    color: Theme.ink0

    property var book: controller.currentBook
    property bool noteVisible: false
    property bool voiceVisible: false
    property int recordingMs: 0

    signal searchRequested()
    signal diagramRequested()
    signal focusToggled()
    signal kindleRequested()

    Component.onCompleted: {
        if (controller.readerOpen && pane.book.path)
            reader.load(pane.book.path, pane.book.format, pane.book.locator)
        reader.annotations = controller.annotations
    }

    Connections {
        target: controller
        function onReaderRequested(bookId, locator) {
            pane.noteVisible = false
            pane.voiceVisible = false
            if (!pane.book.path)
                return
            reader.load(pane.book.path, pane.book.format, locator)
            reader.annotations = controller.annotations
        }
        function onReaderClosed() {
            pane.noteVisible = false
            pane.voiceVisible = false
            reader.closeDocument()
        }
        function onAnnotationsChanged() {
            reader.annotations = controller.annotations
        }
    }

    Timer {
        interval: 100
        repeat: true
        running: controller.recording
        onTriggered: pane.recordingMs += 100
    }

    // ── The desk ────────────────────────────────────────────────────────────
    Canvas {
        id: deskGlow
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            if (typeof ctx.reset === "function")
                ctx.reset()
            else
                ctx.clearRect(0, 0, width, height)
            const g = ctx.createRadialGradient(width * 0.5, height * 0.34, 10,
                                               width * 0.5, height * 0.34, Math.max(width, height) * 0.72)
            g.addColorStop(0, Theme.glow)
            g.addColorStop(0.45, Theme.glowSoft)
            g.addColorStop(1, "rgba(0,0,0,0)")
            ctx.fillStyle = g
            ctx.fillRect(0, 0, width, height)
        }
        Connections {
            target: pane
            function onWidthChanged() { deskGlow.requestPaint() }
            function onHeightChanged() { deskGlow.requestPaint() }
        }
        Connections {
            target: Theme
            function onDarkChanged() { deskGlow.requestPaint() }
        }
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // ── Top bar ─────────────────────────────────────────────────────────
        Rectangle {
            id: topBar
            objectName: "reader-toolbar"
            width: parent.width
            height: Theme.topBar
            color: Theme.ink1

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.line
            }

            // How far through the book you are, in one hairline.
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 1
                width: parent.width
                height: 2
                color: Theme.lineSoft

                Rectangle {
                    width: parent.width * Math.max(0.002, Math.min(1, reader.progress || 0))
                    height: 2
                    color: Theme.copper
                    Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 16
                spacing: 6

                TopAction {
                    iconName: "chevron-left"
                    label: "Back to the desk"
                    onClicked: controller.closeBook()
                }

                Column {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: 6
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 4

                    Text {
                        text: pane.book.title || "Untitled"
                        color: Theme.text
                        font.family: Theme.fontUI
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    Text {
                        text: {
                            const author = pane.book.author || "Unknown author"
                            const format = (pane.book.format || "").toUpperCase()
                            const where = reader.format === "pdf"
                                ? "PAGE " + (reader.currentPage + 1) + " / " + reader.pageCount
                                : Math.round(reader.progress * 100) + "% READ"
                            return author + "   ·   " + format + "   ·   " + where
                        }
                        width: parent.width
                        elide: Text.ElideRight
                        color: Theme.textFaint
                        font.family: Theme.fontUI
                        font.pixelSize: 10
                    }
                }

                TopAction {
                    visible: pane.width > 620
                    iconName: "search"
                    label: "Search"
                    shortcut: "⌘K"
                    onClicked: pane.searchRequested()
                }

                Rectangle {
                    width: 1
                    height: 18
                    color: Theme.line
                    Layout.alignment: Qt.AlignVCenter
                }

                TopAction {
                    iconName: "highlight"
                    label: "Highlight"
                    enabled: reader.selectedText.length > 0
                    active: reader.selectedText.length > 0
                    onClicked: {
                        controller.addHighlight(reader.selectedText, reader.selectionLocator, reader.currentPage)
                        reader.clearSelection()
                    }
                }

                TopAction {
                    iconName: "bookmark"
                    label: "Bookmark"
                    onClicked: controller.addBookmark(reader.locator, reader.currentPage)
                }

                TopAction {
                    iconName: "note"
                    label: "Note"
                    onClicked: {
                        noteInput.text = ""
                        pane.noteVisible = true
                        noteInput.forceActiveFocus()
                    }
                }

                TopAction {
                    iconName: "mic"
                    label: controller.recording ? "Stop recording" : "Voice note"
                    active: controller.recording
                    onClicked: {
                        if (controller.recording) {
                            pane.voiceVisible = true
                        } else {
                            pane.recordingMs = 0
                            if (controller.startVoiceNote())
                                pane.voiceVisible = true
                        }
                    }
                }

                Rectangle {
                    width: 1
                    height: 18
                    color: Theme.line
                    Layout.alignment: Qt.AlignVCenter
                }

                TopAction {
                    iconName: "diagram"
                    label: "Diagram"
                    shortcut: "⌘⇧D"
                    onClicked: pane.diagramRequested()
                }

                TopAction {
                    visible: pane.width > 620
                    iconName: "send"
                    label: "Send to Kindle"
                    shortcut: "⌘⇧K"
                    onClicked: pane.kindleRequested()
                }

                TopAction {
                    iconName: "panel-right"
                    label: "Focus"
                    shortcut: "⌘\\"
                    onClicked: pane.focusToggled()
                }
            }
        }

        // ── Reading well ────────────────────────────────────────────────────
        Item {
            id: well
            width: parent.width
            height: parent.height - Theme.topBar

            // Page turn buttons, revealed over the desk.
            Rectangle {
                id: prevButton
                visible: wellHover.hovered || prevHover.hovered
                opacity: visible ? 1 : 0
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: paperWell.verticalCenter
                width: 36
                height: 36
                radius: 18
                color: prevHover.hovered ? Theme.ink3 : Theme.ink2
                border.width: 1
                border.color: Theme.line

                Behavior on opacity { NumberAnimation { duration: 180 } }

                Icon {
                    anchors.centerIn: parent
                    width: 15
                    height: 15
                    name: "chevron-left"
                    color: prevHover.hovered ? Theme.text : Theme.textMuted
                }
                HoverHandler { id: prevHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: reader.previousPage()
                }
            }

            Rectangle {
                id: nextButton
                visible: wellHover.hovered || nextHover.hovered
                opacity: visible ? 1 : 0
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: paperWell.verticalCenter
                width: 36
                height: 36
                radius: 18
                color: nextHover.hovered ? Theme.ink3 : Theme.ink2
                border.width: 1
                border.color: Theme.line

                Behavior on opacity { NumberAnimation { duration: 180 } }

                Icon {
                    anchors.centerIn: parent
                    width: 15
                    height: 15
                    name: "chevron-right"
                    color: nextHover.hovered ? Theme.text : Theme.textMuted
                }
                HoverHandler { id: nextHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: reader.nextPage()
                }
            }

            // ── The sheet ───────────────────────────────────────────────────
            Item {
                id: paperWell
                anchors.fill: parent
                anchors.leftMargin: 46
                anchors.rightMargin: 46
                anchors.topMargin: 26
                anchors.bottomMargin: 78

                // Shadow cast on the desk. Offset below the sheet: filling the
                // sheet's own rectangle would leave it hidden underneath.
                Rectangle {
                    visible: paperSheet.visible
                    anchors.fill: paperSheet
                    anchors.topMargin: 14
                    anchors.bottomMargin: -10
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    radius: 16
                    color: Theme.deepShadow
                }
                Rectangle {
                    visible: paperSheet.visible
                    anchors.fill: paperSheet
                    anchors.topMargin: 6
                    anchors.bottomMargin: -4
                    radius: 12
                    color: Theme.shadow
                }

                // A bound book: a parchment sheet with a bound edge.
                Rectangle {
                    id: paperSheet
                    visible: reader.format !== "pdf"
                    width: Math.min(paperWell.width, 720)
                    height: paperWell.height
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 3
                    color: Theme.paper
                    clip: true

                    // Paper tooth.
                    Image {
                        anchors.fill: parent
                        source: assetsDir + "/textures/paper.png"
                        fillMode: Image.Tile
                        opacity: Theme.paperTextureOpacity
                    }

                    // The bound edge.
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 18
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.paperEdge }
                            GradientStop { position: 0.35; color: Qt.alpha(Theme.paperEdge, 0.5) }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    // Light catches the top edge of the sheet.
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: 3
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.hair
                    }
                }

                // The content itself. A PDF page is a loose sheet on the desk;
                // an EPUB is the inside of the bound page above, and its text
                // column is tied to the sheet so the measure never drifts.
                ReaderItem {
                    id: reader
                    width: reader.format === "pdf" ? paperWell.width : paperSheet.width
                    height: paperWell.height
                    x: reader.format === "pdf" ? 0 : paperSheet.x
                    y: 0
                    clip: true
                    textColor: Theme.paperInk
                    accentColor: Theme.copper
                    mutedColor: Theme.paperMuted
                    onPositionChanged: function(locator, progress) {
                        controller.savePosition(locator, progress)
                    }
                    onErrorOccurred: function(message) {
                        controller.toast(message)
                    }
                }

                // The running head appears once you are into the book.
                Rectangle {
                    id: runningHead
                    visible: reader.format !== "pdf"
                    z: 5
                    x: paperSheet.x
                    y: paperSheet.y
                    width: paperSheet.width
                    height: 54
                    opacity: Math.max(0, Math.min(1, (reader.scrollY - 24) / 110))

                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: Theme.paper }
                        GradientStop { position: 0.84; color: Theme.paper }
                        GradientStop { position: 1.0; color: "transparent" }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 15
                        text: (pane.book.title || "").toUpperCase()
                        color: Theme.paperMuted
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 1.9
                    }
                }
            }

            HoverHandler { id: wellHover }

            // ── Folio ───────────────────────────────────────────────────────
            Rectangle {
                id: folio
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 16
                height: 40
                radius: 10
                color: Theme.ink1
                border.color: Theme.lineSoft

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: reader.format === "pdf"
                          ? "Page " + (reader.currentPage + 1) + " of " + reader.pageCount
                          : Math.round(reader.progress * 100) + "% complete"
                    color: Theme.textFaint
                    font.family: Theme.fontUI
                    font.pixelSize: 11
                }

                Item {
                    anchors.centerIn: parent
                    width: Math.max(40, Math.min(160, parent.width - 260))
                    height: 3

                    Rectangle {
                        anchors.fill: parent
                        radius: 1.5
                        color: Theme.lineSoft
                    }
                    Rectangle {
                        width: parent.width * Math.max(0.004, Math.min(1, reader.progress || 0))
                        height: 3
                        radius: 1.5
                        color: Theme.copper
                        Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    TopAction { objectName: "reader-previous"; iconName: "chevron-left"; label: "Previous page"; enabled: reader.ready && reader.progress > 0; onClicked: reader.previousPage() }
                    TopAction { objectName: "reader-next"; iconName: "chevron-right"; label: "Next page"; enabled: reader.ready && reader.progress < 1; onClicked: reader.nextPage() }
                }
            }

            // ── Selection actions ───────────────────────────────────────────
            Rectangle {
                id: selectionBar
                visible: reader.selectedText.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 58
                width: Math.min(700, parent.width - 140)
                height: 66
                radius: 14
                color: Theme.ink2
                border.width: 1
                border.color: Theme.lineStrong

                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    anchors.bottomMargin: -6
                    radius: 14
                    color: Theme.shadow
                    z: -1
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 14
                    spacing: 14

                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        width: 3
                        height: 30
                        radius: 1.5
                        color: Theme.copper
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 420
                        text: reader.selectedText
                        color: Theme.text
                        font.family: Theme.fontBook
                        font.pixelSize: 14
                        font.italic: true
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        lineHeight: 1.25
                    }

                    Item { Layout.fillWidth: true }

                    PillButton {
                        label: "Highlight"
                        iconName: "highlight"
                        primary: true
                        onClicked: {
                            controller.addHighlight(reader.selectedText, reader.selectionLocator, reader.currentPage)
                            reader.clearSelection()
                        }
                    }

                    PillButton {
                        label: "Note"
                        iconName: "note"
                        onClicked: {
                            noteInput.text = ""
                            pane.noteVisible = true
                            noteInput.forceActiveFocus()
                        }
                    }
                }
            }

            // ── Note composer ───────────────────────────────────────────────
            Rectangle {
                id: noteCard
                visible: pane.noteVisible
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 40
                anchors.topMargin: 18
                width: 366
                height: 250
                radius: 14
                color: Theme.ink2
                border.width: 1
                border.color: Theme.lineStrong
                z: 40

                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    anchors.bottomMargin: -6
                    radius: 14
                    color: Theme.shadow
                    z: -1
                }

                Column {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Row {
                        width: parent.width
                        Text {
                            text: "MARGIN NOTE"
                            color: Theme.textFaint
                            font.family: Theme.fontMono
                            font.pixelSize: 8
                            font.letterSpacing: 1.5
                        }
                        Item { width: parent.width - 92; height: 1 }
                        KeyCap { label: "esc" }
                    }

                    Text {
                        width: parent.width
                        visible: reader.selectedText.length > 0
                        text: reader.selectedText
                        color: Theme.textMuted
                        font.family: Theme.fontBook
                        font.pixelSize: 12
                        font.italic: true
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                    }

                    TextArea {
                        id: noteInput
                        width: parent.width
                        height: 104
                        color: Theme.text
                        placeholderText: "What do you want to remember?"
                        placeholderTextColor: Theme.textGhost
                        font.family: Theme.fontUI
                        font.pixelSize: 12
                        wrapMode: TextArea.Wrap
                        selectByMouse: true
                        background: Rectangle {
                            color: Theme.ink1
                            radius: 9
                            border.width: 1
                            border.color: Theme.line
                        }
                        Keys.onEscapePressed: pane.noteVisible = false
                    }

                    Item { width: 1; height: 1 }

                    Row {
                        width: parent.width
                        spacing: 8
                        Item { width: parent.width - 168; height: 1 }

                        PillButton {
                            label: "Cancel"
                            onClicked: pane.noteVisible = false
                        }

                        PillButton {
                            label: "Save note"
                            iconName: "note"
                            primary: true
                            onClicked: {
                                const note = noteInput.text.trim()
                                if (note.length === 0)
                                    return
                                controller.addNote(note,
                                                   reader.selectedText,
                                                   reader.selectedText.length > 0 ? reader.selectionLocator : reader.locator,
                                                   reader.currentPage)
                                noteInput.text = ""
                                pane.noteVisible = false
                                reader.clearSelection()
                            }
                        }
                    }
                }
            }

            // ── Voice note recorder ─────────────────────────────────────────
            Rectangle {
                id: voiceCard
                visible: pane.voiceVisible
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 40
                anchors.bottomMargin: 58
                width: 384
                height: controller.recording ? 104 : 196
                radius: 14
                color: Theme.ink2
                border.width: 1
                border.color: controller.recording ? Theme.copperSoft : Theme.lineStrong
                z: 40

                Behavior on height { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }

                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    anchors.bottomMargin: -6
                    radius: 14
                    color: Theme.shadow
                    z: -1
                }

                Column {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Row {
                        width: parent.width
                        spacing: 10

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: controller.recording ? Theme.copper : Theme.textGhost
                            anchors.verticalCenter: parent.verticalCenter
                            SequentialAnimation on opacity {
                                running: controller.recording
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.25; duration: 640 }
                                NumberAnimation { to: 1.0; duration: 640 }
                            }
                        }

                        Text {
                            text: controller.recording ? "RECORDING" : "VOICE NOTE"
                            color: controller.recording ? Theme.copper : Theme.textFaint
                            font.family: Theme.fontMono
                            font.pixelSize: 8
                            font.letterSpacing: 1.5
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Item { width: parent.width - 186; height: 1 }

                        Text {
                            text: {
                                const total = Math.floor(pane.recordingMs / 1000)
                                return String(Math.floor(total / 60)).padStart(2, "0") + ":" + String(total % 60).padStart(2, "0")
                            }
                            color: Theme.textMuted
                            font.family: Theme.fontMono
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Row {
                        width: parent.width
                        height: 30
                        spacing: 4

                        Repeater {
                            model: 26
                            delegate: Rectangle {
                                width: (parent.width - 100) / 26
                                height: 4 + 20 * Math.abs(Math.sin(index * 1.37 + pane.recordingMs / 190.0)) * (0.22 + controller.recordingLevel * 0.95)
                                radius: 1
                                color: controller.recording ? Theme.copper : Theme.textGhost
                                anchors.verticalCenter: parent.verticalCenter
                                opacity: 0.4 + 0.6 * (controller.recordingLevel || 0)
                            }
                        }
                    }

                    TextArea {
                        visible: !controller.recording
                        width: parent.width
                        height: 58
                        color: Theme.text
                        placeholderText: "Optional transcript or note"
                        placeholderTextColor: Theme.textGhost
                        font.family: Theme.fontUI
                        font.pixelSize: 12
                        wrapMode: TextArea.Wrap
                        selectByMouse: true
                        background: Rectangle {
                            color: Theme.ink1
                            radius: 9
                            border.width: 1
                            border.color: Theme.line
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: 8
                        Item { width: parent.width - 162; height: 1 }

                        PillButton {
                            label: "Discard"
                            onClicked: {
                                if (controller.recording)
                                    controller.cancelVoiceNote()
                                pane.voiceVisible = false
                            }
                        }

                        PillButton {
                            label: controller.recording ? "Stop & save" : "Save voice"
                            iconName: "mic"
                            primary: true
                            onClicked: {
                                if (controller.recording) {
                                    controller.stopVoiceNote("")
                                    pane.voiceVisible = false
                                } else {
                                    pane.voiceVisible = false
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Clicking the desk dismisses the open composer.
    MouseArea {
        anchors.fill: parent
        visible: pane.noteVisible || pane.voiceVisible
        z: -1
        onClicked: {
            pane.noteVisible = false
            pane.voiceVisible = false
        }
    }
}
