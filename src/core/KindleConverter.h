#pragma once

#include <QString>
#include <QVector>

namespace stacks {

struct KindleChapter {
    QString title;
    QString body; // XHTML body content
};

class KindleConverter
{
public:
    struct Result {
        bool ok = false;
        QString error;
        QString path;      // the EPUB that was written
        QString title;
        QString author;
        QString sourceFormat;
        int pages = 0;     // source pages (PDF) or chapters
        int words = 0;
        qint64 bytes = 0;
    };

    // Turns a PDF, EPUB, TXT, MD or HTML file into a Kindle-ready EPUB and
    // writes it into outputDir. The result is a clean EPUB 3 package with
    // fixed metadata, a navigation document and a cover — the shape Amazon's
    // Send to Kindle service expects.
    static Result convert(const QString &sourcePath,
                          const QString &sourceFormat,
                          const QString &title,
                          const QString &author,
                          const QString &outputDir);

    // A filename Amazon will show nicely in the library.
    static QString suggestedFileName(const QString &title, const QString &author);
};

} // namespace stacks
