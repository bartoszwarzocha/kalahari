/// @file render_engine.cpp
/// @brief RenderEngine implementation (OpenSpec #00043 Phase 6)

#include <kalahari/editor/render_engine.h>
#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/lazy_layout_manager.h>
#include <kalahari/editor/viewport_manager.h>
#include <kalahari/editor/format_layer.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/kml_converter.h>  // For MetadataLayer
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

void RenderEngine::setSearchEngine(SearchEngine* engine) {
    m_searchEngine = engine;
}

void RenderEngine::setSearchHighlightColor(const QColor& color) {
    if (m_searchHighlightColor != color) {
        m_searchHighlightColor = color;
        if (m_searchEngine && m_searchEngine->isActive()) {
            markAllDirty();
        }
    }
}

void RenderEngine::setCurrentMatchColor(const QColor& color) {
    if (m_currentMatchColor != color) {
        m_currentMatchColor = color;
        if (m_searchEngine && m_searchEngine->isActive()) {
            markAllDirty();
        }
    }
}

void RenderEngine::setMetadataLayer(MetadataLayer* layer) {
    m_metadataLayer = layer;
}

void RenderEngine::setCommentHighlightColor(const QColor& color) {
    if (m_commentHighlightColor != color) {
        m_commentHighlightColor = color;
        if (m_metadataLayer && !m_metadataLayer->allComments().empty()) {
            markAllDirty();
        }
    }
}

void RenderEngine::setCommentBorderColor(const QColor& color) {
    if (m_commentBorderColor != color) {
        m_commentBorderColor = color;
        if (m_metadataLayer && !m_metadataLayer->allComments().empty()) {
            markAllDirty();
        }
    }
}

void RenderEngine::setTodoHighlightColor(const QColor& color) {
    if (m_todoHighlightColor != color) {
        m_todoHighlightColor = color;
        if (m_metadataLayer && !m_metadataLayer->allTodos().empty()) {
            markAllDirty();
        }
    }
}

void RenderEngine::setNoteHighlightColor(const QColor& color) {
    if (m_noteHighlightColor != color) {
        m_noteHighlightColor = color;
        if (m_metadataLayer && !m_metadataLayer->allTodos().empty()) {
            markAllDirty();
        }
    }
}

void RenderEngine::setCompletedTodoColor(const QColor& color) {
    if (m_completedTodoColor != color) {
        m_completedTodoColor = color;
        if (m_metadataLayer && !m_metadataLayer->allTodos().empty()) {
            markAllDirty();
        }
    }
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
        // CRITICAL: Refresh dirty layouts ONCE per frame before any getLayout() calls
        // This ensures layouts are up-to-date without expensive per-call relayout
        m_layoutManager->layoutVisibleParagraphs();

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

    // Paint comment highlights (before search highlights)
    paintCommentHighlights(painter, clipRect);

    // Paint TODO/NOTE marker highlights
    paintMarkerHighlights(painter, clipRect);

    // Paint search highlights
    paintSearchHighlights(painter, clipRect);

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

void RenderEngine::paintSearchHighlights(QPainter* painter, const QRect& clipRect) {
    if (!m_searchEngine || !m_searchEngine->isActive()) {
        return;
    }

    const auto& matches = m_searchEngine->matches();
    int currentIdx = m_searchEngine->currentMatchIndex();

    for (size_t i = 0; i < matches.size(); ++i) {
        const auto& match = matches[i];

        // Get visual rect for match
        QRectF matchRect = getTextRect(static_cast<size_t>(match.paragraph),
                                        match.paragraphOffset,
                                        static_cast<int>(match.length));

        if (matchRect.isEmpty() || !matchRect.intersects(clipRect)) {
            continue;  // Skip if not visible
        }

        // Choose color based on current match
        QColor color = (static_cast<int>(i) == currentIdx)
                           ? m_currentMatchColor
                           : m_searchHighlightColor;

        painter->fillRect(matchRect, color);
    }
}

void RenderEngine::paintCommentHighlights(QPainter* painter, const QRect& clipRect) {
    if (!m_metadataLayer || !m_buffer || !m_viewportManager) {
        return;
    }

    const auto& comments = m_metadataLayer->allComments();
    if (comments.empty()) {
        return;
    }

    // Get visible paragraph range
    size_t firstPara = m_viewportManager->firstVisibleParagraph();
    size_t lastPara = m_viewportManager->lastVisibleParagraph();

    // Calculate absolute position for first visible paragraph
    size_t visibleStartPos = 0;
    for (size_t i = 0; i < firstPara && i < m_buffer->paragraphCount(); ++i) {
        visibleStartPos += m_buffer->paragraphText(i).length() + 1;  // +1 for newline
    }

    // Calculate absolute position for end of last visible paragraph
    size_t visibleEndPos = visibleStartPos;
    for (size_t i = firstPara; i <= lastPara && i < m_buffer->paragraphCount(); ++i) {
        visibleEndPos += m_buffer->paragraphText(i).length() + 1;
    }

    // Paint each comment that intersects visible range
    for (const auto& comment : comments) {
        // Skip if comment is entirely outside visible range
        if (comment.anchorEnd <= visibleStartPos || comment.anchorStart >= visibleEndPos) {
            continue;
        }

        // Paint comment across paragraphs it spans
        size_t pos = 0;
        for (size_t para = 0; para < m_buffer->paragraphCount(); ++para) {
            QString paraText = m_buffer->paragraphText(para);
            size_t paraEnd = pos + paraText.length();

            // Check if comment intersects this paragraph
            if (comment.anchorStart <= paraEnd && comment.anchorEnd > pos) {
                // Calculate offsets within paragraph
                int startOffset = static_cast<int>(
                    comment.anchorStart > pos ? comment.anchorStart - pos : 0);
                int endOffset = static_cast<int>(
                    std::min(comment.anchorEnd, paraEnd) - pos);
                int length = endOffset - startOffset;

                if (length > 0 && para >= firstPara && para <= lastPara) {
                    // Get visual rectangle for this range
                    QRectF commentRect = getTextRect(para, startOffset, length);

                    if (!commentRect.isEmpty() && commentRect.toRect().intersects(clipRect)) {
                        // Fill background
                        painter->fillRect(commentRect, m_commentHighlightColor);

                        // Draw underline
                        QPen pen(m_commentBorderColor);
                        pen.setWidth(2);
                        painter->setPen(pen);
                        painter->drawLine(
                            QPointF(commentRect.left(), commentRect.bottom() - 1),
                            QPointF(commentRect.right(), commentRect.bottom() - 1));
                    }
                }
            }

            pos = paraEnd + 1;  // +1 for newline
            if (pos > comment.anchorEnd) break;  // Past this comment
        }
    }
}

void RenderEngine::paintMarkerHighlights(QPainter* painter, const QRect& clipRect) {
    if (!m_metadataLayer || !m_buffer || !m_viewportManager) {
        return;
    }

    const auto& markers = m_metadataLayer->allTodos();
    if (markers.empty()) {
        return;
    }

    // Get visible paragraph range
    size_t firstPara = m_viewportManager->firstVisibleParagraph();
    size_t lastPara = m_viewportManager->lastVisibleParagraph();

    for (const auto& marker : markers) {
        // Find which paragraph contains this marker position
        size_t pos = 0;
        for (size_t para = 0; para < m_buffer->paragraphCount(); ++para) {
            QString paraText = m_buffer->paragraphText(para);
            size_t paraEnd = pos + static_cast<size_t>(paraText.length());

            if (marker.position >= pos && marker.position <= paraEnd) {
                // Marker is in this paragraph
                if (para >= firstPara && para <= lastPara) {
                    // Get line rect for this paragraph
                    QRectF lineRect = paragraphRect(para);

                    if (!lineRect.isEmpty() && lineRect.toRect().intersects(clipRect)) {
                        // Choose color based on type and completion state
                        QColor highlightColor;
                        if (marker.type == MarkerType::Todo) {
                            highlightColor = marker.completed
                                ? m_completedTodoColor
                                : m_todoHighlightColor;
                        } else {
                            highlightColor = m_noteHighlightColor;
                        }

                        // Draw small indicator in left margin
                        qreal iconSize = 8.0;
                        qreal marginX = lineRect.left() + 2;  // Small offset from left
                        qreal centerY = lineRect.center().y();

                        QRectF iconRect(marginX, centerY - iconSize / 2, iconSize, iconSize);

                        painter->save();
                        painter->setRenderHint(QPainter::Antialiasing);

                        if (marker.type == MarkerType::Todo) {
                            // Draw checkbox
                            QPen pen(highlightColor.darker(150));
                            pen.setWidth(1);
                            painter->setPen(pen);
                            painter->setBrush(marker.completed ? highlightColor : Qt::NoBrush);
                            painter->drawRect(iconRect);

                            if (marker.completed) {
                                // Draw checkmark
                                painter->setPen(QPen(Qt::white, 1.5));
                                painter->drawLine(
                                    QPointF(iconRect.left() + 2, iconRect.center().y()),
                                    QPointF(iconRect.center().x(), iconRect.bottom() - 2));
                                painter->drawLine(
                                    QPointF(iconRect.center().x(), iconRect.bottom() - 2),
                                    QPointF(iconRect.right() - 1, iconRect.top() + 2));
                            }
                        } else {
                            // Draw info circle for NOTE
                            painter->setPen(Qt::NoPen);
                            painter->setBrush(highlightColor);
                            painter->drawEllipse(iconRect);

                            // Draw 'i' in center
                            painter->setPen(QPen(highlightColor.darker(200), 1));
                            QFont font = painter->font();
                            font.setPixelSize(static_cast<int>(iconSize - 2));
                            font.setBold(true);
                            painter->setFont(font);
                            painter->drawText(iconRect, Qt::AlignCenter, "i");
                        }

                        painter->restore();

                        // Draw subtle line highlight
                        QColor lineHighlight = highlightColor;
                        lineHighlight.setAlpha(30);
                        painter->fillRect(lineRect, lineHighlight);
                    }
                }
                break;  // Found the paragraph for this marker
            }

            pos = paraEnd + 1;  // +1 for newline
        }
    }
}

QRectF RenderEngine::getTextRect(size_t paraIndex, int offset, int length) const {
    if (!m_buffer || !m_viewportManager || !m_layoutManager) {
        return QRectF();
    }

    if (paraIndex >= m_buffer->paragraphCount()) {
        return QRectF();
    }

    // Get paragraph Y in document coordinates
    double paraY = paragraphY(paraIndex);

    // Get the layout for this paragraph
    QTextLayout* layout = m_layoutManager->getLayout(paraIndex);
    if (!layout || layout->lineCount() == 0) {
        return QRectF();
    }

    // Find the line containing the start offset
    QTextLine startLine;
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        int lineStart = line.textStart();
        int lineEnd = lineStart + line.textLength();
        if (offset >= lineStart && offset < lineEnd) {
            startLine = line;
            break;
        }
        // If offset is beyond this line, move to next
        if (offset >= lineEnd && i < layout->lineCount() - 1) {
            continue;
        }
        // Last line - use it if offset is at or past the end
        if (i == layout->lineCount() - 1) {
            startLine = line;
        }
    }

    if (!startLine.isValid()) {
        return QRectF();
    }

    // Get x positions for start and end of match
    qreal x1 = startLine.cursorToX(offset);
    qreal x2 = startLine.cursorToX(offset + length);

    // Ensure x1 <= x2
    if (x1 > x2) {
        std::swap(x1, x2);
    }

    // Convert to widget coordinates
    double widgetY = documentToWidgetY(paraY) + startLine.y();
    double widgetX = m_leftMargin + x1;
    double rectWidth = x2 - x1;

    return QRectF(widgetX, widgetY, rectWidth, startLine.height());
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
