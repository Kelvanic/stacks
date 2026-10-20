import QtQuick
import QtQuick.Controls
import "."

// Send to Kindle: pick a file, convert it into a Kindle-ready EPUB, and hand
// it to Amazon's Send to Kindle service.
Item {
    id: sheet
    anchors.fill: parent
    visible: false
    z: 120

    property var selectedBook: null
    property string sourcePath: ""
    property string sourceFormat: ""
    property string sourceTitle: ""
    property string sourceAuthor: ""
    property bool settingsOpen: false

    function open() {
        if (!controller.kindleBusy)
            controller.clearKindleResults()
        detailFlick.contentY = 0
        sheet.visible = true
        sheet.selectedBook = null
        sheet.sourcePath = ""
        sheet.sourceFormat = ""
        sheet.sourceTitle = ""
        sheet.sourceAuthor = ""
    }

    function close() {
        sheet.visible = false
    }

    function pickBook(book) {
        if (controller.kindleBusy)
            return
        controller.clearKindleResults()
        detailFlick.contentY = 0
        sheet.selectedBook = book
        sheet.sourcePath = book.path || ""
        sheet.sourceFormat = book.format || ""
        sheet.sourceTitle = book.title || ""
        sheet.sourceAuthor = book.author || ""
    }

    function saveAccount() {
        controller.saveKindleSettings({
            "fromAddress": fromField.text,
            "smtpHost": hostField.text.length > 0 ? hostField.text : "smtp.gmail.com",
            "smtpPort": (controller.kindleSettings && controller.kindleSettings.smtpPort) || 587,
            "user": userField.text,
            "password": passwordField.text,
            "kindleAddress": kindleAddress.text
        })
    }

    function pickFile(url) {
        if (controller.kindleBusy)
            return
        sheet.selectedBook = null
        sheet.sourcePath = ""
        sheet.sourceTitle = ""
        controller.clearKindleResults()
        controller.convertFileForKindle(url)
    }

    function result() {
        const results = controller.kindleResults
        const latest = results.length > 0 ? results[results.length - 1] : null
        return latest && (!sheet.selectedBook || sheet.sourcePath === latest.sourcePath) ? latest : null
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        MouseArea {
            anchors.fill: parent
            onClicked: sheet.close()
            onWheel: function(wheel) { wheel.accepted = true } // The desk below stays put.
        }
    }

    Rectangle {
        id: panel
        width: Math.min(960, sheet.width - 120)
        height: Math.min(780, sheet.height - 60)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        radius: 18
        color: Theme.ink1
        border.width: 1
        border.color: Theme.lineStrong

        opacity: sheet.visible ? 1 : 0
        scale: sheet.visible ? 1 : 0.99
        Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 10
            anchors.bottomMargin: -8
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            radius: 18
            color: Theme.deepShadow
            z: -1
        }

        // Clicks on the panel's empty space must not reach the scrim, which
        // would close the sheet.
        MouseArea {
            anchors.fill: parent
            onWheel: function(wheel) { wheel.accepted = true }
        }

        Column {
            anchors.fill: parent
            anchors.margins: parent.border.width // Rows and panes stop inside the border.
            spacing: 0

            // ── Header ────────────────────────────────────────────────────────
            Item {
                id: header
                width: parent.width
                height: 72

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 26
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: "Send to Kindle"
                        color: Theme.text
                        font.family: Theme.fontBook
                        font.pixelSize: 21
                        font.weight: Font.Medium
                    }
                    Text {
                        text: "CONVERT A FILE, THEN HAND IT TO AMAZON"
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 1.6
                    }
                }

                TopAction {
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "close"
                    label: "Close"
                    onClicked: sheet.close()
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.line
                }
            }

            // ── Body ──────────────────────────────────────────────────────────
            Row {
                id: body
                width: parent.width
                height: parent.height - header.height - 46
                spacing: 0

                // Sources
                Rectangle {
                    id: sources
                    width: 330
                    height: parent.height
                    color: Theme.ink0

                    Rectangle {
                        anchors.right: parent.right
                        width: 1
                        height: parent.height
                        color: Theme.line
                    }

                    Text {
                        id: sourcesLabel
                        anchors.top: parent.top
                        anchors.topMargin: 20
                        anchors.left: parent.left
                        anchors.leftMargin: 24
                        text: "SOURCE"
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 1.6
                    }

                    ListView {
                        id: sourceList
                        anchors.top: sourcesLabel.bottom
                        anchors.topMargin: 12
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: addFile.top
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        clip: true
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds
                        model: controller.books

                        delegate: Rectangle {
                            id: sourceRow
                            width: sourceList.width - 12
                            height: 62
                            radius: 9
                            color: sheet.selectedBook && sheet.selectedBook.id === modelData.id
                                   ? Theme.ink2 : (rowHover.hovered ? Theme.ink2 : "transparent")
                            Behavior on color { ColorAnimation { duration: 120 } }

                            Cover {
                                id: sourceCover
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                width: 30
                                height: 42
                                title: modelData.title || ""
                                author: modelData.author || ""
                                accent: modelData.accent || Theme.copper
                                format: modelData.format || "pdf"
                            }

                            Column {
                                anchors.left: sourceCover.right
                                anchors.leftMargin: 12
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 4

                                Text {
                                    width: parent.width
                                    text: modelData.title || "Untitled"
                                    color: Theme.text
                                    font.family: Theme.fontUI
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: (modelData.format || "").toUpperCase() + "  ·  "
                                          + (modelData.author || "")
                                    color: Theme.textFaint
                                    font.family: Theme.fontMono
                                    font.pixelSize: 8
                                    font.letterSpacing: 0.8
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                            }

                            HoverHandler { id: rowHover }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: sheet.pickBook(modelData)
                            }
                        }
                    }

                    Item {
                        id: addFile
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.bottomMargin: 16
                        height: 40

                        Rectangle {
                            anchors.fill: parent
                            radius: 9
                            color: addHover.hovered ? Theme.ink3 : "transparent"
                            border.width: 1
                            border.color: addHover.hovered ? Theme.lineStrong : Theme.line
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }

                        Row {
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
                                text: "Add a file…"
                                color: Theme.text
                                font.family: Theme.fontUI
                                font.pixelSize: 12
                            }
                        }

                        HoverHandler { id: addHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: kindleFileDialog.open()
                        }
                    }
                }

                // Detail
                Item {
                    id: detail
                    width: parent.width - sources.width
                    height: parent.height

                    Flickable {
                        id: detailFlick
                        anchors.fill: parent
                        contentWidth: width
                        contentHeight: detailColumn.implicitHeight + 44
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: detailColumn
                        x: 28
                        y: 22
                        width: detailFlick.width - 56
                        spacing: 0

                        // ── Empty state ─────────────────────────────────────
                        Column {
                            visible: sheet.sourceTitle.length === 0 && sheet.result() === null
                            width: parent.width
                            spacing: 10

                            Text {
                                text: "Pick something to send."
                                color: Theme.text
                                font.family: Theme.fontBook
                                font.pixelSize: 22
                                font.italic: true
                            }
                            Text {
                                width: parent.width
                                text: "Choose a book on the left, or add a file. PDFs and text are converted into a Kindle-ready EPUB; EPUBs are cleaned up for the Kindle."
                                color: Theme.textMuted
                                font.family: Theme.fontUI
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                                lineHeight: 1.45
                            }
                        }

                        Item { width: 1; height: 16 }

                        // ── The source ────────────────────────────────────────
                        Rectangle {
                            id: sourceCard
                            width: parent.width
                            height: 128
                            radius: 12
                            color: Theme.ink2
                            border.width: 1
                            border.color: Theme.line

                            visible: sheet.sourceTitle.length > 0 && sheet.result() === null

                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 20
                                anchors.right: parent.right
                                anchors.rightMargin: 20
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 8

                                Text {
                                    text: sheet.sourceFormat === "pdf"
                                          ? "PDF → REFLOWABLE EPUB"
                                          : (sheet.sourceFormat === "epub"
                                             ? "EPUB → CLEAN KINDLE PACKAGE"
                                             : "TEXT → REFLOWABLE EPUB")
                                    color: Theme.copper
                                    font.family: Theme.fontMono
                                    font.pixelSize: 8
                                    font.letterSpacing: 1.4
                                }

                                Text {
                                    width: parent.width
                                    text: sheet.sourceTitle
                                    color: Theme.text
                                    font.family: Theme.fontBook
                                    font.pixelSize: 20
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: "Kindle reads EPUB directly. PDFs and text are re-flowed into one continuous book with a cover, a table of contents and clean metadata."
                                    color: Theme.textMuted
                                    font.family: Theme.fontUI
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                        }

                        Item { width: 1; height: 16 }

                        // ── The result ────────────────────────────────────────
                        Rectangle {
                            id: resultCard
                            width: parent.width
                            height: 116
                            radius: 12
                            color: Theme.ink2
                            border.width: 1
                            border.color: sheet.result() && sheet.result().ok ? Theme.copperSoft : Theme.line

                            visible: sheet.result() !== null

                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 20
                                anchors.right: parent.right
                                anchors.rightMargin: 20
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 7

                                Text {
                                    text: sheet.result() && sheet.result().ok ? "READY TO SEND" : "COULD NOT CONVERT"
                                    color: sheet.result() && sheet.result().ok ? Theme.copper : Theme.rose
                                    font.family: Theme.fontMono
                                    font.pixelSize: 8
                                    font.letterSpacing: 1.4
                                }

                                Text {
                                    width: parent.width
                                    text: sheet.result()
                                          ? (sheet.result().ok ? sheet.result().fileName : sheet.result().error)
                                          : ""
                                    color: Theme.text
                                    font.family: Theme.fontUI
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }

                                Text {
                                    visible: sheet.result() && sheet.result().ok
                                    text: sheet.result() && sheet.result().ok
                                          ? (sheet.result().pages + " pages   ·   "
                                             + sheet.result().words + " words   ·   "
                                             + Math.round((sheet.result().bytes || 0) / 1024) + " KB")
                                          : ""
                                    color: Theme.textFaint
                                    font.family: Theme.fontMono
                                    font.pixelSize: 8
                                    font.letterSpacing: 0.8
                                }
                            }
                        }

                        Item { width: 1; height: 18 }

                        // ── Actions ────────────────────────────────────────────
                        Row {
                            spacing: 8

                            PillButton {
                                label: controller.kindleBusy ? "Working…" : "Convert"
                                iconName: "spark"
                                primary: true
                                enabled: !controller.kindleBusy && sheet.sourcePath.length > 0
                                onClicked: {
                                    if (sheet.selectedBook)
                                        controller.convertBookForKindle(sheet.selectedBook.id)
                                    else
                                        controller.convertFileForKindle("file://" + sheet.sourcePath)
                                }
                            }

                            PillButton {
                                label: "Send to Kindle"
                                iconName: "send"
                                enabled: !controller.kindleBusy && sheet.result() !== null
                                         && sheet.result().ok
                                onClicked: {
                                    const r = sheet.result()
                                    if (!r)
                                        return
                                    sheet.saveAccount()
                                    controller.sendConvertedToKindle(r.path, r.title, r.author)
                                }
                            }

                            PillButton {
                                label: "Open sendtokindle"
                                iconName: "arrow-right"
                                onClicked: controller.openSendToKindle()
                            }

                            PillButton {
                                label: "Reveal"
                                enabled: sheet.result() !== null && sheet.result().ok
                                onClicked: {
                                    const r = sheet.result()
                                    if (r)
                                        controller.revealInFolder(r.path)
                                }
                            }
                        }

                        Item { width: 1; height: 14 }

                        // ── Destination ───────────────────────────────────────
                        Text {
                            text: "SEND TO KINDLE"
                            color: Theme.textGhost
                            font.family: Theme.fontMono
                            font.pixelSize: 8
                            font.letterSpacing: 1.6
                        }

                        Item { width: 1; height: 10 }

                        TextField {
                            id: kindleAddress
                            width: parent.width
                            height: 38
                            color: Theme.text
                            placeholderText: "your-name_123@kindle.com"
                            placeholderTextColor: Theme.textGhost
                            font.family: Theme.fontUI
                            font.pixelSize: 13
                            selectByMouse: true
                            background: Rectangle {
                                color: Theme.ink2
                                radius: 9
                                border.width: 1
                                border.color: Theme.line
                            }
                        }

                        Item { width: 1; height: 6 }

                        Text {
                            width: parent.width
                            text: "Your Send to Kindle address lives in Amazon → Manage Your Content & Devices → Personal Document Settings."
                            color: Theme.textFaint
                            font.family: Theme.fontUI
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        Item { width: 1; height: 16 }

                        // ── Sending account ────────────────────────────────────
                        Rectangle {
                            id: accountCard
                            width: parent.width
                            height: sheet.settingsOpen ? 168 : 40
                            radius: 10
                            color: Theme.ink2
                            border.width: 1
                            border.color: Theme.line
                            clip: true

                            Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                            MouseArea {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: 40
                                z: 1
                                cursorShape: Qt.PointingHandCursor
                                onClicked: sheet.settingsOpen = !sheet.settingsOpen
                            }

                            Row {
                                id: accountHeader
                                z: 2
                                anchors.left: parent.left
                                anchors.leftMargin: 16
                                anchors.top: parent.top
                                anchors.topMargin: 11
                                spacing: 8

                                Icon {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 13
                                    height: 13
                                    name: "spark"
                                    color: Theme.textFaint
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "SENDING ACCOUNT"
                                    color: Theme.textFaint
                                    font.family: Theme.fontMono
                                    font.pixelSize: 8
                                    font.letterSpacing: 1.4
                                }
                            }

                            Text {
                                id: editLabel
                                anchors.right: parent.right
                                anchors.rightMargin: 16
                                anchors.verticalCenter: parent.verticalCenter
                                text: sheet.settingsOpen ? "HIDE" : "EDIT"
                                color: Theme.copper
                                font.family: Theme.fontMono
                                font.pixelSize: 8
                                font.letterSpacing: 1.2

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -10
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: sheet.settingsOpen = !sheet.settingsOpen
                                }
                            }

                            Grid {
                                visible: sheet.settingsOpen
                                anchors.top: parent.top
                                anchors.topMargin: 40
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                columns: 2
                                columnSpacing: 12
                                rowSpacing: 8

                                TextField {
                                    id: fromField
                                    width: (parent.width - 12) / 2
                                    height: 34
                                    color: Theme.text
                                    placeholderText: "from@yourmail.com"
                                    placeholderTextColor: Theme.textGhost
                                    font.family: Theme.fontUI
                                    font.pixelSize: 12
                                    selectByMouse: true
                                    background: Rectangle {
                                        color: Theme.ink1
                                        radius: 8
                                        border.width: 1
                                        border.color: Theme.line
                                    }
                                }

                                TextField {
                                    id: hostField
                                    width: (parent.width - 12) / 2
                                    height: 34
                                    color: Theme.text
                                    placeholderText: "smtp.gmail.com"
                                    placeholderTextColor: Theme.textGhost
                                    font.family: Theme.fontUI
                                    font.pixelSize: 12
                                    selectByMouse: true
                                    background: Rectangle {
                                        color: Theme.ink1
                                        radius: 8
                                        border.width: 1
                                        border.color: Theme.line
                                    }
                                }

                                TextField {
                                    id: userField
                                    width: (parent.width - 12) / 2
                                    height: 34
                                    color: Theme.text
                                    placeholderText: "SMTP user (often your email)"
                                    placeholderTextColor: Theme.textGhost
                                    font.family: Theme.fontUI
                                    font.pixelSize: 12
                                    selectByMouse: true
                                    background: Rectangle {
                                        color: Theme.ink1
                                        radius: 8
                                        border.width: 1
                                        border.color: Theme.line
                                    }
                                }

                                TextField {
                                    id: passwordField
                                    width: (parent.width - 12) / 2
                                    height: 34
                                    color: Theme.text
                                    placeholderText: controller.kindleSettings && controller.kindleSettings.hasPassword
                                                     ? "saved · type to replace" : "app password"
                                    placeholderTextColor: Theme.textGhost
                                    font.family: Theme.fontUI
                                    font.pixelSize: 12
                                    echoMode: TextInput.Password
                                    selectByMouse: true
                                    background: Rectangle {
                                        color: Theme.ink1
                                        radius: 8
                                        border.width: 1
                                        border.color: Theme.line
                                    }
                                }

                                Item {
                                    width: parent.width - 12
                                    height: 30
                                    Row {
                                        anchors.right: parent.right
                                        spacing: 8

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: "Kept on this machine only."
                                            color: Theme.textGhost
                                            font.family: Theme.fontUI
                                            font.pixelSize: 10
                                        }

                                        PillButton {
                                            anchors.verticalCenter: parent.verticalCenter
                                            label: "Save"
                                            primary: true
                                            onClicked: sheet.saveAccount()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

            // ── Footer ───────────────────────────────────────────────────────
            Rectangle {
                width: parent.width
                height: 46
                color: "transparent"

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: Theme.line
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: controller.kindleStatus.length > 0
                          ? controller.kindleStatus
                          : "Amazon converts EPUB to Kindle format on their side."
                    color: controller.kindleBusy ? Theme.copper : Theme.textGhost
                    font.family: Theme.fontUI
                    font.pixelSize: 12
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: "esc close"
                    color: Theme.textGhost
                    font.family: Theme.fontMono
                    font.pixelSize: 8
                    font.letterSpacing: 0.6
                }
            }
        }
    }

    // Seed the fields from the stored settings.
    onVisibleChanged: {
        if (!visible)
            return
        const settings = controller.kindleSettings || {}
        kindleAddress.text = settings.kindleAddress || ""
        fromField.text = settings.fromAddress || ""
        hostField.text = settings.smtpHost || "smtp.gmail.com"
        userField.text = settings.user || ""
        passwordField.text = ""
    }
}
