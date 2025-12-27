/// @file render_engine.cpp
/// @brief RenderEngine implementation (OpenSpec #00043 Phase 6)

#include <kalahari/editor/render_engine.h>
#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/lazy_layout_manager.h>
#include <kalahari/editor/viewport_manager.h>
#include <kalahari/editor/format_layer.h>
#include <QTextLayout>
#include <QTextLine>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructor / Destructor
// =============================================================================

RenderEngine::RenderEngine(QObject* parent)
    : QObject(parent) {
    m_font = QFont("Segoe UI", 11);

    connect(&m_cursorBlinkTimer, &QTimer::timeout,
            this, &RenderEngine::onCursorBlinkTimer);
}

RenderEngine::~RenderEngine() {
    m_cursorBlinkTimer.stop();
}

// =============================================================================
// Component Integration
// =============================================================================

void RenderEngine::setBuffer(TextBuffer* buffer) {
    m_buffer = buffer;
}

void RenderEngine::setLayoutManager(LazyLayoutManager* manager) {
    m_layoutManager = manager;
}

void RenderEngine::setViewportManager(ViewportManager* viewport) {
    m_viewportManager = viewport;
}

void RenderEngine::setFormatLayer(FormatLayer* formatLayer) {
    m_formatLayer = formatLayer;
}

// =============================================================================
// Appearance Configuration
// =============================================================================

void RenderEngine::setFont(const QFont& font) {
    if (m_font != font) {
        m_font = font;
        markAllDirty();
    }
}

void RenderEngine::setBackgroundColor(const QColor& color) {
    if (m_backgroundColor != color) {
        m_backgroundColor = color;
        markAllDirty();
    }
}

void RenderEngine::setTextColor(const QColor& color) {
    if (m_textColor != color) {
        m_textColor = color;
        markAllDirty();
    }
}

void RenderEngine::setSelectionColor(const QColor& color) {
    if (m_selectionColor != color) {
        m_selectionColor = color;
        if (hasSelection()) {
            markAllDirty();
        }
    }
}

void RenderEngine::setSelectionTextColor(const QColor& color) {
    if (m_selectionTextColor != color) {
        m_selectionTextColor = color;
        if (hasSelection()) {
            markAllDirty();
        }
    }
}

void RenderEngine::setCursorColor(const QColor& color) {
    if (m_cursorColor != color) {
        m_cursorColor = color;
        if (m_cursorVisible) {
            markDirty(cursorRect().toAlignedRect());
        }
    }
}

void RenderEngine::setLeftMargin(double margin) {
    if (m_leftMargin != margin) {
        m_leftMargin = margin;
        markAllDirty();
    }
}

void RenderEngine::setTopMargin(double margin) {
    if (m_topMargin != margin) {
        m_topMargin = margin;
        markAllDirty();
    }
}

void RenderEngine::setRightMargin(double margin) {
    if (m_rightMargin != margin) {
        m_rightMargin = margin;
        markAllDirty();
    }
}

void RenderEngine::setLineSpacing(double spacing) {
    if (m_lineSpacing != spacing) {
        m_lineSpacing = spacing;
        markAllDirty();
    }
}

// =============================================================================
// Dirty Region Tracking
// =============================================================================

void RenderEngine::markDirty(const QRect& region) {
    m_dirtyRegion = m_dirtyRegion.united(region);
    emit repaintRequested(QRegion(region));
}

void RenderEngine::markParagraphDirty(size_t paragraphIndex) {
    QRectF rect = paragraphRect(paragraphIndex);
    if (!rect.isEmpty()) {
        markDirty(rect.toAlignedRect());
    }
}

void RenderEngine::markAllDirty() {
    // Mark entire viewport as dirty
    if (m_viewportManager) {
        QSize size = m_viewportManager->viewportSize();
        m_dirtyRegion = QRegion(0, 0, size.width(), size.height());
        emit repaintRequested(m_dirtyRegion);
    }
}

void RenderEngine::clearDirtyRegion() {
    m_dirtyRegion = QRegion();
}

// =============================================================================
// Selection
// =============================================================================

void RenderEngine::setSelection(const SelectionRange& selection) {
    if (m_selection.start != selection.start || m_selection.end != selection.end) {

        // Mark old selection paragraphs dirty
        SelectionRange oldSel = m_selection.normalized();
        for (int i = oldSel.start.paragraph; i <= oldSel.end.paragraph; ++i) {
            markParagraphDirty(static_cast<size_t>(i));
        }

        m_selection = selection;

        // Mark new selection paragraphs dirty
        SelectionRange newSel = m_selection.normalized();
        for (int i = newSel.start.paragraph; i <= newSel.end.paragraph; ++i) {
            markParagraphDirty(static_cast<size_t>(i));
        }
    }
}

void RenderEngine::clearSelection() {
    setSelection(SelectionRange{});
}

// =============================================================================
// Cursor
// =============================================================================

void RenderEngine::setCursorPosition(const CursorPosition& position) {
    if (m_cursorPosition != position) {
        // Mark old cursor position dirty
        markDirty(cursorRect().toAlignedRect());

        m_cursorPosition = position;

        // Mark new cursor position dirty
        markDirty(cursorRect().toAlignedRect());

        // Reset blink state
        m_cursorBlinkState = true;
        if (m_cursorBlinkTimer.isActive()) {
            m_cursorBlinkTimer.start();  // Restart timer
        }
    }
}

void RenderEngine::setCursorVisible(bool visible) {
    if (m_cursorVisible != visible) {
        m_cursorVisible = visible;
        markDirty(cursorRect().toAlignedRect());
    }
}

void RenderEngine::setCursorBlinkInterval(int ms) {
    m_cursorBlinkInterval = ms;
    if (m_cursorBlinkTimer.isActive()) {
        m_cursorBlinkTimer.setInterval(ms);
    }
}

void RenderEngine::startCursorBlink() {
    if (m_cursorBlinkInterval > 0) {
        m_cursorBlinkState = true;
        m_cursorBlinkTimer.start(m_cursorBlinkInterval);
    }
}

void RenderEngine::stopCursorBlink() {
    m_cursorBlinkTimer.stop();
    m_cursorBlinkState = true;  // Keep cursor visible when not blinking
    markDirty(cursorRect().toAlignedRect());
}

void RenderEngine::setCursorWidth(double width) {
    if (m_cursorWidth != width) {
        markDirty(cursorRect().toAlignedRect());
        m_cursorWidth = width;
        markDirty(cursorRect().toAlignedRect());
    }
}

QRectF RenderEngine::cursorRect() const {
    if (!m_buffer || !m_viewportManager || !m_layoutManager) {
        return QRectF();
    }

    int paraIndex = m_cursorPosition.paragraph;
    if (paraIndex < 0 || static_cast<size_t>(paraIndex) >= m_buffer->paragraphCount()) {
        return QRectF();
    }

    // Get paragraph Y in document coordinates
    double paraY = paragraphY(static_cast<size_t>(paraIndex));

    // Get the layout for this paragraph
    QTextLayout* layout = m_layoutManager->getLayout(static_cast<size_t>(paraIndex));
    if (!layout || layout->lineCount() == 0) {
        // Fallback: use font metrics for cursor height
        QFontMetricsF fm(m_font);
        double widgetY = documentToWidgetY(paraY);
        return QRectF(m_leftMargin, widgetY, m_cursorWidth, fm.height());
    }

    // Find the line containing the cursor
    int offset = m_cursorPosition.offset;
    QTextLine line;
    for (int i = 0; i < layout->lineCount(); ++i) {
        line = layout->lineAt(i);
        if (offset >= line.textStart() && offset <= line.textStart() + line.textLength()) {
            break;
        }
    }

    if (!line.isValid()) {
        line = layout->lineAt(layout->lineCount() - 1);
    }

    // Get x position for cursor
    qreal cursorX = line.cursorToX(offset);

    // Convert to widget coordinates
    double widgetY = documentToWidgetY(paraY) + line.y();
    double widgetX = m_leftMargin + cursorX;

    return QRectF(widgetX, widgetY, m_cursorWidth, line.height());
}

void RenderEngine::onCursorBlinkTimer() {
    m_cursorBlinkState = !m_cursorBlinkState;
    markDirty(cursorRect().toAlignedRect());
    emit cursorBlinkChanged(m_cursorBlinkState);
}

// =============================================================================
// Paint
// =============================================================================

void RenderEngine::paint(QPainter* painter, const QRect& clipRect,
                         const QSize& viewportSize) {
    if (!painter) return;

    painter->save();
    painter->setClipRect(clipRect);

    // Paint background
    paintBackground(painter, clipRect);

    // Paint paragraphs
    if (m_buffer && m_viewportManager && m_layoutManager) {
        size_t firstVisible = m_viewportManager->firstVisibleParagraph();
        size_t lastVisible = m_viewportManager->lastVisibleParagraph();

        for (size_t i = firstVisible; i <= lastVisible; ++i) {
            double y = paragraphY(i);
            double widgetY = documentToWidgetY(y);

            // Get paragraph height
            double height = m_buffer->getParagraphHeight(i);

            // Check if paragraph intersects clip rect
            QRectF paraRect(0, widgetY, viewportSize.width(), height);
            if (paraRect.intersects(clipRect)) {
                paintParagraph(painter, i, widgetY);
            }
        }
    }

    // Selection is handled in paintParagraph() via ParagraphLayout

    // Paint cursor
    if (m_cursorVisible && m_cursorBlinkState) {
        paintCursor(painter);
    }

    painter->restore();

    // Clear dirty region after paint
    clearDirtyRegion();
}

void RenderEngine::paintDirty(QPainter* painter, const QSize& viewportSize) {
    if (!isDirty()) return;

    // Paint each dirty rect
    for (const QRect& rect : m_dirtyRegion) {
        paint(painter, rect, viewportSize);
    }
}

// =============================================================================
// Paint Helpers
// =============================================================================

void RenderEngine::paintBackground(QPainter* painter, const QRect& clipRect) {
    painter->fillRect(clipRect, m_backgroundColor);
}

void RenderEngine::paintParagraph(QPainter* painter, size_t index, double y) {
    if (!m_layoutManager) return;

    QTextLayout* layout = m_layoutManager->getLayout(index);
    if (!layout) return;

    // Draw position with margins
    QPointF drawPos(m_leftMargin, y);

    // Build selection format ranges if needed
    QList<QTextLayout::FormatRange> selections;
    if (hasSelection()) {
        SelectionRange sel = m_selection.normalized();
        int paraIdx = static_cast<int>(index);

        if (paraIdx >= sel.start.paragraph && paraIdx <= sel.end.paragraph) {
            int paraStart = (paraIdx == sel.start.paragraph)
                                ? sel.start.offset
                                : 0;
            int textLen = layout->text().length();
            int paraEnd = (paraIdx == sel.end.paragraph)
                              ? std::min(sel.end.offset, textLen)
                              : textLen;

            if (paraStart < paraEnd) {
                QTextLayout::FormatRange selRange;
                selRange.start = paraStart;
                selRange.length = paraEnd - paraStart;
                selRange.format.setBackground(m_selectionColor);
                selRange.format.setForeground(m_selectionTextColor);
                selections.append(selRange);
            }
        }
    }

    // Draw the layout with selections
    layout->draw(painter, drawPos, selections);
}

void RenderEngine::paintSelection(QPainter* /*painter*/) {
    // Selection is now handled in paintParagraph() via ParagraphLayout::setSelection()
    // This method is kept for potential future use (e.g., multi-region selection)
}

void RenderEngine::paintCursor(QPainter* painter) {
    QRectF rect = cursorRect();
    if (rect.isEmpty()) return;

    painter->fillRect(rect, m_cursorColor);
}

QRectF RenderEngine::selectionRectForParagraph(size_t /*paraIndex*/,
                                                size_t /*startOffset*/,
                                                size_t /*endOffset*/,
                                                double /*paraY*/) const {
    // Selection rendering is now handled by ParagraphLayout::draw()
    // This method is kept for API compatibility
    return QRectF();
}

// =============================================================================
// Geometry Queries
// =============================================================================

double RenderEngine::paragraphY(size_t index) const {
    if (!m_buffer) return 0.0;
    return m_buffer->getParagraphY(index);
}

QRectF RenderEngine::paragraphRect(size_t index) const {
    if (!m_buffer || !m_viewportManager) return QRectF();

    double docY = paragraphY(index);
    double widgetY = documentToWidgetY(docY);
    double height = m_buffer->getParagraphHeight(index);
    double width = m_viewportManager->viewportSize().width();

    return QRectF(0, widgetY, width, height);
}

double RenderEngine::documentToWidgetY(double docY) const {
    if (!m_viewportManager) return m_topMargin + docY;
    return m_topMargin + docY - m_viewportManager->scrollPosition();
}

double RenderEngine::widgetToDocumentY(double widgetY) const {
    if (!m_viewportManager) return widgetY - m_topMargin;
    return widgetY - m_topMargin + m_viewportManager->scrollPosition();
}

}  // namespace kalahari::editor
