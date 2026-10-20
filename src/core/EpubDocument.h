#pragma once

#include "Book.h"

#include <QString>
#include <QVector>

namespace stacks {

struct EpubChapter {
    QString id;
    QString title;
    QString html;
    QString plainText;
};

class EpubDocument
{
public:
    EpubDocument() = default;
    ~EpubDocument() = default;

    bool load(const QString &path, QString *error = nullptr);

    bool isValid() const { return !m_html.isEmpty(); }
    QString title() const { return m_title; }
    QString author() const { return m_author; }
    QString html() const { return m_html; }
    QString plainText() const { return m_plainText; }
    QVector<EpubChapter> chapters() const { return m_chapters; }
    QVector<Passage> passages() const { return m_passages; }

    QString lastError() const { return m_lastError; }

private:
    QString m_title;
    QString m_author;
    QString m_html;
    QString m_plainText;
    QVector<EpubChapter> m_chapters;
    QVector<Passage> m_passages;
    QString m_lastError;
};

} // namespace stacks
