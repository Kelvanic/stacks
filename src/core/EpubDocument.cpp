#include "EpubDocument.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextDocument>
#include <QXmlStreamReader>

#include <zip.h>

namespace stacks {

namespace {

QByteArray readZipEntry(zip_t *archive, const QString &path)
{
    const QByteArray encoded = path.toUtf8();
    zip_stat_t stat {};
    if (zip_stat(archive, encoded.constData(), 0, &stat) != 0)
        return {};

    zip_file_t *file = zip_fopen(archive, encoded.constData(), 0);
    if (!file)
        return {};

    QByteArray data;
    data.resize(static_cast<int>(stat.size));
    const zip_int64_t read = zip_fread(file, data.data(), data.size());
    zip_fclose(file);
    if (read < 0)
        return {};
    data.resize(static_cast<int>(read));
    return data;
}

QString stripToFragment(const QString &xhtml)
{
    QString body = xhtml;

    // Extract <body>…</body>.
    static const QRegularExpression bodyRe(QStringLiteral("<body[^>]*>(.*)</body>"),
                                           QRegularExpression::DotMatchesEverythingOption
                                               | QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch bodyMatch = bodyRe.match(body);
    if (bodyMatch.hasMatch())
        body = bodyMatch.captured(1);

    body.remove(QRegularExpression(QStringLiteral("<!--.*?-->"),
                                   QRegularExpression::DotMatchesEverythingOption));
    body.remove(QRegularExpression(QStringLiteral("<script\\b[^>]*>.*?</script>"),
                                   QRegularExpression::DotMatchesEverythingOption
                                       | QRegularExpression::CaseInsensitiveOption));
    body.remove(QRegularExpression(QStringLiteral("<style\\b[^>]*>.*?</style>"),
                                   QRegularExpression::DotMatchesEverythingOption
                                       | QRegularExpression::CaseInsensitiveOption));
    body.remove(QRegularExpression(QStringLiteral("<img\\b[^>]*>"),
                                   QRegularExpression::CaseInsensitiveOption));
    body.remove(QRegularExpression(QStringLiteral("<svg\\b[^>]*>.*?</svg>"),
                                   QRegularExpression::DotMatchesEverythingOption
                                       | QRegularExpression::CaseInsensitiveOption));
    body.remove(QRegularExpression(QStringLiteral("<iframe\\b[^>]*>.*?</iframe>"),
                                   QRegularExpression::DotMatchesEverythingOption
                                       | QRegularExpression::CaseInsensitiveOption));
    body.remove(QRegularExpression(QStringLiteral("\\son[a-z]+\\s*=\\s*\"[^\"]*\""),
                                   QRegularExpression::CaseInsensitiveOption));
    body.remove(QRegularExpression(QStringLiteral("\\son[a-z]+\\s*=\\s*'[^']*'"),
                                   QRegularExpression::CaseInsensitiveOption));
    body.remove(QRegularExpression(QStringLiteral("<link\\b[^>]*>"),
                                   QRegularExpression::CaseInsensitiveOption));
    body.remove(QRegularExpression(QStringLiteral("<meta\\b[^>]*>"),
                                   QRegularExpression::CaseInsensitiveOption));
    return body.trimmed();
}

QString plainTextFromFragment(const QString &fragment)
{
    QTextDocument document;
    document.setHtml(fragment);
    QString text = document.toPlainText();
    text.replace(QChar(0x00a0), QLatin1Char(' '));
    return text.trimmed();
}

QString documentTitleFromXhtml(const QString &xhtml)
{
    static const QRegularExpression titleRe(QStringLiteral("<title[^>]*>(.*?)</title>"),
                                            QRegularExpression::DotMatchesEverythingOption
                                                | QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = titleRe.match(xhtml);
    if (match.hasMatch()) {
        QTextDocument document;
        document.setHtml(match.captured(1));
        return document.toPlainText().trimmed();
    }
    return {};
}

} // namespace

bool EpubDocument::load(const QString &path, QString *error)
{
    m_title.clear();
    m_author.clear();
    m_html.clear();
    m_plainText.clear();
    m_chapters.clear();
    m_passages.clear();
    m_lastError.clear();

    int errorCode = 0;
    zip_t *archive = zip_open(path.toUtf8().constData(), ZIP_RDONLY, &errorCode);
    if (!archive) {
        m_lastError = QStringLiteral("Could not open EPUB archive (%1)").arg(errorCode);
        if (error)
            *error = m_lastError;
        return false;
    }

    const QByteArray containerXml = readZipEntry(archive, QStringLiteral("META-INF/container.xml"));
    QXmlStreamReader container(containerXml);
    QString opfPath;
    while (!container.atEnd()) {
        container.readNext();
        if (container.isStartElement() && container.name() == QStringLiteral("rootfile"))
            opfPath = container.attributes().value(QStringLiteral("full-path")).toString();
    }
    if (opfPath.isEmpty()) {
        m_lastError = QStringLiteral("EPUB is missing META-INF/container.xml rootfile");
        zip_close(archive);
        if (error)
            *error = m_lastError;
        return false;
    }

    const QByteArray opfXml = readZipEntry(archive, opfPath);
    if (opfXml.isEmpty()) {
        m_lastError = QStringLiteral("EPUB package document is missing: %1").arg(opfPath);
        zip_close(archive);
        if (error)
            *error = m_lastError;
        return false;
    }

    const QString opfDir = QFileInfo(opfPath).path();
    QHash<QString, QString> manifestHref;
    QHash<QString, QString> manifestMediaType;
    QVector<QString> spine;

    QXmlStreamReader opf(opfXml);
    while (!opf.atEnd()) {
        opf.readNext();
        if (!opf.isStartElement())
            continue;
        const QString name = opf.name().toString();
        if (name == QStringLiteral("title") && m_title.isEmpty()) {
            m_title = opf.readElementText().trimmed();
        } else if (name == QStringLiteral("creator") && m_author.isEmpty()) {
            m_author = opf.readElementText().trimmed();
        } else if (name == QStringLiteral("item")) {
            const QString id = opf.attributes().value(QStringLiteral("id")).toString();
            const QString href = opf.attributes().value(QStringLiteral("href")).toString();
            const QString mediaType = opf.attributes().value(QStringLiteral("media-type")).toString();
            manifestHref.insert(id, href);
            manifestMediaType.insert(id, mediaType);
        } else if (name == QStringLiteral("itemref")) {
            const QString idref = opf.attributes().value(QStringLiteral("idref")).toString();
            const QString linear = opf.attributes().value(QStringLiteral("linear")).toString();
            if (linear.compare(QStringLiteral("no"), Qt::CaseInsensitive) != 0 && !idref.isEmpty())
                spine.append(idref);
        }
    }

    if (spine.isEmpty()) {
        for (auto it = manifestHref.constBegin(); it != manifestHref.constEnd(); ++it) {
            if (manifestMediaType.value(it.key()).contains(QStringLiteral("xhtml"))
                || it.value().endsWith(QStringLiteral(".html"))
                || it.value().endsWith(QStringLiteral(".xhtml")))
                spine.append(it.key());
        }
    }

    const auto resolve = [&](const QString &href) {
        const QString combined = opfDir.isEmpty() ? href : opfDir + QLatin1Char('/') + href;
        return QDir::cleanPath(combined);
    };

    int chapterIndex = 0;
    QStringList htmlParts;
    QStringList plainParts;
    for (const QString &idref : spine) {
        const QString zipPath = resolve(manifestHref.value(idref));
        const QByteArray data = readZipEntry(archive, zipPath);
        if (data.isEmpty())
            continue;

        const QString xhtml = QString::fromUtf8(data);
        const QString fragment = stripToFragment(xhtml);
        if (fragment.isEmpty())
            continue;

        QString chapterTitle = documentTitleFromXhtml(xhtml);
        if (chapterTitle.isEmpty())
            chapterTitle = QStringLiteral("Chapter %1").arg(chapterIndex + 1);

        EpubChapter chapter;
        chapter.id = idref;
        chapter.title = chapterTitle;
        chapter.html = fragment;
        chapter.plainText = plainTextFromFragment(fragment);

        QStringList paragraphs = chapter.plainText.split(QRegularExpression(QStringLiteral("\\n+")),
                                                         Qt::SkipEmptyParts);
        int paragraphIndex = 0;
        for (QString paragraph : paragraphs) {
            paragraph = paragraph.trimmed();
            if (paragraph.isEmpty())
                continue;
            Passage passage;
            passage.locator = QStringLiteral("epub:%1:%2").arg(chapterIndex).arg(paragraphIndex);
            passage.text = paragraph;
            passage.page = chapterIndex;
            m_passages.append(passage);
            ++paragraphIndex;
        }

        htmlParts.append(QStringLiteral("<section class=\"chapter\" data-chapter=\"%1\">%2</section>")
                             .arg(chapterIndex)
                             .arg(fragment));
        plainParts.append(chapter.plainText);
        m_chapters.append(chapter);
        ++chapterIndex;
    }

    zip_close(archive);

    if (m_chapters.isEmpty()) {
        m_lastError = QStringLiteral("EPUB contained no readable spine documents");
        if (error)
            *error = m_lastError;
        return false;
    }

    if (m_title.isEmpty())
        m_title = QFileInfo(path).completeBaseName();
    if (m_author.isEmpty())
        m_author = QStringLiteral("Unknown author");

    m_plainText = plainParts.join(QStringLiteral("\n\n"));

    const QString css = QStringLiteral(
        "body { color: #211C16; background: transparent; font-size: 23px; }"
        "h1, h2, h3 { color: #171310; font-weight: 600; }"
        "h1 { font-size: 38px; margin: 0 0 0.42em 0; }"
        "h2 { font-size: 30px; margin: 1.6em 0 0.5em 0; }"
        "h3 { font-size: 25px; margin: 1.3em 0 0.4em 0; }"
        "p { margin: 0 0 1.05em 0; line-height: 162%; }"
        "blockquote { margin: 1.5em 0 1.5em 1.2em; padding-left: 1.1em; "
        "             border-left: 2px solid #C9703F; color: #4A423A; font-style: italic; }"
        "em, i { font-style: italic; }"
        "strong, b { font-weight: 600; }"
        "a { color: #A9502A; text-decoration: none; }"
        "hr { border: none; height: 1px; background: rgba(32,28,22,0.14); margin: 2.4em 0; }"
        "section.chapter { margin-bottom: 4em; }");

    m_html = QStringLiteral(
                 "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                 "<style>%1</style></head><body>%2</body></html>")
                 .arg(css, htmlParts.join(QStringLiteral("\n")));

    return true;
}

} // namespace stacks
