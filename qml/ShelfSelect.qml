import QtQuick
import QtQuick.Controls
import "."

ComboBox {
    id: control
    implicitHeight: 36
    leftPadding: 12
    rightPadding: 27
    font.family: Theme.fontUI
    font.pixelSize: 11
    contentItem: Text {
        text: control.displayText
        color: Theme.textMuted
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Icon {
        x: control.width - width - 10
        y: (control.height - height) / 2
        width: 12; height: 12
        name: "chevron-right"
        rotation: 90
        color: Theme.textFaint
    }
    background: Rectangle {
        radius: 8
        color: control.hovered ? Theme.ink2 : Theme.ink1
        border.color: control.activeFocus ? Theme.copper : Theme.line
    }
    delegate: ItemDelegate {
        width: control.width
        height: 36
        highlighted: control.highlightedIndex === index
        contentItem: Text { text: modelData; font: control.font; color: Theme.text; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 5; color: highlighted ? Theme.copperWash : "transparent" }
    }
    popup: Popup {
        y: control.height + 5
        width: control.width
        padding: 5
        implicitHeight: contentItem.implicitHeight + 10
        background: Rectangle { radius: 9; color: Theme.ink1; border.color: Theme.lineStrong }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
        }
    }
}
