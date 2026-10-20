import QtQuick
import QtQuick.Controls
import "."

Item {
    id: desk
    signal searchRequested()
    signal importRequested()
    signal kindleRequested()
    property string shelfFilter: "all"
    property string formatFilter: "all"
    property string sortOrder: "recent"
    property string query: ""
    property bool listMode: false
    readonly property var books: controller.books || []
    readonly property int readingCount: books.filter(b => b.progress > 0.001 && b.progress < 0.999).length
    readonly property int finishedCount: books.filter(b => b.progress >= 0.999).length
    readonly property var filteredBooks: {
        const term = query.trim().toLowerCase()
        const result = books.filter(b =>
            (shelfFilter === "all" || (shelfFilter === "reading" ? b.progress > 0.001 && b.progress < 0.999 : b.progress >= 0.999)) &&
            (formatFilter === "all" || b.format === formatFilter) &&
            (!term || ((b.title || "") + " " + (b.author || "")).toLowerCase().includes(term)))
        return result.sort((a, b) => {
            if (sortOrder === "title") return (a.title || "").localeCompare(b.title || "") || a.id - b.id
            if (sortOrder === "author") return (a.author || "").localeCompare(b.author || "") || a.id - b.id
            const stampA = a.lastOpenedAt || a.addedAt || ""
            const stampB = b.lastOpenedAt || b.addedAt || ""
            return stampB.localeCompare(stampA) || a.id - b.id
        })
    }
    readonly property var resumeBook: {
        for (let i = 0; i < books.length; ++i)
            if (books[i].progress > 0.001 && books[i].progress < 0.999)
                return books[i]
        return books.length ? books[0] : null
    }
    readonly property string shelfTitle: shelfFilter === "reading" ? "In progress" : shelfFilter === "finished" ? "Finished books" : "Your library"

    Rectangle {
        id: topBar
        width: parent.width; height: 76
        color: Theme.ink1
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.lineSoft }
        Row {
            x: 36; anchors.verticalCenter: parent.verticalCenter; spacing: 10
            Icon { name: "library"; width: 16; height: 16; color: Theme.textFaint; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "Workspace"; color: Theme.textFaint; font.family: Theme.fontUI; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "/"; color: Theme.lineStrong; font.pixelSize: 15; anchors.verticalCenter: parent.verticalCenter }
            Text { text: desk.shelfTitle; color: Theme.text; font.family: Theme.fontUI; font.pixelSize: 12; font.weight: Font.Medium; anchors.verticalCenter: parent.verticalCenter }
        }
        Row {
            anchors.right: parent.right; anchors.rightMargin: 36; anchors.verticalCenter: parent.verticalCenter; spacing: 16
            Row {
                visible: desk.width > 860
                anchors.verticalCenter: parent.verticalCenter; spacing: 6
                Rectangle { width: 5; height: 5; radius: 3; color: Theme.moss; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "Local library"; color: Theme.textFaint; font.family: Theme.fontUI; font.pixelSize: 11 }
            }
            TopAction { iconName: "search"; label: "Search every page"; shortcut: "⌘K"; onClicked: desk.searchRequested() }
            PillButton { label: "Add a book"; iconName: "plus"; primary: true; onClicked: desk.importRequested() }
        }
    }

    Column {
        id: contentHeader
        x: shelf.leftMargin
        y: topBar.height + 28
        width: shelf.columnWidth
        spacing: 20
        bottomPadding: 20

            Item {
                width: parent.width; height: 58
                Column {
                    spacing: 7
                    Text { text: desk.shelfTitle; color: Theme.text; font.family: Theme.fontBook; font.pixelSize: 36; font.weight: Font.Medium }
                    Text { text: "Good books. Collected thoughts. A little room to think."; color: Theme.textFaint; font.family: Theme.fontUI; font.pixelSize: 12 }
                }
                Row {
                    visible: shelf.columnWidth > 760
                    anchors.right: parent.right; anchors.top: parent.top; anchors.topMargin: 8; spacing: 20
                    Repeater {
                        model: [{ value: desk.books.length, label: "BOOKS", tone: Theme.text }, { value: desk.readingCount, label: "READING", tone: Theme.copper }, { value: desk.finishedCount, label: "FINISHED", tone: Theme.moss }]
                        Column {
                            spacing: 6
                            Text { anchors.right: parent.right; text: modelData.value.toString().padStart(2, "0"); color: modelData.tone; font.family: Theme.fontBook; font.pixelSize: 27 }
                            Text { text: modelData.label; color: Theme.textGhost; font.family: Theme.fontMono; font.pixelSize: 8; font.letterSpacing: 1 }
                        }
                    }
                }
            }

            Rectangle {
                id: hero
                visible: desk.height >= 820
                objectName: "desk-hero"
                width: parent.width; height: 212; radius: 14
                color: Theme.featureSurface
                clip: true
                // Fine inset rules frame the feature without a texture bitmap.
                Rectangle { anchors.fill: parent; anchors.margins: 7; radius: 9; color: "transparent"; border.color: "#18F4ECDD" }
                Column {
                    x: 28; y: 22; width: parent.width * 0.49 - 28; spacing: 12
                    Text { text: "THE ART OF SLOWING DOWN"; color: "#B7C6B4"; font.family: Theme.fontMono; font.pixelSize: 8; font.letterSpacing: 1.5 }
                    Text { text: "One more page.\nA new perspective."; color: "#F7F2E7"; font.family: Theme.fontBook; font.pixelSize: shelf.columnWidth > 850 ? 34 : 30; lineHeight: 1.05; font.weight: Font.Medium }
                    Text { text: desk.resumeBook ? "Your next thought is waiting right here." : "Make space for your first good book."; width: parent.width; wrapMode: Text.WordWrap; color: "#B7C6B4"; font.family: Theme.fontUI; font.pixelSize: 11 }
                    PillButton {
                        label: desk.resumeBook ? (desk.resumeBook.progress > 0.001 ? "Continue reading" : "Start reading") : "Add your first book"
                        iconName: desk.resumeBook ? "arrow-right" : "plus"
                        primary: true
                        onClicked: desk.resumeBook ? controller.openBook(desk.resumeBook.id) : desk.importRequested()
                    }
                }
                Item {
                    visible: desk.resumeBook !== null
                    x: parent.width * 0.51; y: 22
                    width: parent.width * 0.49 - 28; height: parent.height - 44
                    Rectangle { x: 5; y: 13; width: 94; height: 148; radius: 3; color: "#50000000"; rotation: -6 }
                    Cover {
                        id: featureCover
                        x: 0; y: 6; width: 96; height: 154; rotation: -6
                        title: desk.resumeBook ? desk.resumeBook.title : ""
                        author: desk.resumeBook ? desk.resumeBook.author : ""
                        accent: desk.resumeBook ? desk.resumeBook.accent : Theme.copper
                        format: desk.resumeBook ? desk.resumeBook.format : "pdf"
                    }
                    Column {
                        x: 118; y: 10; width: parent.width - 118; spacing: 9
                        Text { text: "ON YOUR DESK"; color: "#B7C6B4"; font.family: Theme.fontMono; font.pixelSize: 8; font.letterSpacing: 1.4 }
                        Text { width: parent.width; text: desk.resumeBook ? desk.resumeBook.title : ""; color: "#F7F2E7"; font.family: Theme.fontBook; font.pixelSize: 23; font.weight: Font.Medium; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
                        Text { width: parent.width; text: desk.resumeBook ? desk.resumeBook.author : ""; color: "#B7C6B4"; font.family: Theme.fontUI; font.pixelSize: 11; elide: Text.ElideRight }
                        Item { width: 1; height: 2 }
                        Rectangle {
                            width: parent.width; height: 3; radius: 2; color: "#30F7F2E7"
                            Rectangle { width: parent.width * Math.max(0, Math.min(1, desk.resumeBook ? desk.resumeBook.progress : 0)); height: 3; radius: 2; color: "#D8B68A" }
                        }
                        Text { text: Math.round((desk.resumeBook ? desk.resumeBook.progress : 0) * 100) + "% complete"; color: "#D8C9B3"; font.family: Theme.fontUI; font.pixelSize: 10 }
                    }
                }
                BrandMark { visible: !desk.resumeBook; anchors.right: parent.right; anchors.rightMargin: 40; anchors.verticalCenter: parent.verticalCenter; width: 160; height: 160 }
            }

            Column {
                width: parent.width
                spacing: 16
                Row {
                    width: parent.width; height: 36; spacing: 5
                    Repeater {
                        model: [{ key: "all", title: "All books" }, { key: "reading", title: "In progress" }, { key: "finished", title: "Finished" }]
                        PillButton {
                            objectName: "shelf-filter-" + modelData.key
                            label: modelData.title
                            active: desk.shelfFilter === modelData.key
                            onClicked: desk.shelfFilter = modelData.key
                        }
                    }
                }
                Rectangle { width: parent.width; height: 1; color: Theme.line }
                Item {
                    width: parent.width; height: 36
                    TextField {
                        id: shelfSearch
                        objectName: "shelf-search"
                        width: Math.min(240, parent.width * 0.32); height: 36
                        leftPadding: 32; rightPadding: 30
                        placeholderText: "Filter title or author…"
                        color: Theme.text
                        font.family: Theme.fontUI; font.pixelSize: 11
                        text: desk.query
                        onTextEdited: desk.query = text
                        background: Rectangle { radius: 8; color: Theme.ink1; border.color: shelfSearch.activeFocus ? Theme.copper : Theme.line }
                        Icon { x: 10; anchors.verticalCenter: parent.verticalCenter; width: 14; height: 14; name: "search"; color: Theme.textFaint }
                        TopAction { visible: shelfSearch.text.length > 0; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 28; height: 28; iconName: "close"; label: "Clear filter"; onClicked: desk.query = "" }
                    }
                    Row {
                        anchors.right: parent.right; spacing: 10
                        ShelfSelect {
                            id: formatPicker
                            objectName: "shelf-format"
                            width: 114; height: 36
                            model: ["All formats", "PDF", "EPUB"]
                            currentIndex: desk.formatFilter === "pdf" ? 1 : desk.formatFilter === "epub" ? 2 : 0
                            font.family: Theme.fontUI; font.pixelSize: 11
                            Accessible.name: "Filter book format"
                            onActivated: desk.formatFilter = ["all", "pdf", "epub"][currentIndex]
                        }
                        ShelfSelect {
                            id: sortPicker
                            objectName: "shelf-sort"
                            width: 150; height: 36
                            model: ["Recently opened", "Title A–Z", "Author A–Z"]
                            font.family: Theme.fontUI; font.pixelSize: 11
                            Accessible.name: "Sort books"
                            currentIndex: desk.sortOrder === "title" ? 1 : desk.sortOrder === "author" ? 2 : 0
                            onActivated: desk.sortOrder = ["recent", "title", "author"][currentIndex]
                        }
                        Rectangle {
                            width: 73; height: 36; radius: 8; color: Theme.ink1; border.color: Theme.line
                            Row {
                                x: 2; y: 2; spacing: 1
                                TopAction { objectName: "shelf-grid-view"; width: 34; height: 32; iconName: "grid"; label: "Grid view"; active: !desk.listMode; onClicked: desk.listMode = false }
                                TopAction { objectName: "shelf-list-view"; width: 34; height: 32; iconName: "list"; label: "List view"; active: desk.listMode; onClicked: desk.listMode = true }
                            }
                        }
                    }
                }
            }
    }

    // Only book delegates scroll; controls remain stable while the model changes.
    GridView {
        id: shelf
        objectName: "shelf-grid"
        anchors.top: contentHeader.bottom
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        cacheBuffer: 320
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        function recentre() { contentX = -leftMargin }
        onLeftMarginChanged: recentre()
        onWidthChanged: recentre()
        Component.onCompleted: recentre()
        readonly property real gap: 18
        readonly property real columnWidth: Math.max(0, Math.min(1120, width - 72))
        readonly property int columns: desk.listMode ? 1 : columnWidth >= 920 ? 4 : columnWidth >= 650 ? 3 : 2
        leftMargin: Math.max(0, (width - columnWidth) / 2)
        rightMargin: Math.max(0, leftMargin - gap - 1)
        topMargin: 0
        bottomMargin: 28
        cellWidth: (columnWidth + gap) / columns
        cellHeight: (desk.listMode ? 88 : 330) + gap
        model: desk.filteredBooks


        delegate: Item {
            width: shelf.cellWidth; height: shelf.cellHeight
            BookTile {
                objectName: "shelf-book-" + modelData.id
                width: parent.width - shelf.gap
                height: desk.listMode ? 88 : 330
                compact: desk.listMode
                book: modelData
            }
        }

        footer: Column {
            width: shelf.columnWidth; spacing: 20; topPadding: 4
            Rectangle {
                visible: desk.filteredBooks.length === 0
                width: parent.width; height: 180; radius: 12
                color: Theme.ink1; border.color: Theme.lineSoft
                Column {
                    anchors.centerIn: parent; width: parent.width - 40; spacing: 12
                    Icon { name: "book"; width: 28; height: 28; color: Theme.copper; anchors.horizontalCenter: parent.horizontalCenter }
                    Text { width: parent.width; text: desk.query || desk.formatFilter !== "all" ? "No books match these filters." : desk.shelfFilter === "finished" ? "Every finished book starts with a page." : desk.shelfFilter === "reading" ? "Your next chapter is waiting." : "A home for your next good book."; color: Theme.text; font.family: Theme.fontBook; font.pixelSize: 23; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
                    PillButton {
                        objectName: "shelf-reset-filters"
                        anchors.horizontalCenter: parent.horizontalCenter
                        label: desk.books.length ? "Show all books" : "Add a book"; iconName: desk.books.length ? "library" : "plus"
                        onClicked: {
                            if (!desk.books.length) desk.importRequested()
                            else { desk.query = ""; desk.formatFilter = "all"; desk.shelfFilter = "all" }
                        }
                    }
                }
            }
            Item {
                width: parent.width; height: 26
                Text { anchors.verticalCenter: parent.verticalCenter; text: desk.filteredBooks.length + " of " + desk.books.length + " books"; color: Theme.textFaint; font.family: Theme.fontUI; font.pixelSize: 10 }
                Row {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; spacing: 7
                    Icon { name: "shield"; width: 13; height: 13; color: Theme.moss }
                    Text { text: "Your books stay with you."; color: Theme.textFaint; font.family: Theme.fontUI; font.pixelSize: 10 }
                }
            }
        }
    }
}
