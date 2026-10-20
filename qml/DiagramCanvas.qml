import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

Item {
    id: overlay
    anchors.fill: parent
    visible: false
    z: 110

    property int selectedNode: 0
    property int connectFrom: 0
    property bool connectMode: false

    signal closed()

    function open() {
        overlay.visible = true
        overlay.selectedNode = 0
        overlay.connectFrom = 0
        overlay.connectMode = false
        controller.refreshDiagram()
        edgeCanvas.requestPaint()
        gridCanvas.requestPaint()
    }

    function close() {
        overlay.visible = false
        overlay.closed()
    }

    function nodeById(id) {
        for (let i = 0; i < controller.diagramNodes.length; ++i) {
            const node = controller.diagramNodes[i]
            if (node.id === id)
                return node
        }
        return null
    }

    Connections {
        target: controller
        function onDiagramChanged() {
            edgeCanvas.requestPaint()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#D90A0908"
    }

    Rectangle {
        id: sheet
        anchors.fill: parent
        anchors.margins: 30
        radius: 18
        color: Theme.ink1
        border.width: 1
        border.color: Theme.lineStrong
        clip: true

        // ── Top bar ─────────────────────────────────────────────────────────
        Rectangle {
            id: topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 64
            color: Theme.ink1
            z: 2

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.line
            }

            Item {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 18

                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: "Diagram canvas"
                        color: Theme.text
                        font.family: Theme.fontBook
                        font.pixelSize: 19
                        font.weight: Font.Medium
                    }
                    Text {
                        text: "A MAP OF THE MARGIN"
                        color: Theme.textGhost
                        font.family: Theme.fontMono
                        font.pixelSize: 8
                        font.letterSpacing: 1.6
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    PillButton {
                        label: overlay.connectMode ? "Connecting…" : "Connect"
                        icon: "diagram"
                        active: overlay.connectMode
                        onClicked: {
                            overlay.connectMode = !overlay.connectMode
                            overlay.connectFrom = overlay.selectedNode
                        }
                    }

                    PillButton {
                        label: "Add node"
                        icon: "plus"
                        primary: true
                        onClicked: {
                            const id = controller.addDiagramNode("New thought", "A note from the margin.",
                                                                140 + Math.random() * 360,
                                                                130 + Math.random() * 220)
                            if (id > 0)
                                overlay.selectedNode = id
                            edgeCanvas.requestPaint()
                        }
                    }

                    TopAction {
                        icon: "close"
                        label: "Close"
                        onClicked: overlay.close()
                    }
                }
            }
        }

        // ── Canvas ──────────────────────────────────────────────────────────
        Item {
            anchors.top: topBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            // A dotted field, like graph paper under a lamp.
            Canvas {
                id: gridCanvas
                anchors.fill: parent
                onWidthChanged: if (width > 0) requestPaint()
                onHeightChanged: if (height > 0) requestPaint()
                onVisibleChanged: if (visible) requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    if (typeof ctx.reset === "function")
                        ctx.reset()
                    else
                        ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = "rgba(240,233,219,0.07)"
                    const step = 28
                    for (let x = step; x < width; x += step) {
                        for (let y = step; y < height; y += step) {
                            ctx.beginPath()
                            ctx.arc(x, y, 1, 0, Math.PI * 2)
                            ctx.fill()
                        }
                    }
                }
            }

            Canvas {
                id: edgeCanvas
                anchors.fill: parent
                onWidthChanged: if (width > 0 && height > 0) requestPaint()
                onHeightChanged: if (width > 0 && height > 0) requestPaint()
                onVisibleChanged: if (visible) requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    if (typeof ctx.reset === "function")
                        ctx.reset()
                    else
                        ctx.clearRect(0, 0, width, height)
                    ctx.lineWidth = 1.6
                    ctx.strokeStyle = "rgba(201,112,63,0.6)"
                    ctx.fillStyle = "rgba(224,139,87,0.9)"

                    for (let i = 0; i < controller.diagramEdges.length; ++i) {
                        const edge = controller.diagramEdges[i]
                        const from = overlay.nodeById(edge.fromId)
                        const to = overlay.nodeById(edge.toId)
                        if (!from || !to)
                            continue
                        const x1 = from.x + 94
                        const y1 = from.y + 50
                        const x2 = to.x + 94
                        const y2 = to.y + 50
                        const mx = (x1 + x2) / 2
                        const my = (y1 + y2) / 2 - 18
                        ctx.beginPath()
                        ctx.moveTo(x1, y1)
                        ctx.quadraticCurveTo(mx, my, x2, y2)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.arc(x2, y2, 3.2, 0, Math.PI * 2)
                        ctx.fill()
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: controller.diagramNodes.length === 0
                text: "Add a node to begin mapping the margin."
                color: Theme.textFaint
                font.family: Theme.fontBook
                font.pixelSize: 18
                font.italic: true
            }

            Repeater {
                model: controller.diagramNodes

                delegate: Rectangle {
                    id: nodeCard
                    width: 188
                    height: 100
                    x: modelData.x
                    y: modelData.y
                    radius: 9
                    color: Theme.paper
                    border.width: 1
                    border.color: overlay.selectedNode === modelData.id ? Theme.copper : "rgba(32,28,22,0.16)"

                    Behavior on border.color { ColorAnimation { duration: 130 } }

                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: 5
                        radius: 9
                        color: "#33000000"
                        z: -1
                    }

                    Rectangle {
                        width: 3
                        height: parent.height - 20
                        anchors.left: parent.left
                        anchors.leftMargin: 9
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 1.5
                        color: Theme.copper
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.SizeAllCursor
                        drag.target: nodeCard
                        drag.axis: Drag.XAndYAxis
                        drag.minimumX: 20
                        drag.minimumY: 24
                        drag.maximumX: sheet.width - nodeCard.width - 20
                        drag.maximumY: sheet.height - nodeCard.height - 20
                        onPressed: {
                            overlay.selectedNode = modelData.id
                            if (overlay.connectMode) {
                                if (overlay.connectFrom === 0) {
                                    overlay.connectFrom = modelData.id
                                } else if (overlay.connectFrom !== modelData.id) {
                                    controller.addDiagramEdge(overlay.connectFrom, modelData.id)
                                    overlay.connectFrom = 0
                                    overlay.connectMode = false
                                    edgeCanvas.requestPaint()
                                }
                            }
                        }
                        onReleased: {
                            controller.moveDiagramNode(modelData.id, Math.round(nodeCard.x), Math.round(nodeCard.y))
                            edgeCanvas.requestPaint()
                        }
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton)
                                controller.deleteDiagramNode(modelData.id)
                        }
                    }

                    Column {
                        anchors.fill: parent
                        anchors.margins: 15
                        anchors.leftMargin: 20
                        spacing: 5

                        TextInput {
                            id: nodeLabel
                            width: parent.width
                            text: modelData.label
                            color: Theme.paperInk
                            font.family: Theme.fontBook
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            selectByMouse: true
                            clip: true
                            onEditingFinished: controller.updateDiagramNode(modelData.id, text, modelData.body)
                            Keys.onReturnPressed: focus = false
                        }

                        Text {
                            width: parent.width
                            text: modelData.body
                            color: Theme.paperMuted
                            font.family: Theme.fontUI
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Text {
                            text: "MARGIN NOTE"
                            color: Theme.copper
                            font.family: Theme.fontMono
                            font.pixelSize: 8
                            font.letterSpacing: 1.2
                        }
                    }

                    Rectangle {
                        width: 20
                        height: 20
                        radius: 10
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 7
                        color: nodeDeleteHover.hovered ? "rgba(32,28,22,0.12)" : "transparent"

                        Icon {
                            anchors.centerIn: parent
                            width: 11
                            height: 11
                            name: "close"
                            color: nodeDeleteHover.hovered ? Theme.rose : Theme.paperMuted
                        }

                        HoverHandler { id: nodeDeleteHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: controller.deleteDiagramNode(modelData.id)
                        }
                    }
                }
            }
        }
    }
}
