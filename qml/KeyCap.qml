import QtQuick
import "."

// A keycap, drawn the way a designer would draw one: a soft plate with a
// hairline, the label in the mono face.
Item {
    id: cap

    property string label: ""
    property bool filled: false

    implicitWidth: Math.max(22, label.length * 7 + 14)
    implicitHeight: 20

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: cap.filled ? Theme.ink3 : Theme.ink2
        border.width: 1
        border.color: Theme.lineStrong
    }

    Text {
        anchors.centerIn: parent
        text: cap.label
        color: Theme.textMuted
        font.family: Theme.fontMono
        font.pixelSize: 9
        font.letterSpacing: 0.3
    }
}
