#include "AppController.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStyleHints>
#include <QThread>
#include <QStandardPaths>
#include <QUrl>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#  if QT_CONFIG(permissions)
#    include <QPermissions>
#    define STACKS_HAS_PERMISSIONS 1
#  endif
#endif

#include <memory>
#ifdef Q_OS_WIN
#  include <windows.h>
#  include <wincrypt.h>
#endif

namespace stacks {

namespace {

#ifdef STACKS_HAS_PERMISSIONS
const QString kMicrophoneDenied = QStringLiteral(
    "Stacks can’t use the microphone. Allow it in System Settings → Privacy & Security → Microphone.");
#endif

#ifdef Q_OS_WIN
// POSIX mode bits do not restrict Windows ACLs. Keep the credential bound to
// the current Windows account with DPAPI instead of writing a plaintext secret.
QString protectPassword(const QString &password)
{
    QByteArray bytes = password.toUtf8();
    DATA_BLOB input {static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE *>(bytes.data())};
    DATA_BLOB output {};
    if (!CryptProtectData(&input, L"Stacks SMTP password", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output))
        return {};
    const QByteArray protectedBytes(reinterpret_cast<const char *>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return QString::fromLatin1(protectedBytes.toBase64());
}

QString unprotectPassword(const QString &encoded)
{
    QByteArray bytes = QByteArray::fromBase64(encoded.toLatin1());
    DATA_BLOB input {static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE *>(bytes.data())};
    DATA_BLOB output {};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
        return {};
    const QString password = QString::fromUtf8(reinterpret_cast<const char *>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return password;
}
#endif

// Only recordings Stacks made itself may be deleted along with their note.
bool isInsideDirectory(const QString &path, const QString &directory)
{
    const QString file = QFileInfo(path).canonicalFilePath();
    const QString root = QFileInfo(directory).canonicalFilePath();
    return !file.isEmpty() && !root.isEmpty() && file.startsWith(root + QLatin1Char('/'));
}

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    updateSystemTheme();
    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        application->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                this, [this](Qt::ColorScheme scheme) {
                    if (scheme == Qt::ColorScheme::Unknown)
                        updateSystemTheme();
                    else
                        applySystemTheme(scheme == Qt::ColorScheme::Dark);
                });
#endif
    }
    m_positionTimer.setSingleShot(true);
    m_positionTimer.setInterval(450);
    // Scrolling reports positions many times a second. Publishing the book
    // once per pause keeps every pane from re-reading it on each wheel tick.
    connect(&m_positionTimer, &QTimer::timeout, this, [this]() {
        flushPosition();
        emit currentBookChanged();
    });
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
    m_library.reset();
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
    connect(&m_voiceRecorder, &VoiceRecorder::errorOccurred, this, &AppController::notify);
    connect(&m_voiceRecorder, &VoiceRecorder::captureEnded, this, [this](const QString &reason) {
        stopVoiceNote(QString());
        notify(reason);
    });
}

void AppController::notify(const QString &message)
{
    setStatus(message);
    emit toast(message);
}

bool AppController::initialize(const QString &sampleDirectory, const QString &dataDirectory)
{
    const QString base = dataDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : dataDirectory;
    m_dataDirectory = base;
    QDir().mkpath(base);
    QDir().mkpath(QDir(base).filePath(QStringLiteral("voice")));
    QDir().mkpath(QDir(base).filePath(QStringLiteral("kindle")));
    QSettings appearance(QDir(base).filePath(QStringLiteral("appearance.ini")), QSettings::IniFormat);
    const QString mode = appearance.value(QStringLiteral("appearance/mode"), QStringLiteral("system")).toString();
    m_themeMode = mode == QStringLiteral("light") || mode == QStringLiteral("dark") ? mode : QStringLiteral("system");
    updateSystemTheme();
    emit appearanceChanged();
    loadKindleSettings();

    QString error;
    m_library.reset();
    if (!m_database.open(QDir(base).filePath(QStringLiteral("library.db")), &error)) {
        setStatus(QStringLiteral("Database error: %1").arg(error));
        return false;
    }

    m_library = std::make_unique<Library>(&m_database);
    m_library->seedSamples(sampleDirectory);
    refreshBooks();

    setStatus(m_library->ftsAvailable()
                  ? QStringLiteral("Local library ready · full-text search on")
                  : QStringLiteral("Local library ready · search running in fallback mode"));
    return true;
}

void AppController::setThemeMode(const QString &mode)
{
    if (mode != QStringLiteral("system") && mode != QStringLiteral("light") && mode != QStringLiteral("dark"))
        return;
    if (mode == m_themeMode)
        return;
    if (!m_dataDirectory.isEmpty()) {
        QSettings settings(QDir(m_dataDirectory).filePath(QStringLiteral("appearance.ini")), QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/mode"), mode);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            emit toast(QStringLiteral("Could not save the appearance preference."));
            return;
        }
    }
    m_themeMode = mode;
    emit appearanceChanged();
}

bool AppController::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange)
        updateSystemTheme();
    return QObject::eventFilter(object, event);
}

void AppController::updateSystemTheme()
{
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return;
    bool dark = QGuiApplication::palette().color(QPalette::Window).lightnessF() < 0.5;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme != Qt::ColorScheme::Unknown)
        dark = scheme == Qt::ColorScheme::Dark;
#endif
    applySystemTheme(dark);
}

void AppController::applySystemTheme(bool dark)
{
    if (m_systemDark != dark) {
        m_systemDark = dark;
        emit appearanceChanged();
    }
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
    if (!QFileInfo(book.path).isFile()) {
        // Moved, renamed or on an unmounted drive: say so instead of opening
        // an empty reader.
        notify(QStringLiteral("“%1” is no longer at %2.")
                   .arg(book.title, QDir::toNativeSeparators(book.path)));
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
    QStringList recordings;
    if (m_library->deleteAnnotation(annotationId, &recordings)) {
        for (const QString &path : std::as_const(recordings)) {
            if (isInsideDirectory(path, voiceDirectory()))
                QFile::remove(path);
        }
    }
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
#ifdef STACKS_HAS_PERMISSIONS
    // Without access macOS records silence, so ask first and say why.
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        const QMicrophonePermission microphone;
        switch (qApp->checkPermission(microphone)) {
        case Qt::PermissionStatus::Undetermined:
            qApp->requestPermission(microphone, this, [this](const QPermission &permission) {
                notify(permission.status() == Qt::PermissionStatus::Granted
                           ? QStringLiteral("Microphone ready. Press record again to start.")
                           : kMicrophoneDenied);
            });
            return false;
        case Qt::PermissionStatus::Denied:
            notify(kMicrophoneDenied);
            return false;
        case Qt::PermissionStatus::Granted:
            break;
        }
    }
#endif
    QString error;
    if (!m_voiceRecorder.start(voiceDirectory(), &error))
        return false; // The recorder has already reported why.
    setStatus(QStringLiteral("Recording voice note…"));
    return true;
}

void AppController::stopVoiceNote(const QString &transcript)
{
    if (!m_recording)
        return;
    const QString path = m_voiceRecorder.stop();
    const int duration = m_voiceRecorder.durationMs();
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
    m_kindleSettings.insert("kindleAddress", QString());
    m_kindleSettings.insert("hasPassword", false);
    m_kindlePassword.clear();

    QFile file(QDir(m_dataDirectory).filePath(QStringLiteral("kindle.json")));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    const int port = object.value("smtpPort").toInt(587);
#ifdef Q_OS_WIN
    m_kindlePassword = object.contains("passwordProtected")
        ? unprotectPassword(object.value("passwordProtected").toString())
        : object.value("password").toString(); // Legacy files migrate on next save.
#else
    m_kindlePassword = object.value("password").toString();
#endif
    m_kindleSettings.insert("fromAddress", object.value("fromAddress").toString());
    m_kindleSettings.insert("smtpHost", object.value("smtpHost").toString().isEmpty()
                                          ? QStringLiteral("smtp.gmail.com")
                                          : object.value("smtpHost").toString());
    m_kindleSettings.insert("smtpPort", port >= 1 && port <= 65535 ? port : 587);
    m_kindleSettings.insert("user", object.value("user").toString());
    m_kindleSettings.insert("kindleAddress", object.value("kindleAddress").toString());
    m_kindleSettings.insert("hasPassword", !m_kindlePassword.isEmpty());
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
                                 ? m_kindlePassword
                                 : settings.value("password").toString();
    if (!password.isEmpty()) {
#ifdef Q_OS_WIN
        const QString protectedPassword = protectPassword(password);
        if (protectedPassword.isEmpty()) {
            emit toast(QStringLiteral("Windows could not protect the Kindle password. Settings were not saved."));
            return;
        }
        object.insert("passwordProtected", protectedPassword);
#else
        object.insert("password", password);
#endif
    }

    // Write atomically and owner-only from the first byte: a failed write
    // must not truncate the stored account, nor briefly expose the password.
    QSaveFile file(QDir(m_dataDirectory).filePath(QStringLiteral("kindle.json")));
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
    const bool saved = file.open(QIODevice::WriteOnly)
        && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
        && file.write(json) == json.size()
        && file.commit();
    if (!saved) {
        file.cancelWriting();
        emit toast(QStringLiteral("Could not save the Kindle settings: %1").arg(file.errorString()));
        return;
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
        || suffix == QStringLiteral("txt")) {
        format = QStringLiteral("txt");
    } else if (suffix == QStringLiteral("htm") || suffix == QStringLiteral("html")) {
        format = QStringLiteral("html");
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
    settings.password = m_kindlePassword;
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

void AppController::openLocalFile(const QString &path)
{
    // QUrl::fromLocalFile keeps '#', '%' and spaces in names intact, which
    // a hand-built "file://" string does not.
    if (path.isEmpty() || !QFileInfo(path).isFile()) {
        notify(QStringLiteral("That file is missing."));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
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
        const QVector<DiagramEdge> edges = m_library->diagramEdges(bookId);
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
    if (!m_library || !m_library->moveDiagramNode(nodeId, x, y))
        return;
    // The dragged card already sits at its new place, so update the cached
    // copy without a change signal: re-announcing the list would rebuild
    // every card on the canvas after each drag.
    for (QVariant &value : m_diagramNodes) {
        QVariantMap node = value.toMap();
        if (node.value(QStringLiteral("id")).toInt() == nodeId) {
            node.insert(QStringLiteral("x"), x);
            node.insert(QStringLiteral("y"), y);
            value = node;
            break;
        }
    }
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
