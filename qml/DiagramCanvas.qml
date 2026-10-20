import QtQuick
import QtQuick.Controls
import "."

Item {
    id: overlay
    anchors.fill: parent
    visible: false
    z: 110

    property int selectedNode: 0
    property int connectFrom: 0
    property bool connectMode: false
    property int hoveredEdge: 0
    // Bumped whenever a card moves, so edges and their handles follow it
    // during a drag instead of jumping when the mouse is released.
    property int layoutRevision: 0

    signal closed()

    onLayoutRevisionChanged: edgeCanvas.requestPaint()
    onHoveredEdgeChanged: edgeCanvas.requestPaint()

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
        overlay.connectMode = false
        overlay.connectFrom = 0
        overlay.closed()
    }

    function cardsById() {
        const cards = {}
        for (let i = 0; i < nodeRepeater.count; ++i) {
            const card = nodeRepeater.itemAt(i)
            if (card)
                cards[card.nodeId] = card
        }
        return cards
    }

    // Where the line between two card centres crosses a card's border,
    // pushed `gap` pixels outside it.
    function borderPoint(card, dx, dy, gap) {
        const halfWidth = card.width / 2 + gap
        const halfHeight = card.height / 2 + gap
        const t = Math.min(dx !== 0 ? halfWidth / Math.abs(dx) : Infinity,
                           dy !== 0 ? halfHeight / Math.abs(dy) : Infinity)
        return Qt.point(card.x + card.width / 2 + dx * t, card.y + card.height / 2 + dy * t)
    }

    // An edge runs from border to border of the cards' live positions, so it
    // is never buried under a card, and bows gently. Overlapping cards have
    // no visible edge between them.
    function edgeGeometry(edge, cards) {
        const from = cards[edge.fromId]
        const to = cards[edge.toId]
        if (!from || !to)
            return null
        const dx = (to.x + to.width / 2) - (from.x + from.width / 2)
        const dy = (to.y + to.height / 2) - (from.y + from.height / 2)
        if (dx === 0 && dy === 0)
            return null
        const start = borderPoint(from, dx, dy, 3)
        const end = borderPoint(to, -dx, -dy, 3)
        const ex = end.x - start.x
        const ey = end.y - start.y
        const length = Math.sqrt(ex * ex + ey * ey)
        if (length < 18 || ex * dx + ey * dy <= 0)
            return null
        const bow = Math.min(26, length * 0.12)
        const control = Qt.point((start.x + end.x) / 2 + ey / length * bow,
                                 (start.y + end.y) / 2 - ex / length * bow)
        return {
            start: start,
            end: end,
            control: control,
            middle: Qt.point(0.25 * start.x + 0.5 * control.x + 0.25 * end.x,
                             0.25 * start.y + 0.5 * control.y + 0.25 * end.y)
        }
    }

    Connections {
        target: controller
        function onDiagramChanged() {
            edgeCanvas.requestPaint()
        }
    }
    Connections {
        target: Theme
        function onDarkChanged() {
            edgeCanvas.requestPaint()
            gridCanvas.requestPaint()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        // Nothing behind the canvas may receive clicks or scrolling.
        MouseArea {
            anchors.fill: parent
            onClicked: overlay.close()
            onWheel: function(wheel) { wheel.accepted = true }
        }
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
            anchors.margins: 1
            height: 63
            radius: sheet.radius - 1
            color: Theme.ink1
            z: 2

            // Square off the bottom corners; only the top follows the sheet.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.radius
                color: parent.color
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.line
            }

            MouseArea { anchors.fill: parent } // Swallows clicks between the controls.

            Item {
                anchors.fill: parent
                anchors.leftMargin: 23
                anchors.rightMargin: 17

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
                        objectName: "diagramHint"
                        text: !overlay.connectMode ? "A MAP OF THE MARGIN"
                              : overlay.connectFrom === 0 ? "CHOOSE THE FIRST NODE"
                              : "NOW CHOOSE THE NODE TO CONNECT IT TO"
                        color: overlay.connectMode ? Theme.copper : Theme.textGhost
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
                        iconName: "diagram"
                        active: overlay.connectMode
                        onClicked: {
                            overlay.connectMode = !overlay.connectMode
                            overlay.connectFrom = overlay.connectMode ? overlay.selectedNode : 0
                        }
                    }

                    PillButton {
                        label: "Add node"
                        iconName: "plus"
                        primary: true
                        onClicked: {
                            const id = controller.addDiagramNode("New thought", "A note from the margin.",
                                                                140 + Math.random() * Math.max(40, canvasArea.width - 400),
                                                                130 + Math.random() * Math.max(40, canvasArea.height - 300))
                            if (id > 0)
                                overlay.selectedNode = id
                            edgeCanvas.requestPaint()
                        }
                    }

                    TopAction {
                        iconName: "close"
                        label: "Close"
                        onClicked: overlay.close()
                    }
                }
            }
        }

        // ── Canvas ──────────────────────────────────────────────────────────
        Item {
            id: canvasArea
            objectName: "diagramCanvasArea"
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
                    ctx.fillStyle = Theme.gridDots
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
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    const cards = overlay.cardsById()
                    const edges = controller.diagramEdges
                    for (let i = 0; i < edges.length; ++i) {
                        const g = overlay.edgeGeometry(edges[i], cards)
                        if (!g)
                            continue
                        const tone = overlay.hoveredEdge === edges[i].id ? Theme.rose : Theme.copper
                        // The line stops at the arrowhead's base; the head
                        // points along the curve's final direction.
                        const ax = g.end.x - g.control.x
                        const ay = g.end.y - g.control.y
                        const al = Math.max(0.001, Math.sqrt(ax * ax + ay * ay))
                        const ux = ax / al
                        const uy = ay / al
                        const head = 9
                        ctx.strokeStyle = Qt.alpha(tone, 0.7)
                        ctx.beginPath()
                        ctx.moveTo(g.start.x, g.start.y)
                        ctx.quadraticCurveTo(g.control.x, g.control.y, g.end.x - ux * head * 0.7, g.end.y - uy * head * 0.7)
                        ctx.stroke()
                        ctx.fillStyle = tone
                        ctx.beginPath()
                        ctx.moveTo(g.end.x, g.end.y)
                        ctx.lineTo(g.end.x - ux * head - uy * head * 0.45, g.end.y - uy * head + ux * head * 0.45)
                        ctx.lineTo(g.end.x - ux * head + uy * head * 0.45, g.end.y - uy * head - ux * head * 0.45)
                        ctx.closePath()
                        ctx.fill()
                        ctx.beginPath()
                        ctx.arc(g.start.x, g.start.y, 2.4, 0, Math.PI * 2)
                        ctx.fill()
                    }
                }
            }

            // Empty canvas: clears the selection, cancels connecting, and keeps
            // clicks from reaching the reader underneath.
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    overlay.selectedNode = 0
                    overlay.connectMode = false
                    overlay.connectFrom = 0
                }
                onWheel: function(wheel) { wheel.accepted = true }
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

            // A knot at the middle of each edge; hover it to remove the edge.
            Repeater {
                model: controller.diagramEdges

                delegate: Item {
                    id: handle
                    readonly property var geometry: {
                        overlay.layoutRevision
                        return overlay.edgeGeometry(modelData, overlay.cardsById())
                    }
                    objectName: "edgeHandle-" + modelData.id
                    visible: geometry !== null
                    width: 24
                    height: 24
                    x: geometry ? geometry.middle.x - width / 2 : 0
                    y: geometry ? geometry.middle.y - height / 2 : 0

                    Rectangle {
                        anchors.centerIn: parent
                        width: edgeHover.hovered ? 20 : 7
                        height: width
                        radius: width / 2
                        color: edgeHover.hovered ? Theme.ink3 : Theme.copper
                        border.width: edgeHover.hovered ? 1 : 0
                        border.color: Theme.lineStrong
                        Behavior on width { NumberAnimation { duration: 110 } }
                    }
                    Icon {
                        anchors.centerIn: parent
                        visible: edgeHover.hovered
                        width: 10
                        height: 10
                        name: "close"
                        color: Theme.rose
                    }

                    HoverHandler {
                        id: edgeHover
                        onHoveredChanged: overlay.hoveredEdge = hovered ? modelData.id : 0
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            overlay.hoveredEdge = 0
                            controller.deleteDiagramEdge(modelData.id)
                        }
                    }
                    ToolTip.visible: edgeHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: "Remove connection"
                }
            }

            Repeater {
                id: nodeRepeater
                model: controller.diagramNodes

                delegate: Rectangle {
                    id: nodeCard
                    readonly property int nodeId: modelData.id
                    // Where the node belongs; the card itself may be pulled
                    // inside a smaller window and returns when there is room.
                    property real homeX: modelData.x
                    property real homeY: modelData.y

                    objectName: "diagramNode-" + nodeId
                    width: 188
                    height: 100
                    radius: 9
                    color: Theme.paper
                    border.width: 1
                    border.color: overlay.selectedNode === nodeId || overlay.connectFrom === nodeId
                                  ? Theme.copper : Theme.paperLine

                    Behavior on border.color { ColorAnimation { duration: 130 } }

                    function place() {
                        if (canvasArea.width <= width + 40 || canvasArea.height <= height + 40) {
                            x = homeX
                            y = homeY
                            return
                        }
                        x = Math.max(20, Math.min(homeX, canvasArea.width - width - 20))
                        y = Math.max(20, Math.min(homeY, canvasArea.height - height - 20))
                    }

                    Component.onCompleted: place()
                    onXChanged: overlay.layoutRevision++
                    onYChanged: overlay.layoutRevision++

                    Connections {
                        target: canvasArea
                        function onWidthChanged() { nodeCard.place() }
                        function onHeightChanged() { nodeCard.place() }
                    }

                    // A soft shadow beneath the card.
                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: 5
                        anchors.bottomMargin: -4
                        radius: 9
                        color: Theme.shadow
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
                        cursorShape: overlay.connectMode ? Qt.CrossCursor : Qt.SizeAllCursor
                        drag.target: overlay.connectMode ? null : nodeCard
                        drag.axis: Drag.XAndYAxis
                        drag.minimumX: 20
                        drag.minimumY: 20
                        drag.maximumX: Math.max(20, canvasArea.width - nodeCard.width - 20)
                        drag.maximumY: Math.max(20, canvasArea.height - nodeCard.height - 20)
                        onPressed: function(mouse) {
                            if (mouse.button !== Qt.LeftButton)
                                return
                            overlay.selectedNode = nodeCard.nodeId
                            if (overlay.connectMode) {
                                if (overlay.connectFrom === 0) {
                                    overlay.connectFrom = nodeCard.nodeId
                                } else if (overlay.connectFrom !== nodeCard.nodeId) {
                                    controller.addDiagramEdge(overlay.connectFrom, nodeCard.nodeId)
                                    overlay.connectFrom = 0
                                    overlay.connectMode = false
                                    edgeCanvas.requestPaint()
                                }
                            }
                        }
                        onReleased: {
                            // Only an actual drag is saved; a click leaves the
                            // node where it was stored.
                            if (!drag.active)
                                return
                            nodeCard.homeX = nodeCard.x
                            nodeCard.homeY = nodeCard.y
                            controller.moveDiagramNode(nodeCard.nodeId, Math.round(nodeCard.x), Math.round(nodeCard.y))
                        }
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton)
                                controller.deleteDiagramNode(nodeCard.nodeId)
                        }
                    }

                    Column {
                        anchors.fill: parent
                        anchors.margins: 15
                        anchors.leftMargin: 20
                        spacing: 5

                        TextInput {
                            id: nodeLabel
                            width: parent.width - 16
                            text: modelData.label
                            color: Theme.paperInk
                            font.family: Theme.fontBook
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            selectByMouse: true
                            clip: true
                            onEditingFinished: {
                                const label = text.trim()
                                if (label.length === 0) {
                                    text = modelData.label // A node keeps a name.
                                    return
                                }
                                if (label !== modelData.label)
                                    controller.updateDiagramNode(nodeCard.nodeId, label, modelData.body)
                            }
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
                        color: nodeDeleteHover.hovered ? Theme.copperWash : "transparent"

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
                            onClicked: controller.deleteDiagramNode(nodeCard.nodeId)
                        }
                    }
                }
            }
        }
    }
}
