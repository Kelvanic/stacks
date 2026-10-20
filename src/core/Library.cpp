#include "Library.h"

#include "EpubDocument.h"
#include "PdfDocument.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

namespace stacks {

namespace {

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QString escapeHtml(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return escaped;
}

QString highlightSnippet(const QString &raw, int radius = 140)
{
    QString text = escapeHtml(raw);
    // escapeHtml turned the FTS markers into &lt;mark&gt;. Restore them as styled HTML.
    text.replace(QStringLiteral("&lt;mark&gt;"), QStringLiteral("<b><font color=\"#C46A3B\">"));
    text.replace(QStringLiteral("&lt;/mark&gt;"), QStringLiteral("</font></b>"));
    if (text.size() > radius * 2)
        text = QStringLiteral("…") + text.right(radius * 2);
    else if (!raw.startsWith(QStringLiteral("…")))
        text.prepend(QStringLiteral("…"));
    return text + QStringLiteral("…");
}

QVector<Passage> splitIntoPassages(const QString &text, int page, const QString &prefix)
{
    QVector<Passage> result;
    const QStringList chunks = text.split(QRegularExpression(QStringLiteral("[\\n\\r]+")), Qt::SkipEmptyParts);
    int index = 0;
    for (QString chunk : chunks) {
        chunk = chunk.simplified();
        if (chunk.size() < 2)
            continue;
        Passage passage;
        passage.locator = QStringLiteral("%1:%2").arg(prefix).arg(index++);
        passage.text = chunk;
        passage.page = page;
        result.append(passage);
    }
    if (result.isEmpty() && !text.trimmed().isEmpty()) {
        Passage passage;
        passage.locator = QStringLiteral("%1:0").arg(prefix);
        passage.text = text.simplified();
        passage.page = page;
        result.append(passage);
    }
    return result;
}

} // namespace

Library::Library(Database *database)
    : m_database(database)
{
}

QString Library::accentFor(const QString &seed)
{
    static const QStringList palette = {
        QStringLiteral("#C46A3B"), QStringLiteral("#7E8F72"), QStringLiteral("#8B6FA8"),
        QStringLiteral("#B65C5C"), QStringLiteral("#4E7C8A"), QStringLiteral("#A9853B"),
        QStringLiteral("#7A7F5C"), QStringLiteral("#9B6B8F")
    };
    const QByteArray hash = QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Md5);
    const int index = static_cast<int>(static_cast<unsigned char>(hash.at(0))) % palette.size();
    return palette.at(index);
}

bool Library::seedSamples(const QString &sampleDir)
{
    QSqlQuery countQuery(m_database->db());
    if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM books")) || !countQuery.next())
        return false;
    if (countQuery.value(0).toInt() > 0)
        return true;

    const QDir dir(sampleDir);
    const QStringList files = dir.entryList({QStringLiteral("*.pdf"), QStringLiteral("*.epub")}, QDir::Files,
                                             QDir::Name);
    for (const QString &file : files)
        importBook(dir.absoluteFilePath(file));
    return true;
}

int Library::importBook(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        if (error)
            *error = QStringLiteral("File does not exist: %1").arg(path);
        return 0;
    }

    QSqlQuery existing(m_database->db());
    existing.prepare(QStringLiteral("SELECT id FROM books WHERE path = :path"));
    existing.bindValue(QStringLiteral(":path"), info.absoluteFilePath());
    if (existing.exec() && existing.next())
        return existing.value(0).toInt();

    const QString format = info.suffix().toLower();
    QString title = info.completeBaseName();
    QString author = QStringLiteral("Unknown author");
    int pageCount = 0;
    int wordCount = 0;

    if (format == QStringLiteral("pdf")) {
        PdfDocument pdf;
        QString pdfError;
        if (!pdf.load(info.absoluteFilePath(), &pdfError)) {
            if (error)
                *error = pdfError;
            return 0;
        }
        title = pdf.title().isEmpty() ? title : pdf.title();
        if (!pdf.author().isEmpty())
            author = pdf.author();
        pageCount = pdf.pageCount();
    } else if (format == QStringLiteral("epub")) {
        EpubDocument epub;
        QString epubError;
        if (!epub.load(info.absoluteFilePath(), &epubError)) {
            if (error)
                *error = epubError;
            return 0;
        }
        title = epub.title().isEmpty() ? title : epub.title();
        author = epub.author().isEmpty() ? author : epub.author();
        pageCount = epub.chapters().size();
    } else {
        if (error)
            *error = QStringLiteral("Unsupported format: %1").arg(format);
        return 0;
    }

    QSqlQuery insert(m_database->db());
    insert.prepare(QStringLiteral(
        "INSERT INTO books(title, author, format, path, progress, locator, page_count, word_count, accent, added_at, last_opened_at) "
        "VALUES (:title, :author, :format, :path, 0, '', :page_count, 0, :accent, :added, :opened)"));
    insert.bindValue(QStringLiteral(":title"), title);
    insert.bindValue(QStringLiteral(":author"), author);
    insert.bindValue(QStringLiteral(":format"), format);
    insert.bindValue(QStringLiteral(":path"), info.absoluteFilePath());
    insert.bindValue(QStringLiteral(":page_count"), pageCount);
    insert.bindValue(QStringLiteral(":accent"), accentFor(title + author));
    insert.bindValue(QStringLiteral(":added"), nowIso());
    insert.bindValue(QStringLiteral(":opened"), nowIso());
    if (!insert.exec()) {
        if (error)
            *error = insert.lastError().text();
        return 0;
    }

    const int bookId = insert.lastInsertId().toInt();

    if (format == QStringLiteral("pdf"))
        wordCount = indexPdf(bookId, info.absoluteFilePath());
    else
        wordCount = indexEpub(bookId, info.absoluteFilePath());

    QSqlQuery update(m_database->db());
    update.prepare(QStringLiteral("UPDATE books SET word_count = :words WHERE id = :id"));
    update.bindValue(QStringLiteral(":words"), wordCount);
    update.bindValue(QStringLiteral(":id"), bookId);
    update.exec();

    return bookId;
}

int Library::indexPdf(int bookId, const QString &path)
{
    PdfDocument pdf;
    if (!pdf.load(path))
        return 0;

    QVector<Passage> passages;
    int words = 0;
    for (int page = 0; page < pdf.pageCount(); ++page) {
        const QString text = pdf.pageText(page);
        if (text.trimmed().isEmpty())
            continue;
        const QVector<Passage> pagePassages = splitIntoPassages(text, page, QStringLiteral("pdf:%1").arg(page));
        passages += pagePassages;
        for (const Passage &passage : pagePassages)
            words += passage.text.count(QLatin1Char(' ')) + 1;
    }
    insertPassages(bookId, passages);
    return words;
}

int Library::indexEpub(int bookId, const QString &path)
{
    EpubDocument epub;
    if (!epub.load(path))
        return 0;

    int words = 0;
    const QVector<Passage> passages = epub.passages();
    for (const Passage &passage : passages)
        words += passage.text.count(QLatin1Char(' ')) + 1;
    insertPassages(bookId, passages);
    return words;
}

int Library::insertPassages(int bookId, const QVector<Passage> &passages)
{
    if (passages.isEmpty())
        return 0;

    QSqlDatabase db = m_database->db();
    db.transaction();

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO passages(book_id, page, locator, text) VALUES (:book, :page, :locator, :text)"));
    for (const Passage &passage : passages) {
        insert.bindValue(QStringLiteral(":book"), bookId);
        insert.bindValue(QStringLiteral(":page"), passage.page);
        insert.bindValue(QStringLiteral(":locator"), passage.locator);
        insert.bindValue(QStringLiteral(":text"), passage.text);
        if (!insert.exec())
            continue;
    }

    db.commit();
    return passages.size();
}

QVector<Book> Library::books() const
{
    QVector<Book> result;
    QSqlQuery query(m_database->db());
    if (!query.exec(QStringLiteral(
            "SELECT id, title, author, format, path, progress, locator, page_count, word_count, accent, added_at, last_opened_at "
            "FROM books ORDER BY last_opened_at DESC, added_at DESC")))
        return result;

    while (query.next()) {
        Book book;
        book.id = query.value(0).toInt();
        book.title = query.value(1).toString();
        book.author = query.value(2).toString();
        book.format = query.value(3).toString();
        book.path = query.value(4).toString();
        book.progress = query.value(5).toDouble();
        book.locator = query.value(6).toString();
        book.pageCount = query.value(7).toInt();
        book.wordCount = query.value(8).toInt();
        book.accent = query.value(9).toString();
        book.addedAt = QDateTime::fromString(query.value(10).toString(), Qt::ISODate);
        book.lastOpenedAt = QDateTime::fromString(query.value(11).toString(), Qt::ISODate);
        result.append(book);
    }
    return result;
}

Book Library::book(int id) const
{
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral(
        "SELECT id, title, author, format, path, progress, locator, page_count, word_count, accent, added_at, last_opened_at "
        "FROM books WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next())
        return {};

    Book book;
    book.id = query.value(0).toInt();
    book.title = query.value(1).toString();
    book.author = query.value(2).toString();
    book.format = query.value(3).toString();
    book.path = query.value(4).toString();
    book.progress = query.value(5).toDouble();
    book.locator = query.value(6).toString();
    book.pageCount = query.value(7).toInt();
    book.wordCount = query.value(8).toInt();
    book.accent = query.value(9).toString();
    book.addedAt = QDateTime::fromString(query.value(10).toString(), Qt::ISODate);
    book.lastOpenedAt = QDateTime::fromString(query.value(11).toString(), Qt::ISODate);
    return book;
}

bool Library::savePosition(int bookId, const QString &locator, double progress)
{
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral(
        "UPDATE books SET locator = :locator, progress = :progress, last_opened_at = :opened WHERE id = :id"));
    query.bindValue(QStringLiteral(":locator"), locator.isNull() ? QStringLiteral("") : locator);
    query.bindValue(QStringLiteral(":progress"), qBound(0.0, progress, 1.0));
    query.bindValue(QStringLiteral(":opened"), nowIso());
    query.bindValue(QStringLiteral(":id"), bookId);
    return query.exec();
}

QVector<Annotation> Library::annotations(int bookId) const
{
    QVector<Annotation> result;
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral(
        "SELECT a.id, a.book_id, a.kind, a.locator, a.quote, a.note, a.color, a.page, a.voice_id, "
        "       COALESCE(v.path, ''), COALESCE(v.duration_ms, 0), a.created_at "
        "FROM annotations a "
        "LEFT JOIN voice_notes v ON v.id = a.voice_id "
        "WHERE a.book_id = :book "
        "ORDER BY a.created_at DESC, a.id DESC"));
    query.bindValue(QStringLiteral(":book"), bookId);
    if (!query.exec())
        return result;

    while (query.next()) {
        Annotation annotation;
        annotation.id = query.value(0).toInt();
        annotation.bookId = query.value(1).toInt();
        annotation.kind = query.value(2).toString();
        annotation.locator = query.value(3).toString();
        annotation.quote = query.value(4).toString();
        annotation.note = query.value(5).toString();
        annotation.color = query.value(6).toString();
        annotation.page = query.value(7).toInt();
        annotation.voiceId = query.value(8).toInt();
        annotation.audioPath = query.value(9).toString();
        annotation.durationMs = query.value(10).toInt();
        annotation.createdAt = QDateTime::fromString(query.value(11).toString(), Qt::ISODate);
        result.append(annotation);
    }
    return result;
}

int Library::addAnnotation(int bookId, const QString &kind, const QString &locator,
                           const QString &quote, const QString &note, int page,
                           const QString &color)
{
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral(
        "INSERT INTO annotations(book_id, kind, locator, quote, note, color, page, created_at) "
        "VALUES (:book, :kind, :locator, :quote, :note, :color, :page, :created)"));
    query.bindValue(QStringLiteral(":book"), bookId);
    query.bindValue(QStringLiteral(":kind"), kind.isNull() ? QStringLiteral("") : kind);
    query.bindValue(QStringLiteral(":locator"), locator.isNull() ? QStringLiteral("") : locator);
    query.bindValue(QStringLiteral(":quote"), quote.isNull() ? QStringLiteral("") : quote);
    query.bindValue(QStringLiteral(":note"), note.isNull() ? QStringLiteral("") : note);
    query.bindValue(QStringLiteral(":color"), color.isNull() ? QStringLiteral("#C46A3B") : color);
    query.bindValue(QStringLiteral(":page"), page);
    query.bindValue(QStringLiteral(":created"), nowIso());
    if (!query.exec()) {
        qWarning() << "addAnnotation failed:" << query.lastError().text() << query.lastQuery();
        return 0;
    }
    return query.lastInsertId().toInt();
}

bool Library::updateAnnotation(int annotationId, const QString &note, const QString &quote)
{
    QSqlQuery query(m_database->db());
    if (!query.prepare(QStringLiteral(
            "UPDATE annotations SET note = :note, quote = :quote WHERE id = :id"))) {
        qWarning() << "updateAnnotation prepare failed:" << query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":note"), note.isNull() ? QStringLiteral("") : note);
    query.bindValue(QStringLiteral(":quote"), quote.isNull() ? QStringLiteral("") : quote);
    query.bindValue(QStringLiteral(":id"), annotationId);
    if (!query.exec()) {
        qWarning() << "updateAnnotation exec failed:" << query.lastError().text()
                   << query.lastQuery();
        return false;
    }
    return true;
}

bool Library::deleteAnnotation(int annotationId)
{
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral("DELETE FROM annotations WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), annotationId);
    return query.exec();
}

int Library::addVoiceNote(int bookId, int annotationId, const QString &path,
                          int durationMs, const QString &transcript)
{
    QSqlQuery voice(m_database->db());
    voice.prepare(QStringLiteral(
        "INSERT INTO voice_notes(book_id, annotation_id, path, duration_ms, transcript, created_at) "
        "VALUES (:book, :annotation, :path, :duration, :transcript, :created)"));
    voice.bindValue(QStringLiteral(":book"), bookId);
    voice.bindValue(QStringLiteral(":annotation"), annotationId);
    voice.bindValue(QStringLiteral(":path"), path.isNull() ? QStringLiteral("") : path);
    voice.bindValue(QStringLiteral(":duration"), durationMs);
    voice.bindValue(QStringLiteral(":transcript"),
                     transcript.isNull() ? QStringLiteral("") : transcript);
    voice.bindValue(QStringLiteral(":created"), nowIso());
    if (!voice.exec())
        return 0;

    const int voiceId = voice.lastInsertId().toInt();

    if (annotationId <= 0) {
        annotationId = addAnnotation(bookId, QStringLiteral("voice"), QString(), QString(),
                                     transcript, 0, QStringLiteral("#C46A3B"));
    }

    QSqlQuery link(m_database->db());
    link.prepare(QStringLiteral("UPDATE annotations SET voice_id = :voice WHERE id = :id"));
    link.bindValue(QStringLiteral(":voice"), voiceId);
    link.bindValue(QStringLiteral(":id"), annotationId);
    link.exec();

    return voiceId;
}

QVector<SearchResult> Library::search(const QString &queryText, int limit) const
{
    QVector<SearchResult> results;

    QStringList tokens = queryText.split(
        QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+"),
                           QRegularExpression::CaseInsensitiveOption),
        Qt::SkipEmptyParts);
    if (tokens.isEmpty())
        return results;

    QSqlQuery query(m_database->db());
    if (m_database->ftsAvailable()) {
        QStringList terms;
        for (const QString &token : tokens) {
            QString term = token;
            term.replace(QLatin1Char('"'), QStringLiteral("\"\""));
            terms.append(QStringLiteral("\"%1\"*").arg(term));
        }
        const QString match = terms.join(QStringLiteral(" AND "));

        query.prepare(QStringLiteral(
            "SELECT p.id, p.book_id, b.title, b.author, b.format, p.locator, p.page, "
            "       snippet(passages_fts, 0, '<mark>', '</mark>', '…', 14) AS snip, "
            "       bm25(passages_fts) AS rank "
            "FROM passages_fts "
            "JOIN passages p ON p.id = passages_fts.rowid "
            "JOIN books b ON b.id = p.book_id "
            "WHERE passages_fts MATCH :match "
            "ORDER BY rank LIMIT :limit"));
        query.bindValue(QStringLiteral(":match"), match);
        query.bindValue(QStringLiteral(":limit"), limit);
        if (!query.exec())
            return results;

        while (query.next()) {
            SearchResult result;
            result.passageId = query.value(0).toInt();
            result.bookId = query.value(1).toInt();
            result.bookTitle = query.value(2).toString();
            result.bookAuthor = query.value(3).toString();
            result.bookFormat = query.value(4).toString();
            result.locator = query.value(5).toString();
            result.page = query.value(6).toInt();
            result.snippet = highlightSnippet(query.value(7).toString());
            result.rank = query.value(8).toDouble();
            results.append(result);
        }
        return results;
    }

    // Fallback for SQLite builds without FTS5.
    const QString first = tokens.first();
    query.prepare(QStringLiteral(
        "SELECT p.id, p.book_id, b.title, b.author, b.format, p.locator, p.page, p.text "
        "FROM passages p JOIN books b ON b.id = p.book_id "
        "WHERE p.text LIKE :term ORDER BY p.id LIMIT :limit"));
    query.bindValue(QStringLiteral(":term"), QStringLiteral("%") + first + QStringLiteral("%"));
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return results;

    while (query.next()) {
        const QString text = query.value(7).toString();
        SearchResult result;
        result.passageId = query.value(0).toInt();
        result.bookId = query.value(1).toInt();
        result.bookTitle = query.value(2).toString();
        result.bookAuthor = query.value(3).toString();
        result.bookFormat = query.value(4).toString();
        result.locator = query.value(5).toString();
        result.page = query.value(6).toInt();
        const int index = text.indexOf(first, 0, Qt::CaseInsensitive);
        const int start = std::max(0, index - 60);
        result.snippet = escapeHtml(text.mid(start, 180));
        result.rank = 0;
        results.append(result);
    }
    return results;
}

QVector<DiagramNode> Library::diagramNodes(int bookId) const
{
    QVector<DiagramNode> result;
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral(
        "SELECT id, book_id, label, body, x, y, created_at FROM diagram_nodes "
        "WHERE book_id = :book OR book_id = 0 ORDER BY id"));
    query.bindValue(QStringLiteral(":book"), bookId);
    if (!query.exec())
        return result;
    while (query.next()) {
        DiagramNode node;
        node.id = query.value(0).toInt();
        node.bookId = query.value(1).toInt();
        node.label = query.value(2).toString();
        node.body = query.value(3).toString();
        node.x = query.value(4).toDouble();
        node.y = query.value(5).toDouble();
        node.createdAt = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);
        result.append(node);
    }
    return result;
}

int Library::addDiagramNode(int bookId, const QString &label, const QString &body, double x, double y)
{
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral(
        "INSERT INTO diagram_nodes(book_id, label, body, x, y, created_at) "
        "VALUES (:book, :label, :body, :x, :y, :created)"));
    query.bindValue(QStringLiteral(":book"), bookId);
    query.bindValue(QStringLiteral(":label"), label.isNull() ? QStringLiteral("") : label);
    query.bindValue(QStringLiteral(":body"), body.isNull() ? QStringLiteral("") : body);
    query.bindValue(QStringLiteral(":x"), x);
    query.bindValue(QStringLiteral(":y"), y);
    query.bindValue(QStringLiteral(":created"), nowIso());
    if (!query.exec())
        return 0;
    return query.lastInsertId().toInt();
}

bool Library::moveDiagramNode(int nodeId, double x, double y)
{
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral("UPDATE diagram_nodes SET x = :x, y = :y WHERE id = :id"));
    query.bindValue(QStringLiteral(":x"), x);
    query.bindValue(QStringLiteral(":y"), y);
    query.bindValue(QStringLiteral(":id"), nodeId);
    return query.exec();
}

bool Library::updateDiagramNode(int nodeId, const QString &label, const QString &body)
{
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral("UPDATE diagram_nodes SET label = :label, body = :body WHERE id = :id"));
    query.bindValue(QStringLiteral(":label"), label.isNull() ? QStringLiteral("") : label);
    query.bindValue(QStringLiteral(":body"), body.isNull() ? QStringLiteral("") : body);
    query.bindValue(QStringLiteral(":id"), nodeId);
    return query.exec();
}

bool Library::deleteDiagramNode(int nodeId)
{
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral("DELETE FROM diagram_nodes WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), nodeId);
    return query.exec();
}

QVector<DiagramEdge> Library::diagramEdges() const
{
    QVector<DiagramEdge> result;
    QSqlQuery query(m_database->db());
    if (!query.exec(QStringLiteral("SELECT id, from_node, to_node, label FROM diagram_edges ORDER BY id")))
        return result;
    while (query.next()) {
        DiagramEdge edge;
        edge.id = query.value(0).toInt();
        edge.fromNode = query.value(1).toInt();
        edge.toNode = query.value(2).toInt();
        edge.label = query.value(3).toString();
        result.append(edge);
    }
    return result;
}

int Library::addDiagramEdge(int fromNode, int toNode, const QString &label)
{
    if (fromNode <= 0 || toNode <= 0 || fromNode == toNode)
        return 0;
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral(
        "INSERT INTO diagram_edges(from_node, to_node, label) VALUES (:from, :to, :label)"));
    query.bindValue(QStringLiteral(":from"), fromNode);
    query.bindValue(QStringLiteral(":to"), toNode);
    query.bindValue(QStringLiteral(":label"), label.isNull() ? QStringLiteral("") : label);
    if (!query.exec()) {
        qWarning() << "addDiagramEdge failed:" << query.lastError().text();
        return 0;
    }
    return query.lastInsertId().toInt();
}

bool Library::deleteDiagramEdge(int edgeId)
{
    QSqlQuery query(m_database->db());
    query.prepare(QStringLiteral("DELETE FROM diagram_edges WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), edgeId);
    return query.exec();
}

} // namespace stacks
