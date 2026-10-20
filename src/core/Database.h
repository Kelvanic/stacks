#pragma once

#include <QSqlDatabase>
#include <QString>

namespace stacks {

class Database
{
public:
    Database();
    ~Database();

    bool open(const QString &filePath, QString *error = nullptr);
    void close();

    bool isOpen() const { return m_db.isValid() && m_db.isOpen(); }
    QSqlDatabase db() const { return m_db; }
    bool ftsAvailable() const { return m_ftsAvailable; }
    QString lastError() const { return m_lastError; }

private:
    bool migrate(QString *error);
    void setError(const QString &message);

    QString m_connectionName;
    QSqlDatabase m_db;
    bool m_ftsAvailable = false;
    QString m_lastError;
};

} // namespace stacks
