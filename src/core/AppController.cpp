#include "AppController.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QThread>
#include <QStandardPaths>
#include <QUrl>

#include <memory>

namespace stacks {

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_positionTimer.setSingleShot(true);
    m_positionTimer.setInterval(450);
    connect(&m_positionTimer, &QTimer::timeout, this, &AppController::flushPosition);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]() {
        flushPosition();
        if (m_recording)
            stopVoiceNote(QString());
    });
    bindVoiceRecorder();

    m_kindleSender = new KindleSender(this);
    connect(m_kindleSender, &KindleSender::progress, this, [this](const QString &message) {
        m_kindleStatus = message;
        emit kindleChanged();
    });
    connect(m_kindleSender, &KindleSender::finished, this, [this](bool ok, const QString &message) {
        m_kindleBusy = false;
        m_kindleStatus = message;
        emit kindleChanged();
        emit toast(message);
    });
}

AppController::~AppController()
{
    m_positionTimer.stop();
    flushPosition();
    if (m_recording)
        stopVoiceNote(QString());
    // The worker owns only its inputs and result, never this controller.
    if (m_conversionWorker) {
        m_conversionWorker->wait();
        delete m_conversionWorker;
        m_conversionWorker = nullptr;
    }
    delete m_library;
    m_library = nullptr;
}

void AppController::bindVoiceRecorder()
{
    connect(&m_voiceRecorder, &VoiceRecorder::recordingChanged, this, [this](bool recording) {
        m_recording = recording;
        emit recordingChanged();
    });
    connect(&m_voiceRecorder, &VoiceRecorder::levelChanged, this, [this](double level) {
        m_recordingLevel = level;
        emit recordingLevelChanged();
    });
    connect(&m_voiceRecorder, &VoiceRecorder::errorOccurred, this, [this](const QString &message) {
        setStatus(message);
        emit toast(message);
    });
}

bool AppController::initialize(const QString &sampleDirectory, const QString &dataDirectory)
{
    const QString base = dataDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : dataDirectory;
    m_dataDirectory = base;
    QDir().mkpath(base);
    QDir().mkpath(QDir(base).filePath(QStringLiteral("voice")));
    QDir().mkpath(QDir(base).filePath(QStringLiteral("kindle")));
    loadKindleSettings();

    QString error;
    if (!m_database.open(QDir(base).filePath(QStringLiteral("library.db")), &error)) {
        setStatus(QStringLiteral("Database error: %1").arg(error));
        return false;
    }

    m_library = new Library(&m_database);
    m_library->seedSamples(sampleDirectory);
    refreshBooks();

    setStatus(m_library->ftsAvailable()
                  ? QStringLiteral("Local library ready · full-text search on")
                  : QStringLiteral("Local library ready · search running in fallback mode"));
    return true;
}

void AppController::refreshBooks()
{
    m_books.clear();
    if (m_library) {
        const QVector<Book> books = m_library->books();
        for (const Book &book : books)
            m_books.append(book.toVariant());
    }
    emit booksChanged();
}

void AppController::refreshAnnotations()
{
    m_annotations.clear();
    if (m_library && !m_currentBook.isEmpty()) {
        const int bookId = m_currentBook.value(QStringLiteral("id")).toInt();
        const QVector<Annotation> annotations = m_library->annotations(bookId);
        for (const Annotation &annotation : annotations)
            m_annotations.append(annotation.toVariant());
    }
    emit annotationsChanged();
}

void AppController::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void AppController::importBook(const QString &url)
{
    if (!m_library)
        return;
    const QUrl parsed(url);
    const QString path = parsed.isLocalFile() ? parsed.toLocalFile() : url;
    QString error;
    const int id = m_library->importBook(path, &error);
    if (id <= 0) {
        setStatus(QStringLiteral("Could not import: %1").arg(error));
        emit toast(error);
        return;
    }
    refreshBooks();
    setStatus(QStringLiteral("Imported %1").arg(QFileInfo(path).fileName()));
    openBook(id);
}

void AppController::seedSampleLibrary()
{
    if (!m_library)
        return;
    const QString sampleDir = qEnvironmentVariable("STACKS_SAMPLE_DIR");
    if (!sampleDir.isEmpty()) {
        m_library->seedSamples(sampleDir);
        refreshBooks();
    }
}

void AppController::openBook(int bookId)
{
    if (!m_library)
        return;
    m_positionTimer.stop();
    flushPosition();
    const Book book = m_library->book(bookId);
    if (book.id <= 0) {
        setStatus(QStringLiteral("Book not found."));
        return;
    }

    if (m_recording)
        stopVoiceNote(QString());
    m_currentBook = book.toVariant();
    m_readerOpen = true;

    QString locator = m_pendingLocator;
    if (locator.isEmpty())
        locator = book.locator;
    m_pendingLocator.clear();

    m_library->savePosition(bookId, locator, book.progress);
    refreshAnnotations();
    refreshDiagram();

    emit currentBookChanged();
    emit readerOpenChanged();
    emit readerRequested(bookId, locator);
    setStatus(QStringLiteral("Reading %1").arg(book.title));
}

void AppController::closeBook()
{
    if (m_recording)
        stopVoiceNote(QString());
    m_positionTimer.stop();
    flushPosition();
    m_readerOpen = false;
    m_currentBook.clear();
    m_annotations.clear();
    m_searchResults.clear();
    m_diagramNodes.clear();
    m_diagramEdges.clear();
    refreshBooks();
    emit currentBookChanged();
    emit annotationsChanged();
    emit searchResultsChanged();
    emit diagramChanged();
    emit readerClosed();
    emit readerOpenChanged();
}

void AppController::savePosition(const QString &locator, double progress)
{
    if (!m_library || m_currentBook.isEmpty())
        return;
    m_pendingPositionLocator = locator;
    m_pendingPositionBookId = m_currentBook.value(QStringLiteral("id")).toInt();
    m_pendingProgress = progress;
    m_currentBook.insert(QStringLiteral("locator"), locator);
    m_currentBook.insert(QStringLiteral("progress"), progress);
    emit currentBookChanged();
    m_positionTimer.start();
}

void AppController::flushPosition()
{
    if (!m_library)
        return;
    const int bookId = m_pendingPositionBookId;
    if (bookId <= 0 || m_pendingPositionLocator.isEmpty())
        return;
    if (m_library->savePosition(bookId, m_pendingPositionLocator, m_pendingProgress)) {
        m_pendingPositionBookId = 0;
        m_pendingPositionLocator.clear();
    }
}

int AppController::pageForLocator(const QString &locator) const
{
    if (locator.startsWith(QStringLiteral("pdf:"))) {
        const QStringList parts = locator.split(QLatin1Char(':'), Qt::SkipEmptyParts);
        if (parts.size() >= 2)
            return parts.at(1).toInt();
    }
    if (locator.startsWith(QStringLiteral("epub:"))) {
        const QString payload = locator.mid(5);
        const QStringList parts = payload.split(QLatin1Char(':'), Qt::SkipEmptyParts);
        if (!parts.isEmpty())
            return parts.first().toInt();
    }
    return 0;
}

void AppController::addHighlight(const QString &quote, const QString &locator, int page)
{
    if (!m_library || m_currentBook.isEmpty())
        return;
    const int bookId = m_currentBook.value(QStringLiteral("id")).toInt();
    const int resolvedPage = page > 0 ? page : pageForLocator(locator);
    m_library->addAnnotation(bookId, QStringLiteral("highlight"), locator, quote, QString(), resolvedPage);
    refreshAnnotations();
    setStatus(QStringLiteral("Highlight saved"));
}

void AppController::addBookmark(const QString &locator, int page)
{
    if (!m_library || m_currentBook.isEmpty())
        return;
    const int bookId = m_currentBook.value(QStringLiteral("id")).toInt();
    const int resolvedPage = page > 0 ? page : pageForLocator(locator);
    m_library->addAnnotation(bookId, QStringLiteral("bookmark"), locator, QString(), QString(), resolvedPage);
    refreshAnnotations();
    setStatus(QStringLiteral("Bookmark saved"));
}

void AppController::addNote(const QString &note, const QString &quote, const QString &locator, int page)
{
    if (!m_library || m_currentBook.isEmpty())
        return;
    const int bookId = m_currentBook.value(QStringLiteral("id")).toInt();
    const int resolvedPage = page > 0 ? page : pageForLocator(locator);
    m_library->addAnnotation(bookId, QStringLiteral("note"), locator, quote, note, resolvedPage);
    refreshAnnotations();
    setStatus(QStringLiteral("Note saved"));
}

void AppController::updateAnnotation(int annotationId, const QString &note)
{
    if (!m_library)
        return;
    m_library->updateAnnotation(annotationId, note, QString());
    refreshAnnotations();
}

void AppController::deleteAnnotation(int annotationId)
{
    if (!m_library)
        return;
    m_library->deleteAnnotation(annotationId);
    refreshAnnotations();
    setStatus(QStringLiteral("Removed"));
}

void AppController::openAnnotation(int annotationId)
{
    for (const QVariant &value : m_annotations) {
        const QVariantMap annotation = value.toMap();
        if (annotation.value(QStringLiteral("id")).toInt() == annotationId) {
            const QString locator = annotation.value(QStringLiteral("locator")).toString();
            if (!locator.isEmpty())
                emit readerRequested(m_currentBook.value(QStringLiteral("id")).toInt(), locator);
            return;
        }
    }
}

bool AppController::startVoiceNote()
{
    if (!m_library || m_currentBook.isEmpty())
        return false;
    if (m_recording)
        return true;
    QString error;
    if (!m_voiceRecorder.start(voiceDirectory(), &error)) {
        setStatus(error);
        emit toast(error);
        return false;
    }
    setStatus(QStringLiteral("Recording voice note…"));
    return true;
}

void AppController::stopVoiceNote(const QString &transcript)
{
    if (!m_recording)
        return;
    const int duration = m_voiceRecorder.durationMs();
    const QString path = m_voiceRecorder.stop();
    m_recording = false;
    emit recordingChanged();
    if (path.isEmpty()) {
        setStatus(QStringLiteral("Voice note could not be saved"));
        return;
    }
    addVoiceNoteFile(path, duration, transcript);
}

void AppController::cancelVoiceNote()
{
    m_voiceRecorder.cancel();
    m_recording = false;
    emit recordingChanged();
    setStatus(QStringLiteral("Voice note discarded"));
}

int AppController::addVoiceNoteFile(const QString &path, int durationMs, const QString &transcript)
{
    if (!m_library || m_currentBook.isEmpty())
        return 0;
    const int bookId = m_currentBook.value(QStringLiteral("id")).toInt();
    const int annotationId = m_library->addAnnotation(
        bookId, QStringLiteral("voice"), m_currentBook.value(QStringLiteral("locator")).toString(),
        QString(), transcript, pageForLocator(m_currentBook.value(QStringLiteral("locator")).toString()));
    if (annotationId <= 0)
        return 0;
    m_library->addVoiceNote(bookId, annotationId, path, durationMs, transcript);
    refreshAnnotations();
    setStatus(QStringLiteral("Voice note saved"));
    return annotationId;
}

void AppController::search(const QString &query)
{
    if (!m_library) {
        return;
    }
    m_searchResults.clear();
    const QVector<SearchResult> results = m_library->search(query.trimmed(), 40);
    for (const SearchResult &result : results)
        m_searchResults.append(result.toVariant());
    emit searchResultsChanged();
}

void AppController::clearSearch()
{
    m_searchResults.clear();
    emit searchResultsChanged();
}

void AppController::openSearchResult(int row)
{
    if (row < 0 || row >= m_searchResults.size())
        return;
    const QVariantMap result = m_searchResults.at(row).toMap();
    m_pendingLocator = result.value(QStringLiteral("locator")).toString();
    openBook(result.value(QStringLiteral("bookId")).toInt());
    setStatus(QStringLiteral("Jumped to “%1”").arg(result.value(QStringLiteral("bookTitle")).toString()));
}

// ── Send to Kindle ──────────────────────────────────────────────────────────

QString AppController::kindleDirectory() const
{
    return QDir(m_dataDirectory).filePath(QStringLiteral("kindle"));
}

void AppController::loadKindleSettings()
{
    m_kindleSettings.insert("fromAddress", QString());
    m_kindleSettings.insert("smtpHost", QStringLiteral("smtp.gmail.com"));
    m_kindleSettings.insert("smtpPort", 587);
    m_kindleSettings.insert("user", QString());
    m_kindleSettings.insert("password", QString());
    m_kindleSettings.insert("kindleAddress", QString());

    QFile file(QDir(m_dataDirectory).filePath(QStringLiteral("kindle.json")));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    m_kindleSettings.insert("fromAddress", object.value("fromAddress").toString());
    m_kindleSettings.insert("smtpHost", object.value("smtpHost").toString().isEmpty()
                                          ? QStringLiteral("smtp.gmail.com")
                                          : object.value("smtpHost").toString());
    m_kindleSettings.insert("smtpPort", object.value("smtpPort").toInt(587));
    m_kindleSettings.insert("user", object.value("user").toString());
    m_kindleSettings.insert("password", object.value("password").toString());
    m_kindleSettings.insert("kindleAddress", object.value("kindleAddress").toString());
}

void AppController::saveKindleSettings(const QVariantMap &settings)
{
    QJsonObject object;
    object.insert("fromAddress", settings.value("fromAddress").toString());
    object.insert("smtpHost", settings.value("smtpHost", QStringLiteral("smtp.gmail.com")).toString());
    object.insert("smtpPort", settings.value("smtpPort", 587).toInt());
    object.insert("user", settings.value("user").toString());
    object.insert("kindleAddress", settings.value("kindleAddress").toString());

    // An empty password field keeps the one already stored.
    const QString password = settings.value("password").toString().isEmpty()
                                 ? m_kindleSettings.value("password").toString()
                                 : settings.value("password").toString();
    if (!password.isEmpty())
        object.insert("password", password);

    const QString path = QDir(m_dataDirectory).filePath(QStringLiteral("kindle.json"));
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        file.close();
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    }

    loadKindleSettings();
    emit kindleChanged();
    emit toast(QStringLiteral("Kindle settings saved on this machine."));
}

void AppController::startConversion(const QString &path, const QString &format,
                                    const QString &title, const QString &author)
{
    if (m_kindleBusy) {
        emit toast(QStringLiteral("Wait for the current conversion to finish."));
        return;
    }

    m_kindleBusy = true;
    m_kindleResults.clear();
    m_kindleStatus = QStringLiteral("Converting %1…").arg(QFileInfo(path).fileName());
    emit kindleChanged();

    const QString outputDirectory = kindleDirectory();
    if (m_conversionWorker) {
        m_conversionWorker->wait();
        delete m_conversionWorker;
    }
    const auto result = std::make_shared<KindleConverter::Result>();
    m_conversionWorker = QThread::create([path, format, title, author, outputDirectory, result]() {
        *result = KindleConverter::convert(path, format, title, author, outputDirectory);
    });
    connect(m_conversionWorker, &QThread::finished, this,
            [this, result]() { onConversionFinished(*result); });
    m_conversionWorker->start();
}

void AppController::onConversionFinished(const KindleConverter::Result &result)
{
    m_kindleBusy = false;

    QVariantMap entry;
    entry.insert("ok", result.ok);
    entry.insert("error", result.error);
    entry.insert("path", result.path);
    entry.insert("fileName", result.path.isEmpty() ? QString() : QFileInfo(result.path).fileName());
    entry.insert("title", result.title);
    entry.insert("author", result.author);
    entry.insert("format", result.sourceFormat);
    entry.insert("sourcePath", result.sourcePath);
    entry.insert("pages", result.pages);
    entry.insert("words", result.words);
    entry.insert("bytes", result.bytes);

    m_kindleResults.append(entry);
    m_kindleStatus = result.ok ? QStringLiteral("%1 is ready to send.").arg(entry.value("fileName").toString())
                                : result.error;
    emit kindleChanged();
    emit toast(m_kindleStatus);
}

void AppController::convertBookForKindle(int bookId)
{
    for (const QVariant &value : m_books) {
        const QVariantMap book = value.toMap();
        if (book.value("id").toInt() != bookId)
            continue;
        startConversion(book.value("path").toString(), book.value("format").toString(),
                        book.value("title").toString(), book.value("author").toString());
        return;
    }
}

void AppController::convertFileForKindle(const QString &url)
{
    if (m_kindleBusy) {
        emit toast(QStringLiteral("Wait for the current transfer to finish."));
        return;
    }
    const QString path = url.startsWith(QLatin1String("file:"))
                             ? QUrl(url).toLocalFile()
                             : url;
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        emit toast(QStringLiteral("That file could not be found."));
        return;
    }

    const QString suffix = QFileInfo(path).suffix().toLower();
    QString format = suffix;
    if (suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")
        || suffix == QStringLiteral("txt") || suffix == QStringLiteral("htm")
        || suffix == QStringLiteral("html")) {
        format = QStringLiteral("txt");
    } else if (suffix == QStringLiteral("pdf")) {
        format = QStringLiteral("pdf");
    } else if (suffix == QStringLiteral("epub")) {
        format = QStringLiteral("epub");
    } else {
        // Amazon also accepts DOC, DOCX, RTF and images as they are.
        emit toast(QStringLiteral("%1 is sent to Amazon as it is — no conversion needed.").arg(suffix.toUpper()));
        m_kindleResults = {QVariantMap{
            {QStringLiteral("ok"), true},
            {QStringLiteral("asIs"), true},
            {QStringLiteral("path"), path},
            {QStringLiteral("sourcePath"), path},
            {QStringLiteral("fileName"), QFileInfo(path).fileName()},
            {QStringLiteral("title"), QFileInfo(path).completeBaseName()},
            {QStringLiteral("author"), QString()},
            {QStringLiteral("format"), suffix},
            {QStringLiteral("bytes"), QFileInfo(path).size()},
        }};
        m_kindleStatus = QStringLiteral("%1 is ready to send.").arg(QFileInfo(path).fileName());
        emit kindleChanged();
        return;
    }

    startConversion(path, format, QFileInfo(path).completeBaseName(), QString());
}

void AppController::sendConvertedToKindle(const QString &path, const QString &title, const QString &author)
{
    if (m_kindleBusy) {
        emit toast(QStringLiteral("Wait for the current transfer to finish."));
        return;
    }
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        emit toast(QStringLiteral("That file is missing."));
        return;
    }

    KindleSender::Settings settings;
    settings.fromAddress = m_kindleSettings.value("fromAddress").toString();
    settings.smtpHost = m_kindleSettings.value("smtpHost").toString();
    settings.smtpPort = m_kindleSettings.value("smtpPort", 587).toInt();
    settings.user = m_kindleSettings.value("user").toString();
    settings.password = m_kindleSettings.value("password").toString();
    settings.kindleAddress = m_kindleSettings.value("kindleAddress").toString();

    m_kindleBusy = true;
    m_kindleStatus = QStringLiteral("Sending to %1…").arg(settings.kindleAddress);
    emit kindleChanged();
    m_kindleSender->send(settings, path, title, author);
}

void AppController::revealInFolder(const QString &path)
{
    if (path.isEmpty())
        return;
    const QString directory = QFileInfo(path).absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

void AppController::openSendToKindle()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.amazon.com/sendtokindle")));
}

void AppController::clearKindleResults()
{
    m_kindleResults.clear();
    m_kindleStatus.clear();
    emit kindleChanged();
}

void AppController::refreshDiagram()
{
    m_diagramNodes.clear();
    m_diagramEdges.clear();
    if (m_library && !m_currentBook.isEmpty()) {
        const int bookId = m_currentBook.value(QStringLiteral("id")).toInt();
        const QVector<DiagramNode> nodes = m_library->diagramNodes(bookId);
        for (const DiagramNode &node : nodes)
            m_diagramNodes.append(node.toVariant());
        const QVector<DiagramEdge> edges = m_library->diagramEdges();
        for (const DiagramEdge &edge : edges)
            m_diagramEdges.append(edge.toVariant());
    }
    emit diagramChanged();
}

int AppController::addDiagramNode(const QString &label, const QString &body, double x, double y)
{
    if (!m_library || m_currentBook.isEmpty())
        return 0;
    const int bookId = m_currentBook.value(QStringLiteral("id")).toInt();
    const int id = m_library->addDiagramNode(bookId, label, body, x, y);
    refreshDiagram();
    return id;
}

void AppController::moveDiagramNode(int nodeId, double x, double y)
{
    if (!m_library)
        return;
    m_library->moveDiagramNode(nodeId, x, y);
    for (QVariant &value : m_diagramNodes) {
        QVariantMap node = value.toMap();
        if (node.value(QStringLiteral("id")).toInt() == nodeId) {
            node.insert(QStringLiteral("x"), x);
            node.insert(QStringLiteral("y"), y);
            value = node;
            break;
        }
    }
    emit diagramChanged();
}

void AppController::updateDiagramNode(int nodeId, const QString &label, const QString &body)
{
    if (!m_library)
        return;
    m_library->updateDiagramNode(nodeId, label, body);
    refreshDiagram();
}

void AppController::deleteDiagramNode(int nodeId)
{
    if (!m_library)
        return;
    m_library->deleteDiagramNode(nodeId);
    refreshDiagram();
}

int AppController::addDiagramEdge(int fromNode, int toNode)
{
    if (!m_library)
        return 0;
    const int id = m_library->addDiagramEdge(fromNode, toNode);
    refreshDiagram();
    return id;
}

void AppController::deleteDiagramEdge(int edgeId)
{
    if (!m_library)
        return;
    m_library->deleteDiagramEdge(edgeId);
    refreshDiagram();
}

void AppController::requestDiagram()
{
    refreshDiagram();
    emit diagramRequested();
}

QString AppController::voiceDirectory() const
{
    return QDir(m_dataDirectory).filePath(QStringLiteral("voice"));
}

} // namespace stacks
