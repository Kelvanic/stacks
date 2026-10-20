#include "core/Database.h"
#include "core/EpubDocument.h"
#include "core/KindleConverter.h"
#include "core/Library.h"
#include "core/PdfDocument.h"
#include "native/ReaderItem.h"

#include <QDataStream>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

#include <zip.h>

using namespace stacks;

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testSeedLibrary();
    void testPositionMemory();
    void testAnnotations();
    void testVoiceNotes();
    void testSearch();
    void testDiagrams();
    void testEpubParsing();
    void testPdfRenderingAndText();
    void testReaderItemPdf();
    void testReaderItemEpub();
    void testReaderRendersContent();
    void testKindleConverter();
    void cleanupTestCase();

private:
    QTemporaryDir m_temp;
    Database *m_database = nullptr;
    Library *m_library = nullptr;
    int m_epubBookId = 0;
    int m_pdfBookId = 0;
};

void TestCore::initTestCase()
{
    QVERIFY(m_temp.isValid());
    PdfDocument::globalInit();

    m_database = new Database();
    QString error;
    QVERIFY2(m_database->open(QDir(m_temp.path()).filePath(QStringLiteral("library.db")), &error),
             qUtf8Printable(error));
    QVERIFY(m_database->ftsAvailable());

    m_library = new Library(m_database);
    QVERIFY(m_library->seedSamples(QStringLiteral(STACKS_SAMPLE_DIR)));
}

void TestCore::testSeedLibrary()
{
    const QVector<Book> books = m_library->books();
    QCOMPARE(books.size(), 4);

    for (const Book &book : books) {
        QVERIFY(book.id > 0);
        QVERIFY(!book.title.isEmpty());
        QVERIFY(!book.author.isEmpty());
        QVERIFY(book.format == QStringLiteral("pdf") || book.format == QStringLiteral("epub"));
        QVERIFY(book.pageCount > 0);
        QVERIFY(book.wordCount > 0);
        if (book.format == QStringLiteral("epub"))
            m_epubBookId = book.id;
        else
            m_pdfBookId = book.id;
    }

    QVERIFY(m_epubBookId > 0);
    QVERIFY(m_pdfBookId > 0);
    const bool hasAttention = std::any_of(books.cbegin(), books.cend(), [](const Book &book) {
        return book.title == QStringLiteral("The Art of Attention");
    });
    QVERIFY(hasAttention);
}

void TestCore::testPositionMemory()
{
    const Book before = m_library->book(m_epubBookId);
    QVERIFY(m_library->savePosition(m_epubBookId, QStringLiteral("epub:420"), 0.37));

    const Book after = m_library->book(m_epubBookId);
    QCOMPARE(after.locator, QStringLiteral("epub:420"));
    QCOMPARE(after.progress, 0.37);
    QVERIFY(after.lastOpenedAt >= before.lastOpenedAt);
}

void TestCore::testAnnotations()
{
    const int id = m_library->addAnnotation(
        m_epubBookId, QStringLiteral("highlight"), QStringLiteral("epub:12-48"),
        QStringLiteral("Attention is a practice, not a possession."), QStringLiteral("Start here"), 0);
    QVERIFY(id > 0);

    const QVector<Annotation> annotations = m_library->annotations(m_epubBookId);
    QVERIFY(!annotations.isEmpty());
    const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
                                 [id](const Annotation &a) { return a.id == id; });
    QVERIFY(it != annotations.cend());
    QCOMPARE(it->kind, QStringLiteral("highlight"));
    QCOMPARE(it->note, QStringLiteral("Start here"));
    QVERIFY(it->quote.startsWith(QStringLiteral("Attention")));

    QVERIFY(m_library->updateAnnotation(id, QStringLiteral("Revised note"), QString()));
    const QVector<Annotation> updated = m_library->annotations(m_epubBookId);
    const auto updatedIt = std::find_if(updated.cbegin(), updated.cend(),
                                        [id](const Annotation &a) { return a.id == id; });
    QVERIFY(updatedIt != updated.cend());
    QCOMPARE(updatedIt->note, QStringLiteral("Revised note"));
    QCOMPARE(updatedIt->quote, QStringLiteral("Attention is a practice, not a possession."));
}

void TestCore::testVoiceNotes()
{
    const QString path = QDir(m_temp.path()).filePath(QStringLiteral("voice.wav"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + 3200);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(16000) << quint32(32000)
        << quint16(2) << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(3200);
    QByteArray pcm(3200, '\0');
    out.writeRawData(pcm.constData(), pcm.size());
    file.close();

    const int voiceId = m_library->addVoiceNote(m_epubBookId, 0, path, 100,
                                                QStringLiteral("A spoken margin note."));
    QVERIFY(voiceId > 0);

    const QVector<Annotation> annotations = m_library->annotations(m_epubBookId);
    const auto it = std::find_if(annotations.cbegin(), annotations.cend(),
                                 [](const Annotation &a) { return a.kind == QStringLiteral("voice"); });
    QVERIFY(it != annotations.cend());
    QCOMPARE(it->audioPath, path);
    QCOMPARE(it->durationMs, 100);
    QCOMPARE(it->note, QStringLiteral("A spoken margin note."));
}

void TestCore::testSearch()
{
    const QVector<SearchResult> margin = m_library->search(QStringLiteral("margin"));
    QVERIFY(!margin.isEmpty());
    bool foundAttention = false;
    for (const SearchResult &result : margin) {
        if (result.bookTitle == QStringLiteral("The Art of Attention"))
            foundAttention = true;
        QVERIFY(!result.snippet.isEmpty());
        QVERIFY(!result.locator.isEmpty());
    }
    QVERIFY(foundAttention);

    const QVector<SearchResult> attention = m_library->search(QStringLiteral("attention"));
    QVERIFY(attention.size() >= 2);

    const QVector<SearchResult> missing = m_library->search(QStringLiteral("zzzzzzzz"));
    QVERIFY(missing.isEmpty());
}

void TestCore::testDiagrams()
{
    const int first = m_library->addDiagramNode(m_epubBookId, QStringLiteral("Attention"),
                                                QStringLiteral("A practice"), 120, 160);
    const int second = m_library->addDiagramNode(m_epubBookId, QStringLiteral("Margin"),
                                                 QStringLiteral("Where thinking happens"), 380, 240);
    QVERIFY(first > 0);
    QVERIFY(second > 0);

    const int edge = m_library->addDiagramEdge(first, second, QStringLiteral("produces"));
    QVERIFY(edge > 0);

    QVERIFY(m_library->moveDiagramNode(first, 200, 210));
    const QVector<DiagramNode> nodes = m_library->diagramNodes(m_epubBookId);
    const auto node = std::find_if(nodes.cbegin(), nodes.cend(),
                                   [first](const DiagramNode &n) { return n.id == first; });
    QVERIFY(node != nodes.cend());
    QCOMPARE(node->x, 200.0);
    QCOMPARE(node->y, 210.0);

    const QVector<DiagramEdge> edges = m_library->diagramEdges(m_epubBookId);
    QCOMPARE(edges.size(), 1);
    QCOMPARE(edges.first().label, QStringLiteral("produces"));
    QVERIFY(m_library->deleteDiagramEdge(edge));
    QVERIFY(m_library->diagramEdges(m_epubBookId).isEmpty());

    // A default (null) label must not trip the NOT NULL constraint.
    const int third = m_library->addDiagramNode(m_epubBookId, QStringLiteral("Voice"),
                                                QString(), 220, 420);
    QVERIFY(third > 0);
    const int plainEdge = m_library->addDiagramEdge(second, third);
    QVERIFY(plainEdge > 0);
    const QVector<DiagramEdge> plainEdges = m_library->diagramEdges(m_epubBookId);
    QCOMPARE(plainEdges.size(), 1);
    QVERIFY(plainEdges.first().label.isEmpty());
    QVERIFY(m_library->deleteDiagramEdge(plainEdge));
}

void TestCore::testEpubParsing()
{
    EpubDocument epub;
    QString error;
    QVERIFY2(epub.load(QDir(QStringLiteral(STACKS_SAMPLE_DIR)).filePath(QStringLiteral("systems-that-breathe.epub")), &error),
             qUtf8Printable(error));
    QCOMPARE(epub.title(), QStringLiteral("Systems That Breathe"));
    QCOMPARE(epub.author(), QStringLiteral("Mira Chen"));
    QVERIFY(epub.passages().size() >= 12);
    QVERIFY(epub.html().contains(QStringLiteral("The Room That Listens")));
    QVERIFY(epub.plainText().contains(QStringLiteral("A system that breathes")));
}

void TestCore::testPdfRenderingAndText()
{
    PdfDocument pdf;
    QString error;
    QVERIFY2(pdf.load(QDir(QStringLiteral(STACKS_SAMPLE_DIR)).filePath(QStringLiteral("the-art-of-attention.pdf")), &error),
             qUtf8Printable(error));
    QVERIFY(pdf.pageCount() >= 5);
    const QString text = pdf.pageText(1);
    QVERIFY(text.contains(QStringLiteral("Attention"), Qt::CaseInsensitive));
    const QImage image = pdf.renderPage(1, 1.0);
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 100);
    QVERIFY(image.height() > 100);
}

void TestCore::testReaderItemPdf()
{
    ReaderItem reader;
    reader.setSize(QSizeF(820, 620));

    const QString path = QDir(QStringLiteral(STACKS_SAMPLE_DIR))
                             .filePath(QStringLiteral("the-art-of-attention.pdf"));
    QVERIFY(reader.load(path, QStringLiteral("pdf"), QStringLiteral("pdf:1")));
    QCOMPARE(reader.format(), QStringLiteral("pdf"));
    QCOMPARE(reader.currentPage(), 1);
    QVERIFY(reader.pageCount() >= 5);
    QVERIFY(reader.locator().startsWith(QStringLiteral("pdf:")));

    QSignalSpy spy(&reader, &ReaderItem::positionChanged);
    reader.nextPage();
    QCOMPARE(reader.currentPage(), 2);
    QVERIFY(spy.count() >= 1);
    QCOMPARE(reader.locator(), QStringLiteral("pdf:2"));
}

void TestCore::testReaderItemEpub()
{
    ReaderItem reader;
    reader.setSize(QSizeF(820, 620));

    const QString path = QDir(QStringLiteral(STACKS_SAMPLE_DIR))
                             .filePath(QStringLiteral("systems-that-breathe.epub"));
    QVERIFY(reader.load(path, QStringLiteral("epub"), QStringLiteral("epub:0:0")));
    QCOMPARE(reader.format(), QStringLiteral("epub"));
    QVERIFY(reader.pageCount() >= 4);
    QVERIFY(reader.locator().startsWith(QStringLiteral("epub:")));
    QVERIFY(reader.progress() >= 0.0);

    reader.goToLocator(QStringLiteral("epub:0:2"));
    QVERIFY(reader.locator().startsWith(QStringLiteral("epub:")));
}

// Exercises the actual paint path of ReaderItem. This is the regression test
// for the EPUB clip: if the paint clip is not offset by the scroll position,
// scrolled reading goes blank even though the data layer is fine.
void TestCore::testReaderRendersContent()
{
    const QString sampleDir = QStringLiteral(STACKS_SAMPLE_DIR);

    {
        ReaderItem reader;
        reader.setSize(QSizeF(820, 620));
        QVERIFY(reader.load(QDir(sampleDir).filePath(QStringLiteral("the-art-of-attention.pdf")),
                            QStringLiteral("pdf"), QString()));

        QImage canvas(820, 620, QImage::Format_ARGB32);
        canvas.fill(Qt::magenta);
        QPainter painter(&canvas);
        reader.paint(&painter);
        painter.end();

        int bright = 0;
        for (int y = 120; y < 500; y += 7) {
            for (int x = 260; x < 560; x += 7) {
                const QColor color = canvas.pixelColor(x, y);
                if (color.red() > 230 && color.green() > 230 && color.blue() > 230)
                    ++bright;
            }
        }
        QVERIFY(bright > 50);
    }

    {
        ReaderItem reader;
        reader.setSize(QSizeF(820, 620));
        QVERIFY(reader.load(QDir(sampleDir).filePath(QStringLiteral("systems-that-breathe.epub")),
                            QStringLiteral("epub"), QString()));

        const double startProgress = reader.progress();
        for (int i = 0; i < 6; ++i)
            reader.nextPage();
        QVERIFY(reader.progress() > startProgress);

        QImage canvas(820, 620, QImage::Format_ARGB32);
        canvas.fill(Qt::magenta);
        QPainter painter(&canvas);
        reader.paint(&painter);
        painter.end();

        int dark = 0;
        for (int y = 60; y < 560; y += 3) {
            for (int x = 80; x < 740; x += 3) {
                const QColor color = canvas.pixelColor(x, y);
                if (color.red() < 120 && color.green() < 120 && color.blue() < 120)
                    ++dark;
            }
        }
        QVERIFY(dark > 200);
    }
}

void TestCore::testKindleConverter()
{
    const QString sampleDir = QStringLiteral(STACKS_SAMPLE_DIR);
    const QString outputDir = m_temp.filePath(QStringLiteral("kindle"));

    // A PDF becomes a Kindle-ready EPUB.
    const KindleConverter::Result converted = KindleConverter::convert(
        QDir(sampleDir).filePath(QStringLiteral("the-art-of-attention.pdf")),
        QStringLiteral("pdf"), QStringLiteral("The Art of Attention"),
        QStringLiteral("Mira Chen"), outputDir);

    QVERIFY2(converted.ok, qPrintable(converted.error));
    QVERIFY(QFileInfo::exists(converted.path));
    QVERIFY(converted.words > 100);
    QVERIFY(converted.pages > 0);
    QVERIFY(converted.bytes > 1000);

    // The package is a well-formed EPUB: the mimetype comes first and is
    // stored, and the package document is where a reader expects it.
    int errorCode = 0;
    zip_t *archive = zip_open(converted.path.toUtf8().constData(), ZIP_RDONLY, &errorCode);
    QVERIFY(archive != nullptr);
    QCOMPARE(zip_get_num_entries(archive, 0) > 4, true);
    QCOMPARE(QString::fromUtf8(zip_get_name(archive, 0, 0)), QStringLiteral("mimetype"));
    QVERIFY(zip_name_locate(archive, "META-INF/container.xml", 0) > 0);
    QVERIFY(zip_name_locate(archive, "OEBPS/content.opf", 0) > 0);
    QVERIFY(zip_name_locate(archive, "OEBPS/nav.xhtml", 0) > 0);
    QVERIFY(zip_name_locate(archive, "OEBPS/toc.ncx", 0) > 0);
    zip_close(archive);

    // The file we produced is readable by our own reader.
    EpubDocument produced;
    QString error;
    QVERIFY2(produced.load(converted.path, &error), qPrintable(error));
    QCOMPARE(produced.title(), QStringLiteral("The Art of Attention"));
    QCOMPARE(produced.author(), QStringLiteral("Mira Chen"));
    QVERIFY(produced.plainText().size() > 500);
    QVERIFY(produced.chapters().size() >= 1);

    // An EPUB is normalised rather than rebuilt: it keeps its own text.
    const KindleConverter::Result normalized = KindleConverter::convert(
        QDir(sampleDir).filePath(QStringLiteral("systems-that-breathe.epub")),
        QStringLiteral("epub"), QStringLiteral("Systems That Breathe"),
        QStringLiteral("Mira Chen"), outputDir);

    QVERIFY2(normalized.ok, qPrintable(normalized.error));
    QVERIFY(QFileInfo::exists(normalized.path));

    EpubDocument normalizedDocument;
    QVERIFY2(normalizedDocument.load(normalized.path, &error), qPrintable(error));
    QCOMPARE(normalizedDocument.title(), QStringLiteral("Systems That Breathe"));
    QVERIFY(normalizedDocument.plainText().size() > 500);
}

void TestCore::cleanupTestCase()
{
    delete m_library;
    m_library = nullptr;
    delete m_database;
    m_database = nullptr;
    PdfDocument::globalShutdown();
}

QTEST_MAIN(TestCore)
#include "tst_core.moc"
