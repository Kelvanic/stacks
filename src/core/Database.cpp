#include "Database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace stacks {

Database::Database()
    : m_connectionName(QStringLiteral("stacks-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

Database::~Database()
{
    close();
}

void Database::setError(const QString &message)
{
    m_lastError = message;
}

bool Database::open(const QString &filePath, QString *error)
{
    close();

    const QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        setError(QStringLiteral("Could not create data directory: %1").arg(info.absolutePath()));
        if (error)
            *error = m_lastError;
        return false;
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(filePath);
    if (!m_db.open()) {
        setError(m_db.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=4000"));

    if (!migrate(error))
        return false;

    m_lastError.clear();
    return true;
}

void Database::close()
{
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
    }
    if (QSqlDatabase::contains(m_connectionName))
        QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::migrate(QString *error)
{
    QSqlQuery query(m_db);

    const auto run = [&](const QString &sql) -> bool {
        if (!query.exec(sql)) {
            setError(QStringLiteral("%1\nSQL: %2").arg(query.lastError().text(), sql));
            if (error)
                *error = m_lastError;
            return false;
        }
        return true;
    };

    if (!run(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS meta ("
            "  key TEXT PRIMARY KEY,"
            "  value TEXT NOT NULL"
            ")")))
        return false;

    if (!run(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS books ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  title TEXT NOT NULL,"
            "  author TEXT NOT NULL DEFAULT '',"
            "  format TEXT NOT NULL,"
            "  path TEXT NOT NULL UNIQUE,"
            "  progress REAL NOT NULL DEFAULT 0,"
            "  locator TEXT NOT NULL DEFAULT '',"
            "  page_count INTEGER NOT NULL DEFAULT 0,"
            "  word_count INTEGER NOT NULL DEFAULT 0,"
            "  accent TEXT NOT NULL DEFAULT '#C46A3B',"
            "  added_at TEXT NOT NULL,"
            "  last_opened_at TEXT NOT NULL"
            ")")))
        return false;

    if (!run(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS passages ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  book_id INTEGER NOT NULL REFERENCES books(id) ON DELETE CASCADE,"
            "  page INTEGER NOT NULL DEFAULT 0,"
            "  locator TEXT NOT NULL,"
            "  text TEXT NOT NULL,"
            "  UNIQUE(book_id, locator)"
            ")")))
        return false;

    if (!run(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS annotations ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  book_id INTEGER NOT NULL REFERENCES books(id) ON DELETE CASCADE,"
            "  kind TEXT NOT NULL,"
            "  locator TEXT NOT NULL DEFAULT '',"
            "  quote TEXT NOT NULL DEFAULT '',"
            "  note TEXT NOT NULL DEFAULT '',"
            "  color TEXT NOT NULL DEFAULT '#C46A3B',"
            "  page INTEGER NOT NULL DEFAULT 0,"
            "  voice_id INTEGER NOT NULL DEFAULT 0,"
            "  created_at TEXT NOT NULL"
            ")")))
        return false;

    if (!run(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS voice_notes ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  book_id INTEGER NOT NULL REFERENCES books(id) ON DELETE CASCADE,"
            "  annotation_id INTEGER NOT NULL DEFAULT 0,"
            "  path TEXT NOT NULL,"
            "  duration_ms INTEGER NOT NULL DEFAULT 0,"
            "  transcript TEXT NOT NULL DEFAULT '',"
            "  created_at TEXT NOT NULL"
            ")")))
        return false;

    if (!run(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS diagram_nodes ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  book_id INTEGER NOT NULL DEFAULT 0,"
            "  label TEXT NOT NULL,"
            "  body TEXT NOT NULL DEFAULT '',"
            "  x REAL NOT NULL DEFAULT 0,"
            "  y REAL NOT NULL DEFAULT 0,"
            "  created_at TEXT NOT NULL"
            ")")))
        return false;

    if (!run(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS diagram_edges ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  from_node INTEGER NOT NULL REFERENCES diagram_nodes(id) ON DELETE CASCADE,"
            "  to_node INTEGER NOT NULL REFERENCES diagram_nodes(id) ON DELETE CASCADE,"
            "  label TEXT NOT NULL DEFAULT ''"
            ")")))
        return false;

    // FTS5 is optional at build time for some SQLite distributions. We still
    // provide a useful LIKE fallback if the virtual table cannot be created.
    query.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS passages_fts USING fts5("
        "  text, content='passages', content_rowid='id', tokenize='porter unicode61'"
        ")"));
    if (query.lastError().isValid()) {
        m_ftsAvailable = false;
    } else {
        m_ftsAvailable = true;
        run(QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS passages_ai AFTER INSERT ON passages BEGIN "
            "  INSERT INTO passages_fts(rowid, text) VALUES (new.id, new.text); "
            "END"));
        run(QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS passages_ad AFTER DELETE ON passages BEGIN "
            "  INSERT INTO passages_fts(passages_fts, rowid, text) VALUES ('delete', old.id, old.text); "
            "END"));
        run(QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS passages_au AFTER UPDATE ON passages BEGIN "
            "  INSERT INTO passages_fts(passages_fts, rowid, text) VALUES ('delete', old.id, old.text); "
            "  INSERT INTO passages_fts(rowid, text) VALUES (new.id, new.text); "
            "END"));

        QSqlQuery exists(m_db);
        exists.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE name = 'annotations_fts'"));
        const bool hadAnnotationIndex = exists.next();
        if (!run(QStringLiteral(
                "CREATE VIRTUAL TABLE IF NOT EXISTS annotations_fts USING fts5("
                "quote, note, transcript, tokenize='porter unicode61')")))
            return false;
        if (!run(QStringLiteral(
                "CREATE TRIGGER IF NOT EXISTS annotations_ai AFTER INSERT ON annotations BEGIN "
                "INSERT INTO annotations_fts(rowid, quote, note, transcript) "
                "VALUES (new.id, new.quote, new.note, COALESCE((SELECT transcript FROM voice_notes WHERE id = new.voice_id), '')); END"))
            || !run(QStringLiteral(
                "CREATE TRIGGER IF NOT EXISTS annotations_au AFTER UPDATE ON annotations BEGIN "
                "DELETE FROM annotations_fts WHERE rowid = old.id; "
                "INSERT INTO annotations_fts(rowid, quote, note, transcript) "
                "VALUES (new.id, new.quote, new.note, COALESCE((SELECT transcript FROM voice_notes WHERE id = new.voice_id), '')); END"))
            || !run(QStringLiteral(
                "CREATE TRIGGER IF NOT EXISTS annotations_ad AFTER DELETE ON annotations BEGIN "
                "DELETE FROM annotations_fts WHERE rowid = old.id; END"))
            || !run(QStringLiteral(
                "CREATE TRIGGER IF NOT EXISTS voice_notes_search_au AFTER UPDATE OF transcript ON voice_notes BEGIN "
                "UPDATE annotations_fts SET transcript = new.transcript "
                "WHERE rowid IN (SELECT id FROM annotations WHERE voice_id = new.id); END"))
            || !run(QStringLiteral(
                "CREATE TRIGGER IF NOT EXISTS voice_notes_search_ad AFTER DELETE ON voice_notes BEGIN "
                "UPDATE annotations_fts SET transcript = '' "
                "WHERE rowid IN (SELECT id FROM annotations WHERE voice_id = old.id); END")))
            return false;
        if (!hadAnnotationIndex && !run(QStringLiteral(
                "INSERT INTO annotations_fts(rowid, quote, note, transcript) "
                "SELECT a.id, a.quote, a.note, COALESCE(v.transcript, '') "
                "FROM annotations a LEFT JOIN voice_notes v ON v.id = a.voice_id")))
            return false;
    }

    run(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_books_last_opened ON books(last_opened_at DESC)"));
    run(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_passages_book ON passages(book_id)"));
    run(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_annotations_book ON annotations(book_id, created_at DESC)"));
    run(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_diagram_nodes_book ON diagram_nodes(book_id)"));
    run(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_diagram_edges_from ON diagram_edges(from_node)"));
    // Older builds could store the same connection twice, or once in each
    // direction. Keep the first of each pair and drop any edge whose node is gone.
    run(QStringLiteral(
        "DELETE FROM diagram_edges WHERE id NOT IN ("
        "  SELECT MIN(id) FROM diagram_edges GROUP BY MIN(from_node, to_node), MAX(from_node, to_node))"
        " OR from_node NOT IN (SELECT id FROM diagram_nodes)"
        " OR to_node NOT IN (SELECT id FROM diagram_nodes)"));

    if (!run(QStringLiteral(
            "INSERT OR REPLACE INTO meta(key, value) VALUES ('schema_version', '2')")))
        return false;

    return true;
}

} // namespace stacks
