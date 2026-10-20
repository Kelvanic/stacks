#include "KindleConverter.h"

#include "PdfDocument.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QUuid>
#include <QXmlStreamReader>

#include <zip.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace stacks {

namespace {

// ── Zip helpers ────────────────────────────────────────────────────────────

class ZipWriter
{
public:
    explicit ZipWriter(const QString &path)
    {
        int error = 0;
        m_archive = zip_open(path.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    }

    ~ZipWriter() { if (m_archive) zip_discard(m_archive); }

    bool isValid() const { return m_archive != nullptr; }

    bool add(const QString &name, const QByteArray &data, bool store = false)
    {
        if (!m_archive || !m_ok)
            return false;

        // libzip keeps a pointer to the buffer until the archive is closed,
        // so the bytes are copied onto the heap and handed over to libzip.
        void *buffer = std::malloc(static_cast<size_t>(data.size()) + 1);
        if (!buffer)
            return m_ok = false;
        if (!data.isEmpty())
            std::memcpy(buffer, data.constData(), static_cast<size_t>(data.size()));
        static_cast<char *>(buffer)[data.size()] = '\0';

        zip_source_t *source = zip_source_buffer(m_archive, buffer,
                                                 static_cast<zip_uint64_t>(data.size()), 1);
        if (!source) {
            std::free(buffer);
            return m_ok = false;
        }
        if (zip_file_add(m_archive, name.toUtf8().constData(), source,
                         ZIP_FL_OVERWRITE | ZIP_FL_ENC_GUESS) < 0) {
            zip_source_free(source);
            return m_ok = false;
        }
        if (store) {
            const zip_int64_t index = zip_name_locate(m_archive, name.toUtf8().constData(), 0);
            if (index < 0 || zip_set_file_compression(m_archive, zip_uint64_t(index), ZIP_CM_STORE, 0) != 0)
                return m_ok = false;
        }
        return true;
    }

    // Copies an entry from another archive without decompressing it.
    bool copyFrom(zip_t *source, const QString &name)
    {
        if (!m_archive || !source || !m_ok)
            return false;
        const zip_int64_t index = zip_name_locate(source, name.toUtf8().constData(), 0);
        if (index < 0)
            return m_ok = false;
#if LIBZIP_VERSION_MAJOR > 1 || (LIBZIP_VERSION_MAJOR == 1 && LIBZIP_VERSION_MINOR >= 12)
        zip_source_t *copied = zip_source_zip_file(m_archive, source, zip_uint64_t(index), 0, 0, -1, nullptr);
#else
        zip_source_t *copied = zip_source_zip(m_archive, source, zip_uint64_t(index), 0, 0, -1);
#endif
        if (!copied)
            return m_ok = false;
        if (zip_file_add(m_archive, name.toUtf8().constData(), copied,
                         ZIP_FL_OVERWRITE | ZIP_FL_ENC_GUESS) < 0) {
            zip_source_free(copied);
            return m_ok = false;
        }
        return true;
    }

    bool close()
    {
        if (m_archive) {
            if (!m_ok || zip_close(m_archive) != 0) {
                zip_discard(m_archive);
                m_ok = false;
            }
            m_archive = nullptr;
        }
        return m_ok;
    }

private:
    zip_t *m_archive = nullptr;
    bool m_ok = true;
};

// ── Text helpers ───────────────────────────────────────────────────────────

QString xmlEscape(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    out.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    out.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    out.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    return out;
}

// Turns the raw text of a page into paragraphs. Lines are re-flowed, words
// broken across a line end are joined, and blank lines separate paragraphs.
QStringList blocksFromText(const QString &text)
{
    QString normalised = text;
    normalised.remove(QRegularExpression(QStringLiteral("\\s+$"),
                                         QRegularExpression::MultilineOption));
    const QStringList lines = normalised.split(QLatin1Char('\n'));
    QStringList blocks;
    QString current;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i).trimmed();
        if (line.isEmpty()) {
            if (!current.isEmpty()) {
                blocks.append(current.trimmed());
                current.clear();
            }
            continue;
        }
        if (current.isEmpty()) {
            current = line;
            continue;
        }
        // A word split across lines is one word again.
        if (current.endsWith(QLatin1Char('-'))
            && line.at(0).isLower() && line.size() > 2) {
            current.chop(1);
            current += line;
        } else {
            current += QLatin1Char(' ');
            current += line;
        }
    }
    if (!current.isEmpty())
        blocks.append(current.trimmed());

    return blocks;
}

bool looksLikeHeading(const QString &block)
{
    if (block.isEmpty() || block.size() > 64)
        return false;
    if (block.contains(QLatin1Char('\n')))
        return false;
    if (block.endsWith(QLatin1Char('.')) || block.endsWith(QLatin1Char(','))
        || block.endsWith(QLatin1Char(';')) || block.endsWith(QLatin1Char(':'))
        || block.endsWith(QLatin1Char('"')))
        return false;
    if (block.count(QLatin1Char(' ')) > 8)
        return false;
    // Titles are either capitalised or shouted, never sentence case.
    const bool capitalised = block.at(0).isUpper();
    bool allCaps = true;
    for (const QChar &c : block) {
        if (c.isLetter() && !c.isUpper())
            allCaps = false;
    }
    return capitalised || allCaps;
}

int countWords(const QString &text)
{
    return text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
}

// ── EPUB package parts ─────────────────────────────────────────────────────

const char *kStyleSheet = R"CSS(
body { font-family: serif; line-height: 1.5; margin: 0 5%; }
h1 { font-size: 1.6em; margin: 1.2em 0 0.6em 0; page-break-after: avoid; }
h2 { font-size: 1.2em; margin: 1.1em 0 0.5em 0; page-break-after: avoid; }
p { margin: 0 0 0.9em 0; text-align: justify; }
blockquote { margin: 1em 1.4em; font-style: italic; }
)CSS";

QString containerXml()
{
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
               "  <rootfiles>\n"
               "    <rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
               "  </rootfiles>\n"
               "</container>\n");
}

QString opfDocument(const QString &title,
                     const QString &author,
                     const QString &identifier,
                     const QString &language,
                     const QStringList &manifestItems,
                     const QStringList &spineItems,
                     bool hasCover)
{
    QString manifest;
    for (const QString &item : manifestItems)
        manifest += QStringLiteral("    %1\n").arg(item);

    QString spine;
    for (const QString &item : spineItems)
        spine += QStringLiteral("    %1\n").arg(item);

    const QString stamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"bookid\">\n"
               "  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
               "    <dc:identifier id=\"bookid\">%1</dc:identifier>\n"
               "    <dc:title>%2</dc:title>\n"
               "    <dc:creator>%3</dc:creator>\n"
               "    <dc:language>%4</dc:language>\n"
               "    <dc:publisher>Converted with Stacks</dc:publisher>\n"
               "    <dc:date>%5</dc:date>\n"
               "    <meta property=\"dcterms:modified\">%5</meta>\n"
               "%6"
               "  </metadata>\n"
               "  <manifest>\n"
               "%7"
               "  </manifest>\n"
               "  <spine toc=\"ncx\">\n"
               "%8"
               "  </spine>\n"
               "</package>\n")
        .arg(xmlEscape(identifier), xmlEscape(title), xmlEscape(author), xmlEscape(language),
             stamp,
             hasCover ? QStringLiteral("    <meta name=\"cover\" content=\"cover-image\"/>\n") : QString(),
             manifest, spine);
}

QString navDocument(const QString &title, const QVector<KindleChapter> &chapters)
{
    QString items;
    for (int i = 0; i < chapters.size(); ++i) {
        items += QStringLiteral("        <li><a href=\"text/chapter-%1.xhtml\">%2</a></li>\n")
                     .arg(i + 1, 2, 10, QLatin1Char('0'))
                     .arg(xmlEscape(chapters.at(i).title));
    }
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<!DOCTYPE html>\n"
               "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">\n"
               "  <head>\n"
               "    <title>%1</title>\n"
               "    <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"/>\n"
               "  </head>\n"
               "  <body>\n"
               "    <nav epub:type=\"toc\" id=\"toc\">\n"
               "      <h1>Contents</h1>\n"
               "      <ol>\n"
               "%2"
               "      </ol>\n"
               "    </nav>\n"
               "  </body>\n"
               "</html>\n")
        .arg(xmlEscape(title), items);
}

QString ncxDocument(const QString &title, const QVector<KindleChapter> &chapters)
{
    QString points;
    for (int i = 0; i < chapters.size(); ++i) {
        points += QStringLiteral(
                      "    <navPoint id=\"navpoint-%1\" playOrder=\"%1\">\n"
                      "      <navLabel><text>%2</text></navLabel>\n"
                      "      <content src=\"text/chapter-%1.xhtml\"/>\n"
                      "    </navPoint>\n")
                      .arg(i + 1, 2, 10, QLatin1Char('0'))
                      .arg(xmlEscape(chapters.at(i).title));
    }
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\">\n"
               "  <head>\n"
               "    <meta name=\"dtb:uid\" content=\"urn:uuid:%1\"/>\n"
               "    <meta name=\"dtb:depth\" content=\"1\"/>\n"
               "    <meta name=\"dtb:totalPageCount\" content=\"0\"/>\n"
               "    <meta name=\"dtb:maxPageNumber\" content=\"0\"/>\n"
               "  </head>\n"
               "  <docTitle><text>%2</text></docTitle>\n"
               "  <navMap>\n"
               "%3"
               "  </navMap>\n"
               "</ncx>\n")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces), xmlEscape(title), points);
}

QString chapterDocument(const QString &title, const QString &body)
{
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<!DOCTYPE html>\n"
               "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
               "  <head>\n"
               "    <title>%1</title>\n"
               "    <link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\"/>\n"
               "  </head>\n"
               "  <body>\n"
               "%2"
               "  </body>\n"
               "</html>\n")
        .arg(xmlEscape(title), body);
}

// ── Source readers ─────────────────────────────────────────────────────────

bool readPdfChapters(const QString &path, QVector<KindleChapter> &chapters, int &pages, int &words)
{
    PdfDocument pdf;
    if (!pdf.load(path))
        return false;

    pages = pdf.pageCount();
    KindleChapter current;
    current.title = QStringLiteral("Beginning");

    for (int page = 0; page < pages; ++page) {
        const QStringList blocks = blocksFromText(pdf.pageText(page));
        for (const QString &block : blocks) {
            words += countWords(block);
            if (looksLikeHeading(block) && !current.body.isEmpty()) {
                chapters.append(current);
                current = KindleChapter();
                current.title = block;
                continue;
            }
            if (looksLikeHeading(block) && current.body.isEmpty()) {
                current.title = block;
                continue;
            }
            if (!current.body.isEmpty())
                current.body += QLatin1Char('\n');
            current.body += QStringLiteral("    <p>%1</p>").arg(xmlEscape(block));
        }
    }
    if (!current.body.isEmpty())
        chapters.append(current);

    if (chapters.isEmpty())
        chapters.append(KindleChapter{QStringLiteral("Beginning"), QString()});
    return true;
}

// Reduces an HTML page to the line-oriented text below: headings become "# "
// lines that start chapters, block ends become line breaks, tags are dropped
// and entities decoded, so the EPUB shows prose rather than escaped markup.
QString htmlToText(QString html)
{
    static const QRegularExpression dropped(
        QStringLiteral("<(script|style|head|svg|template)\\b[^>]*>.*?</\\1\\s*>|<!--.*?-->"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression heading(QStringLiteral("<h[1-3]\\b[^>]*>"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression breaks(
        QStringLiteral("<br\\b[^>]*>|</(p|div|li|h[1-6]|blockquote|pre|tr|section|article|header|footer)\\s*>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
    static const QRegularExpression entity(QStringLiteral("&(#[xX][0-9a-fA-F]{1,6}|#[0-9]{1,7}|[a-zA-Z]{2,8});"));
    static const QHash<QString, char32_t> named = {
        {QStringLiteral("amp"), U'&'}, {QStringLiteral("lt"), U'<'}, {QStringLiteral("gt"), U'>'},
        {QStringLiteral("quot"), U'"'}, {QStringLiteral("apos"), U'\''}, {QStringLiteral("nbsp"), U' '},
        {QStringLiteral("mdash"), U'—'}, {QStringLiteral("ndash"), U'–'}, {QStringLiteral("hellip"), U'…'},
        {QStringLiteral("lsquo"), U'‘'}, {QStringLiteral("rsquo"), U'’'}, {QStringLiteral("ldquo"), U'“'},
        {QStringLiteral("rdquo"), U'”'}, {QStringLiteral("laquo"), U'«'}, {QStringLiteral("raquo"), U'»'},
        {QStringLiteral("bull"), U'•'}, {QStringLiteral("middot"), U'·'}, {QStringLiteral("copy"), U'©'},
        {QStringLiteral("reg"), U'®'}, {QStringLiteral("trade"), U'™'}, {QStringLiteral("deg"), U'°'},
        {QStringLiteral("eacute"), U'é'}, {QStringLiteral("egrave"), U'è'}, {QStringLiteral("agrave"), U'à'},
        {QStringLiteral("ccedil"), U'ç'}, {QStringLiteral("auml"), U'ä'}, {QStringLiteral("ouml"), U'ö'},
        {QStringLiteral("uuml"), U'ü'}, {QStringLiteral("szlig"), U'ß'}};

    html.remove(dropped);
    html.replace(heading, QStringLiteral("\n# "));
    html.replace(breaks, QStringLiteral("\n"));
    html.remove(tags);

    QString text;
    text.reserve(html.size());
    qsizetype last = 0;
    QRegularExpressionMatchIterator matches = entity.globalMatch(html);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        text += QStringView(html).mid(last, match.capturedStart() - last);
        const QString name = match.captured(1);
        char32_t code = 0;
        if (name.startsWith(QLatin1String("#x"), Qt::CaseInsensitive))
            code = name.mid(2).toUInt(nullptr, 16);
        else if (name.startsWith(QLatin1Char('#')))
            code = name.mid(1).toUInt();
        else
            code = named.value(name, 0);
        if (code > 0 && code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF))
            text += QString::fromUcs4(&code, 1);
        else
            text += match.captured(0);
        last = match.capturedEnd();
    }
    text += QStringView(html).mid(last);
    return text;
}

bool readTextChapters(const QString &path, bool html, QVector<KindleChapter> &chapters, int &pages, int &words)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    const QString text = html ? htmlToText(stream.readAll()) : stream.readAll();
    file.close();

    KindleChapter current;
    current.title = QStringLiteral("Beginning");

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('#'))) {
            if (!current.body.isEmpty())
                chapters.append(current);
            current = KindleChapter();
            current.title = line.mid(1).trimmed();
            continue;
        }
        if (line.isEmpty())
            continue;
        words += countWords(line);
        if (!current.body.isEmpty())
            current.body += QLatin1Char('\n');
        current.body += QStringLiteral("    <p>%1</p>").arg(xmlEscape(line));
    }
    if (!current.body.isEmpty())
        chapters.append(current);
    pages = chapters.size();
    if (chapters.isEmpty())
        chapters.append(KindleChapter{QStringLiteral("Beginning"), QString()});
    return true;
}

QByteArray readZipData(zip_t *archive, const QString &path)
{
    zip_stat_t stat {};
    if (zip_stat(archive, path.toUtf8().constData(), 0, &stat) != 0 || stat.size > 64 * 1024 * 1024)
        return {};
    zip_file_t *file = zip_fopen(archive, path.toUtf8().constData(), 0);
    if (!file)
        return {};
    QByteArray data(static_cast<qsizetype>(stat.size), '\0');
    zip_uint64_t total = 0;
    while (total < stat.size) {
        const zip_int64_t count = zip_fread(file, data.data() + total, stat.size - total);
        if (count <= 0)
            break;
        total += zip_uint64_t(count);
    }
    zip_fclose(file);
    return total == stat.size ? data : QByteArray();
}

} // namespace

// ── Public API ─────────────────────────────────────────────────────────────

QString KindleConverter::suggestedFileName(const QString &title, const QString &author)
{
    QString name = title.trimmed().isEmpty() ? QStringLiteral("Untitled") : title.trimmed();
    if (!author.trimmed().isEmpty())
        name += QStringLiteral(" - ") + author.trimmed();

    // Fold accents (Café → Cafe), then keep a portable ASCII subset.
    QString cleaned;
    for (const QChar c : name.normalized(QString::NormalizationForm_KD)) {
        if (c.unicode() < 0x80 && (c.isLetterOrNumber() || c == QLatin1Char('_')
                                   || c == QLatin1Char('-') || c == QLatin1Char('.')))
            cleaned += c;
        else if (c.isSpace())
            cleaned += QLatin1Char('-');
    }
    while (cleaned.contains(QLatin1String("--")))
        cleaned.replace(QLatin1String("--"), QLatin1String("-"));
    // A title in another script must not become ".epub", a hidden file, nor
    // exceed what file systems allow.
    const auto trim = [&cleaned]() {
        while (cleaned.startsWith(QLatin1Char('.')) || cleaned.startsWith(QLatin1Char('-')))
            cleaned.remove(0, 1);
        while (cleaned.endsWith(QLatin1Char('.')) || cleaned.endsWith(QLatin1Char('-')))
            cleaned.chop(1);
    };
    trim();
    cleaned.truncate(120);
    trim();
    const bool readable = std::any_of(cleaned.cbegin(), cleaned.cend(),
                                      [](QChar c) { return c.isLetterOrNumber(); });
    return (readable ? cleaned : QStringLiteral("Untitled")) + QStringLiteral(".epub");
}

KindleConverter::Result KindleConverter::convert(const QString &sourcePath,
                                                  const QString &sourceFormat,
                                                  const QString &title,
                                                  const QString &author,
                                                  const QString &outputDir)
{
    Result result;
    result.sourceFormat = sourceFormat;
    result.sourcePath = sourcePath;
    result.title = title.trimmed().isEmpty() ? QFileInfo(sourcePath).completeBaseName() : title.trimmed();
    result.author = author.trimmed();

    if (!QFileInfo::exists(sourcePath)) {
        result.error = QStringLiteral("The source file no longer exists.");
        return result;
    }
    QDir().mkpath(outputDir);

    QString outPath = QDir(outputDir).filePath(suggestedFileName(result.title, result.author));
    if (QFileInfo::exists(outPath)) {
        outPath = QDir(outputDir).filePath(QFileInfo(outPath).completeBaseName()
            + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".epub"));
    }
    QVector<KindleChapter> chapters;

    const QString format = sourceFormat.toLower();
    if (format == QStringLiteral("pdf")) {
        if (!readPdfChapters(sourcePath, chapters, result.pages, result.words)) {
            result.error = QStringLiteral("This PDF could not be read.");
            return result;
        }
    } else if (format == QStringLiteral("epub")) {
        // An EPUB is already a Kindle format: normalise the package instead.
        int error = 0;
        zip_t *source = zip_open(sourcePath.toUtf8().constData(), ZIP_RDONLY, &error);
        if (!source) {
            result.error = QStringLiteral("This EPUB could not be opened.");
            return result;
        }

        const QByteArray container = readZipData(source, QStringLiteral("META-INF/container.xml"));

        QString opfPath;
        {
            QXmlStreamReader reader(container);
            while (!reader.atEnd()) {
                reader.readNext();
                if (reader.isStartElement() && reader.name() == QStringLiteral("rootfile"))
                    opfPath = reader.attributes().value(QStringLiteral("full-path")).toString();
            }
        }
        QDomDocument package;
        const QString opfNs = QStringLiteral("http://www.idpf.org/2007/opf");
        const QString dcNs = QStringLiteral("http://purl.org/dc/elements/1.1/");
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        const bool parsed = bool(package.setContent(readZipData(source, opfPath), QDomDocument::ParseOption::UseNamespaceProcessing));
#else
        const bool parsed = package.setContent(readZipData(source, opfPath), true);
#endif
        if (opfPath.isEmpty() || !parsed) {
            zip_close(source);
            result.error = QStringLiteral("This EPUB has no readable package document.");
            return result;
        }
        QDomElement manifest = package.elementsByTagNameNS(opfNs, QStringLiteral("manifest")).item(0).toElement();
        QDomElement spine = package.elementsByTagNameNS(opfNs, QStringLiteral("spine")).item(0).toElement();
        QDomElement metadata = package.elementsByTagNameNS(opfNs, QStringLiteral("metadata")).item(0).toElement();
        if (manifest.isNull() || spine.isNull() || metadata.isNull()) {
            zip_close(source);
            result.error = QStringLiteral("The EPUB package is missing its manifest, spine or metadata.");
            return result;
        }
        QSet<QString> ids;
        QHash<QString, QString> hrefs;
        bool hasNav = false;
        for (QDomElement item = manifest.firstChildElement(); !item.isNull(); item = item.nextSiblingElement()) {
            const QString id = item.attribute(QStringLiteral("id"));
            if (id.isEmpty() || ids.contains(id)) {
                zip_close(source);
                result.error = QStringLiteral("The EPUB contains missing or duplicate manifest identifiers.");
                return result;
            }
            ids.insert(id);
            hrefs.insert(id, item.attribute(QStringLiteral("href")));
            hasNav |= item.attribute(QStringLiteral("properties")).split(QLatin1Char(' '), Qt::SkipEmptyParts).contains(QStringLiteral("nav"));
        }
        QString navItems;
        for (QDomElement ref = spine.firstChildElement(); !ref.isNull(); ref = ref.nextSiblingElement()) {
            const QString href = hrefs.value(ref.attribute(QStringLiteral("idref")));
            if (href.isEmpty()) {
                zip_close(source);
                result.error = QStringLiteral("The EPUB spine references an unknown document.");
                return result;
            }
            ++result.pages;
            navItems += QStringLiteral("<li><a href=\"%1\">Section %2</a></li>").arg(xmlEscape(href)).arg(result.pages);
        }
        if (result.pages == 0) {
            zip_close(source);
            result.error = QStringLiteral("The EPUB has an empty spine.");
            return result;
        }
        // Preserve original metadata, identifiers, navigation, asset paths and spine attributes.
        package.documentElement().setAttribute(QStringLiteral("version"), QStringLiteral("3.0"));
        const auto setMetadata = [&](const QString &name, const QString &value) {
            QDomElement element = metadata.elementsByTagNameNS(dcNs, name).item(0).toElement();
            if (element.isNull()) {
                element = package.createElementNS(dcNs, QStringLiteral("dc:") + name);
                metadata.appendChild(element);
            }
            while (!element.firstChild().isNull()) element.removeChild(element.firstChild());
            element.appendChild(package.createTextNode(value));
        };
        setMetadata(QStringLiteral("title"), result.title);
        if (!result.author.isEmpty())
            setMetadata(QStringLiteral("creator"), result.author);
        else
            result.author = metadata.elementsByTagNameNS(dcNs, QStringLiteral("creator")).item(0).toElement().text();
        for (QDomElement meta = metadata.firstChildElement(); !meta.isNull();) {
            QDomElement next = meta.nextSiblingElement();
            if (meta.attribute(QStringLiteral("property")) == QStringLiteral("dcterms:modified"))
                metadata.removeChild(meta);
            meta = next;
        }
        QDomElement modified = package.createElementNS(opfNs, QStringLiteral("meta"));
        modified.setAttribute(QStringLiteral("property"), QStringLiteral("dcterms:modified"));
        modified.appendChild(package.createTextNode(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss'Z'"))));
        metadata.appendChild(modified);

        ZipWriter writer(outPath);
        if (!writer.isValid()) {
            zip_close(source);
            result.error = QStringLiteral("Could not write the converted EPUB.");
            return result;
        }
        writer.add(QStringLiteral("mimetype"), QByteArrayLiteral("application/epub+zip"), true);
        writer.add(QStringLiteral("META-INF/container.xml"),
                   container.isEmpty() ? containerXml().toUtf8() : container);

        // Copy the original documents and their assets verbatim.
        const zip_int64_t entries = zip_get_num_entries(source, 0);
        for (zip_int64_t i = 0; i < entries; ++i) {
            zip_stat_t entry {};
            if (zip_stat_index(source, i, 0, &entry) != 0)
                continue;
            const QString name = QString::fromUtf8(entry.name);
            if (name == QStringLiteral("mimetype") || name == opfPath || name == QStringLiteral("META-INF/container.xml"))
                continue;
            writer.copyFrom(source, name);
        }

        if (!hasNav) {
            QString navId = QStringLiteral("stacks-nav");
            while (ids.contains(navId)) navId += QLatin1Char('-');
            QString navName = QStringLiteral("stacks-nav.xhtml");
            const QDir opfDir(QFileInfo(opfPath).path());
            while (zip_name_locate(source, QDir::cleanPath(opfDir.filePath(navName)).toUtf8().constData(), 0) >= 0)
                navName.prepend(QLatin1Char('_'));
            QDomElement nav = package.createElementNS(opfNs, QStringLiteral("item"));
            nav.setAttribute(QStringLiteral("id"), navId);
            nav.setAttribute(QStringLiteral("href"), navName);
            nav.setAttribute(QStringLiteral("media-type"), QStringLiteral("application/xhtml+xml"));
            nav.setAttribute(QStringLiteral("properties"), QStringLiteral("nav"));
            manifest.appendChild(nav);
            writer.add(QDir::cleanPath(opfDir.filePath(navName)), QStringLiteral(
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\" "
                "xmlns:epub=\"http://www.idpf.org/2007/ops\"><head><title>Contents</title></head>"
                "<body><nav epub:type=\"toc\"><h1>Contents</h1><ol>%1</ol></nav></body></html>").arg(navItems).toUtf8());
        }
        writer.add(opfPath, package.toByteArray());
        const bool written = writer.close();
        zip_close(source);
        if (!written) {
            result.error = QStringLiteral("Could not finish writing the converted EPUB.");
            return result;
        }
        result.ok = true;
        result.path = outPath;
        result.bytes = QFileInfo(outPath).size();
        return result;
    } else {
        if (!readTextChapters(sourcePath, format == QStringLiteral("html"), chapters, result.pages, result.words)) {
            result.error = QStringLiteral("This file could not be read.");
            return result;
        }
    }

    // ── Write a fresh EPUB for PDF and text sources ──────────────────────
    ZipWriter writer(outPath);
    if (!writer.isValid()) {
        result.error = QStringLiteral("Could not write the converted EPUB.");
        return result;
    }

    writer.add(QStringLiteral("mimetype"), QByteArrayLiteral("application/epub+zip"), true);
    writer.add(QStringLiteral("META-INF/container.xml"), containerXml().toUtf8());
    writer.add(QStringLiteral("OEBPS/style.css"), QByteArray(kStyleSheet));

    // A cover: the first page of a PDF, or a plain title card otherwise.
    bool hasCover = false;
    if (format == QStringLiteral("pdf")) {
        PdfDocument pdf;
        if (pdf.load(sourcePath)) {
            const QImage cover = pdf.renderPage(0, 1.0);
            if (!cover.isNull()) {
                QByteArray png;
                QBuffer buffer(&png);
                buffer.open(QIODevice::WriteOnly);
                cover.save(&buffer, "PNG");
                buffer.close();
                if (!png.isEmpty()) {
                    writer.add(QStringLiteral("OEBPS/cover.png"), png);
                    writer.add(QStringLiteral("OEBPS/cover.xhtml"),
                               chapterDocument(QStringLiteral("Cover"),
                                               QStringLiteral("    <div><img src=\"cover.png\" alt=\"%1\"/></div>")
                                   .arg(xmlEscape(result.title)))
                                   .toUtf8());
                    hasCover = true;
                }
            }
        }
    }

    QStringList manifestLines;
    QStringList spineLines;
    if (hasCover) {
        manifestLines.append(QStringLiteral("<item id=\"cover-image\" href=\"cover.png\" media-type=\"image/png\" properties=\"cover-image\"/>"));
        manifestLines.append(QStringLiteral("<item id=\"cover\" href=\"cover.xhtml\" media-type=\"application/xhtml+xml\"/>"));
        spineLines.append(QStringLiteral("<itemref idref=\"cover\"/>"));
    }
    manifestLines.append(QStringLiteral("<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>"));
    manifestLines.append(QStringLiteral("<item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>"));
    manifestLines.append(QStringLiteral("<item id=\"css\" href=\"style.css\" media-type=\"text/css\"/>"));

    for (int i = 0; i < chapters.size(); ++i) {
        const QString name = QStringLiteral("OEBPS/text/chapter-%1.xhtml").arg(i + 1, 2, 10, QLatin1Char('0'));
        writer.add(name, chapterDocument(chapters.at(i).title, chapters.at(i).body).toUtf8());
        manifestLines.append(QStringLiteral("<item id=\"ch%1\" href=\"text/chapter-%1.xhtml\" media-type=\"application/xhtml+xml\"/>")
                                 .arg(i + 1, 2, 10, QLatin1Char('0')));
        spineLines.append(QStringLiteral("<itemref idref=\"ch%1\"/>").arg(i + 1, 2, 10, QLatin1Char('0')));
    }

    writer.add(QStringLiteral("OEBPS/nav.xhtml"), navDocument(result.title, chapters).toUtf8());
    writer.add(QStringLiteral("OEBPS/toc.ncx"), ncxDocument(result.title, chapters).toUtf8());
    writer.add(QStringLiteral("OEBPS/content.opf"),
               opfDocument(result.title, result.author,
                           QStringLiteral("urn:uuid:") + QUuid::createUuid().toString(QUuid::WithoutBraces),
                           QStringLiteral("en"), manifestLines, spineLines, hasCover)
                   .toUtf8());
    if (!writer.close()) {
        result.error = QStringLiteral("Could not finish writing the converted EPUB.");
        return result;
    }

    result.ok = true;
    result.path = outPath;
    result.bytes = QFileInfo(outPath).size();
    if (result.pages <= 0)
        result.pages = chapters.size();
    return result;
}

} // namespace stacks
