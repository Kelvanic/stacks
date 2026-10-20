import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

Rectangle {
    id: pane

    color: Theme.ink1

    signal diagramRequested()

    property string filter: "all"

    readonly property var months: ["JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                   "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"]

    function visibleAnnotations() {
        const all = controller.annotations || []
        if (pane.filter === "all")
            return all
        return all.filter(a => (a && a.kind) === pane.filter)
    }

    function colorFor(kind) {
        if (kind === "bookmark")
            return Theme.moss
        if (kind === "note")
            return Theme.ochre
        if (kind === "voice")
            return Theme.violet
        return Theme.copper
    }

    function labelFor(kind) {
        if (kind === "bookmark")
            return "BOOKMARK"
        if (kind === "note")
            return "NOTE"
        if (kind === "voice")
            return "VOICE"
        return "HIGHLIGHT"
    }

    function dateFor(stamp) {
        const s = String(stamp || "")
        if (s.length < 10)
            return ""
        const month = parseInt(s.substring(5, 7), 10) - 1
        const day = parseInt(s.substring(8, 10), 10)
        if (month < 0 || month > 11)
            return ""
        return pane.months[month] + " " + day
    }

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: Theme.line
    }

    // ── Header ───────────────────────────────────────────────────────────────
    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 62

        Text {
            id: headerLabel
            anchors.left: parent.left
            anchors.leftMargin: 22
            anchors.verticalCenter: parent.verticalCenter
            text: "MARGIN"
            color: Theme.textGhost
            font.family: Theme.fontMono
            font.pixelSize: 8
            font.letterSpacing: 1.6
        }

        Rectangle {
            anchors.left: headerLabel.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: countLabel.implicitWidth + 14
            height: 18
            radius: 9
            color: Theme.ink3

            Text {
                id: countLabel
                anchors.centerIn: parent
                text: controller.annotations.length
                color: Theme.textMuted
                font.family: Theme.fontMono
                font.pixelSize: 9
            }
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 22
            anchors.verticalCenter: parent.verticalCenter
            text: (controller.currentBook.title || "").toUpperCase()
            color: Theme.textGhost
            font.family: Theme.fontMono
            font.pixelSize: 8
            font.letterSpacing: 1
            elide: Text.ElideLeft
            width: Math.min(implicitWidth, 140)
        }
    }

    // ── Filters ─────────────────────────────────────────────────────────────
    Row {
        id: filters
        anchors.top: header.bottom
        anchors.topMargin: 2
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.right: parent.right
        anchors.rightMargin: 22
        height: 24
        spacing: 6

        Repeater {
            model: [
                { key: "all", label: "ALL" },
                { key: "highlight", label: "MARKS" },
                { key: "note", label: "NOTES" },
                { key: "voice", label: "VOICE" }
            ]
            delegate: Rectangle {
                id: chip
                height: 24
                width: chipText.implicitWidth + 18
                radius: 6
                color: pane.filter === modelData.key ? Theme.copperWash
                             : (chipHover.hovered ? Theme.ink3 : "transparent")
                border.width: 1
                border.color: pane.filter === modelData.key ? Theme.copperSoft : Theme.line

                Behavior on color { ColorAnimation { duration: 120 } }

                Text {
                    id: chipText
                    anchors.centerIn: parent
                    text: modelData.label
                    color: pane.filter === modelData.key ? Theme.copper : Theme.textFaint
                    font.family: Theme.fontMono
                    font.pixelSize: 8
                    font.letterSpacing: 1.1
                }

                HoverHandler { id: chipHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: pane.filter = modelData.key
                }
            }
        }
    }

    // ── Empty state ─────────────────────────────────────────────────────────
    Item {
        id: emptyState
        visible: pane.visibleAnnotations().length === 0
        anchors.top: filters.bottom
        anchors.topMargin: 18
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: diagram.top
        anchors.bottomMargin: 12

        Column {
            anchors.centerIn: parent
            width: parent.width - 70
            spacing: 12

            // A margin that has not been written on yet.
            Item {
                width: parent.width
                height: 34
                Repeater {
                    model: 3
                    delegate: Rectangle {
                        x: index * 10
                        y: index * 11
                        width: parent.width - index * 34
                        height: 1
                        color: Theme.lineStrong
                        opacity: 1 - index * 0.3
                    }
                }
            }

            Text {
                width: parent.width
                text: "The margin is yours."
                color: Theme.text
                font.family: Theme.fontBook
                font.pixelSize: 19
                font.italic: true
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                text: "Select a line of text to highlight it, or press the note button to write in the margin."
                color: Theme.textFaint
                font.family: Theme.fontUI
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.4
            }
        }
    }

    // ── Timeline ────────────────────────────────────────────────────────────
    ListView {
        id: timeline
        visible: pane.visibleAnnotations().length > 0
        anchors.top: filters.bottom
        anchors.topMargin: 14
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.right: parent.right
        anchors.rightMargin: 22
        anchors.bottom: diagram.top
        anchors.bottomMargin: 8
        clip: true
        spacing: 0
        boundsBehavior: Flickable.StopAtBounds
        model: pane.visibleAnnotations()

        delegate: Item {
            id: entry
            width: timeline.width
            height: body.implicitHeight + 28

            property var annotation: modelData
            property string kind: (modelData && modelData.kind) ? modelData.kind : "note"
            property color tone: pane.colorFor(entry.kind)

            // The thread that ties the margin together.
            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.lineSoft
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 8
                width: 9
                height: 9
                radius: 4.5
                color: entry.tone

                Rectangle {
                    anchors.centerIn: parent
                    width: 17
                    height: 17
                    radius: 8.5
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.alpha(entry.tone, 0.35)
                }
            }

            Column {
                id: body
                anchors.left: parent.left
                anchors.leftMargin: 22
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.top: parent.top
                anchors.topMargin: 4
                spacing: 7

                Text {
                    visible: (entry.annotation.quote || "").length > 0
                    width: parent.width
                    text: entry.annotation.quote || ""
                    color: Theme.text
                    font.family: Theme.fontBook
                    font.pixelSize: 15
                    font.italic: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                    lineHeight: 1.3
                }

                Text {
                    visible: (entry.annotation.note || "").length > 0
                    width: parent.width
                    text: entry.annotation.note || ""
                    color: Theme.textMuted
                    font.family: Theme.fontUI
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    maximumLineCount: 5
                    elide: Text.ElideRight
                    lineHeight: 1.42
                }

                Row {
                    visible: entry.kind === "voice"
                    spacing: 4
                    Repeater {
                        model: 20
                        delegate: Rectangle {
                            width: 2
                            height: 4 + 9 * Math.abs(Math.sin(index * 1.5))
                            radius: 1
                            color: Theme.violet
                            opacity: 0.65
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Math.round((entry.annotation.durationMs || 0) / 1000) + "s"
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        leftPadding: 6
                    }
                }

                Row {
                    spacing: 8

                    Text {
                        text: pane.labelFor(entry.kind)
                        color: entry.tone
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 1.2
                    }
                    Text {
                        text: "·"
                        color: Theme.textGhost
                        font.pixelSize: 9
                    }
                    Text {
                        text: "PAGE " + ((entry.annotation.page || 0) + 1)
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 1
                    }
                    Text {
                        visible: pane.dateFor(entry.annotation.createdAt).length > 0
                        text: "· " + pane.dateFor(entry.annotation.createdAt)
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 1
                    }
                }
            }

            // Delete, on hover.
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 2
                z: 1
                objectName: "annotationDelete-" + entry.annotation.id
                width: 22
                height: 22
                radius: 11
                color: deleteHover.hovered ? Theme.ink4 : "transparent"
                opacity: entryHover.hovered || deleteHover.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 120 } }

                Icon {
                    anchors.centerIn: parent
                    width: 12
                    height: 12
                    name: "close"
                    color: deleteHover.hovered ? Theme.rose : Theme.textGhost
                }

                HoverHandler { id: deleteHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: controller.deleteAnnotation(entry.annotation.id)
                }
            }

            HoverHandler { id: entryHover }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (entry.kind === "voice" && entry.annotation.audioPath)
                        Qt.openUrlExternally("file://" + entry.annotation.audioPath)
                    else
                        controller.openAnnotation(entry.annotation.id)
                }
            }
        }
    }

    // ── Diagram ─────────────────────────────────────────────────────────────
    Item {
        id: diagram
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 96

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.topMargin: 10
            anchors.bottomMargin: 14
            radius: 12
            color: diagramHover.hovered ? Theme.ink3 : Theme.ink2
            border.width: 1
            border.color: diagramHover.hovered ? Theme.lineStrong : Theme.line

            Behavior on color { ColorAnimation { duration: 130 } }

            Canvas {
                id: mapPreview
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                width: 52
                height: 42

                onPaint: {
                    const ctx = getContext("2d")
                    if (typeof ctx.reset === "function")
                        ctx.reset()
                    else
                        ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = Theme.copper
                    ctx.fillStyle = Theme.ink1
                    ctx.lineWidth = 1.2
                    ctx.beginPath()
                    ctx.moveTo(14, 12)
                    ctx.lineTo(36, 20)
                    ctx.lineTo(18, 34)
                    ctx.closePath()
                    ctx.stroke()
                    for (const p of [[14, 12], [36, 20], [18, 34]]) {
                        ctx.beginPath()
                        ctx.arc(p[0], p[1], 3.2, 0, Math.PI * 2)
                        ctx.fill()
                        ctx.stroke()
                    }
                }
            }

            Column {
                anchors.left: mapPreview.right
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    text: "Diagram canvas"
                    color: Theme.text
                    font.family: Theme.fontUI
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
                Text {
                    text: "Turn the margin into a map"
                    color: Theme.textFaint
                    font.family: Theme.fontUI
                    font.pixelSize: 11
                }
            }

            KeyCap {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                label: "⌘⇧D"
            }

            HoverHandler { id: diagramHover }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: pane.diagramRequested()
            }
        }
    }
}
