#include "core/AppController.h"
#include "core/Database.h"
#include "core/EpubDocument.h"
#include "core/Library.h"
#include "core/PdfDocument.h"
#include "native/ReaderItem.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <QSet>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
#include <future>
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

QTEST_MAIN(TestRegressions)
#include "tst_regressions.moc"
