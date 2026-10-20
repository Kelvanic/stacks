#include "core/AppController.h"
#include "core/PdfDocument.h"
#include "native/ReaderItem.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

using namespace stacks;

namespace {

void loadFonts()
{
    const QString fontDir = QStringLiteral(STACKS_ASSETS_DIR "/fonts");
    const QStringList fonts = {
        QStringLiteral("Inter.ttf"),
        QStringLiteral("Newsreader.ttf"),
        QStringLiteral("JetBrainsMono.ttf")
    };
    for (const QString &font : fonts)
        QFontDatabase::addApplicationFont(QDir(fontDir).filePath(font));

    QFont defaultFont(QStringLiteral("Inter"));
    defaultFont.setPointSizeF(13.0);
    QGuiApplication::setFont(defaultFont);
}

} // namespace

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsSet("STACKS_TEST_MODE"))
        QStandardPaths::setTestModeEnabled(true);

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QCoreApplication::setOrganizationName(QStringLiteral("Stacks"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("stacks.local"));
    QCoreApplication::setApplicationName(QStringLiteral("Stacks"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    loadFonts();
    PdfDocument::globalInit();

    qmlRegisterType<ReaderItem>("Stacks.Native", 1, 0, "ReaderItem");

    AppController controller;

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Stacks — a fast, local-first reading desk"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption importOption(QStringList { QStringLiteral("import") },
                                    QStringLiteral("Import a PDF or EPUB before launching."),
                                     QStringLiteral("path"));
    QCommandLineOption dataOption(QStringList { QStringLiteral("data-dir") },
                                  QStringLiteral("Use an explicit local data directory."), QStringLiteral("path"));
    QCommandLineOption screenshotOption(QStringList { QStringLiteral("screenshot") },
                                        QStringLiteral("Render the UI once and save a PNG."),
                                        QStringLiteral("path"));
    QCommandLineOption smokeOption(QStringList { QStringLiteral("smoke-test") },
                                   QStringLiteral("Load the UI, verify the library, and exit."));
    QCommandLineOption queryOption(QStringList { QStringLiteral("query") },
                                   QStringLiteral("Run a library search and exit (headless)."),
                                   QStringLiteral("text"));
    QCommandLineOption openOption(QStringList { QStringLiteral("open") },
                                  QStringLiteral("Open a book by id before showing the window."),
                                  QStringLiteral("id"));
    QCommandLineOption demoOption(QStringList { QStringLiteral("demo") },
                                  QStringLiteral("Seed demo marks (highlight, note, bookmark, diagram)."));
    QCommandLineOption diagramOption(QStringList { QStringLiteral("diagram") },
                                     QStringLiteral("Open the diagram canvas after launch."));
    parser.addOption(importOption);
    parser.addOption(dataOption);
    parser.addOption(screenshotOption);
    parser.addOption(smokeOption);
    parser.addOption(queryOption);
    parser.addOption(openOption);
    parser.addOption(demoOption);
    parser.addOption(diagramOption);
    parser.process(app);

    const QString sampleDir = QStringLiteral(STACKS_SAMPLE_DIR);
    if (!controller.initialize(sampleDir, parser.value(dataOption))) {
        qCritical("Stacks failed to initialize its local library.");
        PdfDocument::globalShutdown();
        return 2;
    }

    if (parser.isSet(importOption))
        controller.importBook(QUrl::fromLocalFile(parser.value(importOption)).toString());

    if (parser.isSet(openOption)) {
        bool ok = false;
        const int bookId = parser.value(openOption).toInt(&ok);
        if (ok && bookId > 0)
            controller.openBook(bookId);
    }

    if (parser.isSet(demoOption) && controller.readerOpen()) {
        const QVariantMap book = controller.currentBook();
        const bool isEpub = book.value(QStringLiteral("format")).toString() == QStringLiteral("epub");

        // Demo marks are idempotent: seed once, then leave the library alone.
        const bool alreadySeeded = !controller.annotations().isEmpty()
                                   || !controller.diagramNodes().isEmpty();
        if (!alreadySeeded && isEpub) {
            controller.addHighlight(QStringLiteral("Attention is a practice, not a possession."),
                                    QStringLiteral("epub:0:0"), 0);
            controller.addNote(QStringLiteral("The margin is the point. Keep the note before the category."),
                               QStringLiteral("We mistake speed for fluency."),
                               QStringLiteral("epub:1:0"), 1);
            controller.addBookmark(QStringLiteral("epub:3:0"), 3);
            const int attention = controller.addDiagramNode(QStringLiteral("Attention"),
                                                            QStringLiteral("A practice, rebuilt daily"), 180, 150);
            const int margin = controller.addDiagramNode(QStringLiteral("Margin"),
                                                         QStringLiteral("Where thinking happens"), 500, 300);
            if (attention > 0 && margin > 0)
                controller.addDiagramEdge(attention, margin);
        } else if (!alreadySeeded) {
            controller.addHighlight(QStringLiteral("A reading desk should disappear."),
                                    QStringLiteral("pdf:1:120,300,260,46"), 1);
            controller.addNote(QStringLiteral("The page is the whole interface."),
                               QStringLiteral("Everything else is a tax on thought."),
                               QStringLiteral("pdf:2"), 2);
            controller.addBookmark(QStringLiteral("pdf:3"), 3);
            const int attention = controller.addDiagramNode(QStringLiteral("Attention"),
                                                            QStringLiteral("A practice, rebuilt daily"), 180, 150);
            const int margin = controller.addDiagramNode(QStringLiteral("Margin"),
                                                         QStringLiteral("Where thinking happens"), 500, 300);
            if (attention > 0 && margin > 0)
                controller.addDiagramEdge(attention, margin);
        }
    }

    if (parser.isSet(queryOption)) {
        controller.search(parser.value(queryOption));
        const QVariantList results = controller.searchResults();
        for (const QVariant &value : results) {
            const QVariantMap result = value.toMap();
            printf("%s — %s\n    %s\n",
                   qUtf8Printable(result.value(QStringLiteral("bookTitle")).toString()),
                   qUtf8Printable(result.value(QStringLiteral("locator")).toString()),
                   qUtf8Printable(result.value(QStringLiteral("snippet")).toString()));
        }
        PdfDocument::globalShutdown();
        return results.isEmpty() ? 1 : 0;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("assetsDir"), QString(STACKS_ASSETS_DIR));
    engine.load(QUrl::fromLocalFile(QStringLiteral(STACKS_QML_DIR "/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        qCritical("Stacks could not load its QML interface.");
        PdfDocument::globalShutdown();
        return 3;
    }

    QObject *rootObject = engine.rootObjects().first();
    QQuickWindow *window = qobject_cast<QQuickWindow *>(rootObject);

    if (parser.isSet(diagramOption))
        QTimer::singleShot(400, &app, [&controller]() { controller.requestDiagram(); });

    int result = 0;

    if (parser.isSet(smokeOption) || parser.isSet(screenshotOption)) {
        const QString screenshotPath = parser.value(screenshotOption);
        QTimer::singleShot(1400, &app, [&]() {
            if (!screenshotPath.isEmpty() && window) {
                const QImage image = window->grabWindow();
                if (!image.isNull())
                    image.save(screenshotPath, "PNG");
            }
            const int bookCount = controller.books().size();
            fprintf(stderr, "stacks-smoke: books=%d fts=%s\n", bookCount,
                    controller.ftsAvailable() ? "on" : "off");
            result = bookCount > 0 ? 0 : 4;
            app.quit();
        });
    }

    const int exitCode = app.exec();
    PdfDocument::globalShutdown();
    return result != 0 ? result : exitCode;
}
