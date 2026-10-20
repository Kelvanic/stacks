import QtQuick
import "."

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
    color: accent

    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter(cover.accent, 1.22) }
        GradientStop { position: 0.55; color: cover.accent }
        GradientStop { position: 1.0; color: Qt.darker(cover.accent, 1.38) }
    }

    opacity: dim ? 0.55 : 1

    // Deterministic jacket art gives each local book its own identity. These
    // are designed fallback covers, never presented as publisher artwork.
    readonly property int artwork: {
        let hash = 0
        for (let i = 0; i < title.length; ++i)
            hash = (hash + title.charCodeAt(i) * (i + 1)) % 997
        return hash % 3
    }
    readonly property real spineWidth: Math.max(4, cover.width * 0.085)

    onArtworkChanged: jacketArt.requestPaint()
    onAccentChanged: jacketArt.requestPaint()

    // Spine, sheen, art and plate share one canvas clipped to the jacket's
    // rounded outline. Separate rectangles painted square corners over it,
    // and an item clip turns into an aliased stencil when the jacket tilts.
    Canvas {
        id: jacketArt
        anchors.fill: parent
        antialiasing: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width
            const h = height
            ctx.save()
            ctx.beginPath()
            ctx.roundedRect(0, 0, w, h, cover.radius, cover.radius)
            ctx.clip()

            // The spine: a darker band with a highlight where the cloth folds.
            ctx.globalAlpha = 0.55
            ctx.fillStyle = Qt.darker(cover.accent, 1.6)
            ctx.fillRect(0, 0, cover.spineWidth, h)
            ctx.globalAlpha = 1
            ctx.fillStyle = "rgba(255,255,255,0.22)"
            ctx.fillRect(cover.spineWidth, 0, 1, h)

            // Cloth sheen.
            const sheen = ctx.createLinearGradient(0, 0, w, 0)
            sheen.addColorStop(0.0, "rgba(255,255,255,0.10)")
            sheen.addColorStop(0.45, "rgba(255,255,255,0)")
            sheen.addColorStop(1.0, "rgba(0,0,0,0.14)")
            ctx.fillStyle = sheen
            ctx.fillRect(0, 0, w, h)

            ctx.strokeStyle = "rgba(255,244,221,0.27)"
            ctx.lineWidth = Math.max(0.6, w * 0.007)
            if (cover.artwork === 0) {
                for (let i = 0; i < 7; ++i) {
                    ctx.beginPath()
                    ctx.arc(w * 0.74, h * 0.73, w * (0.17 + i * 0.08), 0, Math.PI * 2)
                    ctx.stroke()
                }
            } else if (cover.artwork === 1) {
                for (let i = 0; i < 5; ++i) {
                    const x = w * (0.19 + i * 0.12)
                    const y = h * (0.78 - i * 0.035)
                    ctx.beginPath()
                    ctx.moveTo(x, h)
                    ctx.lineTo(x, y)
                    ctx.bezierCurveTo(x, y - w * 0.38, w * 0.90, y - w * 0.38, w * 0.90, y)
                    ctx.stroke()
                }
            } else {
                for (let i = 0; i < 7; ++i) {
                    ctx.beginPath()
                    ctx.moveTo(w * 0.16, h * (0.64 + i * 0.05))
                    ctx.bezierCurveTo(w * 0.45, h * (0.44 + i * 0.06), w * 0.65, h * (0.93 + i * 0.03), w * 1.12, h * (0.62 + i * 0.05))
                    ctx.stroke()
                }
            }

            // The hairline plate.
            const inset = Math.max(3, w * 0.045)
            const left = Math.max(6, w * 0.12)
            ctx.strokeStyle = "#22FFF4DD"
            ctx.lineWidth = 1
            ctx.strokeRect(left + 0.5, inset + 0.5, w - left - inset - 1, h - 2 * inset - 1)
            ctx.restore()
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
