#include "PdfDocument.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QVector>
#include <QtMath>

#include <fpdf_doc.h>
#include <fpdf_text.h>
#include <fpdfview.h>

#include <algorithm>
#include <mutex>

namespace stacks {

namespace {
int s_pdfiumRefCount = 0;
// PDFium requires process-wide serialization, even for different documents.
std::recursive_mutex s_pdfiumMutex;

QString pdfErrorText(unsigned long code)
{
    switch (code) {
    case FPDF_ERR_SUCCESS:
        return QStringLiteral("No error");
    case FPDF_ERR_UNKNOWN:
        return QStringLiteral("Unknown PDF error");
    case FPDF_ERR_FILE:
        return QStringLiteral("PDF file could not be opened");
    case FPDF_ERR_FORMAT:
        return QStringLiteral("Not a valid PDF file");
    case FPDF_ERR_PASSWORD:
        return QStringLiteral("This PDF is password-protected");
    case FPDF_ERR_SECURITY:
        return QStringLiteral("Unsupported PDF security scheme");
    case FPDF_ERR_PAGE:
        return QStringLiteral("PDF page not found");
    default:
        return QStringLiteral("PDF error %1").arg(code);
    }
}
} // namespace

PdfDocument::PdfDocument() = default;

PdfDocument::~PdfDocument()
{
    close();
}

void PdfDocument::globalInit()
{
    const std::lock_guard lock(s_pdfiumMutex);
    if (s_pdfiumRefCount++ == 0) {
        FPDF_LIBRARY_CONFIG config;
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);
    }
}

void PdfDocument::globalShutdown()
{
    const std::lock_guard lock(s_pdfiumMutex);
    if (s_pdfiumRefCount > 0 && --s_pdfiumRefCount == 0)
        FPDF_DestroyLibrary();
}

bool PdfDocument::load(const QString &path, QString *error)
{
    const std::lock_guard lock(s_pdfiumMutex);
    close();

    if (!QFileInfo::exists(path)) {
        m_lastError = QStringLiteral("File does not exist: %1").arg(path);
        if (error)
            *error = m_lastError;
        return false;
    }

    globalInit();
    m_libraryInitialized = true;
    m_document = FPDF_LoadDocument(path.toUtf8().constData(), nullptr);
    if (!m_document) {
        m_lastError = pdfErrorText(FPDF_GetLastError());
        if (error)
            *error = m_lastError;
        close();
        return false;
    }

    m_pageCount = FPDF_GetPageCount(static_cast<FPDF_DOCUMENT>(m_document));
    if (m_pageCount <= 0) {
        m_lastError = QStringLiteral("The PDF has no pages");
        if (error)
            *error = m_lastError;
        close();
        return false;
    }

    m_lastError.clear();
    return true;
}

void PdfDocument::close()
{
    const std::lock_guard lock(s_pdfiumMutex);
    if (m_document) {
        FPDF_CloseDocument(static_cast<FPDF_DOCUMENT>(m_document));
        m_document = nullptr;
    }
    m_pageCount = 0;
    if (m_libraryInitialized) {
        globalShutdown();
        m_libraryInitialized = false;
    }
}

QSizeF PdfDocument::pageSize(int pageIndex) const
{
    const std::lock_guard lock(s_pdfiumMutex);
    if (!m_document || pageIndex < 0 || pageIndex >= m_pageCount)
        return {};
    FS_SIZEF size {};
    if (!FPDF_GetPageSizeByIndexF(static_cast<FPDF_DOCUMENT>(m_document), pageIndex, &size))
        return {};
    return QSizeF(size.width, size.height);
}

QImage PdfDocument::renderPage(int pageIndex, double scale) const
{
    const std::lock_guard lock(s_pdfiumMutex);
    if (!m_document || pageIndex < 0 || pageIndex >= m_pageCount)
        return {};

    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(m_document), pageIndex);
    if (!page)
        return {};

    const QSizeF size = pageSize(pageIndex);
    const int width = std::max(1, qRound(size.width() * scale));
    const int height = std::max(1, qRound(size.height() * scale));

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::white);

    FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(width, height, FPDFBitmap_BGRA,
                                             image.bits(), image.bytesPerLine());
    if (!bitmap) {
        FPDF_ClosePage(page);
        return {};
    }

    FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, FPDF_ANNOT);
    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);

    return image;
}

QString PdfDocument::pageText(int pageIndex) const
{
    const std::lock_guard lock(s_pdfiumMutex);
    if (!m_document || pageIndex < 0 || pageIndex >= m_pageCount)
        return {};

    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(m_document), pageIndex);
    if (!page)
        return {};

    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        return {};
    }

    const int count = FPDFText_CountChars(textPage);
    QString text;
    if (count > 0) {
        QVector<unsigned short> buffer(count + 1, 0);
        const int written = FPDFText_GetText(textPage, 0, count, buffer.data());
        int length = written;
        if (length > 0 && buffer.at(length - 1) == 0)
            --length;
        text = QString::fromUtf16(reinterpret_cast<const char16_t *>(buffer.constData()), std::max(0, length));
    }

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return text;
}

QString PdfDocument::textInRect(int pageIndex, const QRectF &pageRect) const
{
    const std::lock_guard lock(s_pdfiumMutex);
    if (!m_document || pageIndex < 0 || pageIndex >= m_pageCount || pageRect.isEmpty())
        return {};

    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(m_document), pageIndex);
    if (!page)
        return {};

    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        return {};
    }

    const int capacity = 8192;
    // Reader rectangles use top-left coordinates; PDF page space is bottom-left.
    const double pageHeight = pageSize(pageIndex).height();
    QVector<unsigned short> buffer(capacity, 0);
    const int written = FPDFText_GetBoundedText(
        textPage,
        static_cast<double>(pageRect.left()),
        pageHeight - pageRect.top(),
        static_cast<double>(pageRect.right()),
        pageHeight - pageRect.bottom(),
        buffer.data(),
        capacity);

    int length = written;
    if (length > 0 && buffer.at(length - 1) == 0)
        --length;
    QString text = QString::fromUtf16(reinterpret_cast<const char16_t *>(buffer.constData()), std::max(0, length)).trimmed();

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return text;
}

QString PdfDocument::author() const
{
    const std::lock_guard lock(s_pdfiumMutex);
    if (!m_document)
        return {};
    const unsigned long length = FPDF_GetMetaText(static_cast<FPDF_DOCUMENT>(m_document), "Author", nullptr, 0);
    if (length <= 2)
        return {};
    QVector<unsigned short> buffer(static_cast<int>(length / 2) + 1, 0);
    FPDF_GetMetaText(static_cast<FPDF_DOCUMENT>(m_document), "Author",
                     buffer.data(), length);
    int chars = buffer.size();
    while (chars > 0 && buffer.at(chars - 1) == 0)
        --chars;
    return QString::fromUtf16(reinterpret_cast<const char16_t *>(buffer.constData()), chars).trimmed();
}

QString PdfDocument::title() const
{
    const std::lock_guard lock(s_pdfiumMutex);
    if (!m_document)
        return {};
    const unsigned long length = FPDF_GetMetaText(static_cast<FPDF_DOCUMENT>(m_document), "Title", nullptr, 0);
    if (length <= 2)
        return {};
    QVector<unsigned short> buffer(static_cast<int>(length / 2) + 1, 0);
    FPDF_GetMetaText(static_cast<FPDF_DOCUMENT>(m_document), "Title",
                     buffer.data(), length);
    int chars = buffer.size();
    while (chars > 0 && buffer.at(chars - 1) == 0)
        --chars;
    return QString::fromUtf16(reinterpret_cast<const char16_t *>(buffer.constData()), chars).trimmed();
}

} // namespace stacks
