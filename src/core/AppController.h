#pragma once

#include "Database.h"
#include "KindleConverter.h"
#include "KindleSender.h"
#include "Library.h"
#include "VoiceRecorder.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

class QThread;

namespace stacks {

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList books READ books NOTIFY booksChanged)
    Q_PROPERTY(QVariantMap currentBook READ currentBook NOTIFY currentBookChanged)
    Q_PROPERTY(QVariantList annotations READ annotations NOTIFY annotationsChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(QVariantList diagramNodes READ diagramNodes NOTIFY diagramChanged)
    Q_PROPERTY(QVariantList diagramEdges READ diagramEdges NOTIFY diagramChanged)
    Q_PROPERTY(bool readerOpen READ readerOpen NOTIFY readerOpenChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(double recordingLevel READ recordingLevel NOTIFY recordingLevelChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString dataDirectory READ dataDirectory CONSTANT)
    Q_PROPERTY(bool ftsAvailable READ ftsAvailable CONSTANT)
    Q_PROPERTY(QVariantList kindleResults READ kindleResults NOTIFY kindleChanged)
    Q_PROPERTY(QVariantMap kindleSettings READ kindleSettings NOTIFY kindleChanged)
    Q_PROPERTY(bool kindleBusy READ kindleBusy NOTIFY kindleChanged)
    Q_PROPERTY(QString kindleStatus READ kindleStatus NOTIFY kindleChanged)
    Q_PROPERTY(QString kindleDirectory READ kindleDirectory NOTIFY kindleChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY appearanceChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY appearanceChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    bool initialize(const QString &sampleDirectory, const QString &dataDirectory = QString());

    QVariantList books() const { return m_books; }
    QVariantMap currentBook() const { return m_currentBook; }
    QVariantList annotations() const { return m_annotations; }
    QVariantList searchResults() const { return m_searchResults; }
    QVariantList diagramNodes() const { return m_diagramNodes; }
    QVariantList diagramEdges() const { return m_diagramEdges; }
    bool readerOpen() const { return m_readerOpen; }
    bool recording() const { return m_recording; }
    double recordingLevel() const { return m_recordingLevel; }
    QString status() const { return m_status; }
    QString dataDirectory() const { return m_dataDirectory; }
    bool ftsAvailable() const { return m_library && m_library->ftsAvailable(); }
    QString themeMode() const { return m_themeMode; }
    bool darkTheme() const { return m_themeMode == QStringLiteral("dark")
        || (m_themeMode == QStringLiteral("system") && m_systemDark); }
    Q_INVOKABLE void setThemeMode(const QString &mode);

    Q_INVOKABLE void importBook(const QString &url);
    Q_INVOKABLE void seedSampleLibrary();
    Q_INVOKABLE void openBook(int bookId);
    Q_INVOKABLE void closeBook();
    Q_INVOKABLE void savePosition(const QString &locator, double progress);

    Q_INVOKABLE void addHighlight(const QString &quote, const QString &locator, int page);
    Q_INVOKABLE void addBookmark(const QString &locator, int page);
    Q_INVOKABLE void addNote(const QString &note, const QString &quote, const QString &locator, int page);
    Q_INVOKABLE void updateAnnotation(int annotationId, const QString &note);
    Q_INVOKABLE void deleteAnnotation(int annotationId);
    Q_INVOKABLE void openAnnotation(int annotationId);

    Q_INVOKABLE bool startVoiceNote();
    Q_INVOKABLE void stopVoiceNote(const QString &transcript);
    Q_INVOKABLE void cancelVoiceNote();
    Q_INVOKABLE int addVoiceNoteFile(const QString &path, int durationMs, const QString &transcript);

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void openSearchResult(int row);

    // ── Send to Kindle ────────────────────────────────────────────────────
    Q_INVOKABLE void convertBookForKindle(int bookId);
    void loadKindleSettings();
    void startConversion(const QString &path, const QString &format,
                         const QString &title, const QString &author);
    void onConversionFinished(const KindleConverter::Result &result);
    Q_INVOKABLE void convertFileForKindle(const QString &url);
    Q_INVOKABLE void sendConvertedToKindle(const QString &path, const QString &title, const QString &author);
    Q_INVOKABLE void saveKindleSettings(const QVariantMap &settings);
    Q_INVOKABLE void revealInFolder(const QString &path);
    Q_INVOKABLE void openLocalFile(const QString &path);
    Q_INVOKABLE void openSendToKindle();
    Q_INVOKABLE void clearKindleResults();

    Q_INVOKABLE void refreshDiagram();
    Q_INVOKABLE int addDiagramNode(const QString &label, const QString &body, double x, double y);
    Q_INVOKABLE void moveDiagramNode(int nodeId, double x, double y);
    Q_INVOKABLE void updateDiagramNode(int nodeId, const QString &label, const QString &body);
    Q_INVOKABLE void deleteDiagramNode(int nodeId);
    Q_INVOKABLE int addDiagramEdge(int fromNode, int toNode);
    Q_INVOKABLE void deleteDiagramEdge(int edgeId);
    Q_INVOKABLE void requestDiagram();

    Q_INVOKABLE QString voiceDirectory() const;

    QVariantList kindleResults() const { return m_kindleResults; }
    QVariantMap kindleSettings() const { return m_kindleSettings; }
    bool kindleBusy() const { return m_kindleBusy; }
    QString kindleStatus() const { return m_kindleStatus; }
    QString kindleDirectory() const;

signals:
    void booksChanged();
    void currentBookChanged();
    void annotationsChanged();
    void searchResultsChanged();
    void diagramChanged();
    void readerOpenChanged();
    void readerRequested(int bookId, const QString &locator);
    void readerClosed();
    void diagramRequested();
    void recordingChanged();
    void recordingLevelChanged();
    void statusChanged();
    void toast(const QString &message);
    void kindleChanged();
    void appearanceChanged();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void refreshBooks();
    void refreshAnnotations();
    void bindVoiceRecorder();
    void setStatus(const QString &status);
    int pageForLocator(const QString &locator) const;
    void flushPosition();
    void updateSystemTheme();
    void applySystemTheme(bool dark);
    void notify(const QString &message);

    Database m_database;
    std::unique_ptr<Library> m_library;

    QVariantList m_books;
    QVariantMap m_currentBook;
    QVariantList m_annotations;
    QVariantList m_searchResults;
    QVariantList m_diagramNodes;
    QVariantList m_diagramEdges;

    bool m_readerOpen = false;
    QString m_status;
    QString m_dataDirectory;
    QString m_pendingLocator;
    QString m_themeMode = QStringLiteral("system");
    bool m_systemDark = false;

    VoiceRecorder m_voiceRecorder;
    KindleSender *m_kindleSender = nullptr;
    QThread *m_conversionWorker = nullptr;
    QVariantList m_kindleResults;
    QVariantMap m_kindleSettings; // Exposed to QML, so it never carries the password.
    QString m_kindlePassword;
    bool m_kindleBusy = false;
    QString m_kindleStatus;
    bool m_recording = false;
    double m_recordingLevel = 0.0;

    QTimer m_positionTimer;
    int m_pendingPositionBookId = 0;
    QString m_pendingPositionLocator;
    double m_pendingProgress = 0.0;
};

} // namespace stacks
