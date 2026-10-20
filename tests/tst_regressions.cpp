#include "core/AppController.h"
#include "core/Database.h"
#include "core/EpubDocument.h"
#include "core/KindleSender.h"
#include "core/Library.h"
#include "core/PdfDocument.h"
#include "core/VoiceRecorder.h"
#include "native/ReaderItem.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QGuiApplication>
#include <QJSValue>
#include <QPainter>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <QSet>
#include <QScopeGuard>
#include <QSettings>
#include <QSqlQuery>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QtTest>
#include <future>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <zip.h>

using namespace stacks;

namespace {
const QString samples = QStringLiteral(STACKS_SAMPLE_DIR);
QQuickItem *findVisualItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (QQuickItem *child : root->childItems()) {
        if (auto *found = findVisualItem(child, name)) return found;
    }
    return nullptr;
}
QByteArray zipEntry(zip_t *archive, const QString &name)
{
    zip_stat_t stat {};
    if (zip_stat(archive, name.toUtf8().constData(), 0, &stat) != 0)
        return {};
    zip_file_t *file = zip_fopen(archive, name.toUtf8().constData(), 0);
    if (!file) return {};
    QByteArray bytes(qsizetype(stat.size), '\0');
    const auto count = zip_fread(file, bytes.data(), stat.size);
    zip_fclose(file);
    return count == zip_int64_t(stat.size) ? bytes : QByteArray();
}
}

class TestRegressions : public QObject
{
    Q_OBJECT
private slots:
    void positionsSurviveSwitchAndShutdown();
    void conversionCanOutliveEventLoop();
    void annotationSearchTracksEditsAndMigration();
    void epubRestoresSavedPosition();
    void pdfRectanglesUseTopLeftCoordinates();
    void pdfAccessIsSerialized();
    void normalizedEpubPreservesNavigation();
    void legacyEpubGetsNavigationInItsOwnDirectory();
    void marginDeleteReceivesClicks();
    void kindleSelectionInvalidatesOldResult();
    void appearancePreferencePersists();
    void appearanceFollowsSystem();
    void themePickerUpdatesLivePalette();
    void primaryButtonUsesThemedForeground();
    void readerThemeChangesPreservePosition();
    void shelfFiltersAndKeyboardActivation();
    void shelfCreatesOnlyVisibleTiles();
    void shelfSearchSortAndListView();
    void libraryNavigationIsKeyboardAccessible();
    void workspaceNavigationAndResponsiveRails();
    void readerToolbarFitsNarrowWindow();
    void voiceNoteDeletionRemovesRecording();
    void diagramEdgesStayScopedAndUnique();
    void diagramEdgesFollowDraggedNodes();
    void kindleFileNamesStayVisible();
    void htmlConvertsToProse();
    void kindleRejectsOversizedAttachments();
    void kindlePasswordStaysOutOfQml();
    void searchSnippetsKeepMarkupBalanced();
    void pdfRenderingIsBounded();
    void pdfTrackpadSwipeTurnsOnePage();
    void epubProgressReachesTheEnd();
    void missingBookIsNotOpened();
    void overlayPanelsKeepTheirClicks();
    void roundedCornersStayRounded();
};

void TestRegressions::positionsSurviveSwitchAndShutdown()
{
    QTemporaryDir dir;
    int first = 0, second = 0;
    {
        AppController controller;
        QVERIFY(controller.initialize(samples, dir.path()));
        first = controller.books().at(0).toMap().value("id").toInt();
        second = controller.books().at(1).toMap().value("id").toInt();
        controller.openBook(first);
        controller.savePosition(QStringLiteral("epub:420"), 0.37);
        controller.openBook(first); // Even reopening the same book must flush first.
        QCOMPARE(controller.currentBook().value("locator").toString(), QStringLiteral("epub:420"));
        controller.openBook(second);
        controller.closeBook(); // No stale first-book position may be applied here.
        controller.openBook(second);
        QVERIFY(controller.currentBook().value("locator").toString() != QStringLiteral("epub:420"));
        controller.savePosition(QStringLiteral("pdf:3"), 0.6);
    }
    AppController reopened;
    QVERIFY(reopened.initialize(samples, dir.path()));
    reopened.openBook(first);
    QCOMPARE(reopened.currentBook().value("locator").toString(), QStringLiteral("epub:420"));
    reopened.openBook(second);
    QCOMPARE(reopened.currentBook().value("locator").toString(), QStringLiteral("pdf:3"));
    QVERIFY(!AppController().startVoiceNote()); // No recording without an open book.
}

void TestRegressions::conversionCanOutliveEventLoop()
{
    QTemporaryDir dir;
    const QString input = dir.filePath(QStringLiteral("source.txt"));
    QFile file(input);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(200000, 'a'));
    file.close();
    {
        AppController controller;
        QVERIFY(controller.initialize(samples, dir.filePath(QStringLiteral("data"))));
        controller.convertFileForKindle(input);
        QVERIFY(controller.kindleBusy());
        // Destruction must join the worker without needing queued completion callbacks.
    }
    const QDir output(dir.filePath(QStringLiteral("data/kindle")));
    QCOMPARE(output.entryList({QStringLiteral("*.epub")}, QDir::Files).size(), 1);
    EpubDocument document;
    QVERIFY(document.load(output.filePath(output.entryList({QStringLiteral("*.epub")}).first())));
}

void TestRegressions::annotationSearchTracksEditsAndMigration()
{
    QTemporaryDir dir;
    Database database;
    QVERIFY(database.open(dir.filePath(QStringLiteral("library.db"))));
    Library library(&database);
    const int book = library.importBook(QDir(samples).filePath(QStringLiteral("systems-that-breathe.epub")));
    const int id = library.addAnnotation(book, QStringLiteral("note"), QStringLiteral("epub:20-40"),
                                         QStringLiteral("originalquoteuniqueword"), QStringLiteral("uniquenoteword"));
    QVERIFY(id > 0);
    QCOMPARE(library.search(QStringLiteral("uniquenoteword")).size(), 1);
    QCOMPARE(library.search(QStringLiteral("uniquenoteword")).first().annotationId, id);
    QVERIFY(library.updateAnnotation(id, QStringLiteral("replacementword"), QString()));
    QVERIFY(library.search(QStringLiteral("uniquenoteword")).isEmpty());
    QCOMPARE(library.search(QStringLiteral("originalquoteuniqueword")).size(), 1);
    const int voice = library.addVoiceNote(book, id, QStringLiteral("test.wav"), 1000, QStringLiteral("uniquetranscriptword"));
    QVERIFY(voice > 0);
    QCOMPARE(library.search(QStringLiteral("uniquetranscriptword")).size(), 1);
    QCOMPARE(library.search(QStringLiteral("uniquetranscriptword")).first().annotationId, id);
    QSqlQuery query(database.db());
    QVERIFY(query.exec(QStringLiteral("UPDATE voice_notes SET transcript = 'editedtranscriptword'")));
    QVERIFY(library.search(QStringLiteral("uniquetranscriptword")).isEmpty());
    QCOMPARE(library.search(QStringLiteral("editedtranscriptword")).size(), 1);
    // Simulate an existing v1 library: migration must backfill its annotations.
    for (const QString &trigger : {QStringLiteral("annotations_ai"), QStringLiteral("annotations_au"),
                                  QStringLiteral("annotations_ad"), QStringLiteral("voice_notes_search_au"),
                                  QStringLiteral("voice_notes_search_ad")})
        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER ") + trigger));
    QVERIFY(query.exec(QStringLiteral("DROP TABLE annotations_fts")));
    query = QSqlQuery();
    database.close();
    QVERIFY(database.open(dir.filePath(QStringLiteral("library.db"))));
    QCOMPARE(library.search(QStringLiteral("replacementword")).size(), 1);
    QCOMPARE(library.search(QStringLiteral("editedtranscriptword")).size(), 1);
    QVERIFY(library.deleteAnnotation(id));
    QVERIFY(library.search(QStringLiteral("replacementword")).isEmpty());
}

void TestRegressions::epubRestoresSavedPosition()
{
    const QString path = QDir(samples).filePath(QStringLiteral("systems-that-breathe.epub"));
    ReaderItem first;
    first.setSize(QSizeF(720, 600));
    QVERIFY(first.load(path, QStringLiteral("epub")));
    for (int i = 0; i < 3; ++i) first.nextPage();
    QVERIFY(first.progress() > 0.1);
    ReaderItem restored;
    restored.setSize(QSizeF(720, 600));
    QVERIFY(restored.load(path, QStringLiteral("epub"), first.locator()));
    QVERIFY(restored.scrollY() > 0);
    QVERIFY(qAbs(restored.progress() - first.progress()) < 0.05);
    // Animated rail/focus changes must not move back a paragraph on every frame.
    const double progress = restored.progress();
    for (int width = 720; width >= 600; width -= 8) restored.setWidth(width);
    restored.nextPage();
    restored.previousPage();
    QVERIFY2(qAbs(restored.progress() - progress) < 0.05,
             qPrintable(QStringLiteral("Before reflow: %1; after reflow: %2").arg(progress).arg(restored.progress())));
    QVERIFY(restored.load(path, QStringLiteral("epub"), QStringLiteral("epub:2:1")));
    QVERIFY(restored.scrollY() > 0);
}

void TestRegressions::pdfRectanglesUseTopLeftCoordinates()
{
    PdfDocument pdf;
    QVERIFY(pdf.load(QDir(samples).filePath(QStringLiteral("the-art-of-attention.pdf"))));
    const QSizeF size = pdf.pageSize(0);
    const QString top = pdf.textInRect(0, QRectF(0, 0, size.width(), size.height() * 0.65));
    QVERIFY2(top.contains(QStringLiteral("Attention"), Qt::CaseInsensitive), qPrintable(top));
}

void TestRegressions::pdfAccessIsSerialized()
{
    const auto exercise = []() {
        for (int i = 0; i < 6; ++i) {
            PdfDocument pdf;
            if (!pdf.load(QDir(samples).filePath(QStringLiteral("the-art-of-attention.pdf")))
                || pdf.pageText(1).isEmpty() || pdf.renderPage(1, 0.3).isNull()) return false;
        }
        return true;
    };
    auto worker = std::async(std::launch::async, exercise);
    QVERIFY(exercise());
    QVERIFY(worker.get());
}

void TestRegressions::normalizedEpubPreservesNavigation()
{
    QTemporaryDir dir;
    const QString path = QDir(samples).filePath(QStringLiteral("systems-that-breathe.epub"));
    auto result = KindleConverter::convert(path, QStringLiteral("epub"), QStringLiteral("A & B"), QString(), dir.path());
    QVERIFY2(result.ok, qPrintable(result.error));
    int error = 0;
    zip_t *source = zip_open(path.toUtf8().constData(), ZIP_RDONLY, &error);
    zip_t *output = zip_open(result.path.toUtf8().constData(), ZIP_RDONLY, &error);
    QVERIFY(source && output);
    QCOMPARE(zipEntry(output, QStringLiteral("OEBPS/nav.xhtml")), zipEntry(source, QStringLiteral("OEBPS/nav.xhtml")));
    QDomDocument package;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QVERIFY(package.setContent(zipEntry(output, QStringLiteral("OEBPS/content.opf")), QDomDocument::ParseOption::UseNamespaceProcessing));
#else
    QVERIFY(package.setContent(zipEntry(output, QStringLiteral("OEBPS/content.opf")), true));
#endif
    QSet<QString> ids;
    const auto items = package.elementsByTagNameNS(QStringLiteral("http://www.idpf.org/2007/opf"), QStringLiteral("item"));
    for (int i = 0; i < items.size(); ++i) {
        const QString id = items.at(i).toElement().attribute(QStringLiteral("id"));
        QVERIFY(!ids.contains(id));
        ids.insert(id);
    }
    zip_close(output);
    zip_close(source);
    // Re-conversion must not overwrite a source file already in the output directory.
    const auto second = KindleConverter::convert(result.path, QStringLiteral("epub"), QStringLiteral("A & B"), QString(), dir.path());
    QVERIFY(second.ok);
    QVERIFY(second.path != result.path);
}

void TestRegressions::legacyEpubGetsNavigationInItsOwnDirectory()
{
    QTemporaryDir dir;
    const QString input = dir.filePath(QStringLiteral("legacy.epub"));
    int error = 0;
    zip_t *archive = zip_open(input.toUtf8().constData(), ZIP_CREATE, &error);
    QVERIFY(archive);
    const auto add = [archive](const char *name, const QByteArray &bytes) {
        void *copy = std::malloc(size_t(bytes.size()));
        if (!copy) return false;
        std::memcpy(copy, bytes.constData(), size_t(bytes.size()));
        zip_source_t *source = zip_source_buffer(archive, copy, bytes.size(), 1);
        return source && zip_file_add(archive, name, source, 0) >= 0;
    };
    QVERIFY(add("mimetype", "application/epub+zip"));
    QVERIFY(add("META-INF/container.xml", "<container><rootfiles><rootfile full-path=\"OPS/book.opf\"/></rootfiles></container>"));
    QVERIFY(add("OPS/book.opf", "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"2.0\" unique-identifier=\"uid\">"
        "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:identifier id=\"uid\">test</dc:identifier>"
        "<dc:title>Legacy</dc:title><dc:language>en</dc:language></metadata>"
        "<manifest><item id=\"chapter\" href=\"chapter.xhtml\" media-type=\"application/xhtml+xml\"/></manifest>"
        "<spine><itemref idref=\"chapter\" linear=\"yes\"/></spine></package>"));
    QVERIFY(add("OPS/chapter.xhtml", "<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Chapter</title></head><body><p>Legacy text.</p></body></html>"));
    QCOMPARE(zip_close(archive), 0);
    const auto result = KindleConverter::convert(input, QStringLiteral("epub"), QStringLiteral("Legacy"), QString(), dir.filePath(QStringLiteral("out")));
    QVERIFY2(result.ok, qPrintable(result.error));
    archive = zip_open(result.path.toUtf8().constData(), ZIP_RDONLY, &error);
    QVERIFY(archive);
    const QByteArray nav = zipEntry(archive, QStringLiteral("OPS/stacks-nav.xhtml"));
    QVERIFY(nav.contains("href=\"chapter.xhtml\""));
    QVERIFY(zip_name_locate(archive, "OEBPS/nav.xhtml", 0) < 0);
    zip_close(archive);
    EpubDocument document;
    QVERIFY(document.load(result.path));
}

void TestRegressions::marginDeleteReceivesClicks()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    controller.openBook(controller.books().first().toMap().value("id").toInt());
    controller.addNote(QStringLiteral("A test note"), QStringLiteral("A quote"), QStringLiteral("epub:10"), 0);
    const int id = controller.annotations().first().toMap().value("id").toInt();
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(360, 720);
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/MarginPane.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QQuickItem *button = nullptr;
    QTRY_VERIFY((button = findVisualItem(view.rootObject(), QStringLiteral("annotationDelete-%1").arg(id))) != nullptr);
    const QPoint point = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, point);
    QTRY_VERIFY(controller.annotations().isEmpty());
}

void TestRegressions::kindleSelectionInvalidatesOldResult()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    const QVariantMap first = controller.books().first().toMap();
    controller.convertBookForKindle(first.value("id").toInt());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.kindleBusy(), 15000);
    QVERIFY(!controller.kindleResults().isEmpty());
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/KindleSheet.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    const QVariantMap second = controller.books().at(1).toMap();
    QVERIFY(QMetaObject::invokeMethod(view.rootObject(), "pickBook", Q_ARG(QVariant, QVariant(second))));
    QVERIFY(controller.kindleResults().isEmpty());
}

void TestRegressions::appearancePreferencePersists()
{
    QTemporaryDir dir;
    {
        AppController controller;
        QVERIFY(controller.initialize(samples, dir.path()));
        QCOMPARE(controller.themeMode(), QStringLiteral("system"));
        controller.setThemeMode(QStringLiteral("dark"));
        QVERIFY(controller.darkTheme());
        controller.setThemeMode(QStringLiteral("invalid"));
        QCOMPARE(controller.themeMode(), QStringLiteral("dark"));
    }
    {
        AppController controller;
        QVERIFY(controller.initialize(samples, dir.path()));
        QCOMPARE(controller.themeMode(), QStringLiteral("dark"));
        QVERIFY(controller.darkTheme());
        controller.setThemeMode(QStringLiteral("light"));
        QVERIFY(!controller.darkTheme());
    }
    {
        AppController controller;
        QVERIFY(controller.initialize(samples, dir.path()));
        QCOMPARE(controller.themeMode(), QStringLiteral("light"));
        controller.setThemeMode(QStringLiteral("system"));
    }
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    QCOMPARE(controller.themeMode(), QStringLiteral("system"));
}

void TestRegressions::appearanceFollowsSystem()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QStyleHints *hints = QGuiApplication::styleHints();
    const auto original = hints->colorScheme();
    const auto reset = qScopeGuard([hints, original]() { hints->colorSchemeChanged(original); });
    // Simulate OS notifications: offscreen plugins cannot request real OS changes.
    hints->colorSchemeChanged(Qt::ColorScheme::Light);
    QTRY_VERIFY(!controller.darkTheme());
    hints->colorSchemeChanged(Qt::ColorScheme::Dark);
    QTRY_VERIFY(controller.darkTheme());
    controller.setThemeMode(QStringLiteral("light"));
    QVERIFY(!controller.darkTheme());
    hints->colorSchemeChanged(Qt::ColorScheme::Light);
    controller.setThemeMode(QStringLiteral("dark"));
    QVERIFY(controller.darkTheme());
    controller.setThemeMode(QStringLiteral("system"));
    QTRY_VERIFY(!controller.darkTheme());
#else
    // Qt 6.4 uses the application palette when no native colour-scheme API is available.
    const QPalette original = QGuiApplication::palette();
    const auto reset = qScopeGuard([original]() { QGuiApplication::setPalette(original); });
    QPalette palette = original;
    palette.setColor(QPalette::Window, Qt::black);
    QGuiApplication::setPalette(palette);
    QTRY_VERIFY(controller.darkTheme());
    palette.setColor(QPalette::Window, Qt::white);
    QGuiApplication::setPalette(palette);
    QTRY_VERIFY(!controller.darkTheme());
#endif
}

void TestRegressions::themePickerUpdatesLivePalette()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    controller.setThemeMode(QStringLiteral("light"));
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(320, 900);
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/LibraryPane.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QCOMPARE(view.rootObject()->property("color").value<QColor>(), QColor(QStringLiteral("#FCFAF6")));
    QQuickItem *darkButton = findVisualItem(view.rootObject(), QStringLiteral("theme-dark"));
    QVERIFY(darkButton);
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier,
                     darkButton->mapToScene(QPointF(darkButton->width() / 2, darkButton->height() / 2)).toPoint());
    QTRY_COMPARE(controller.themeMode(), QStringLiteral("dark"));
    QTRY_COMPARE(view.rootObject()->property("color").value<QColor>(), QColor(QStringLiteral("#12100E")));
    QVERIFY(darkButton->property("checked").toBool());
    controller.setThemeMode(QStringLiteral("light"));
    QTRY_COMPARE(view.rootObject()->property("color").value<QColor>(), QColor(QStringLiteral("#FCFAF6")));
    QVERIFY(!darkButton->property("checked").toBool());
}

void TestRegressions::primaryButtonUsesThemedForeground()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    controller.setThemeMode(QStringLiteral("light"));
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/PillButton.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.rootObject()->setProperty("primary", true);
    auto *label = findVisualItem(view.rootObject(), QStringLiteral("buttonLabel"));
    QVERIFY(label);
    QCOMPARE(label->property("color").value<QColor>(), QColor(QStringLiteral("#FFF8EF")));
    controller.setThemeMode(QStringLiteral("dark"));
    QCOMPARE(label->property("color").value<QColor>(), QColor(QStringLiteral("#1A130D")));
}

void TestRegressions::readerThemeChangesPreservePosition()
{
    ReaderItem reader;
    reader.setSize(QSizeF(720, 600));
    QVERIFY(reader.load(QDir(samples).filePath(QStringLiteral("systems-that-breathe.epub")), QStringLiteral("epub")));
    reader.nextPage();
    const QString locator = reader.locator();
    const qreal scroll = reader.scrollY();
    const double progress = reader.progress();
    QSignalSpy position(&reader, &ReaderItem::positionChanged);
    reader.setTextColor(QColor(QStringLiteral("#EDE4D5")));
    reader.setAccentColor(QColor(QStringLiteral("#C9703F")));
    reader.setMutedColor(QColor(QStringLiteral("#B0A593")));
    QCOMPARE(reader.locator(), locator);
    QCOMPARE(reader.scrollY(), scroll);
    QCOMPARE(reader.progress(), progress);
    QCOMPARE(position.size(), 0);
    const auto paintedPixels = [&reader](bool bright) {
        QImage image(720, 600, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        reader.paint(&painter);
        painter.end();
        int count = 0;
        for (int y = 0; y < image.height(); y += 2) {
            for (int x = 0; x < image.width(); x += 2) {
                const QColor color = image.pixelColor(x, y);
                if (color.alpha() > 180 && (bright ? color.lightness() > 180 : color.lightness() < 90))
                    ++count;
            }
        }
        return count;
    };
    QVERIFY(paintedPixels(true) > 200);
    reader.setTextColor(QColor(QStringLiteral("#272119")));
    QVERIFY(paintedPixels(false) > 200);
    QCOMPARE(reader.scrollY(), scroll);
    QCOMPARE(reader.locator(), locator);
    QCOMPARE(position.size(), 0);
}

void TestRegressions::shelfFiltersAndKeyboardActivation()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    const int first = controller.books().first().toMap().value("id").toInt();
    const int second = controller.books().at(1).toMap().value("id").toInt();
    controller.openBook(first);
    controller.savePosition(QStringLiteral("pdf:1"), 0.4);
    controller.openBook(second);
    controller.savePosition(QStringLiteral("epub:0:0"), 1.0);
    controller.closeBook();

    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(800, 720); // The desk at the application's minimum window width.
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/DeskView.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    auto activateFilter = [&](const QString &key) {
        auto *button = findVisualItem(view.rootObject(), QStringLiteral("shelf-filter-") + key);
        QVERIFY(button);
        button->forceActiveFocus();
        QTest::keyClick(&view, Qt::Key_Space);
        QTRY_COMPARE(view.rootObject()->property("shelfFilter").toString(), key);
    };
    activateFilter(QStringLiteral("reading"));
    QTRY_VERIFY(findVisualItem(view.rootObject(), QStringLiteral("shelf-book-%1").arg(first)));
    QVERIFY(!findVisualItem(view.rootObject(), QStringLiteral("shelf-book-%1").arg(second)));
    activateFilter(QStringLiteral("finished"));
    QTRY_VERIFY(findVisualItem(view.rootObject(), QStringLiteral("shelf-book-%1").arg(second)));
    QVERIFY(!findVisualItem(view.rootObject(), QStringLiteral("shelf-book-%1").arg(first)));
    activateFilter(QStringLiteral("all"));
    auto *tile = findVisualItem(view.rootObject(), QStringLiteral("shelf-book-%1").arg(first));
    QVERIFY(tile);
    QVERIFY(tile->mapToScene(QPointF(tile->width(), 0)).x() <= view.width());
    tile->forceActiveFocus();
    QTest::keyClick(&view, Qt::Key_Return);
    QTRY_VERIFY(controller.readerOpen());
    QCOMPARE(controller.currentBook().value("id").toInt(), first);
}

void TestRegressions::shelfCreatesOnlyVisibleTiles()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(800, 720);
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/DeskView.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    auto *grid = findVisualItem(view.rootObject(), QStringLiteral("shelf-grid"));
    QVERIFY(grid);

    // Exercise the real shelf delegate with a large model without importing
    // hundreds of duplicate documents or relying on platform memory counters.
    QVariantList books;
    QVariantMap book = controller.books().first().toMap();
    for (int i = 1; i <= 400; ++i) {
        book.insert(QStringLiteral("id"), i);
        books.append(book);
    }
    QVERIFY(grid->setProperty("model", books));
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTRY_COMPARE(grid->property("count").toInt(), 400);
    QTRY_VERIFY(findVisualItem(view.rootObject(), QStringLiteral("shelf-book-1")));

    std::function<int(QQuickItem *)> liveTiles = [&](QQuickItem *item) {
        int count = item->objectName().startsWith(QStringLiteral("shelf-book-")) ? 1 : 0;
        for (auto *child : item->childItems())
            count += liveTiles(child);
        return count;
    };
    const auto bounded = [&]() { return liveTiles(view.rootObject()) < 40; };
    QVERIFY(bounded()); // Only visible rows and the small scroll buffer exist.
    QVERIFY(!findVisualItem(view.rootObject(), QStringLiteral("shelf-book-400")));
    for (int i = 0; i < 3; ++i) {
        QVERIFY(QMetaObject::invokeMethod(grid, "positionViewAtEnd"));
        QTRY_VERIFY(findVisualItem(view.rootObject(), QStringLiteral("shelf-book-400")));
        QTRY_VERIFY(bounded());
        QVERIFY(QMetaObject::invokeMethod(grid, "positionViewAtBeginning"));
        QTRY_VERIFY(findVisualItem(view.rootObject(), QStringLiteral("shelf-book-1")));
        QTRY_VERIFY(bounded());
    }
    QVERIFY(view.rootObject()->setProperty("listMode", true));
    QVERIFY(QMetaObject::invokeMethod(grid, "positionViewAtEnd"));
    QTRY_VERIFY(findVisualItem(view.rootObject(), QStringLiteral("shelf-book-400")));
    QTRY_VERIFY(bounded());
    QVERIFY(findVisualItem(view.rootObject(), QStringLiteral("shelf-book-400"))->property("compact").toBool());
}

void TestRegressions::shelfSearchSortAndListView()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(800, 720);
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/DeskView.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *desk = view.rootObject();
    auto *grid = findVisualItem(desk, QStringLiteral("shelf-grid"));
    QVERIFY(grid);
    QTRY_COMPARE(grid->property("count").toInt(), 4);

    auto *search = findVisualItem(desk, QStringLiteral("shelf-search"));
    QVERIFY(search);
    search->forceActiveFocus();
    for (char key : QByteArrayLiteral("MIRA")) // Case-insensitive author filtering.
        QTest::keyClick(&view, key);
    QTRY_COMPARE(desk->property("query").toString(), QStringLiteral("MIRA"));
    QTRY_COMPARE(grid->property("count").toInt(), 2);
    QVERIFY(desk->setProperty("formatFilter", QStringLiteral("pdf")));
    QTRY_COMPARE(grid->property("count").toInt(), 1);
    QVERIFY(desk->setProperty("query", QStringLiteral("no matching book")));
    QTRY_COMPARE(grid->property("count").toInt(), 0);
    auto *reset = findVisualItem(desk, QStringLiteral("shelf-reset-filters"));
    QVERIFY(reset);
    reset->forceActiveFocus();
    QTest::keyClick(&view, Qt::Key_Return);
    QTRY_COMPARE(grid->property("count").toInt(), 4);
    QCOMPARE(desk->property("query").toString(), QString());
    QCOMPARE(desk->property("formatFilter").toString(), QStringLiteral("all"));

    auto *sort = findVisualItem(desk, QStringLiteral("shelf-sort"));
    QVERIFY(sort);
    sort->forceActiveFocus();
    QTest::keyClick(&view, Qt::Key_Home);
    QTest::keyClick(&view, Qt::Key_Down);
    QTRY_COMPARE(desk->property("sortOrder").toString(), QStringLiteral("title"));
    const auto sortedBooks = [&]() {
        return desk->property("filteredBooks").value<QJSValue>().toVariant().toList();
    };
    QCOMPARE(sortedBooks().first().toMap().value("title").toString(), QStringLiteral("Field Notes on Quiet Software"));
    QTest::keyClick(&view, Qt::Key_Down);
    QTRY_COMPARE(desk->property("sortOrder").toString(), QStringLiteral("author"));
    QCOMPARE(sortedBooks().first().toMap().value("author").toString(), QStringLiteral("Ari Vale"));

    auto *listToggle = findVisualItem(desk, QStringLiteral("shelf-list-view"));
    QVERIFY(listToggle);
    listToggle->forceActiveFocus();
    QTest::keyClick(&view, Qt::Key_Return);
    QTRY_VERIFY(desk->property("listMode").toBool());
    QTRY_VERIFY(findVisualItem(desk, QStringLiteral("shelf-book-%1").arg(controller.books().first().toMap().value("id").toInt())));
    const int firstId = sortedBooks().first().toMap().value("id").toInt();
    auto *tile = findVisualItem(desk, QStringLiteral("shelf-book-%1").arg(firstId));
    QVERIFY(tile);
    QVERIFY(tile->property("compact").toBool());
    tile->forceActiveFocus();
    QTest::keyClick(&view, Qt::Key_Return);
    QTRY_VERIFY(controller.readerOpen());
    QCOMPARE(controller.currentBook().value("id").toInt(), firstId);
}

void TestRegressions::libraryNavigationIsKeyboardAccessible()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(320, 720);
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/LibraryPane.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QSignalSpy requested(view.rootObject(), SIGNAL(shelfRequested(QString)));
    QVERIFY(requested.isValid());
    for (const auto &key : {QStringLiteral("all"), QStringLiteral("reading"), QStringLiteral("finished")}) {
        auto *button = findVisualItem(view.rootObject(), QStringLiteral("nav-") + key);
        QVERIFY(button);
        button->forceActiveFocus();
        QTest::keyClick(&view, Qt::Key_Space);
        QCOMPARE(requested.takeFirst().first().toString(), key);
        QVERIFY(view.rootObject()->setProperty("selectedShelf", key));
        QVERIFY(button->property("selected").toBool());
    }
}

void TestRegressions::workspaceNavigationAndResponsiveRails()
{
    qmlRegisterType<ReaderItem>("Stacks.Native", 1, 0, "ReaderItem");
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    const int book = controller.books().first().toMap().value("id").toInt();
    controller.openBook(book);
    controller.savePosition(QStringLiteral("pdf:1"), 0.25);
    controller.closeBook();
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("assetsDir"), QDir(samples).absoluteFilePath(QStringLiteral("..")));
    engine.load(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    window->resize(1120, 720);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto *library = findVisualItem(window->contentItem(), QStringLiteral("library-pane"));
    auto *margin = findVisualItem(window->contentItem(), QStringLiteral("margin-pane"));
    auto *desk = findVisualItem(window->contentItem(), QStringLiteral("desk-view"));
    auto *reader = findVisualItem(window->contentItem(), QStringLiteral("reader-pane"));
    QVERIFY(library && margin && desk && reader);
    QTRY_COMPARE(library->width(), 320.0);
    controller.openBook(book);
    QTRY_COMPARE(library->width(), 256.0);
    QTRY_COMPARE(margin->width(), 300.0);
    QTRY_COMPARE(reader->width(), 564.0);
    auto *reading = findVisualItem(library, QStringLiteral("nav-reading"));
    QVERIFY(reading);
    reading->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Space);
    QTRY_VERIFY(!controller.readerOpen());
    QCOMPARE(desk->property("shelfFilter").toString(), QStringLiteral("reading"));
    QTRY_COMPARE(library->width(), 320.0);
    QTRY_COMPARE(margin->width(), 0.0);
    auto *grid = findVisualItem(desk, QStringLiteral("shelf-grid"));
    QVERIFY(grid);
    QTRY_COMPARE(grid->property("count").toInt(), 1);
    auto *hero = findVisualItem(desk, QStringLiteral("desk-hero"));
    QVERIFY(hero);
    QVERIFY(!hero->isVisible()); // Short windows keep the shelf, not decoration.
    window->resize(1440, 900);
    QTRY_VERIFY(hero->isVisible());
}

void TestRegressions::readerToolbarFitsNarrowWindow()
{
    qmlRegisterType<ReaderItem>("Stacks.Native", 1, 0, "ReaderItem");
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    controller.openBook(controller.books().at(1).toMap().value("id").toInt());
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.rootContext()->setContextProperty(QStringLiteral("assetsDir"),
                                         QDir(samples).absoluteFilePath(QStringLiteral("..")));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(452, 720); // 1120px window minus the 320px shelf and 348px margin.
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/ReaderPane.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *toolbar = findVisualItem(view.rootObject(), QStringLiteral("reader-toolbar"));
    QVERIFY(toolbar);
    int actions = 0;
    std::function<void(QQuickItem *)> verify = [&](QQuickItem *item) {
        if (item->isVisible() && item->property("iconName").isValid()) {
            const QRectF bounds = item->mapRectToScene(item->boundingRect());
            QVERIFY2(bounds.left() >= 0 && bounds.right() <= view.width(),
                     qPrintable(item->property("label").toString()));
            ++actions;
        }
        for (QQuickItem *child : item->childItems()) verify(child);
    };
    verify(toolbar);
    QCOMPARE(actions, 7);
}

void TestRegressions::voiceNoteDeletionRemovesRecording()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    controller.openBook(controller.books().first().toMap().value("id").toInt());
    const auto writeWav = [](const QString &path) {
        QAudioFormat format;
        format.setSampleRate(16000);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Int16);
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(VoiceRecorder::wavHeader(format, 3200)) == 44
            && file.write(QByteArray(3200, '\0')) == 3200;
    };
    const QString owned = QDir(controller.voiceDirectory()).filePath(QStringLiteral("voice-test.wav"));
    const QString foreign = dir.filePath(QStringLiteral("elsewhere.wav"));
    QVERIFY(writeWav(owned) && writeWav(foreign));
    const int ownedNote = controller.addVoiceNoteFile(owned, 100, QStringLiteral("owned"));
    const int foreignNote = controller.addVoiceNoteFile(foreign, 100, QStringLiteral("foreign"));
    QVERIFY(ownedNote > 0 && foreignNote > 0);

    controller.deleteAnnotation(ownedNote);
    QVERIFY(!QFile::exists(owned));
    controller.deleteAnnotation(foreignNote);
    QVERIFY(QFile::exists(foreign)); // Only recordings Stacks made itself are removed.

    Database database;
    QVERIFY(database.open(dir.filePath(QStringLiteral("library.db"))));
    QSqlQuery query(database.db());
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM voice_notes")) && query.next());
    QCOMPARE(query.value(0).toInt(), 0);
}

void TestRegressions::diagramEdgesStayScopedAndUnique()
{
    QTemporaryDir dir;
    Database database;
    QVERIFY(database.open(dir.filePath(QStringLiteral("library.db"))));
    Library library(&database);
    const int a = library.addDiagramNode(1, QStringLiteral("A"), QString(), 0, 0);
    const int b = library.addDiagramNode(1, QStringLiteral("B"), QString(), 0, 0);
    const int shared = library.addDiagramNode(0, QStringLiteral("Shared"), QString(), 0, 0);
    const int other = library.addDiagramNode(2, QStringLiteral("Other"), QString(), 0, 0);

    const int edge = library.addDiagramEdge(a, b);
    QVERIFY(edge > 0);
    QCOMPARE(library.addDiagramEdge(a, b), edge);
    QCOMPARE(library.addDiagramEdge(b, a), edge); // One line, whichever way it was drawn.
    QCOMPARE(library.addDiagramEdge(a, a), 0);
    QCOMPARE(library.addDiagramEdge(a, other), 0); // Never across two books' canvases.
    QCOMPARE(library.addDiagramEdge(a, 9999), 0);
    QVERIFY(library.addDiagramEdge(b, shared) > 0);
    QVERIFY(library.addDiagramEdge(other, shared) > 0);
    QCOMPARE(library.diagramEdges(1).size(), 2);
    QCOMPARE(library.diagramEdges(2).size(), 1);

    QVERIFY(library.deleteDiagramNode(b));
    QVERIFY(library.diagramEdges(1).isEmpty());

    // Duplicates left by older builds collapse when the library opens.
    QSqlQuery query(database.db());
    QVERIFY(query.exec(QStringLiteral("INSERT INTO diagram_edges(from_node, to_node, label) "
                                      "VALUES (%1, %2, ''), (%2, %1, ''), (%1, %2, '')").arg(a).arg(shared)));
    query = QSqlQuery();
    database.close();
    QVERIFY(database.open(dir.filePath(QStringLiteral("library.db"))));
    QCOMPARE(library.diagramEdges(1).size(), 1);
}

void TestRegressions::diagramEdgesFollowDraggedNodes()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    controller.openBook(controller.books().first().toMap().value("id").toInt());
    const int a = controller.addDiagramNode(QStringLiteral("A"), QString(), 100, 100);
    const int b = controller.addDiagramNode(QStringLiteral("B"), QString(), 560, 360);
    const int edge = controller.addDiagramEdge(a, b);
    QVERIFY(edge > 0);
    QCOMPARE(controller.addDiagramEdge(b, a), edge);
    QCOMPARE(controller.diagramEdges().size(), 1);

    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1100, 760);
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/DiagramCanvas.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(QMetaObject::invokeMethod(view.rootObject(), "open"));

    QQuickItem *handle = nullptr;
    QTRY_VERIFY((handle = findVisualItem(view.rootObject(), QStringLiteral("edgeHandle-%1").arg(edge)))
                && handle->isVisible());
    QQuickItem *card = findVisualItem(view.rootObject(), QStringLiteral("diagramNode-%1").arg(a));
    QVERIFY(card);
    const QPointF before = handle->position();
    const QPoint grab = card->mapToScene(QPointF(card->width() - 40, card->height() - 8)).toPoint();
    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, grab);
    for (int step = 1; step <= 12; ++step)
        QTest::mouseMove(&view, grab + QPoint(step * 10, step * 6));
    // The edge has followed the card while the button is still down.
    QTRY_VERIFY(handle->position() != before);
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, grab + QPoint(120, 72));

    // The drag is saved without rebuilding the canvas under the user.
    QTRY_VERIFY(card->x() > 150);
    QCOMPARE(findVisualItem(view.rootObject(), QStringLiteral("diagramNode-%1").arg(a)), card);
    const auto stored = [&controller, a]() {
        for (const QVariant &value : controller.diagramNodes())
            if (value.toMap().value("id").toInt() == a)
                return value.toMap();
        return QVariantMap();
    }();
    QCOMPARE(stored.value("x").toDouble(), qRound(card->x()) * 1.0);
    Database database;
    QVERIFY(database.open(dir.filePath(QStringLiteral("library.db"))));
    QSqlQuery query(database.db());
    QVERIFY(query.exec(QStringLiteral("SELECT x FROM diagram_nodes WHERE id = %1").arg(a)) && query.next());
    QCOMPARE(query.value(0).toDouble(), stored.value("x").toDouble());

    // The knot in the middle of the edge removes it.
    const QPoint knot = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)).toPoint();
    QTest::mouseMove(&view, knot);
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, knot);
    QTRY_VERIFY(controller.diagramEdges().isEmpty());
}

void TestRegressions::kindleFileNamesStayVisible()
{
    QCOMPARE(KindleConverter::suggestedFileName(QStringLiteral("吾輩は猫である"), QString()),
             QStringLiteral("Untitled.epub"));
    QCOMPARE(KindleConverter::suggestedFileName(QStringLiteral("吾輩は猫である"), QStringLiteral("Natsume Sōseki")),
             QStringLiteral("Natsume-Soseki.epub"));
    QCOMPARE(KindleConverter::suggestedFileName(QStringLiteral("Café Society"), QStringLiteral("Zoë")),
             QStringLiteral("Cafe-Society-Zoe.epub"));
    QCOMPARE(KindleConverter::suggestedFileName(QStringLiteral(".hidden"), QString()), QStringLiteral("hidden.epub"));
    QCOMPARE(KindleConverter::suggestedFileName(QStringLiteral("..."), QString()), QStringLiteral("Untitled.epub"));
    QVERIFY(KindleConverter::suggestedFileName(QString(400, QLatin1Char('a')), QString()).size() <= 125);
}

void TestRegressions::htmlConvertsToProse()
{
    QTemporaryDir dir;
    const QString html = dir.filePath(QStringLiteral("page.html"));
    QFile file(html);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("<html><head><title>Ignored</title><style>p { color: red }</style></head><body>"
               "<h1>First &amp; Foremost</h1><p>Hello&nbsp;<b>bold</b> world &#8212; caf&eacute; &#x263A;</p>"
               "<script>alert('no')</script><!-- hidden -->"
               "<h2 class=\"x\">Second</h2><p>Line one<br/>Line two</p></body></html>");
    file.close();
    const KindleConverter::Result result = KindleConverter::convert(
        html, QStringLiteral("html"), QStringLiteral("Page"), QString(), dir.filePath(QStringLiteral("out")));
    QVERIFY2(result.ok, qPrintable(result.error));
    EpubDocument epub;
    QVERIFY(epub.load(result.path));
    const QString text = epub.plainText();
    QVERIFY2(text.contains(QStringLiteral("Hello bold world — café ☺")), qPrintable(text));
    QVERIFY(text.contains(QStringLiteral("Line two")));
    for (const QString &leak : {QStringLiteral("<"), QStringLiteral("alert"), QStringLiteral("color: red"),
                                QStringLiteral("hidden"), QStringLiteral("&amp;")})
        QVERIFY2(!text.contains(leak), qPrintable(leak));
    QCOMPARE(epub.chapters().size(), 2);
    QCOMPARE(epub.chapters().first().title, QStringLiteral("First & Foremost"));
    QCOMPARE(epub.chapters().last().title, QStringLiteral("Second"));
}

void TestRegressions::kindleRejectsOversizedAttachments()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("huge.epub"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(KindleSender::kMaxAttachmentBytes + 1)); // Sparse: costs no disk.
    file.close();

    KindleSender sender;
    QSignalSpy finished(&sender, &KindleSender::finished);
    KindleSender::Settings settings;
    settings.fromAddress = QStringLiteral("me@example.com");
    settings.kindleAddress = QStringLiteral("me@kindle.com");
    settings.user = QStringLiteral("me");
    settings.password = QStringLiteral("secret");
    settings.smtpHost = QStringLiteral("127.0.0.1");
    settings.smtpPort = 1;
    sender.send(settings, path, QStringLiteral("Huge"), QString());
    QCOMPARE(finished.size(), 1);
    QVERIFY(!finished.first().at(0).toBool());
    QVERIFY2(finished.first().at(1).toString().contains(QStringLiteral("50 MB")),
             qPrintable(finished.first().at(1).toString()));
    QVERIFY(!sender.isBusy());
}

void TestRegressions::kindlePasswordStaysOutOfQml()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    controller.saveKindleSettings({{"fromAddress", "me@example.com"}, {"user", "me"}, {"password", "secret"},
                                   {"kindleAddress", "me@kindle.com"}, {"smtpPort", 465}});
    QVERIFY(!controller.kindleSettings().contains(QStringLiteral("password")));
    QVERIFY(controller.kindleSettings().value("hasPassword").toBool());
    QCOMPARE(controller.kindleSettings().value("smtpPort").toInt(), 465);

    const QString json = dir.filePath(QStringLiteral("kindle.json"));
    const QFile::Permissions others = QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther;
    QCOMPARE(QFile::permissions(json) & others, QFile::Permissions());

    // Saving with an empty password field keeps the stored one.
    controller.saveKindleSettings({{"fromAddress", "me@example.com"}, {"user", "me"}, {"password", ""},
                                   {"kindleAddress", "me@kindle.com"}, {"smtpPort", 465}});
    QFile file(json);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll().contains("secret"));
    QVERIFY(controller.kindleSettings().value("hasPassword").toBool());
}

void TestRegressions::searchSnippetsKeepMarkupBalanced()
{
    QTemporaryDir dir;
    Database database;
    QVERIFY(database.open(dir.filePath(QStringLiteral("library.db"))));
    Library library(&database);
    const int book = library.importBook(QDir(samples).filePath(QStringLiteral("systems-that-breathe.epub")));
    // Long words push the snippet past its length limit, with the hit first.
    QString note = QStringLiteral("needleword");
    for (int i = 0; i < 40; ++i)
        note += QStringLiteral(" exceptionallylongpaddingword%1").arg(i);
    QVERIFY(library.addAnnotation(book, QStringLiteral("note"), QStringLiteral("epub:1-2"), QString(), note) > 0);
    const QVector<SearchResult> results = library.search(QStringLiteral("needleword"));
    QVERIFY(!results.isEmpty());
    for (const SearchResult &result : results) {
        QVERIFY2(result.snippet.contains(QStringLiteral(">needleword<")), qPrintable(result.snippet));
        QCOMPARE(result.snippet.count(QStringLiteral("<b>")), result.snippet.count(QStringLiteral("</b>")));
        QCOMPARE(result.snippet.count(QStringLiteral("<font")), result.snippet.count(QStringLiteral("</font>")));
        QVERIFY(!result.snippet.contains(QStringLiteral("&lt;")));
        QVERIFY(result.snippet.size() < 400);
    }
}

void TestRegressions::pdfRenderingIsBounded()
{
    PdfDocument pdf;
    QVERIFY(pdf.load(QDir(samples).filePath(QStringLiteral("the-art-of-attention.pdf"))));
    // A page claiming an absurd size, or an absurd zoom, must not allocate gigabytes.
    const QImage image = pdf.renderPage(0, 500.0);
    QVERIFY(!image.isNull());
    QVERIFY(image.width() <= 8192 && image.height() <= 8192);
    QVERIFY(qint64(image.width()) * image.height() <= 24LL * 1024 * 1024 + 8192);
    QVERIFY(pdf.renderPage(0, 0.0).isNull());
    QVERIFY(pdf.renderPage(0, -2.0).isNull());
}

void TestRegressions::pdfTrackpadSwipeTurnsOnePage()
{
    struct WheelReader : ReaderItem {
        using ReaderItem::wheelEvent;
    };
    WheelReader reader;
    reader.setSize(QSizeF(600, 800));
    QVERIFY(reader.load(QDir(samples).filePath(QStringLiteral("the-art-of-attention.pdf")), QStringLiteral("pdf")));
    QVERIFY(reader.pageCount() >= 4);
    const auto wheel = [&reader](Qt::ScrollPhase phase, int pixels, int angle) {
        QWheelEvent event(QPointF(100, 100), QPointF(100, 100), QPoint(0, pixels), QPoint(0, angle),
                          Qt::NoButton, Qt::NoModifier, phase, false);
        reader.wheelEvent(&event);
    };
    // One long trackpad swipe, then its momentum: one page.
    wheel(Qt::ScrollBegin, 0, 0);
    for (int i = 0; i < 40; ++i)
        wheel(Qt::ScrollUpdate, -12, -24);
    wheel(Qt::ScrollEnd, 0, 0);
    for (int i = 0; i < 30; ++i)
        wheel(Qt::ScrollMomentum, -12, -24);
    wheel(Qt::ScrollEnd, 0, 0);
    QCOMPARE(reader.currentPage(), 1);
    // A mouse wheel turns a page per notch, both ways.
    wheel(Qt::NoScrollPhase, 0, -120);
    wheel(Qt::NoScrollPhase, 0, -120);
    QCOMPARE(reader.currentPage(), 3);
    wheel(Qt::NoScrollPhase, 0, 120);
    QCOMPARE(reader.currentPage(), 2);
}

void TestRegressions::epubProgressReachesTheEnd()
{
    ReaderItem reader;
    reader.setSize(QSizeF(720, 600));
    QVERIFY(reader.load(QDir(samples).filePath(QStringLiteral("systems-that-breathe.epub")), QStringLiteral("epub")));
    QVERIFY(reader.maxScrollY() > 0);
    for (int i = 0; i < 1000 && reader.scrollY() < reader.maxScrollY(); ++i)
        reader.nextPage();
    QCOMPARE(reader.scrollY(), reader.maxScrollY());
    QCOMPARE(reader.progress(), 1.0); // "Finished" is reachable.
    reader.previousPage();
    QVERIFY(reader.progress() < 1.0);
}

void TestRegressions::missingBookIsNotOpened()
{
    QTemporaryDir dir;
    const QString copy = dir.filePath(QStringLiteral("moved.epub"));
    QVERIFY(QFile::copy(QDir(samples).filePath(QStringLiteral("systems-that-breathe.epub")), copy));
    AppController controller;
    QVERIFY(controller.initialize(dir.filePath(QStringLiteral("no-samples")), dir.filePath(QStringLiteral("data"))));
    controller.importBook(copy);
    QVERIFY(controller.readerOpen());
    controller.closeBook();
    const int id = controller.books().first().toMap().value("id").toInt();
    QVERIFY(QFile::remove(copy));
    QSignalSpy toasts(&controller, &AppController::toast);
    controller.openBook(id);
    QVERIFY(!controller.readerOpen());
    QCOMPARE(toasts.size(), 1);
    QVERIFY(toasts.first().first().toString().contains(QStringLiteral("no longer")));
}

void TestRegressions::overlayPanelsKeepTheirClicks()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1000, 760);
    view.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/CommandPalette.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(QMetaObject::invokeMethod(view.rootObject(), "open"));
    QVERIFY(view.rootObject()->isVisible());
    // Empty space inside the panel, below the search field.
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(view.width() / 2, 86 + 68 + 60));
    QTest::qWait(50);
    QVERIFY(view.rootObject()->isVisible());
    // The scrim around the panel still dismisses it.
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(12, view.height() - 12));
    QTRY_VERIFY(!view.rootObject()->isVisible());
}

void TestRegressions::roundedCornersStayRounded()
{
    QTemporaryDir dir;
    AppController controller;
    QVERIFY(controller.initialize(samples, dir.path()));
    controller.setThemeMode(QStringLiteral("dark"));
    const auto difference = [](const QColor &a, const QColor &b) {
        return std::max({qAbs(a.red() - b.red()), qAbs(a.green() - b.green()), qAbs(a.blue() - b.blue())});
    };

    // The jacket's spine and sheen used to paint square corners over it.
    QQuickView cover;
    cover.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    cover.setColor(Qt::white);
    cover.setResizeMode(QQuickView::SizeRootObjectToView);
    cover.resize(112, 160);
    cover.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/Cover.qml")));
    QCOMPARE(cover.status(), QQuickView::Ready);
    cover.rootObject()->setProperty("title", QStringLiteral("Corners"));
    cover.show();
    QVERIFY(QTest::qWaitForWindowExposed(&cover));
    QImage jacket;
    QTRY_VERIFY((jacket = cover.grabWindow()).pixelColor(3, 80).lightness() < 150); // Spine painted.
    for (const QPoint corner : {QPoint(0, 0), QPoint(0, 159)})
        QVERIFY2(difference(jacket.pixelColor(corner), Qt::white) < 60,
                 qPrintable(jacket.pixelColor(corner).name()));

    // The desk's hero card: its tinted panel used to poke past the corners.
    QQuickView desk;
    desk.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    desk.setColor(Qt::black);
    desk.setResizeMode(QQuickView::SizeRootObjectToView);
    desk.resize(1100, 900); // The reading feature is intentionally hidden in short windows.
    desk.setSource(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/DeskView.qml")));
    QCOMPARE(desk.status(), QQuickView::Ready);
    desk.show();
    QVERIFY(QTest::qWaitForWindowExposed(&desk));
    QQuickItem *hero = nullptr;
    QTRY_VERIFY((hero = findVisualItem(desk.rootObject(), QStringLiteral("desk-hero"))) != nullptr);
    QVERIFY(hero->isVisible());
    const QRect card = hero->mapRectToScene(hero->boundingRect()).toAlignedRect();
    QVERIFY(card.top() >= 0 && card.bottom() < desk.height());
    QVERIFY(card.left() > 0 && card.right() < desk.width()); // Centred, not pushed off-screen.
    const QImage image = desk.grabWindow();
    const QColor outside = image.pixelColor(card.right() + 4, card.top() + 1);
    for (const QPoint corner : {QPoint(card.right() - 1, card.top() + 1), QPoint(card.right() - 1, card.bottom() - 1)})
        QVERIFY2(difference(image.pixelColor(corner), outside) <= 2,
                 qPrintable(image.pixelColor(corner).name() + QStringLiteral(" vs ") + outside.name()));
}

QTEST_MAIN(TestRegressions)
#include "tst_regressions.moc"
