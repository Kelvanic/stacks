#pragma once

#include "Book.h"
#include "Database.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace stacks {

class Library
{
public:
    explicit Library(Database *database);

    bool seedSamples(const QString &sampleDir);
    int importBook(const QString &path, QString *error = nullptr);

    QVector<Book> books() const;
    Book book(int id) const;
    bool savePosition(int bookId, const QString &locator, double progress);

    QVector<Annotation> annotations(int bookId) const;
    int addAnnotation(int bookId, const QString &kind, const QString &locator,
                      const QString &quote, const QString &note, int page = 0,
                      const QString &color = QStringLiteral("#C46A3B"));
    bool updateAnnotation(int annotationId, const QString &note, const QString &quote);
    // Also drops the note's voice recording rows; their audio file paths are
    // returned so the caller can remove the files it owns.
    bool deleteAnnotation(int annotationId, QStringList *recordingPaths = nullptr);

    int addVoiceNote(int bookId, int annotationId, const QString &path,
                     int durationMs, const QString &transcript);

    QVector<SearchResult> search(const QString &query, int limit = 40) const;

    QVector<DiagramNode> diagramNodes(int bookId) const;
    int addDiagramNode(int bookId, const QString &label, const QString &body, double x, double y);
    bool moveDiagramNode(int nodeId, double x, double y);
    bool updateDiagramNode(int nodeId, const QString &label, const QString &body);
    bool deleteDiagramNode(int nodeId);

    // Edges between the nodes visible for this book (its own and shared ones).
    QVector<DiagramEdge> diagramEdges(int bookId) const;
    // Connecting two already connected nodes returns the existing edge.
    int addDiagramEdge(int fromNode, int toNode, const QString &label = QString());
    bool deleteDiagramEdge(int edgeId);

    bool ftsAvailable() const { return m_database->ftsAvailable(); }

private:
    int indexPdf(int bookId, const QString &path);
    int indexEpub(int bookId, const QString &path);
    int insertPassages(int bookId, const QVector<Passage> &passages);
    static QString accentFor(const QString &seed);

    Database *m_database = nullptr;
};

} // namespace stacks
