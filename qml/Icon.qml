import QtQuick

// A small, hand-drawn icon set. Everything is stroked on a canvas so the
// geometry stays identical on every platform and never falls back to a
// glyph from a font that may not ship the character.
Item {
    id: icon

    property string name: ""
    property color color: "#F0E9DB"
    property real weight: 1.6

    implicitWidth: 18
    implicitHeight: 18

    onNameChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()
    onWeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            if (typeof ctx.reset === "function")
                ctx.reset()
            else
                ctx.clearRect(0, 0, width, height)
            const s = Math.min(width, height)
            ctx.strokeStyle = icon.color
            ctx.fillStyle = icon.color
            ctx.lineWidth = icon.weight * (s / 18)
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            const P = (x, y) => [x * s, y * s]

            switch (icon.name) {
            case "search": {
                ctx.beginPath()
                ctx.arc(s * 0.43, s * 0.43, s * 0.27, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.63, 0.63))
                ctx.lineTo(...P(0.86, 0.86))
                ctx.stroke()
                break
            }
            case "highlight": {
                // A chisel-tip marker leaning over a rule.
                ctx.beginPath()
                ctx.moveTo(...P(0.30, 0.74))
                ctx.lineTo(...P(0.62, 0.40))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.66, 0.34))
                ctx.lineTo(...P(0.78, 0.46))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.30, 0.74))
                ctx.lineTo(...P(0.24, 0.80))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.20, 0.86))
                ctx.lineTo(...P(0.62, 0.86))
                ctx.stroke()
                break
            }
            case "bookmark": {
                ctx.beginPath()
                ctx.moveTo(...P(0.34, 0.16))
                ctx.lineTo(...P(0.66, 0.16))
                ctx.lineTo(...P(0.66, 0.86))
                ctx.lineTo(...P(0.50, 0.68))
                ctx.lineTo(...P(0.34, 0.86))
                ctx.closePath()
                ctx.stroke()
                break
            }
            case "note": {
                ctx.beginPath()
                ctx.moveTo(...P(0.30, 0.70))
                ctx.lineTo(...P(0.66, 0.34))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.62, 0.28))
                ctx.lineTo(...P(0.74, 0.40))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.30, 0.70))
                ctx.lineTo(...P(0.24, 0.78))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.24, 0.84))
                ctx.lineTo(...P(0.74, 0.84))
                ctx.stroke()
                break
            }
            case "mic": {
                ctx.beginPath()
                ctx.moveTo(...P(0.50, 0.14))
                ctx.bezierCurveTo(...P(0.62, 0.14), 0.62 * s, 0.26 * s, 0.62 * s, 0.26 * s)
                ctx.lineTo(...P(0.62, 0.54))
                ctx.bezierCurveTo(...P(0.62, 0.66), 0.62 * s, 0.70 * s, 0.50 * s, 0.70 * s)
                ctx.bezierCurveTo(...P(0.38, 0.70), 0.38 * s, 0.66 * s, 0.38 * s, 0.54 * s)
                ctx.lineTo(...P(0.38, 0.26))
                ctx.bezierCurveTo(...P(0.38, 0.26), 0.38 * s, 0.14 * s, 0.50 * s, 0.14 * s)
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.28, 0.60))
                ctx.quadraticCurveTo(...P(0.50, 0.86), 0.50 * s, 0.86 * s)
                ctx.quadraticCurveTo(...P(0.72, 0.86), 0.72 * s, 0.60 * s)
                ctx.stroke()
                break
            }
            case "diagram": {
                ctx.beginPath()
                ctx.moveTo(...P(0.32, 0.26))
                ctx.lineTo(...P(0.68, 0.44))
                ctx.lineTo(...P(0.40, 0.76))
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(s * 0.28, s * 0.22, s * 0.09, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(s * 0.72, s * 0.42, s * 0.09, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(s * 0.38, s * 0.80, s * 0.09, 0, Math.PI * 2)
                ctx.stroke()
                break
            }
            case "plus": {
                ctx.beginPath()
                ctx.moveTo(...P(0.5, 0.24))
                ctx.lineTo(...P(0.5, 0.76))
                ctx.moveTo(...P(0.24, 0.5))
                ctx.lineTo(...P(0.76, 0.5))
                ctx.stroke()
                break
            }
            case "close": {
                ctx.beginPath()
                ctx.moveTo(...P(0.28, 0.28))
                ctx.lineTo(...P(0.72, 0.72))
                ctx.moveTo(...P(0.72, 0.28))
                ctx.lineTo(...P(0.28, 0.72))
                ctx.stroke()
                break
            }
            case "chevron-left": {
                ctx.beginPath()
                ctx.moveTo(...P(0.60, 0.24))
                ctx.lineTo(...P(0.36, 0.5))
                ctx.lineTo(...P(0.60, 0.76))
                ctx.stroke()
                break
            }
            case "chevron-right": {
                ctx.beginPath()
                ctx.moveTo(...P(0.40, 0.24))
                ctx.lineTo(...P(0.64, 0.5))
                ctx.lineTo(...P(0.40, 0.76))
                ctx.stroke()
                break
            }
            case "arrow-right": {
                ctx.beginPath()
                ctx.moveTo(...P(0.22, 0.5))
                ctx.lineTo(...P(0.76, 0.5))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.60, 0.34))
                ctx.lineTo(...P(0.76, 0.5))
                ctx.lineTo(...P(0.60, 0.66))
                ctx.stroke()
                break
            }
            case "trash": {
                ctx.beginPath()
                ctx.moveTo(...P(0.24, 0.32))
                ctx.lineTo(...P(0.76, 0.32))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.40, 0.22))
                ctx.lineTo(...P(0.60, 0.22))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.30, 0.32))
                ctx.lineTo(...P(0.34, 0.80))
                ctx.lineTo(...P(0.66, 0.80))
                ctx.lineTo(...P(0.70, 0.32))
                ctx.stroke()
                break
            }
            case "clock": {
                ctx.beginPath()
                ctx.arc(s * 0.5, s * 0.5, s * 0.32, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.5, 0.32))
                ctx.lineTo(...P(0.5, 0.52))
                ctx.lineTo(...P(0.66, 0.60))
                ctx.stroke()
                break
            }
            case "spark": {
                ctx.beginPath()
                ctx.moveTo(...P(0.5, 0.12))
                ctx.quadraticCurveTo(...P(0.54, 0.46), 0.88 * s, 0.50 * s)
                ctx.quadraticCurveTo(...P(0.54, 0.54), 0.50 * s, 0.88 * s)
                ctx.quadraticCurveTo(...P(0.46, 0.54), 0.12 * s, 0.50 * s)
                ctx.quadraticCurveTo(...P(0.46, 0.46), 0.50 * s, 0.12 * s)
                ctx.stroke()
                break
            }
            case "quote": {
                ctx.beginPath()
                ctx.moveTo(...P(0.30, 0.34))
                ctx.quadraticCurveTo(...P(0.20, 0.52), 0.30 * s, 0.68 * s)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.58, 0.34))
                ctx.quadraticCurveTo(...P(0.48, 0.52), 0.58 * s, 0.68 * s)
                ctx.stroke()
                break
            }
            case "panel-left": {
                ctx.beginPath()
                ctx.moveTo(...P(0.16, 0.26))
                ctx.lineTo(...P(0.84, 0.26))
                ctx.lineTo(...P(0.84, 0.74))
                ctx.lineTo(...P(0.16, 0.74))
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.40, 0.26))
                ctx.lineTo(...P(0.40, 0.74))
                ctx.stroke()
                break
            }
            case "panel-right": {
                ctx.beginPath()
                ctx.moveTo(...P(0.16, 0.26))
                ctx.lineTo(...P(0.84, 0.26))
                ctx.lineTo(...P(0.84, 0.74))
                ctx.lineTo(...P(0.16, 0.74))
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.60, 0.26))
                ctx.lineTo(...P(0.60, 0.74))
                ctx.stroke()
                break
            }
            case "grid": {
                for (let i = 0; i < 4; ++i) {
                    const gx = 0.22 + (i % 2) * 0.32
                    const gy = 0.22 + Math.floor(i / 2) * 0.32
                    ctx.beginPath()
                    ctx.rect(gx * s, gy * s, s * 0.26, s * 0.26)
                    ctx.stroke()
                }
                break
            }
            case "list": {
                for (let i = 0; i < 3; ++i) {
                    const gy = 0.30 + i * 0.20
                    ctx.beginPath()
                    ctx.arc(s * 0.24, s * gy, s * 0.055, 0, Math.PI * 2)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(...P(0.40, gy))
                    ctx.lineTo(...P(0.80, gy))
                    ctx.stroke()
                }
                break
            }
            case "send": {
                // An arrow lifting out of a tray: sending a file away.
                ctx.beginPath()
                ctx.moveTo(...P(0.50, 0.74))
                ctx.lineTo(...P(0.50, 0.24))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.36, 0.38))
                ctx.lineTo(...P(0.50, 0.24))
                ctx.lineTo(...P(0.64, 0.38))
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.24, 0.60))
                ctx.lineTo(...P(0.24, 0.82))
                ctx.lineTo(...P(0.76, 0.82))
                ctx.lineTo(...P(0.76, 0.60))
                ctx.stroke()
                break
            }
            case "book": {
                ctx.beginPath()
                ctx.moveTo(...P(0.24, 0.22))
                ctx.quadraticCurveTo(...P(0.50, 0.16), 0.50 * s, 0.26 * s)
                ctx.lineTo(...P(0.50, 0.78))
                ctx.quadraticCurveTo(...P(0.50, 0.84), 0.24 * s, 0.80 * s)
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.76, 0.22))
                ctx.quadraticCurveTo(...P(0.50, 0.16), 0.50 * s, 0.26 * s)
                ctx.lineTo(...P(0.50, 0.78))
                ctx.quadraticCurveTo(...P(0.50, 0.84), 0.76 * s, 0.80 * s)
                ctx.closePath()
                ctx.stroke()
                break
            }
            case "layers": {
                ctx.beginPath()
                ctx.moveTo(...P(0.50, 0.16))
                ctx.lineTo(...P(0.86, 0.34))
                ctx.lineTo(...P(0.50, 0.52))
                ctx.lineTo(...P(0.14, 0.34))
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(...P(0.18, 0.52))
                ctx.lineTo(...P(0.50, 0.68))
                ctx.lineTo(...P(0.82, 0.52))
                ctx.stroke()
                break
            }
            default:
                break
            }
        }
    }
}
