#pragma once

#include "core/Book.h"

#include <QImage>
#include <QColor>
#include <QQuickPaintedItem>
#include <QTextCursor>
#include <QVariantList>

#include <memory>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace stacks {

class PdfDocument;

class ReaderItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY documentChanged)
    Q_PROPERTY(QString format READ format NOTIFY documentChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY documentChanged)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY positionChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY documentChanged)
    Q_PROPERTY(double progress READ progress NOTIFY positionChanged)
    Q_PROPERTY(QString locator READ locator NOTIFY positionChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectionChanged)
    Q_PROPERTY(QString selectionLocator READ selectionLocator NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList annotations READ annotations WRITE setAnnotations NOTIFY annotationsChanged)
    Q_PROPERTY(qreal scrollY READ scrollY NOTIFY scrollChanged)
    Q_PROPERTY(qreal maxScrollY READ maxScrollY NOTIFY scrollChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor mutedColor READ mutedColor WRITE setMutedColor NOTIFY appearanceChanged)

public:
    explicit ReaderItem(QQuickItem *parent = nullptr);
    ~ReaderItem() override;

    void paint(QPainter *painter) override;

    QString filePath() const { return m_filePath; }
    QString format() const { return m_format; }
    bool ready() const { return m_ready; }
    int currentPage() const { return m_page; }
    int pageCount() const { return m_pageCount; }
    double progress() const { return m_progress; }
    QString locator() const { return m_locator; }
    QString selectedText() const { return m_selectedText; }
    QString selectionLocator() const { return m_selectionLocator; }
    QVariantList annotations() const { return m_annotations; }
    qreal scrollY() const { return m_scrollY; }
    qreal maxScrollY() const { return m_maxScrollY; }
    QColor textColor() const { return m_textColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor mutedColor() const { return m_mutedColor; }
    void setTextColor(const QColor &color);
    void setAccentColor(const QColor &color);
    void setMutedColor(const QColor &color);

    void setAnnotations(const QVariantList &annotations);

    Q_INVOKABLE bool load(const QString &path, const QString &format, const QString &locator = QString());
    Q_INVOKABLE void closeDocument();
    Q_INVOKABLE void goToLocator(const QString &locator);
    Q_INVOKABLE void goToPage(int page);
    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void previousPage();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setAnnotationsForBook(const QVariantList &annotations);

signals:
    void documentChanged();
    void positionChanged(const QString &locator, double progress);
    void selectionChanged();
    void scrollChanged();
    void annotationsChanged();
    void appearanceChanged();
    void errorOccurred(const QString &message);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void rebuildTextDocument();
    void applyTextColors();
    void layoutTextDocument();
    void renderPdfPage();
    void emitPosition();
    QString epubLocatorAtTop() const;
    qreal yForCharacter(int position) const;
    int characterAt(const QPointF &point) const;
    QRectF pageRectInItem() const;
    void resolvePassageHighlights();

    QString m_filePath;
    QString m_format;
    bool m_ready = false;
    int m_page = 0;
    int m_pageCount = 0;
    double m_progress = 0.0;
    QString m_locator;
    QString m_selectedText;
    QString m_selectionLocator;
    QVariantList m_annotations;
    QColor m_textColor = QColor(QStringLiteral("#211C16"));
    QColor m_accentColor = QColor(QStringLiteral("#C46A3B"));
    QColor m_mutedColor = QColor(QStringLiteral("#8A8073"));

    std::unique_ptr<PdfDocument> m_pdf;
    QImage m_pdfImage;

    std::unique_ptr<QTextDocument> m_textDocument;
    QVector<Passage> m_epubPassages;
    // Character ranges of paragraph highlights, found once per annotation
    // change rather than by searching the whole book on every paint.
    QVector<QPair<int, int>> m_passageHighlights;
    qreal m_scrollY = 0.0;
    qreal m_maxScrollY = 0.0;
    bool m_draggingText = false;
    int m_dragAnchor = 0;
    QPointF m_dragStart;
    QRectF m_pdfSelectionRect;
    bool m_draggingPdf = false;
    int m_wheelAccumulator = 0;
    bool m_pageTurnedThisGesture = false;
};

} // namespace stacks
