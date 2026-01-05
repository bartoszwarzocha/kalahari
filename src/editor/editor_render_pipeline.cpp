/// @file editor_render_pipeline.cpp
/// @brief Implementation of unified rendering pipeline (OpenSpec #00043 Phase 12.1)

#include <kalahari/editor/editor_render_pipeline.h>
#include <kalahari/editor/viewport_manager.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/core/logger.h>
#include <QPainter>
#include <QTextLayout>
#include <QTextLine>
#include <QTextBlock>
#include <QTextFragment>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructor / Destructor
// =============================================================================

EditorRenderPipeline::EditorRenderPipeline(QObject* parent)
    : QObject(parent) {
    // Initialize with default context
    m_context.font = QFont("Segoe UI", 11);

    // Setup cursor blink timer
    connect(&m_cursorBlinkTimer, &QTimer::timeout,
            this, &EditorRenderPipeline::onCursorBlinkTimeout);
}

EditorRenderPipeline::~EditorRenderPipeline() {
    m_cursorBlinkTimer.stop();
}

// =============================================================================
// Text Source (Stage 1)
// =============================================================================

void EditorRenderPipeline::setTextSource(std::unique_ptr<ITextSource> source) {
    m_textSource = std::move(source);
    m_heightDirty = true;
    markAllDirty();
}

// =============================================================================
// Render Context (Stage 2)
// =============================================================================

void EditorRenderPipeline::setContext(const RenderContext& context) {
    bool fontChanged = (m_context.font != context.font);
    bool widthChanged = (m_context.textWidth != context.textWidth);
    bool marginsChanged = (m_context.margins != context.margins);

    m_context = context;

    // Update text source if font, width, or margins changed
    if (m_textSource && (fontChanged || widthChanged || marginsChanged)) {
        m_textSource->setFont(m_context.font);
        m_textSource->setTextWidth(m_context.effectiveTextWidth());
        m_heightDirty = true;
    }

    markAllDirty();
}

void EditorRenderPipeline::setMargins(double left, double top, double right, double bottom) {
    RenderMargins newMargins{left, top, right, bottom};
    if (m_context.margins == newMargins) {
        return;  // No change, skip expensive relayout
    }
    m_context.margins = newMargins;
    if (m_textSource) {
        m_textSource->setTextWidth(m_context.effectiveTextWidth());
        m_heightDirty = true;
    }
    markAllDirty();
}

void EditorRenderPipeline::setMargins(const RenderMargins& margins) {
    setMargins(margins.left, margins.top, margins.right, margins.bottom);
}

void EditorRenderPipeline::setScaleFactor(double scale) {
    if (m_context.scaleFactor != scale) {
        m_context.scaleFactor = scale;
        if (m_textSource) {
            m_textSource->setTextWidth(m_context.effectiveTextWidth());
            m_heightDirty = true;
        }
        markAllDirty();
    }
}

void EditorRenderPipeline::setTextWidth(double width) {
    if (m_context.textWidth != width) {
        m_context.textWidth = width;
        if (m_textSource) {
            m_textSource->setTextWidth(m_context.effectiveTextWidth());
            m_heightDirty = true;
        }
        markAllDirty();
    }
}

void EditorRenderPipeline::setFont(const QFont& font) {
    if (m_context.font != font) {
        m_context.font = font;
        if (m_textSource) {
            m_textSource->setFont(font);
            m_heightDirty = true;
        }
        markAllDirty();
    }
}

void EditorRenderPipeline::setTextColor(const QColor& color) {
    if (m_context.colors.text != color) {
        m_context.colors.text = color;
        markAllDirty();
    }
}

void EditorRenderPipeline::setBackgroundColor(const QColor& color) {
    if (m_context.colors.background != color) {
        m_context.colors.background = color;
        markAllDirty();
    }
}

void EditorRenderPipeline::setViewMode(ViewMode mode) {
    if (m_context.viewMode != mode) {
        m_context.viewMode = mode;
        markAllDirty();
    }
}

void EditorRenderPipeline::setScrollY(double y) {
    m_context.scrollY = y;
    updateVisibleRange();
}

void EditorRenderPipeline::setViewportSize(const QSizeF& size) {
    if (m_context.viewportSize == size) {
        return;  // No change
    }
    m_context.viewportSize = size;
    updateVisibleRange();
}

// =============================================================================
// Cursor & Selection
// =============================================================================

void EditorRenderPipeline::setCursorPosition(const CursorPosition& position) {
    if (m_cursorPosition != position) {
        // Mark old cursor position dirty
        markDirty(cursorRect().toAlignedRect());

        m_cursorPosition = position;

        // Mark new cursor position dirty
        markDirty(cursorRect().toAlignedRect());

        // Update focus mode if enabled
        if (m_context.focusMode.enabled) {
            m_context.focusMode.focusedParagraph = position.paragraph;
            markAllDirty();
        }
    }
}

void EditorRenderPipeline::setCursorVisible(bool visible) {
    if (m_context.cursor.visible != visible) {
        m_context.cursor.visible = visible;
        markDirty(cursorRect().toAlignedRect());
    }
}

void EditorRenderPipeline::setCursorBlinkState(bool on) {
    if (m_context.cursor.blinkState != on) {
        m_context.cursor.blinkState = on;
        markDirty(cursorRect().toAlignedRect());
        emit cursorBlinkChanged(on);
    }
}

void EditorRenderPipeline::setCursorStyle(int style) {
    if (m_cursorStyle != style) {
        markDirty(cursorRect().toAlignedRect());
        m_cursorStyle = style;
        markDirty(cursorRect().toAlignedRect());
    }
}

void EditorRenderPipeline::startCursorBlink() {
    if (m_context.cursor.blinkInterval > 0) {
        m_context.cursor.blinkState = true;
        m_cursorBlinkTimer.start(m_context.cursor.blinkInterval);
    }
}

void EditorRenderPipeline::stopCursorBlink() {
    m_cursorBlinkTimer.stop();
    m_context.cursor.blinkState = true;  // Keep cursor visible when not blinking
    markDirty(cursorRect().toAlignedRect());
}

void EditorRenderPipeline::onCursorBlinkTimeout() {
    m_context.cursor.blinkState = !m_context.cursor.blinkState;
    markDirty(cursorRect().toAlignedRect());
    emit cursorBlinkChanged(m_context.cursor.blinkState);
}

void EditorRenderPipeline::setSelection(const SelectionRange& selection) {
    if (m_selection.start != selection.start || m_selection.end != selection.end) {
        // Mark old selection dirty
        SelectionRange oldSel = m_selection.normalized();
        for (int i = oldSel.start.paragraph; i <= oldSel.end.paragraph; ++i) {
            markParagraphDirty(static_cast<size_t>(i));
        }

        m_selection = selection;

        // Mark new selection dirty
        SelectionRange newSel = m_selection.normalized();
        for (int i = newSel.start.paragraph; i <= newSel.end.paragraph; ++i) {
            markParagraphDirty(static_cast<size_t>(i));
        }
    }
}

bool EditorRenderPipeline::hasSelection() const {
    return m_selection.start != m_selection.end;
}

void EditorRenderPipeline::clearSelection() {
    setSelection(SelectionRange{});
}

QRectF EditorRenderPipeline::cursorRect() const {
    if (!m_textSource) {
        return QRectF();
    }

    int paraIndex = m_cursorPosition.paragraph;
    if (paraIndex < 0 || static_cast<size_t>(paraIndex) >= m_textSource->paragraphCount()) {
        return QRectF();
    }

    QTextLayout* layout = m_textSource->layout(static_cast<size_t>(paraIndex));
    if (!layout || layout->lineCount() == 0) {
        // Fallback: return default cursor rect
        double widgetY = paragraphWidgetY(static_cast<size_t>(paraIndex));
        QFontMetricsF fm(m_context.font);
        return QRectF(m_context.margins.left, widgetY,
                      m_context.cursor.width, fm.height());
    }

    // Find line containing cursor
    int offset = m_cursorPosition.offset;
    QTextLine line;
    for (int i = 0; i < layout->lineCount(); ++i) {
        line = layout->lineAt(i);
        if (offset >= line.textStart() &&
            offset <= line.textStart() + line.textLength()) {
            break;
        }
    }

    if (!line.isValid()) {
        line = layout->lineAt(layout->lineCount() - 1);
    }

    // Calculate cursor X position
    qreal cursorX = line.cursorToX(offset);

    // Convert to widget coordinates
    double docY = m_textSource->paragraphY(static_cast<size_t>(paraIndex));
    double widgetY = m_context.documentToWidgetY(docY) + line.y() * m_context.scaleFactor;
    double widgetX = m_context.documentToWidgetX(cursorX);

    return QRectF(widgetX, widgetY,
                  m_context.cursor.width,
                  line.height() * m_context.scaleFactor);
}

// =============================================================================
// Integration
// =============================================================================

void EditorRenderPipeline::setViewportManager(ViewportManager* viewport) {
    m_viewportManager = viewport;
}

void EditorRenderPipeline::setSearchEngine(SearchEngine* engine) {
    m_searchEngine = engine;
}

// =============================================================================
// Main Render Entry Point (Stage 3+4)
// =============================================================================

void EditorRenderPipeline::render(QPainter* painter, const QRect& clipRect) {
    if (!painter) return;

    painter->save();
    painter->setClipRect(clipRect);

    // Stage 1+2: Get visible range and ensure layouts
    updateVisibleRange();

    if (m_textSource) {
        m_textSource->ensureLayouted(
            m_context.firstVisibleParagraph,
            m_context.lastVisibleParagraph
        );
    }

    // Stage 4: Render
    renderBackground(painter, clipRect);
    renderTextFrameBorder(painter);
    renderParagraphs(painter, clipRect);
    renderSelection(painter, clipRect);
    renderCommentHighlights(painter, clipRect);
    renderMarkerHighlights(painter, clipRect);
    renderSearchHighlights(painter, clipRect);
    renderFocusOverlay(painter, clipRect);
    renderCursor(painter);

    painter->restore();

    // Clear dirty region after paint
    clearDirtyRegion();
}

// =============================================================================
// Dirty Region Tracking
// =============================================================================

void EditorRenderPipeline::markAllDirty() {
    int w = static_cast<int>(m_context.viewportSize.width());
    int h = static_cast<int>(m_context.viewportSize.height());
    if (w > 0 && h > 0) {
        m_dirtyRegion = QRegion(0, 0, w, h);
        emit repaintRequested(m_dirtyRegion);
    }
}

void EditorRenderPipeline::markDirty(const QRect& region) {
    if (!region.isEmpty()) {
        m_dirtyRegion = m_dirtyRegion.united(region);
        emit repaintRequested(QRegion(region));
    }
}

void EditorRenderPipeline::markParagraphDirty(size_t paragraphIndex) {
    if (!m_textSource) return;

    double docY = m_textSource->paragraphY(paragraphIndex);
    double widgetY = m_context.documentToWidgetY(docY);
    double height = m_textSource->paragraphHeight(paragraphIndex) * m_context.scaleFactor;

    QRect rect(0, static_cast<int>(widgetY),
               static_cast<int>(m_context.viewportSize.width()),
               static_cast<int>(height + 1));

    markDirty(rect);
}

void EditorRenderPipeline::clearDirtyRegion() {
    m_dirtyRegion = QRegion();
}

// =============================================================================
// Internal Render Methods
// =============================================================================

void EditorRenderPipeline::renderBackground(QPainter* painter, const QRect& clipRect) {
    painter->fillRect(clipRect, m_context.colors.background);
}

void EditorRenderPipeline::renderTextFrameBorder(QPainter* painter) {
    if (!m_context.showTextFrameBorder || !m_textSource) return;

    // Calculate text area rectangle based on document content (not viewport)
    double docHeight = m_textSource->totalHeight();
    if (docHeight <= 0) return;

    // Frame surrounds the document content, scrolling with it
    double left = m_context.margins.left;
    double top = m_context.documentToWidgetY(0);  // Top of document in widget coords
    double width = m_context.effectiveTextWidth();
    double height = docHeight * m_context.scaleFactor;

    QRectF textFrame(left, top, width, height);

    painter->save();
    painter->setPen(QPen(m_context.textFrameBorderColor, m_context.textFrameBorderWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(textFrame);
    painter->restore();
}

void EditorRenderPipeline::renderParagraphs(QPainter* painter, const QRect& clipRect) {
    if (!m_textSource) return;

    size_t first = m_context.firstVisibleParagraph;
    size_t last = m_context.lastVisibleParagraph;
    size_t count = m_textSource->paragraphCount();

    for (size_t i = first; i <= last && i < count; ++i) {
        double docY = m_textSource->paragraphY(i);
        double widgetY = m_context.documentToWidgetY(docY);
        double height = m_textSource->paragraphHeight(i) * m_context.scaleFactor;

        // Check if paragraph intersects clip rect
        QRectF paraRect(0, widgetY, m_context.viewportSize.width(), height);
        if (paraRect.intersects(clipRect)) {
            renderParagraph(painter, i, widgetY);
        }
    }
}

void EditorRenderPipeline::renderParagraph(QPainter* painter, size_t index, double widgetY) {
    QTextLayout* layout = m_textSource->layout(index);
    if (!layout) return;

    // Draw position with margins and scale
    QPointF drawPos(m_context.margins.left, widgetY);

    // Determine text color (focus mode dimming)
    bool isDimmed = m_context.focusMode.enabled &&
                    static_cast<int>(index) != m_context.focusMode.focusedParagraph;

    QColor textColor = isDimmed ? m_context.colors.inactiveText : m_context.colors.text;
    painter->setPen(textColor);

    // Apply scale transform
    painter->save();
    painter->translate(drawPos);
    painter->scale(m_context.scaleFactor, m_context.scaleFactor);

    // Draw the layout
    layout->draw(painter, QPointF(0, 0));

    painter->restore();
}

void EditorRenderPipeline::renderSelection(QPainter* painter, [[maybe_unused]] const QRect& clipRect) {
    if (!hasSelection() || !m_textSource) return;

    SelectionRange sel = m_selection.normalized();
    size_t count = m_textSource->paragraphCount();

    for (int para = sel.start.paragraph; para <= sel.end.paragraph; ++para) {
        if (para < 0 || static_cast<size_t>(para) >= count) continue;

        // Calculate selection bounds for this paragraph
        QString text = m_textSource->paragraphText(static_cast<size_t>(para));
        int textLen = text.length();

        int startOffset = (para == sel.start.paragraph) ? sel.start.offset : 0;
        int endOffset = (para == sel.end.paragraph)
                            ? std::min(sel.end.offset, textLen)
                            : textLen;

        if (startOffset < endOffset) {
            double docY = m_textSource->paragraphY(static_cast<size_t>(para));
            double widgetY = m_context.documentToWidgetY(docY);

            renderParagraphSelection(painter, static_cast<size_t>(para),
                                     startOffset, endOffset, widgetY);
        }
    }
}

void EditorRenderPipeline::renderParagraphSelection(QPainter* painter, size_t paraIndex,
                                                     int startOffset, int endOffset,
                                                     double widgetY) {
    QTextLayout* layout = m_textSource->layout(paraIndex);
    if (!layout) return;

    // Find lines containing selection
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        int lineStart = line.textStart();
        int lineEnd = lineStart + line.textLength();

        // Check if selection intersects this line
        if (startOffset < lineEnd && endOffset > lineStart) {
            int selStart = std::max(startOffset, lineStart);
            int selEnd = std::min(endOffset, lineEnd);

            qreal x1 = line.cursorToX(selStart);
            qreal x2 = line.cursorToX(selEnd);
            if (x1 > x2) std::swap(x1, x2);

            // Convert to widget coordinates
            double wx1 = m_context.documentToWidgetX(x1);
            double wx2 = m_context.documentToWidgetX(x2);
            double wy = widgetY + line.y() * m_context.scaleFactor;
            double wh = line.height() * m_context.scaleFactor;

            QRectF selRect(wx1, wy, wx2 - wx1, wh);
            painter->fillRect(selRect, m_context.colors.selection);
        }
    }
}

void EditorRenderPipeline::renderSearchHighlights(QPainter* painter, const QRect& clipRect) {
    if (!m_searchEngine || !m_searchEngine->isActive()) return;

    const auto& matches = m_searchEngine->matches();
    int currentIdx = m_searchEngine->currentMatchIndex();

    for (size_t i = 0; i < matches.size(); ++i) {
        const auto& match = matches[i];

        QRectF matchRect = getTextRect(static_cast<size_t>(match.paragraph),
                                       match.paragraphOffset,
                                       static_cast<int>(match.length));

        if (matchRect.isEmpty() || !matchRect.intersects(clipRect)) continue;

        QColor color = (static_cast<int>(i) == currentIdx)
                           ? m_context.colors.currentMatch
                           : m_context.colors.searchHighlight;

        painter->fillRect(matchRect, color);
    }
}

void EditorRenderPipeline::renderCursor(QPainter* painter) {
    if (!m_context.cursor.visible || !m_context.cursor.blinkState) return;

    QRectF rect = cursorRect();
    if (rect.isEmpty()) return;

    // Adjust rect based on cursor style
    // m_cursorStyle: 0=Line, 1=Block, 2=Underline
    if (m_cursorStyle == 1) {
        // Block cursor: use character width
        if (m_textSource) {
            QTextLayout* layout = m_textSource->layout(
                static_cast<size_t>(m_cursorPosition.paragraph));
            if (layout && layout->lineCount() > 0) {
                QString text = m_textSource->paragraphText(
                    static_cast<size_t>(m_cursorPosition.paragraph));
                if (m_cursorPosition.offset < text.length()) {
                    // Get width of character at cursor
                    QFontMetricsF fm(m_context.font);
                    QChar ch = text.at(m_cursorPosition.offset);
                    double charWidth = fm.horizontalAdvance(ch) * m_context.scaleFactor;
                    if (charWidth > 0) {
                        rect.setWidth(charWidth);
                    }
                }
            }
        }
        // Semi-transparent block to show character underneath
        QColor blockColor = m_context.colors.cursor;
        blockColor.setAlpha(180);
        painter->fillRect(rect, blockColor);
    } else if (m_cursorStyle == 2) {
        // Underline cursor
        double underlineHeight = 2.0 * m_context.scaleFactor;
        QFontMetricsF fm(m_context.font);
        double charWidth = fm.averageCharWidth() * m_context.scaleFactor;
        rect.setTop(rect.bottom() - underlineHeight);
        rect.setHeight(underlineHeight);
        rect.setWidth(charWidth);
        painter->fillRect(rect, m_context.colors.cursor);
    } else {
        // Line cursor (default)
        painter->fillRect(rect, m_context.colors.cursor);
    }
}

void EditorRenderPipeline::renderFocusOverlay([[maybe_unused]] QPainter* painter,
                                              [[maybe_unused]] const QRect& clipRect) {
    if (!m_context.focusMode.enabled || !m_textSource) return;

    // Focus overlay is handled in renderParagraph via dimming
    // This method can be extended for more complex focus effects
}

void EditorRenderPipeline::renderMarkerHighlights(QPainter* painter, const QRect& clipRect) {
    if (!m_textSource) return;

    // Get visible paragraph range
    size_t firstPara = m_context.firstVisibleParagraph;
    size_t lastPara = m_context.lastVisibleParagraph;
    size_t count = m_textSource->paragraphCount();

    // Check each visible paragraph for TODO/NOTE markers
    for (size_t para = firstPara; para <= lastPara && para < count; ++para) {
        // Check if paragraph has TODO marker in first fragment
        // Markers are stored in QTextCharFormat properties via KmlPropTodo
        bool hasTodo = false;
        bool isCompleted = false;
        bool isNote = false;

        // Get paragraph text to check for TODO/NOTE markers
        QString text = m_textSource->paragraphText(para);

        // Simple marker detection: look for [TODO], [NOTE], [DONE] at start
        if (text.startsWith("[TODO]") || text.startsWith("TODO:")) {
            hasTodo = true;
        } else if (text.startsWith("[DONE]") || text.startsWith("[x]")) {
            hasTodo = true;
            isCompleted = true;
        } else if (text.startsWith("[NOTE]") || text.startsWith("NOTE:")) {
            isNote = true;
            hasTodo = true;  // Treat as marker
        }

        if (!hasTodo) continue;

        // Get line rect for this paragraph
        double docY = m_textSource->paragraphY(para);
        double widgetY = m_context.documentToWidgetY(docY);
        double height = m_textSource->paragraphHeight(para) * m_context.scaleFactor;

        QRectF lineRect(0, widgetY, m_context.viewportSize.width(), height);

        if (!lineRect.toRect().intersects(clipRect)) continue;

        // Choose color based on type and completion state
        QColor highlightColor;
        if (!isNote) {
            highlightColor = isCompleted ? m_context.colors.completedTodo
                                        : m_context.colors.todoHighlight;
        } else {
            highlightColor = m_context.colors.noteHighlight;
        }

        // Draw small indicator in left margin
        qreal iconSize = 8.0 * m_context.scaleFactor;
        qreal marginX = lineRect.left() + 2;
        qreal centerY = lineRect.center().y();

        QRectF iconRect(marginX, centerY - iconSize / 2, iconSize, iconSize);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        if (!isNote) {
            // Draw checkbox for TODO
            QPen pen(highlightColor.darker(150));
            pen.setWidth(1);
            painter->setPen(pen);
            painter->setBrush(isCompleted ? highlightColor : Qt::NoBrush);
            painter->drawRect(iconRect);

            if (isCompleted) {
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

void EditorRenderPipeline::renderCommentHighlights(QPainter* painter, const QRect& clipRect) {
    if (!m_textSource) return;

    // Get visible paragraph range
    size_t firstPara = m_context.firstVisibleParagraph;
    size_t lastPara = m_context.lastVisibleParagraph;
    size_t count = m_textSource->paragraphCount();

    // Comment detection is based on KML format properties
    // For now, we scan for comment patterns in text
    for (size_t para = firstPara; para <= lastPara && para < count; ++para) {
        QString text = m_textSource->paragraphText(para);

        // Look for comment markers: /* ... */ or <!-- ... -->
        int commentStart = -1;
        int commentEnd = -1;

        // HTML-style comments
        int htmlStart = text.indexOf("<!--");
        if (htmlStart >= 0) {
            commentStart = htmlStart;
            int htmlEnd = text.indexOf("-->", htmlStart + 4);
            commentEnd = (htmlEnd >= 0) ? htmlEnd + 3 : text.length();
        }

        // C-style comments
        int cStart = text.indexOf("/*");
        if (cStart >= 0 && (commentStart < 0 || cStart < commentStart)) {
            commentStart = cStart;
            int cEnd = text.indexOf("*/", cStart + 2);
            commentEnd = (cEnd >= 0) ? cEnd + 2 : text.length();
        }

        if (commentStart < 0) continue;

        // Get visual rectangle for comment range
        QRectF commentRect = getTextRect(para, commentStart, commentEnd - commentStart);

        if (!commentRect.isEmpty() && commentRect.toRect().intersects(clipRect)) {
            // Fill background
            painter->fillRect(commentRect, m_context.colors.commentHighlight);

            // Draw underline
            QPen pen(m_context.colors.commentBorder);
            pen.setWidth(2);
            painter->setPen(pen);
            painter->drawLine(
                QPointF(commentRect.left(), commentRect.bottom() - 1),
                QPointF(commentRect.right(), commentRect.bottom() - 1));
        }
    }
}

// =============================================================================
// Layout Helpers
// =============================================================================

void EditorRenderPipeline::updateVisibleRange() {
    if (!m_textSource || m_context.viewportSize.isEmpty()) {
        m_context.firstVisibleParagraph = 0;
        m_context.lastVisibleParagraph = 0;
        return;
    }

    // Use viewport manager if available
    if (m_viewportManager) {
        m_context.firstVisibleParagraph = m_viewportManager->firstVisibleParagraph();
        m_context.lastVisibleParagraph = m_viewportManager->lastVisibleParagraph();
        return;
    }

    // Calculate visible range from scroll position
    double viewTop = m_context.scrollY;
    double viewBottom = viewTop + m_context.viewportSize.height() / m_context.scaleFactor;

    m_context.firstVisibleParagraph = m_textSource->paragraphAtY(viewTop);
    m_context.lastVisibleParagraph = m_textSource->paragraphAtY(viewBottom);

    // Clamp to valid range
    size_t count = m_textSource->paragraphCount();
    if (m_context.lastVisibleParagraph >= count && count > 0) {
        m_context.lastVisibleParagraph = count - 1;
    }
}

double EditorRenderPipeline::paragraphWidgetY(size_t index) const {
    if (!m_textSource) return m_context.margins.top;

    double docY = m_textSource->paragraphY(index);
    return m_context.documentToWidgetY(docY);
}

QRectF EditorRenderPipeline::getTextRect(size_t paraIndex, int offset, int length) const {
    if (!m_textSource) return QRectF();

    QTextLayout* layout = m_textSource->layout(paraIndex);
    if (!layout || layout->lineCount() == 0) return QRectF();

    // Find line containing offset
    QTextLine line;
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine l = layout->lineAt(i);
        int lineStart = l.textStart();
        int lineEnd = lineStart + l.textLength();
        if (offset >= lineStart && offset < lineEnd) {
            line = l;
            break;
        }
        if (i == layout->lineCount() - 1) {
            line = l;  // Use last line if offset is beyond
        }
    }

    if (!line.isValid()) return QRectF();

    // Get x positions
    qreal x1 = line.cursorToX(offset);
    qreal x2 = line.cursorToX(offset + length);
    if (x1 > x2) std::swap(x1, x2);

    // Convert to widget coordinates
    double docY = m_textSource->paragraphY(paraIndex);
    double widgetY = m_context.documentToWidgetY(docY) + line.y() * m_context.scaleFactor;
    double widgetX = m_context.documentToWidgetX(x1);
    double width = (x2 - x1) * m_context.scaleFactor;
    double height = line.height() * m_context.scaleFactor;

    return QRectF(widgetX, widgetY, width, height);
}

}  // namespace kalahari::editor
