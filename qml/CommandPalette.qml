import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

Item {
    id: overlay
    anchors.fill: parent
    visible: false
    z: 100

    property int selectedIndex: 0
    signal closed()

    function open() {
        overlay.visible = true
        queryInput.text = ""
        overlay.selectedIndex = 0
        controller.clearSearch()
        queryInput.forceActiveFocus()
    }

    function close() {
        overlay.visible = false
        overlay.closed()
    }

    Rectangle {
        anchors.fill: parent
        color: "#B3000000"
        MouseArea {
            anchors.fill: parent
            onClicked: overlay.close()
        }
    }

    Rectangle {
        id: panel
        width: 740
        height: controller.searchResults.length > 0
                ? Math.min(600, 68 + 1 + Math.min(6, controller.searchResults.length) * 78 + 46)
                : Math.min(480, overlay.height - 140)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 86
        radius: 16
        color: Theme.ink1
        border.width: 1
        border.color: Theme.lineStrong

        opacity: overlay.visible ? 1 : 0
        scale: overlay.visible ? 1 : 0.985
        transformOrigin: Item.Top
        Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 10
            radius: 16
            color: "#55000000"
            z: -1
        }

        Column {
            anchors.fill: parent
            spacing: 0

            // ── Query ────────────────────────────────────────────────────────
            Item {
                width: parent.width
                height: 68

                Icon {
                    id: magnifier
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    width: 17
                    height: 17
                    name: "search"
                    color: Theme.textFaint
                }

                TextField {
                    id: queryInput
                    anchors.left: magnifier.right
                    anchors.leftMargin: 14
                    anchors.right: escCap.left
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.text
                    placeholderText: "Search every page, highlight and note…"
                    placeholderTextColor: Theme.textGhost
                    font.family: Theme.fontUI
                    font.pixelSize: 18
                    selectByMouse: true
                    background: Rectangle { color: "transparent" }

                    onTextChanged: searchTimer.restart()

                    Keys.onEscapePressed: overlay.close()
                    Keys.onDownPressed: {
                        if (controller.searchResults.length > 0)
                            overlay.selectedIndex = Math.min(overlay.selectedIndex + 1, controller.searchResults.length - 1)
                    }
                    Keys.onUpPressed: {
                        overlay.selectedIndex = Math.max(0, overlay.selectedIndex - 1)
                    }
                    Keys.onReturnPressed: {
                        if (controller.searchResults.length > 0) {
                            controller.openSearchResult(overlay.selectedIndex)
                            overlay.close()
                        }
                    }
                }

                KeyCap {
                    id: escCap
                    anchors.right: parent.right
                    anchors.rightMargin: 22
                    anchors.verticalCenter: parent.verticalCenter
                    label: "esc"
                }

                Timer {
                    id: searchTimer
                    interval: 90
                    onTriggered: {
                        overlay.selectedIndex = 0
                        controller.search(queryInput.text)
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.line
            }

            // ── Results ─────────────────────────────────────────────────────
            ListView {
                id: resultList
                visible: controller.searchResults.length > 0
                width: parent.width
                height: parent.height - 68 - 1 - 46
                model: controller.searchResults
                clip: true
                spacing: 0
                boundsBehavior: Flickable.StopAtBounds
                currentIndex: overlay.selectedIndex

                delegate: Rectangle {
                    id: resultRow
                    width: resultList.width
                    height: 78
                    color: index === overlay.selectedIndex ? Theme.ink3
                           : (rowHover.hovered ? Theme.ink2 : "transparent")

                    Behavior on color { ColorAnimation { duration: 100 } }

                    Rectangle {
                        width: 2
                        height: 44
                        radius: 1
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.copper
                        visible: index === overlay.selectedIndex
                    }

                    // Format badge.
                    Rectangle {
                        id: badge
                        anchors.left: parent.left
                        anchors.leftMargin: 24
                        anchors.verticalCenter: parent.verticalCenter
                        width: 34
                        height: 34
                        radius: 8
                        color: Theme.ink1
                        border.width: 1
                        border.color: Theme.line

                        Text {
                            anchors.centerIn: parent
                            text: (modelData.format || "epub").toUpperCase()
                            color: Theme.textFaint
                            font.family: Theme.fontMono
                            font.pixelSize: 8
                            font.letterSpacing: 0.6
                        }
                    }

                    Column {
                        anchors.left: badge.right
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 24
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5

                        Row {
                            width: parent.width
                            spacing: 8
                            Text {
                                text: modelData.bookTitle || ""
                                color: Theme.text
                                font.family: Theme.fontUI
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                width: Math.min(implicitWidth, parent.width - 130)
                            }
                            Text {
                                text: modelData.locator || ""
                                color: Theme.textGhost
                                font.family: Theme.fontMono
                                font.pixelSize: 8
                                font.letterSpacing: 0.5
                            }
                        }

                        Text {
                            width: parent.width
                            text: String(modelData.snippet || "").replace(/<mark>/g, "<font color='#E08B57'><b>").replace(/<\/mark>/g, "</b></font>")
                            color: Theme.textMuted
                            font.family: Theme.fontUI
                            font.pixelSize: 12
                            textFormat: Text.RichText
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                        }
                    }

                    HoverHandler { id: rowHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onEntered: overlay.selectedIndex = index
                        onClicked: {
                            controller.openSearchResult(index)
                            overlay.close()
                        }
                    }
                }
            }

            // ── Empty state ─────────────────────────────────────────────────
            Item {
                visible: controller.searchResults.length === 0
                width: parent.width
                height: parent.height - 68 - 1 - 46

                Column {
                    anchors.centerIn: parent
                    width: parent.width - 120
                    spacing: 14

                    Text {
                        width: parent.width
                        text: queryInput.text.length > 0 ? "Nothing matches that." : "Search the whole desk."
                        color: Theme.text
                        font.family: Theme.fontBook
                        font.pixelSize: 24
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        width: parent.width
                        text: queryInput.text.length > 0
                              ? "Try a different word, or search for a phrase you remember from the page."
                              : "Every page, highlight, note and voice memo is indexed locally. Nothing leaves this machine."
                        color: Theme.textFaint
                        font.family: Theme.fontUI
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.45
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 10
                        visible: queryInput.text.length === 0

                        Repeater {
                            model: [
                                { key: "⌘O", label: "Import a book" },
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
            }

            // ── Footer ──────────────────────────────────────────────────────
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

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 22
                    spacing: 14

                    Text {
                        text: "↑↓ navigate"
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 0.6
                    }
                    Text {
                        text: "↵ open"
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 0.6
                    }
                    Text {
                        text: "esc close"
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 0.6
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 22
                    text: controller.searchResults.length > 0
                          ? controller.searchResults.length + " RESULTS"
                          : (queryInput.text.length > 0 ? "NO MATCHES" : "FULL-TEXT INDEX")
                    color: Theme.textGhost
                    font.family: Theme.fontMono
                    font.pixelSize: 8
                    font.letterSpacing: 1
                }
            }
        }
    }
}
