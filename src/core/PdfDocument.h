#pragma once

#include <QImage>
#include <QPageSize>
#include <QRectF>
#include <QSizeF>
#include <QString>

namespace stacks {

class PdfDocument
{
public:
    PdfDocument();
    ~PdfDocument();

    PdfDocument(const PdfDocument &) = delete;
    PdfDocument &operator=(const PdfDocument &) = delete;

    static void globalInit();
    static void globalShutdown();

    bool load(const QString &path, QString *error = nullptr);
    void close();

    bool isValid() const { return m_document != nullptr; }
    int pageCount() const { return m_pageCount; }
    QSizeF pageSize(int pageIndex) const;

    QImage renderPage(int pageIndex, double scale = 1.5) const;
    QString pageText(int pageIndex) const;
    QString textInRect(int pageIndex, const QRectF &pageRect) const;

    QString title() const;
    QString author() const;
    QString lastError() const { return m_lastError; }

private:
    void *m_document = nullptr;
    bool m_libraryInitialized = false;
    int m_pageCount = 0;
    QString m_lastError;
};

} // namespace stacks
