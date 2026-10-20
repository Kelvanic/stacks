#include "ReaderItem.h"

#include "core/EpubDocument.h"
#include "core/PdfDocument.h"

#include <QAbstractTextDocumentLayout>
#include <QDateTime>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QQuickWindow>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QWheelEvent>

#include <algorithm>

namespace stacks {

namespace {

constexpr qreal kHorizontalMargin = 76.0;
constexpr qreal kVerticalMargin = 56.0;
constexpr qreal kPdfMargin = 18.0;

struct TextHighlight {
    int start = 0;
    int end = 0;
    QString color;
};

QVector<int> parsePair(const QString &value, QChar separator)
{
    const QStringList parts = value.split(separator, Qt::SkipEmptyParts);
    QVector<int> result;
    for (const QString &part : parts) {
        bool ok = false;
        const int number = part.toInt(&ok);
        if (ok)
            result.append(number);
        else
            result.append(-1);
    }
    return result;
}

} // namespace

ReaderItem::ReaderItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setCursor(Qt::IBeamCursor);
    setAntialiasing(true);
}

ReaderItem::~ReaderItem() = default;

void ReaderItem::setAnnotations(const QVariantList &annotations)
{
    m_annotations = annotations;
    emit annotationsChanged();
    update();
}

void ReaderItem::setAnnotationsForBook(const QVariantList &annotations)
{
    setAnnotations(annotations);
}

bool ReaderItem::load(const QString &path, const QString &format, const QString &locator)
{
    closeDocument();

    m_filePath = path;
    m_format = format.toLower();
    m_selectedText.clear();
    m_selectionLocator.clear();

    if (m_format == QStringLiteral("pdf")) {
        m_pdf = std::make_unique<PdfDocument>();
        QString error;
        if (!m_pdf->load(path, &error)) {
            emit errorOccurred(error);
            closeDocument();
            return false;
        }
        m_pageCount = m_pdf->pageCount();
        m_page = 0;
        m_ready = true;
        setCursor(Qt::CrossCursor);
        if (!locator.isEmpty())
            goToLocator(locator);
        renderPdfPage();
        emit documentChanged();
        emitPosition();
        update();
        return true;
    }

    if (m_format == QStringLiteral("epub")) {
        EpubDocument epub;
        QString error;
        if (!epub.load(path, &error)) {
            emit errorOccurred(error);
            closeDocument();
            return false;
        }
        m_epubPassages = epub.passages();
        m_pageCount = std::max(1, static_cast<int>(epub.chapters().size()));
        m_page = 0;
        m_ready = true;
        setCursor(Qt::IBeamCursor);
        rebuildTextDocument();
        if (!locator.isEmpty())
            goToLocator(locator);
        emit documentChanged();
        layoutTextDocument();
        emitPosition();
        update();
        return true;
    }

    emit errorOccurred(QStringLiteral("Unsupported format: %1").arg(format));
    return false;
}

void ReaderItem::closeDocument()
{
    m_ready = false;
    m_pdf.reset();
    m_pdfImage = QImage();
    m_textDocument.reset();
    m_epubPassages.clear();
    m_scrollY = 0.0;
    m_maxScrollY = 0.0;
    m_page = 0;
    m_pageCount = 0;
    m_progress = 0.0;
    m_locator.clear();
    m_filePath.clear();
    m_format.clear();
    m_selectedText.clear();
    m_selectionLocator.clear();
    m_pdfSelectionRect = QRectF();
    update();
}

void ReaderItem::rebuildTextDocument()
{
    m_textDocument = std::make_unique<QTextDocument>();
    QFont bodyFont(QStringLiteral("Newsreader"));
    if (!QFontDatabase::families().contains(QStringLiteral("Newsreader")))
        bodyFont = QFont(QStringLiteral("Georgia"));
    bodyFont.setPointSizeF(17.0);
    m_textDocument->setDefaultFont(bodyFont);
    m_textDocument->setDocumentMargin(0);

    // ReaderItem re-reads the EPUB for its raw HTML. The document is small
    // enough to keep in memory and makes selection, search and highlighting
    // behave like a single continuous book.
    EpubDocument epub;
    if (epub.load(m_filePath))
        m_textDocument->setHtml(epub.html());
    else
        m_textDocument->setPlainText(QString());
}

void ReaderItem::layoutTextDocument()
{
    if (!m_textDocument)
        return;

    const QString restoreLocator = epubLocatorAtTop();
    const qreal available = qMax(260.0, width() - 2.0 * kHorizontalMargin);
    m_textDocument->setTextWidth(available);
    m_maxScrollY = qMax(0.0, m_textDocument->size().height() - qMax(120.0, height() - 2.0 * kVerticalMargin));

    if (!restoreLocator.isEmpty()) {
        const QStringList parts = restoreLocator.mid(5).split(QLatin1Char(':'));
        if (parts.size() == 1) {
            bool ok = false;
            const int position = parts.first().toInt(&ok);
            if (ok)
                m_scrollY = qBound(0.0, yForCharacter(position) - 24.0, m_maxScrollY);
        }
    }
    m_scrollY = qBound(0.0, m_scrollY, m_maxScrollY);
    emit scrollChanged();
    update();
}

qreal ReaderItem::yForCharacter(int position) const
{
    if (!m_textDocument)
        return 0.0;
    QTextCursor cursor(m_textDocument.get());
    cursor.setPosition(qBound(0, position, m_textDocument->characterCount() - 1));
    return m_textDocument->documentLayout()->blockBoundingRect(cursor.block()).top();
}

int ReaderItem::characterAt(const QPointF &point) const
{
    if (!m_textDocument)
        return 0;
    const QPointF documentPoint(point.x() - kHorizontalMargin, point.y() + m_scrollY - kVerticalMargin);
    const int position = m_textDocument->documentLayout()->hitTest(documentPoint, Qt::FuzzyHit);
    if (position < 0)
        return m_textDocument->characterCount() - 1;
    return position;
}

QString ReaderItem::epubLocatorAtTop() const
{
    if (!m_textDocument)
        return {};
    const int character = m_textDocument->documentLayout()->hitTest(
        QPointF(1.0, m_scrollY + 1.0), Qt::FuzzyHit);
    return QStringLiteral("epub:%1").arg(std::max(0, character));
}

void ReaderItem::emitPosition()
{
    if (!m_ready)
        return;

    if (m_format == QStringLiteral("pdf")) {
        m_progress = m_pageCount > 0 ? double(m_page + 1) / double(m_pageCount) : 0.0;
        m_locator = QStringLiteral("pdf:%1").arg(m_page);
        emit positionChanged(m_locator, m_progress);
        return;
    }

    if (m_textDocument) {
        const int character = std::max(0, m_textDocument->documentLayout()->hitTest(
                                             QPointF(1.0, m_scrollY + 1.0), Qt::FuzzyHit));
        const int total = std::max(1, static_cast<int>(m_textDocument->characterCount()));
        m_progress = qBound(0.0, double(character) / double(total), 1.0);
        m_locator = QStringLiteral("epub:%1").arg(character);
        emit positionChanged(m_locator, m_progress);
    }
}

void ReaderItem::renderPdfPage()
{
    if (!m_pdf || !m_pdf->isValid())
        return;
    const QSizeF pageSize = m_pdf->pageSize(m_page);
    if (pageSize.isEmpty())
        return;

    double dpr = 1.0;
    if (window())
        dpr = window()->effectiveDevicePixelRatio();
    dpr = std::max(1.0, dpr);

    const qreal availableWidth = qMax(120.0, width() - 2.0 * kPdfMargin);
    const qreal availableHeight = qMax(120.0, height() - 2.0 * kPdfMargin);
    const qreal fitScale = qMin(availableWidth / pageSize.width(), availableHeight / pageSize.height());
    const qreal renderScale = qBound(1.0, fitScale * dpr, 4.0);
    m_pdfImage = m_pdf->renderPage(m_page, renderScale);
    update();
}

QRectF ReaderItem::pageRectInItem() const
{
    if (!m_pdf || !m_pdf->isValid())
        return {};
    const QSizeF pageSize = m_pdf->pageSize(m_page);
    if (pageSize.isEmpty())
        return {};
    const qreal availableWidth = qMax(120.0, width() - 2.0 * kPdfMargin);
    const qreal availableHeight = qMax(120.0, height() - 2.0 * kPdfMargin);
    const qreal scale = qMin(availableWidth / pageSize.width(), availableHeight / pageSize.height());
    const QSizeF drawn(pageSize.width() * scale, pageSize.height() * scale);
    return QRectF((width() - drawn.width()) / 2.0, (height() - drawn.height()) / 2.0,
                  drawn.width(), drawn.height());
}

void ReaderItem::goToLocator(const QString &locator)
{
    if (!m_ready || locator.isEmpty())
        return;

    if (m_format == QStringLiteral("pdf")) {
        if (locator.startsWith(QStringLiteral("pdf:"))) {
            const QStringList parts = locator.split(QLatin1Char(':'), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                bool ok = false;
                const int page = parts.at(1).toInt(&ok);
                if (ok)
                    goToPage(page);
            }
        }
        return;
    }

    if (m_format != QStringLiteral("epub") || !m_textDocument)
        return;

    const QString payload = locator.startsWith(QStringLiteral("epub:")) ? locator.mid(5) : locator;
    const QStringList sections = payload.split(QLatin1Char(':'));

    int target = -1;
    if (sections.size() == 2) {
        // Search result: epub:<chapter>:<paragraph>
        const QString passageLocator = QStringLiteral("epub:%1:%2").arg(sections.at(0), sections.at(1));
        for (const Passage &passage : m_epubPassages) {
            if (passage.locator == passageLocator) {
                QTextCursor finder(m_textDocument.get());
                QTextCursor found = m_textDocument->find(passage.text, finder);
                if (!found.isNull())
                    target = found.selectionStart();
                break;
            }
        }
    } else if (payload.contains(QLatin1Char('-'))) {
        const QStringList pair = payload.split(QLatin1Char('-'), Qt::SkipEmptyParts);
        if (pair.size() == 2) {
            bool ok = false;
            target = pair.first().toInt(&ok);
            if (!ok)
                target = -1;
        }
    } else {
        bool ok = false;
        target = payload.toInt(&ok);
        if (!ok)
            target = -1;
    }

    if (target >= 0) {
        m_scrollY = qBound(0.0, yForCharacter(target) - 24.0, m_maxScrollY);
        emit scrollChanged();
        emitPosition();
        update();
    }
}

void ReaderItem::goToPage(int page)
{
    if (m_format != QStringLiteral("pdf") || !m_pdf)
        return;
    const int clamped = qBound(0, page, std::max(0, m_pageCount - 1));
    if (clamped == m_page)
        return;
    m_page = clamped;
    m_pdfSelectionRect = QRectF();
    renderPdfPage();
    emitPosition();
    update();
}

void ReaderItem::nextPage()
{
    if (m_format == QStringLiteral("pdf")) {
        goToPage(m_page + 1);
    } else {
        m_scrollY = qBound(0.0, m_scrollY + (height() - 2.0 * kVerticalMargin) * 0.92, m_maxScrollY);
        emit scrollChanged();
        emitPosition();
        update();
    }
}

void ReaderItem::previousPage()
{
    if (m_format == QStringLiteral("pdf")) {
        goToPage(m_page - 1);
    } else {
        m_scrollY = qBound(0.0, m_scrollY - (height() - 2.0 * kVerticalMargin) * 0.92, m_maxScrollY);
        emit scrollChanged();
        emitPosition();
        update();
    }
}

void ReaderItem::clearSelection()
{
    m_selectedText.clear();
    m_selectionLocator.clear();
    m_pdfSelectionRect = QRectF();
    m_draggingPdf = false;
    m_draggingText = false;
    emit selectionChanged();
    update();
}

void ReaderItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (!m_ready)
        return;
    if (m_format == QStringLiteral("pdf"))
        renderPdfPage();
    else
        layoutTextDocument();
}

void ReaderItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (m_format == QStringLiteral("pdf")) {
        m_draggingPdf = true;
        m_dragStart = event->position();
        m_pdfSelectionRect = QRectF(m_dragStart, QSizeF());
        m_selectedText.clear();
        m_selectionLocator.clear();
        emit selectionChanged();
    } else {
        m_draggingText = true;
        m_dragStart = event->position();
        m_dragAnchor = characterAt(event->position());
        QTextCursor cursor(m_textDocument.get());
        cursor.setPosition(m_dragAnchor);
        m_selectedText.clear();
        m_selectionLocator.clear();
        emit selectionChanged();
    }
    update();
}

void ReaderItem::mouseMoveEvent(QMouseEvent *event)
{
    if (m_format == QStringLiteral("pdf")) {
        if (m_draggingPdf) {
            m_pdfSelectionRect = QRectF(m_dragStart, event->position()).normalized();
            update();
        }
        return;
    }

    if (!m_draggingText || !m_textDocument)
        return;

    const int current = characterAt(event->position());
    const int start = std::min(m_dragAnchor, current);
    const int end = std::max(m_dragAnchor, current);
    QTextCursor cursor(m_textDocument.get());
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    m_selectedText = cursor.selectedText();
    m_selectionLocator = QStringLiteral("epub:%1-%2").arg(start).arg(end);
    emit selectionChanged();
    update();
}

void ReaderItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_format == QStringLiteral("pdf")) {
        if (!m_draggingPdf)
            return;
        m_draggingPdf = false;
        m_pdfSelectionRect = QRectF(m_dragStart, event->position()).normalized();

        const QRectF pageRect = pageRectInItem();
        if (m_pdf && m_pdf->isValid() && m_pdfSelectionRect.width() > 6.0
            && m_pdfSelectionRect.height() > 6.0 && !pageRect.isEmpty()) {
            const QSizeF pageSize = m_pdf->pageSize(m_page);
            const qreal scale = pageRect.width() / pageSize.width();
            const QRectF clipped = m_pdfSelectionRect.intersected(pageRect);
            const QRectF pageSelection((clipped.left() - pageRect.left()) / scale,
                                       (clipped.top() - pageRect.top()) / scale,
                                       clipped.width() / scale, clipped.height() / scale);
            const QString quote = m_pdf->textInRect(m_page, pageSelection);
            m_selectedText = quote;
            m_selectionLocator = QStringLiteral("pdf:%1:%2,%3,%4,%5")
                                     .arg(m_page)
                                     .arg(qRound(pageSelection.left()))
                                     .arg(qRound(pageSelection.top()))
                                     .arg(qRound(pageSelection.width()))
                                     .arg(qRound(pageSelection.height()));
            if (!quote.isEmpty() || !pageSelection.isEmpty())
                emit selectionChanged();
        }
        update();
        return;
    }

    if (!m_draggingText)
        return;
    m_draggingText = false;
    if (m_selectedText.trimmed().isEmpty()) {
        m_selectedText.clear();
        m_selectionLocator.clear();
        clearSelection();
    } else {
        emitPosition();
        emit selectionChanged();
    }
    update();
}

void ReaderItem::wheelEvent(QWheelEvent *event)
{
    if (!m_ready) {
        event->ignore();
        return;
    }

    const int delta = event->angleDelta().y();
    if (m_format == QStringLiteral("pdf")) {
        if (delta < 0)
            nextPage();
        else if (delta > 0)
            previousPage();
        event->accept();
        return;
    }

    m_scrollY = qBound(0.0, m_scrollY - delta * 0.9, m_maxScrollY);
    emit scrollChanged();
    emitPosition();
    update();
    event->accept();
}

void ReaderItem::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    // The sheet itself belongs to the QML layer: it owns the paper colour,
    // the tooth of the paper and the bound edge. This item only draws the
    // content that sits on top of it.
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    painter->fillRect(QRectF(0, 0, width(), height()), Qt::transparent);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);

    if (!m_ready) {
        painter->setPen(QColor(QStringLiteral("#8A8073")));
        painter->setFont(QFont(QStringLiteral("Inter"), 13));
        painter->drawText(QRectF(0, 0, width(), height()), Qt::AlignCenter,
                          QStringLiteral("Open a book to start reading."));
        return;
    }

    if (m_format == QStringLiteral("pdf")) {
        const QRectF pageRect = pageRectInItem();
        if (pageRect.isEmpty() || m_pdfImage.isNull())
            return;

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 46));
        painter->drawRoundedRect(pageRect.translated(0, 3).adjusted(0, 0, 0, 0), 3, 3);
        painter->drawImage(pageRect, m_pdfImage);

        // Persisted PDF region highlights.
        for (const QVariant &value : m_annotations) {
            const QVariantMap annotation = value.toMap();
            if (annotation.value(QStringLiteral("kind")).toString() != QStringLiteral("highlight"))
                continue;
            const QString locator = annotation.value(QStringLiteral("locator")).toString();
            if (!locator.startsWith(QStringLiteral("pdf:")))
                continue;
            const QStringList parts = locator.split(QLatin1Char(':'), Qt::SkipEmptyParts);
            if (parts.size() < 3 || parts.at(1).toInt() != m_page)
                continue;
            const QVector<int> rect = parsePair(parts.at(2), QLatin1Char(','));
            if (rect.size() < 4)
                continue;
            const qreal scale = pageRect.width() / std::max(1.0, m_pdf->pageSize(m_page).width());
            const QRectF highlight(pageRect.left() + rect.at(0) * scale,
                                   pageRect.top() + rect.at(1) * scale,
                                   rect.at(2) * scale, rect.at(3) * scale);
            painter->setBrush(QColor(196, 106, 59, 64));
            painter->setPen(QPen(QColor(196, 106, 59, 150), 1.0));
            painter->drawRoundedRect(highlight, 2, 2);
        }

        if (m_draggingPdf && !m_pdfSelectionRect.isEmpty()) {
            painter->setBrush(QColor(196, 106, 59, 58));
            painter->setPen(QPen(QColor(196, 106, 59, 190), 1.0));
            painter->drawRect(m_pdfSelectionRect);
        }
        return;
    }

    if (!m_textDocument)
        return;

    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, QColor(QStringLiteral("#201C16")));
    context.palette.setColor(QPalette::Base, Qt::transparent);
    context.clip = QRectF(-kHorizontalMargin, m_scrollY - kVerticalMargin, width(), height());

    const auto addSelection = [&](int start, int end, const QColor &color) {
        if (start >= end)
            return;
        QAbstractTextDocumentLayout::Selection selection;
        QTextCursor cursor(m_textDocument.get());
        cursor.setPosition(qBound(0, start, m_textDocument->characterCount() - 1));
        cursor.setPosition(qBound(0, end, m_textDocument->characterCount() - 1), QTextCursor::KeepAnchor);
        QTextCharFormat format;
        format.setBackground(color);
        selection.cursor = cursor;
        selection.format = format;
        context.selections.append(selection);
    };

    for (const QVariant &value : m_annotations) {
        const QVariantMap annotation = value.toMap();
        const QString kind = annotation.value(QStringLiteral("kind")).toString();
        if (kind != QStringLiteral("highlight"))
            continue;
        const QString locator = annotation.value(QStringLiteral("locator")).toString();
        if (!locator.startsWith(QStringLiteral("epub:")))
            continue;
        const QString payload = locator.mid(5);
        if (payload.contains(QLatin1Char('-'))) {
            const QVector<int> pair = parsePair(payload, QLatin1Char('-'));
            if (pair.size() == 2 && pair.at(0) >= 0 && pair.at(1) > pair.at(0))
                addSelection(pair.at(0), pair.at(1), QColor(196, 106, 59, 58));
        } else if (payload.count(QLatin1Char(':')) == 1) {
            for (const Passage &passage : m_epubPassages) {
                if (passage.locator != locator)
                    continue;
                QTextCursor finder(m_textDocument.get());
                const QTextCursor found = m_textDocument->find(passage.text, finder);
                if (!found.isNull())
                    addSelection(found.selectionStart(), found.selectionEnd(), QColor(196, 106, 59, 52));
                break;
            }
        }
    }

    if (m_draggingText && !m_selectedText.isEmpty() && !m_selectionLocator.isEmpty()) {
        const QVector<int> pair = parsePair(m_selectionLocator.mid(5), QLatin1Char('-'));
        if (pair.size() == 2)
            addSelection(pair.at(0), pair.at(1), QColor(196, 106, 59, 88));
    }

    painter->save();
    painter->translate(kHorizontalMargin, kVerticalMargin - m_scrollY);
    m_textDocument->documentLayout()->draw(painter, context);
    painter->restore();
}

} // namespace stacks
