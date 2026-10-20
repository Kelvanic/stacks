import QtQuick
import QtQuick.Controls
import "."

Rectangle {
    id: pane
    color: Theme.ink1
    property string selectedShelf: "all"
    signal searchRequested()
    signal importRequested()
    signal focusToggled()
    signal kindleRequested()
    signal shelfRequested(string key)
    readonly property var books: controller.books || []
    readonly property int readingCount: books.filter(b => b.progress > 0.001 && b.progress < 0.999).length
    readonly property int finishedCount: books.filter(b => b.progress >= 0.999).length

    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.lineSoft }

    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        height: 54
        AbstractButton {
            width: parent.width - 40; height: 54
            Accessible.name: "Stacks. Back to library"
            onClicked: controller.closeBook()
            contentItem: Row {
                spacing: 10
                BrandMark { width: 42; height: 42; anchors.verticalCenter: parent.verticalCenter }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text { text: "Stacks"; color: Theme.text; font.family: Theme.fontBook; font.pixelSize: 29; font.weight: Font.Medium }
                    Text { text: "YOUR READING ROOM"; color: Theme.textFaint; font.family: Theme.fontMono; font.pixelSize: pane.width < 300 ? 8 : 9; font.letterSpacing: 1.1 }
                }
            }
        }
        TopAction {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            iconName: "panel-left"; label: "Focus mode"; shortcut: "⌘\\"
            onClicked: pane.focusToggled()
        }
    }

    AbstractButton {
        id: search
        anchors.top: header.bottom
        anchors.topMargin: 24
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 20; anchors.rightMargin: 20
        height: 42
        hoverEnabled: true
        Accessible.name: "Search books, passages and notes"
        onClicked: pane.searchRequested()
        background: Rectangle { radius: 9; color: search.hovered ? Theme.ink3 : Theme.ink2; border.color: search.activeFocus ? Theme.copper : Theme.lineSoft }
        contentItem: Item {
            Icon { x: 12; anchors.verticalCenter: parent.verticalCenter; name: "search"; width: 16; height: 16; color: Theme.textFaint }
            Text { x: 38; anchors.verticalCenter: parent.verticalCenter; text: "Search anything"; color: Theme.textMuted; font.family: Theme.fontUI; font.pixelSize: 12 }
            KeyCap { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; label: "⌘K" }
        }
    }

    Column {
        id: navigation
        anchors.top: search.bottom
        anchors.topMargin: 20
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 20; anchors.rightMargin: 20
        spacing: 4
        Text { leftPadding: 12; bottomPadding: 8; text: "WORKSPACE"; color: Theme.textGhost; font.family: Theme.fontMono; font.pixelSize: 9; font.letterSpacing: 1.5 }
        Repeater {
            model: [
                { key: "all", label: "All books", icon: "library", count: pane.books.length },
                { key: "reading", label: "In progress", icon: "book", count: pane.readingCount },
                { key: "finished", label: "Finished", icon: "check", count: pane.finishedCount }
            ]
            NavigationItem {
                width: navigation.width
                objectName: "nav-" + modelData.key
                label: modelData.label; iconName: modelData.icon; count: modelData.count
                selected: !controller.readerOpen && pane.selectedShelf === modelData.key
                onClicked: pane.shelfRequested(modelData.key)
            }
        }
        Item { width: 1; height: 10 }
        Rectangle { width: parent.width; height: 1; color: Theme.lineSoft }
        Item { width: 1; height: 8 }
        NavigationItem { width: navigation.width; label: "Send to Kindle"; iconName: "send"; onClicked: pane.kindleRequested() }
    }

    Item {
        id: shelfLabel
        anchors.top: navigation.bottom
        anchors.topMargin: 20
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 32; anchors.rightMargin: 32
        height: 20
        Text { text: "YOUR BOOKS"; color: Theme.textGhost; font.family: Theme.fontMono; font.pixelSize: 9; font.letterSpacing: 1.5 }
        TopAction { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 24; height: 24; iconName: "plus"; label: "Add a book"; onClicked: pane.importRequested() }
    }

    ListView {
        anchors.top: shelfLabel.bottom; anchors.topMargin: 8
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 10; anchors.rightMargin: 10
        anchors.bottom: localCard.top; anchors.bottomMargin: 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: pane.books
        spacing: 0
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        delegate: BookRow {
            width: ListView.view.width
            height: 62
            book: modelData
            selected: controller.readerOpen && controller.currentBook.id === modelData.id
            onActivated: controller.openBook(modelData.id)
        }
        Text {
            visible: pane.books.length === 0
            x: 22; y: 12
            width: parent.width - 44
            text: "A good idea starts with a book."
            wrapMode: Text.WordWrap
            color: Theme.textFaint; font.family: Theme.fontUI; font.pixelSize: 12
        }
    }

    Rectangle {
        id: localCard
        visible: pane.height >= 820
        anchors.bottom: appearance.top; anchors.bottomMargin: visible ? 22 : 4
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 20; anchors.rightMargin: 20
        height: visible ? 78 : 0; radius: 10
        color: Theme.ink2
        Icon { x: 14; y: 16; name: "shield"; color: Theme.moss; width: 17; height: 17 }
        Column {
            x: 42; y: 15; width: parent.width - 54; spacing: 5
            Text { text: "A library that's yours."; color: Theme.text; font.family: Theme.fontUI; font.pixelSize: 12; font.weight: Font.Medium }
            Text { width: parent.width; text: "Stored on your device.\nReady when you are."; color: Theme.textFaint; font.family: Theme.fontUI; font.pixelSize: 11; lineHeight: 1.25 }
        }
    }
    ThemePicker {
        id: appearance
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 20
    }
}
