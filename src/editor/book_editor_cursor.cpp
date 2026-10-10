/// @file book_editor_cursor.cpp
/// @brief BookEditor: the cursor, its movement (Page Up/Down included) and the selection

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <QAbstractTextDocumentLayout>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QTextBoundaryFinder>
#include <QTextLine>
#include <QTimer>
#include <algorithm>
#include <utility>

namespace kalahari::editor {

// Room kept between the cursor and the edges of the view when the view scrolls to the
// cursor (view pixels)
constexpr qreal CURSOR_SCROLL_MARGIN = 30.0;

namespace {

/// @brief A character words are made of: a letter or a digit, with its marks
bool isWordCharacter(QChar ch) {
    return ch.isLetterOrNumber() || ch.isMark();
}

/// @brief The offset one character before or after an offset of a paragraph's text: a
///        letter goes with its marks, a surrogate pair whole
int characterBoundary(const QString& text, int offset, bool forward) {
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    finder.setPosition(offset);
    const qsizetype boundary = forward ? finder.toNextBoundary() : finder.toPreviousBoundary();
    if (boundary < 0) {
        return std::clamp(offset + (forward ? 1 : -1), 0, static_cast<int>(text.size()));
    }
    return static_cast<int>(boundary);
}

}  // namespace

// =============================================================================
// Cursor Position (Phase 3.4)
// =============================================================================

CursorPosition BookEditor::cursorPosition() const
{
    return m_cursorPosition;
}

void BookEditor::setCursorPosition(const CursorPosition& position)
{
    CursorPosition validatedPos = validateCursorPosition(position);

    if (m_cursorPosition != validatedPos) {
        // Track old position for focus mode optimization
        CursorPosition oldPos = m_cursorPosition;
        m_cursorPosition = validatedPos;

        ensureCursorVisible();
        emit cursorPositionChanged(m_cursorPosition);

        // Phase 11.11: Optimized cursor sync - only update cursor, not full state
        syncPipelineCursor();

        // Targeted repaint for cursor movement
        if (m_renderPipeline) {
            // Focus: the paragraph left is dimmed, the one entered is not
            if (m_appearance.focusMode.enabled && oldPos.paragraph != validatedPos.paragraph) {
                update();
            } else {
                // Just cursor moved within same paragraph or no focus mode
                updateCursorArea();
            }
        } else {
            update();
        }
    } else {
        // Position unchanged but still reset cursor blink to visible state
        // This ensures cursor is always visible after a click, even in same position
        resetCursorBlink();
    }
}

bool BookEditor::isCursorVisible() const
{
    return m_cursorVisible;
}

void BookEditor::setCursorBlinkingEnabled(bool enabled)
{
    if (m_cursorBlinkingEnabled == enabled) {
        return;
    }

    m_cursorBlinkingEnabled = enabled;

    if (enabled) {
        // Start blinking
        if (m_cursorBlinkTimer != nullptr && hasFocus()) {
            m_cursorBlinkTimer->start(m_cursorBlinkInterval);
        }
    } else {
        // Stop blinking and keep cursor visible
        if (m_cursorBlinkTimer != nullptr) {
            m_cursorBlinkTimer->stop();
        }
        if (!m_cursorVisible) {
            m_cursorVisible = true;
            if (m_renderPipeline) {
                m_renderPipeline->setCursorBlinkState(true);
            }
            updateCursorArea();
        }
    }
}

bool BookEditor::isCursorBlinkingEnabled() const
{
    return m_cursorBlinkingEnabled;
}

int BookEditor::cursorBlinkInterval() const
{
    return m_cursorBlinkInterval;
}

void BookEditor::setCursorBlinkInterval(int interval)
{
    m_cursorBlinkInterval = qMax(100, interval);  // Minimum 100ms

    if (m_cursorBlinkTimer != nullptr && m_cursorBlinkingEnabled) {
        m_cursorBlinkTimer->setInterval(m_cursorBlinkInterval);
    }
}

void BookEditor::resetCursorBlink()
{
    // Reset blink state to visible and restart timer
    // Used when cursor position is set to same location (click on same spot)
    m_cursorVisible = true;

    if (m_cursorBlinkTimer != nullptr && m_cursorBlinkingEnabled && hasFocus()) {
        m_cursorBlinkTimer->start(m_cursorBlinkInterval);
    }

    // Sync to RenderPipeline (the cursor is drawn only while the editor has focus)
    if (m_renderPipeline) {
        m_renderPipeline->setCursorVisible(hasFocus());
        m_renderPipeline->setCursorBlinkState(true);
    }

    updateCursorArea();
}

void BookEditor::updateCursorArea()
{
    // The pipeline paints the cursor, so it also says where
    const QRectF cursorRect = m_renderPipeline ? m_renderPipeline->cursorPaintRect() : QRectF();
    if (cursorRect.isEmpty()) {
        update();
    } else {
        // Small margin for antialiasing
        update(cursorRect.toAlignedRect().adjusted(-2, -2, 2, 2));
    }
}

void BookEditor::ensureCursorVisible()
{
    // Reset blink state to visible
    m_cursorVisible = true;

    // Restart blink timer if blinking is enabled
    if (m_cursorBlinkTimer != nullptr && m_cursorBlinkingEnabled && hasFocus()) {
        m_cursorBlinkTimer->start(m_cursorBlinkInterval);
    }

    // Sync to RenderPipeline (Phase 12 fix)
    if (m_renderPipeline) {
        m_renderPipeline->setCursorVisible(hasFocus());  // Drawn only while focused
        m_renderPipeline->setCursorBlinkState(true);
    }

    // Typewriter scrolling keeps the cursor line at the focus height, except where the mouse
    // put the cursor: a click or a drag selection leaves the view where it is
    if (m_appearance.typewriter.enabled && !m_pointerMovesCursor) {
        updateTypewriterScroll();
    } else {
        scrollToCursorLine();
    }
    scrollSidewaysToCursor();
}

void BookEditor::scrollToCursorLine()
{
    // Scroll viewport to make cursor visible (only when line is partially clipped)
    if (!m_textBuffer || !m_viewportManager) {
        return;
    }

    // Get current cursor line info
    QTextBlock block = m_textBuffer->findBlockByNumber(static_cast<int>(m_cursorPosition.paragraph));
    if (!block.isValid()) return;

    QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
    if (!layout || layout->lineCount() == 0) return;

    // Find the line containing cursor offset (O(log n) using Qt's binary search)
    QTextLine cursorLine = layout->lineForTextPosition(m_cursorPosition.offset);
    if (!cursorLine.isValid()) {
        // Fallback: cursor at end of block, use last line
        cursorLine = layout->lineAt(layout->lineCount() - 1);
    }

    // Cursor line position in document coordinates: the whole line box, with the line
    // spacing around the glyphs (scrolling up to the first line shows the document top)
    auto* kalahariLayout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout());
    const QRectF lineBox = KalahariTextDocumentLayout::lineBox(
        cursorLine, kalahariLayout ? kalahariLayout->typography().lineSpacing : 1.0);
    const auto lineTop = [&] {
        return m_textBuffer->documentLayout()->blockBoundingRect(block).y() + lineBox.top();
    };

    // The band of the view the line must be within: the view without a margin at its top
    // and bottom edges, as document y relative to the scroll position (the scroll position
    // is drawn at the view's top inset, and the zoom scales by the view scale)
    const qreal scrollY = m_viewportManager->scrollPosition();
    const qreal scale = m_viewportManager->viewScale();
    const qreal inset = m_viewportManager->viewTopInset();
    const qreal bandTop = (CURSOR_SCROLL_MARGIN - inset) / scale;
    const qreal bandBottom =
        (static_cast<qreal>(height()) - CURSOR_SCROLL_MARGIN - inset) / scale;

    // Scroll only if line is NOT fully visible
    if (lineTop() < scrollY + bandTop) {
        // Line is clipped at top - scroll up to show full line; the first line shows the top
        // of the page too, as the last one shows its bottom (the scroll range ends there)
        const bool firstLine = block.blockNumber() == 0 && cursorLine.lineNumber() == 0;
        setScrollOffset(firstLine ? 0.0 : lineTop() - bandTop);
    } else if (lineTop() + lineBox.height() > scrollY + bandBottom) {
        // Line is clipped at bottom - scroll down to show full line. The blocks above it
        // that come into view are laid out first: with estimated heights the line could
        // end up short of the bottom edge or past it.
        qreal newScroll = 0.0;
        do {
            newScroll = lineTop() + lineBox.height() - bandBottom;
        } while (kalahariLayout &&
                 kalahariLayout->ensureLaidOut(
                     kalahariLayout->blockNumberAtY(newScroll + std::min<qreal>(bandTop, 0.0)),
                     block.blockNumber() - 1));
        setScrollOffset(qMax(0.0, newScroll));
    }
    // If line is fully visible, don't scroll
}

void BookEditor::scrollSidewaysToCursor()
{
    // A page wider than the view (a narrow window, a high zoom) scrolls sideways to the
    // cursor, keeping it off the view's edges - only into the view where the mouse put it
    if (!m_renderPipeline || m_renderPipeline->maxScrollX() <= 0.0) {
        return;
    }
    const QRectF caret = m_renderPipeline->caretRect(m_cursorPosition);
    if (caret.isNull()) {
        return;
    }
    const RenderContext& ctx = m_renderPipeline->context();
    const qreal viewWidth = std::max(0.0, ctx.viewportSize.width() - ctx.scrollBarWidth);
    const qreal margin =
        m_pointerMovesCursor ? 0.0 : std::min(CURSOR_SCROLL_MARGIN, viewWidth / 4.0);
    if (caret.left() < margin) {
        setHorizontalScrollOffset(ctx.scrollX + caret.left() - margin);
    } else if (caret.right() > viewWidth - margin) {
        setHorizontalScrollOffset(ctx.scrollX + caret.right() - (viewWidth - margin));
    }
}

// =============================================================================
// Cursor Navigation (Phase 3.6/3.7/3.8)
// =============================================================================

void BookEditor::moveCursorLeft()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Invalidate preferred X position (horizontal movement resets it)
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;

    if (newPos.offset > 0) {
        newPos.offset = characterBoundary(
            m_textBuffer->findBlockByNumber(newPos.paragraph).text(), newPos.offset, false);
    } else if (newPos.paragraph > 0) {
        --newPos.paragraph;
        newPos.offset = paragraphLength(m_textBuffer.get(), newPos.paragraph);
    }
    // else: already at document start, do nothing

    setCursorPosition(newPos);
    // NOTE: ensureCursorVisible() is already called inside setCursorPosition()
}

void BookEditor::moveCursorRight()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Invalidate preferred X position (horizontal movement resets it)
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;
    int paraLen = paragraphLength(m_textBuffer.get(), newPos.paragraph);

    if (newPos.offset < paraLen) {
        newPos.offset = characterBoundary(
            m_textBuffer->findBlockByNumber(newPos.paragraph).text(), newPos.offset, true);
    } else if (newPos.paragraph + 1 < m_textBuffer->blockCount()) {
        ++newPos.paragraph;
        newPos.offset = 0;
    }
    // else: already at document end, do nothing

    setCursorPosition(newPos);
    // NOTE: ensureCursorVisible() is already called inside setCursorPosition()
}

void BookEditor::moveCursorUp()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid()) return;

    QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
    if (!layout || layout->lineCount() == 0) return;

    // Find current line within this paragraph
    int currentLine = layout->lineForTextPosition(m_cursorPosition.offset).lineNumber();

    // Remember preferred X position for vertical navigation (it holds while the cursor stays
    // where the last vertical move put it: typing or a click drops it)
    if (!m_preferredCursorXValid || m_preferredCursorXPosition != m_cursorPosition) {
        QTextLine line = layout->lineAt(currentLine);
        m_preferredCursorX = line.cursorToX(m_cursorPosition.offset);
        m_preferredCursorXValid = true;
    }

    CursorPosition newPos = m_cursorPosition;

    if (currentLine > 0) {
        // Move to previous line within same paragraph
        QTextLine prevLine = layout->lineAt(currentLine - 1);
        newPos.offset = prevLine.xToCursor(m_preferredCursorX);
    } else if (newPos.paragraph > 0) {
        // Move to last line of previous paragraph
        --newPos.paragraph;
        QTextLayout* prevLayout = KalahariTextDocumentLayout::blockLayout(
            m_textBuffer->findBlockByNumber(newPos.paragraph));
        if (prevLayout && prevLayout->lineCount() > 0) {
            QTextLine lastLine = prevLayout->lineAt(prevLayout->lineCount() - 1);
            newPos.offset = lastLine.xToCursor(m_preferredCursorX);
        } else {
            newPos.offset = paragraphLength(m_textBuffer.get(), newPos.paragraph);
        }
    } else {
        // At first line of first paragraph: move to start
        newPos.offset = 0;
        m_preferredCursorXValid = false;
    }

    setCursorPosition(newPos);
    m_preferredCursorXPosition = m_cursorPosition;
    // NOTE: ensureCursorVisible() is already called inside setCursorPosition()
}

void BookEditor::moveCursorDown()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid()) return;

    QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
    if (!layout || layout->lineCount() == 0) return;

    // Find current line within this paragraph
    int currentLine = layout->lineForTextPosition(m_cursorPosition.offset).lineNumber();

    // Remember preferred X position for vertical navigation (it holds while the cursor stays
    // where the last vertical move put it: typing or a click drops it)
    if (!m_preferredCursorXValid || m_preferredCursorXPosition != m_cursorPosition) {
        QTextLine line = layout->lineAt(currentLine);
        m_preferredCursorX = line.cursorToX(m_cursorPosition.offset);
        m_preferredCursorXValid = true;
    }

    CursorPosition newPos = m_cursorPosition;

    if (currentLine < layout->lineCount() - 1) {
        // Move to next line within same paragraph
        QTextLine nextLine = layout->lineAt(currentLine + 1);
        newPos.offset = nextLine.xToCursor(m_preferredCursorX);
    } else if (newPos.paragraph + 1 < m_textBuffer->blockCount()) {
        // Move to first line of next paragraph
        ++newPos.paragraph;
        QTextLayout* nextLayout = KalahariTextDocumentLayout::blockLayout(
            m_textBuffer->findBlockByNumber(newPos.paragraph));
        if (nextLayout && nextLayout->lineCount() > 0) {
            QTextLine firstLine = nextLayout->lineAt(0);
            newPos.offset = firstLine.xToCursor(m_preferredCursorX);
        } else {
            newPos.offset = 0;
        }
    } else {
        // At last line of last paragraph: move to end
        newPos.offset = paragraphLength(m_textBuffer.get(), newPos.paragraph);
        m_preferredCursorXValid = false;
    }

    setCursorPosition(newPos);
    m_preferredCursorXPosition = m_cursorPosition;
    // NOTE: ensureCursorVisible() is already called inside setCursorPosition()
}

void BookEditor::moveCursorWordLeft()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }
    m_preferredCursorXValid = false;
    setCursorPosition(textKeyTarget(TextKeyAction::WordLeft, m_cursorPosition));
}

void BookEditor::moveCursorWordRight()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }
    m_preferredCursorXValid = false;
    setCursorPosition(textKeyTarget(TextKeyAction::WordRight, m_cursorPosition));
}

void BookEditor::moveCursorToLineStart()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }
    m_preferredCursorXValid = false;
    setCursorPosition(lineStartOf(m_cursorPosition));
}

void BookEditor::moveCursorToLineEnd()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }
    m_preferredCursorXValid = false;
    setCursorPosition(lineEndOf(m_cursorPosition));
}

void BookEditor::moveCursorToDocStart()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    setCursorPosition({0, 0});

    // Scroll to top (typewriter scrolling has put the first line at its height instead)
    if (!m_appearance.typewriter.enabled) {
        setScrollOffset(0.0);
    }
}

void BookEditor::moveCursorToDocEnd()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    int lastPara = m_textBuffer->blockCount() - 1;
    QTextBlock lastBlock = m_textBuffer->lastBlock();
    int lastOffset = lastBlock.isValid() ? lastBlock.length() - 1 : 0;
    if (lastOffset < 0) lastOffset = 0;

    setCursorPosition({lastPara, lastOffset});

    // Scroll to bottom (typewriter scrolling has put the last line at its height instead)
    if (!m_appearance.typewriter.enabled) {
        setScrollOffset(m_viewportManager->maxScrollPosition());
    }
}

void BookEditor::moveCursorPageUp()
{
    moveCursorByViewHeight(-1.0);
}

void BookEditor::moveCursorPageDown()
{
    moveCursorByViewHeight(1.0);
}

void BookEditor::moveCursorByViewHeight(double direction)
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0 || !m_viewportManager ||
        !m_renderPipeline) {
        return;
    }
    const double viewHeight = m_viewportManager->visibleDocumentHeight();
    if (viewHeight <= 0.0) {
        return;
    }
    m_renderPipeline->ensureVisibleLaidOut();

    QTextDocument* doc = m_textBuffer.get();
    const auto viewTop = [this] {
        return m_renderPipeline->widgetToDocument(QPointF(0.0, 0.0)).y();
    };
    const auto caretTop = [this](const CursorPosition& position) {
        return m_renderPipeline->widgetToDocument(m_renderPipeline->caretRect(position).topLeft())
            .y();
    };
    // The cursor at a position, its line in a row of the view
    const auto placeInRow = [&](const CursorPosition& position, double row) {
        setScrollOffset(scrollOffset() + caretTop(position) - row - viewTop());
        setCursorPosition(position);
    };

    // The cursor's row: how far below the top of the view its line starts (a cursor out of
    // view is taken at the nearest edge). The move keeps it, as in word processors.
    const double caretHeight =
        m_renderPipeline->caretRect(m_cursorPosition).height() / m_viewportManager->viewScale();
    const double row = std::clamp(caretTop(m_cursorPosition) - viewTop(), 0.0,
                                  std::max(0.0, viewHeight - caretHeight));

    // A press right after presses the other way takes the cursor back where each of them
    // found it, so that Page Down and then Page Up bring back the same character in the
    // same row (another move of the cursor ends the run)
    if (m_pageMoveCursor != m_cursorPosition) {
        m_pageMoves.clear();
    }
    if (!m_pageMoves.empty() && m_pageMovesDirection == -direction) {
        const PageMove back = m_pageMoves.back();
        m_pageMoves.pop_back();
        if (back.cursor.paragraph < doc->blockCount() &&
            back.cursor.offset <= paragraphLength(doc, back.cursor.paragraph)) {
            placeInRow(back.cursor, back.row);
            m_pageMoveCursor = m_cursorPosition;
            m_preferredCursorXPosition = m_cursorPosition;
            return;
        }
        m_pageMoves.clear();
    }

    // The lines from the cursor's line on, the way of the move. Their tops are measured from
    // the cursor's line: laying a block out moves the lines after it, that line among them.
    struct Line {
        int block = 0;
        int index = 0;
    };
    const auto textLine = [doc](const Line& line) {
        QTextLayout* layout =
            KalahariTextDocumentLayout::blockLayout(doc->findBlockByNumber(line.block));
        return layout && line.index < layout->lineCount() ? layout->lineAt(line.index)
                                                          : QTextLine();
    };
    const auto lineTop = [doc, &textLine](const Line& line) {
        const QTextLine text = textLine(line);
        return doc->documentLayout()->blockBoundingRect(doc->findBlockByNumber(line.block)).top() +
               (text.isValid() ? text.y() : 0.0);
    };
    // The line below (way 1) or above (way -1) a line; false at the end (start) of the text
    const auto step = [doc](Line& line, double way) {
        if (way > 0) {
            QTextLayout* layout =
                KalahariTextDocumentLayout::blockLayout(doc->findBlockByNumber(line.block));
            if (layout && line.index + 1 < layout->lineCount()) {
                ++line.index;
            } else if (line.block + 1 < doc->blockCount()) {
                line = {line.block + 1, 0};
            } else {
                return false;
            }
        } else if (line.index > 0) {
            --line.index;
        } else if (line.block > 0) {
            QTextLayout* layout =
                KalahariTextDocumentLayout::blockLayout(doc->findBlockByNumber(line.block - 1));
            line = {line.block - 1, layout ? std::max(0, layout->lineCount() - 1) : 0};
        } else {
            return false;
        }
        return true;
    };
    const auto next = [&step, direction](Line& line) { return step(line, direction); };

    QTextLayout* cursorLayout = KalahariTextDocumentLayout::blockLayout(
        doc->findBlockByNumber(m_cursorPosition.paragraph));
    if (!cursorLayout || cursorLayout->lineCount() == 0) {
        return;
    }
    const QTextLine cursorLine = cursorLayout->lineForTextPosition(m_cursorPosition.offset);
    const Line start{m_cursorPosition.paragraph,
                     cursorLine.isValid() ? cursorLine.lineNumber()
                                          : cursorLayout->lineCount() - 1};
    // The column the vertical moves keep, or the cursor's
    const double x = m_preferredCursorXValid && m_preferredCursorXPosition == m_cursorPosition
        ? m_preferredCursorX
        : textLine(start).cursorToX(m_cursorPosition.offset);

    const double pitch = m_renderPipeline->context().computed.pagePitch;
    if (m_viewMode == ViewMode::Page && pitch > 0.0) {
        // Page mode moves by one page: the next (previous) page is shown where this one
        // was, the cursor on the same line of it, counted from the top of the page (the
        // lines of two pages do not line up where their paragraphs differ). The text down
        // to the cursor and as far as that view is laid out first: estimated heights above
        // a page shift its text, and laying a block out breaks the pages below it anew.
        if (auto* layout = qobject_cast<KalahariTextDocumentLayout*>(doc->documentLayout())) {
            layout->ensureLaidOut(0, m_cursorPosition.paragraph);
        }
        Line ahead = start;
        while (next(ahead) &&
               direction * (lineTop(ahead) - lineTop(start)) <= pitch + viewHeight) {
            // lineTop() has laid the line's block out
        }
        const auto pageOf = [&](const Line& line) {
            return m_renderPipeline->pageAtDocumentY(lineTop(line));
        };

        // The cursor's line on its page. The presses this way keep the line the first of
        // them started from: a page with fewer lines takes the cursor to its last line,
        // the next page back to that line.
        const int page = pageOf(start);
        int pageLine = 0;
        for (Line line = start; step(line, -1.0) && pageOf(line) == page;) {
            ++pageLine;
        }
        const int goalLine = m_pageMoves.empty() ? pageLine : m_pageMoves.front().pageLine;

        // The first line on the next (previous) page, the end (start) of the text without
        // one; down from it to the goal line
        Line target = start;
        bool otherPage = false;
        while (!otherPage && next(target)) {
            otherPage = pageOf(target) != page;
        }
        if (otherPage) {
            const int targetPage = pageOf(target);
            if (direction < 0) {
                // The walk up came to the page's last line: its first line
                for (Line above = target; step(above, -1.0) && pageOf(above) == targetPage;) {
                    target = above;
                }
            }
            for (int i = 0; i < goalLine; ++i) {
                Line below = target;
                if (!step(below, 1.0) || pageOf(below) != targetPage) {
                    break;
                }
                target = below;
            }
        }

        const double cursorTop = caretTop(m_cursorPosition);
        const double cursorRow = std::clamp(cursorTop - viewTop(), 0.0,
                                            std::max(0.0, viewHeight - caretHeight));
        const double newViewTop = cursorTop - cursorRow + direction * pitch;
        const QTextLine targetLine = textLine(target);
        const CursorPosition to{target.block, targetLine.isValid() ? targetLine.xToCursor(x) : 0};
        if (to == m_cursorPosition) {
            return;  // At the start or the end of the text
        }
        if (m_pageMoves.empty()) {
            m_pageMovesDirection = direction;
        }
        m_pageMoves.push_back({m_cursorPosition, cursorRow, pageLine});
        placeInRow(to, caretTop(to) - newViewTop);
        m_pageMoveCursor = m_cursorPosition;
        m_preferredCursorX = x;
        m_preferredCursorXValid = true;
        m_preferredCursorXPosition = m_cursorPosition;
        return;
    }

    // The line the cursor goes to: as far as one view height, but no farther than the first
    // line not fully in view at the bottom (top) of the view comes fully into view at the
    // top (bottom), so that no line is skipped. Lines between the pages count as nothing.
    Line target = start;
    double targetDistance = 0.0;
    std::optional<double> limit;  // Known once that line is reached
    std::optional<Line> nearest;
    for (Line line = start; next(line);) {
        const double top = lineTop(line);  // Lays the line's block out first
        const double distance = direction * (top - lineTop(start));
        const double height = textLine(line).height();
        if (!nearest) {
            nearest = line;
        }
        if (!limit) {
            if (direction > 0 && distance + height > viewHeight - row) {
                limit = distance + row;
            } else if (direction < 0 && distance > row) {
                limit = viewHeight - row - (height - distance);
            }
        }
        if (limit && distance > *limit) {
            // Up, the lines that come into view above the target are laid out before the
            // view goes there (laid out, they would move it)
            if (direction < 0 && distance <= targetDistance + row) {
                continue;
            }
            break;
        }
        target = line;
        targetDistance = distance;
    }
    // A view lower than two lines still moves by one
    if (target.block == start.block && target.index == start.index && nearest) {
        target = *nearest;
    }

    const QTextLine targetLine = textLine(target);
    const CursorPosition to{target.block, targetLine.isValid() ? targetLine.xToCursor(x) : 0};
    if (to == m_cursorPosition) {
        return;  // At the start or the end of the text
    }
    if (m_pageMoves.empty()) {
        m_pageMovesDirection = direction;
    }
    m_pageMoves.push_back({m_cursorPosition, row});
    placeInRow(to, row);
    m_pageMoveCursor = m_cursorPosition;
    m_preferredCursorX = x;
    m_preferredCursorXValid = true;
    m_preferredCursorXPosition = m_cursorPosition;
}

// =============================================================================
// Selection (Phase 3.10/3.12)
// =============================================================================

SelectionRange BookEditor::selection() const
{
    return m_selection;
}

void BookEditor::setSelection(const SelectionRange& range)
{
    SelectionRange normalized = range.normalized();

    const bool hasContent = m_textBuffer && m_textBuffer->blockCount() > 0;

    if (hasContent) {
        // Clamp start and end to valid positions
        normalized.start = validateCursorPosition(normalized.start);
        normalized.end = validateCursorPosition(normalized.end);
    } else {
        normalized = {};
    }

    if (m_selection.start != normalized.start || m_selection.end != normalized.end) {
        m_selection = normalized;

        // Update paragraph layouts with selection ranges
        updateSelectionInLayouts();

        emit selectionChanged();
        syncPipelineCursor();  // Phase 14: lightweight cursor/selection only
        update();
    }
}

void BookEditor::clearSelection()
{
    if (!m_selection.isEmpty()) {
        m_selection = {};

        // Clear selection in layouts
        updateSelectionInLayouts();

        emit selectionChanged();
        syncPipelineCursor();  // Phase 14: lightweight cursor/selection only
        update();
    }
}

bool BookEditor::hasSelection() const
{
    return !m_selection.isEmpty();
}

QString BookEditor::selectedText() const
{
    if (m_selection.isEmpty()) {
        return QString();
    }

    SelectionRange sel = m_selection.normalized();
    QString result;

    if (m_textBuffer) {
        for (int paraIdx = sel.start.paragraph; paraIdx <= sel.end.paragraph; ++paraIdx) {
            QTextBlock block = m_textBuffer->findBlockByNumber(paraIdx);
            if (!block.isValid()) {
                continue;
            }

            QString text = block.text();
            int startOffset = (paraIdx == sel.start.paragraph) ? sel.start.offset : 0;
            int endOffset = (paraIdx == sel.end.paragraph) ? sel.end.offset : text.length();

            result += text.mid(startOffset, endOffset - startOffset);

            // Add paragraph separator for multi-paragraph selection
            if (paraIdx < sel.end.paragraph) {
                result += QChar::ParagraphSeparator;
            }
        }
    }

    return result;
}

void BookEditor::selectAll()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // O(1) operation - just set start and end positions
    SelectionRange range;
    range.start = {0, 0};

    int lastPara = m_textBuffer->blockCount() - 1;
    range.end = {lastPara, paragraphLength(m_textBuffer.get(), lastPara)};

    m_selectionAnchor = range.start;
    m_cursorPosition = range.end;
    setSelection(range);

    update();
}

void BookEditor::onCursorBlinkTimeout()
{
    m_cursorVisible = !m_cursorVisible;

    // Sync to RenderPipeline (Phase 12 fix)
    if (m_renderPipeline) {
        m_renderPipeline->setCursorBlinkState(m_cursorVisible);
    }

    // Only repaint the cursor area instead of the entire widget
    updateCursorArea();
}

CursorPosition BookEditor::validateCursorPosition(const CursorPosition& position) const
{
    CursorPosition result = position;

    if (m_textBuffer && m_textBuffer->blockCount() > 0) {
        int maxParagraph = m_textBuffer->blockCount() - 1;
        result.paragraph = qBound(0, result.paragraph, maxParagraph);

        int maxOffset = paragraphLength(m_textBuffer.get(), result.paragraph);
        result.offset = qBound(0, result.offset, maxOffset);
        return result;
    }

    return {0, 0};
}

void BookEditor::setupCursorBlinkTimer()
{
    m_cursorBlinkTimer = new QTimer(this);

    connect(m_cursorBlinkTimer, &QTimer::timeout,
            this, &BookEditor::onCursorBlinkTimeout);

    // The timer runs only while the editor has focus (see focusInEvent/focusOutEvent)
}

// =============================================================================
// Private Methods (Phase 3.9/3.10/3.11/3.12)
// =============================================================================

CursorPosition BookEditor::positionFromPoint(const QPointF& widgetPos) const
{
    // Phase 11.6: Position calculation using QTextDocument layout
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return {0, 0};
    }

    QTextDocument* doc = m_textBuffer.get();
    if (!doc) {
        return {0, 0};
    }

    // The document point under the widget point, through the same mapping as the painting
    // (the layout has the lines on their pages in page mode)
    const QPointF docPoint = m_renderPipeline->widgetToDocument(widgetPos);
    const double docY = std::max(0.0, docPoint.y());
    const double localX = std::max(0.0, docPoint.x());

    // The document layout's hit test: block from the cached positions, line from the
    // line boxes (a click between lines lands on the nearest one), offset within the line
    const int position = doc->documentLayout()->hitTest(QPointF(localX, docY), Qt::FuzzyHit);
    const QTextBlock block = doc->findBlock(std::max(0, position));
    if (!block.isValid()) {
        return {0, 0};
    }
    return {block.blockNumber(), std::max(0, position - block.position())};
}

void BookEditor::selectWordAtCursor()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    auto [wordStart, wordEnd] = findWordBoundaries(m_cursorPosition.paragraph, m_cursorPosition.offset);

    SelectionRange range;
    range.start = {m_cursorPosition.paragraph, wordStart};
    range.end = {m_cursorPosition.paragraph, wordEnd};

    m_selectionAnchor = range.start;
    m_cursorPosition = range.end;

    setSelection(range);
}

void BookEditor::selectParagraphAtCursor()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Phase 11: Use QTextBlock
    QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid()) {
        return;
    }

    SelectionRange range;
    range.start = {m_cursorPosition.paragraph, 0};
    int charCount = block.length() - 1;
    if (charCount < 0) charCount = 0;
    range.end = {m_cursorPosition.paragraph, charCount};

    m_selectionAnchor = range.start;
    m_cursorPosition = range.end;

    setSelection(range);
}

std::pair<int, int> BookEditor::findWordBoundaries(int paraIndex, int offset) const
{
    if (!m_textBuffer) {
        return {0, 0};
    }

    // Phase 11: Use QTextBlock
    QTextBlock block = m_textBuffer->findBlockByNumber(paraIndex);
    if (!block.isValid()) {
        return {0, 0};
    }

    QString text = block.text();
    if (text.isEmpty()) {
        return {0, 0};
    }

    // Clamp offset
    offset = qBound(0, offset, text.length());

    // If at end of text, select last word if exists
    if (offset == text.length() && offset > 0) {
        --offset;
    }

    // Find word boundaries
    int start = offset;
    int end = offset;

    // Move start back to beginning of word
    while (start > 0 && text.at(start - 1).isLetterOrNumber()) {
        --start;
    }

    // Move end forward to end of word
    while (end < text.length() && text.at(end).isLetterOrNumber()) {
        ++end;
    }

    // If we didn't find a word (e.g., clicked on whitespace), select the whitespace
    if (start == end) {
        // Try selecting whitespace/punctuation
        while (start > 0 && !text.at(start - 1).isLetterOrNumber()) {
            --start;
        }
        while (end < text.length() && !text.at(end).isLetterOrNumber()) {
            ++end;
        }
    }

    return {start, end};
}

void BookEditor::extendSelection(const CursorPosition& newCursor)
{
    // Create selection from anchor to new cursor
    SelectionRange range;
    range.start = m_selectionAnchor;
    range.end = newCursor;

    m_cursorPosition = newCursor;
    setSelection(range);
}

void BookEditor::updateSelectionInLayouts()
{
    // Phase 11: Selection is stored in m_selection and drawn by drawSelection/RenderPipeline
    // No need to update individual paragraph layouts - they use QTextLayout from QTextDocument
    // Just trigger a repaint
    update();
}

// =============================================================================
// The keys of the text (text_keys.h)
// =============================================================================

void BookEditor::performTextKey(TextKeyAction action, bool select)
{
    using Action = TextKeyAction;

    // What changes the text or only the view
    switch (action) {
    case Action::None:
        return;
    case Action::ScrollPageUp:
        scrollByViewHeight(-1.0);
        return;
    case Action::ScrollPageDown:
        scrollByViewHeight(1.0);
        return;
    case Action::ScrollToStart:
        scrollTo(0.0, false);
        return;
    case Action::ScrollToEnd:
        if (m_viewportManager) {
            scrollTo(m_viewportManager->maxScrollPosition(), false);
        }
        return;
    case Action::CenterCursor:
        centerCursorInView();
        return;
    case Action::DeleteBackward:
        deleteBackward();
        return;
    case Action::DeleteForward:
        deleteForward();
        return;
    case Action::DeleteDiacritic:
        deleteDiacritic();
        return;
    case Action::DeleteWordBackward:
        deleteTo(textKeyTarget(Action::WordLeft, m_cursorPosition));
        return;
    case Action::DeleteWordForward:
        deleteTo(textKeyTarget(Action::WordRight, m_cursorPosition));
        return;
    case Action::DeleteToWordStart:
        deleteTo(textKeyTarget(Action::WordStartBackward, m_cursorPosition));
        return;
    case Action::DeleteToWordEnd:
        deleteTo(textKeyTarget(Action::WordEndForward, m_cursorPosition));
        return;
    case Action::DeleteToLineStart:
        if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
            return;
        }
        // At the start of a line: the character before it, as Backspace
        if (!hasSelection() && lineStartOf(m_cursorPosition) == m_cursorPosition) {
            deleteBackward();
        } else {
            deleteTo(lineStartOf(m_cursorPosition));
        }
        return;
    case Action::KillToParagraphEnd:
        killToParagraphEnd();
        return;
    case Action::NewParagraph:
        insertNewline();
        return;
    case Action::OpenLine:
        // The paragraph break goes after the cursor
        insertNewline();
        moveCursorLeft();
        return;
    case Action::Transpose:
        transposeCharacters();
        return;
    case Action::Yank:
        yank();
        return;
    case Action::Undo:
        undo();
        return;
    case Action::Redo:
        redo();
        return;
    case Action::Cut:
        cut();
        return;
    case Action::Copy:
        copy();
        return;
    case Action::Paste:
        paste();
        return;
    case Action::ContextMenu:
        showContextMenuFromKeyboard();
        return;
    default:
        break;  // A move of the cursor
    }

    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Left and Right take the cursor to the start (end) of the selection, which goes
    if (!select && hasSelection() &&
        (action == Action::CharacterLeft || action == Action::CharacterRight)) {
        const SelectionRange selection = m_selection.normalized();
        m_preferredCursorXValid = false;
        setCursorPosition(action == Action::CharacterLeft ? selection.start : selection.end);
        clearSelection();
        return;
    }

    if (select && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }
    switch (action) {
    case Action::CharacterLeft:
        moveCursorLeft();
        break;
    case Action::CharacterRight:
        moveCursorRight();
        break;
    case Action::LineUp:
        moveCursorUp();
        break;
    case Action::LineDown:
        moveCursorDown();
        break;
    case Action::DocumentStart:
        moveCursorToDocStart();
        break;
    case Action::DocumentEnd:
        moveCursorToDocEnd();
        break;
    case Action::PageUp:
        moveCursorPageUp();
        break;
    case Action::PageDown:
        moveCursorPageDown();
        break;
    default:
        m_preferredCursorXValid = false;
        setCursorPosition(textKeyTarget(action, m_cursorPosition));
        break;
    }
    if (select) {
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
    }
}

CursorPosition BookEditor::textKeyTarget(TextKeyAction action, const CursorPosition& from) const
{
    using Action = TextKeyAction;
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return from;
    }
    QTextDocument* doc = m_textBuffer.get();
    const int lastParagraph = doc->blockCount() - 1;
    const CursorPosition position = validateCursorPosition(from);
    const QString text = doc->findBlockByNumber(position.paragraph).text();
    const int length = static_cast<int>(text.size());

    switch (action) {
    case Action::WordLeft: {
        // Back over the spaces and punctuation, then over the word; at the start of a
        // paragraph to the end of the one before
        if (position.offset == 0) {
            if (position.paragraph == 0) {
                return position;
            }
            return {position.paragraph - 1, paragraphLength(doc, position.paragraph - 1)};
        }
        int offset = position.offset;
        while (offset > 0 && !isWordCharacter(text.at(offset - 1))) {
            --offset;
        }
        while (offset > 0 && isWordCharacter(text.at(offset - 1))) {
            --offset;
        }
        return {position.paragraph, offset};
    }
    case Action::WordRight: {
        // Over the rest of the word, then over the spaces and punctuation; at the end of a
        // paragraph to the start of the next one
        if (position.offset >= length) {
            return position.paragraph < lastParagraph ? CursorPosition{position.paragraph + 1, 0}
                                                      : position;
        }
        int offset = position.offset;
        while (offset < length && isWordCharacter(text.at(offset))) {
            ++offset;
        }
        while (offset < length && !isWordCharacter(text.at(offset))) {
            ++offset;
        }
        return {position.paragraph, offset};
    }
    case Action::WordStartBackward: {
        // Back over what is not a word, paragraph breaks too, then over the word
        CursorPosition to = position;
        QString paragraphText = text;
        while (to.offset == 0 || !isWordCharacter(paragraphText.at(to.offset - 1))) {
            if (to.offset > 0) {
                --to.offset;
            } else if (to.paragraph > 0) {
                --to.paragraph;
                paragraphText = doc->findBlockByNumber(to.paragraph).text();
                to.offset = static_cast<int>(paragraphText.size());
            } else {
                return to;  // The start of the text
            }
        }
        while (to.offset > 0 && isWordCharacter(paragraphText.at(to.offset - 1))) {
            --to.offset;
        }
        return to;
    }
    case Action::WordEndForward: {
        // Over what is not a word, paragraph breaks too, then over the word
        CursorPosition to = position;
        QString paragraphText = text;
        while (to.offset >= paragraphText.size() || !isWordCharacter(paragraphText.at(to.offset))) {
            if (to.offset < paragraphText.size()) {
                ++to.offset;
            } else if (to.paragraph < lastParagraph) {
                ++to.paragraph;
                paragraphText = doc->findBlockByNumber(to.paragraph).text();
                to.offset = 0;
            } else {
                return to;  // The end of the text
            }
        }
        while (to.offset < paragraphText.size() && isWordCharacter(paragraphText.at(to.offset))) {
            ++to.offset;
        }
        return to;
    }
    case Action::LineStart:
        return lineStartOf(position);
    case Action::LineEnd:
        return lineEndOf(position);
    case Action::ParagraphStart:
        return {position.paragraph, 0};
    case Action::ParagraphEnd:
        return {position.paragraph, length};
    case Action::ParagraphUp:
        if (position.offset > 0) {
            return {position.paragraph, 0};
        }
        return position.paragraph > 0 ? CursorPosition{position.paragraph - 1, 0} : position;
    case Action::ParagraphDown:
        return position.paragraph < lastParagraph ? CursorPosition{position.paragraph + 1, 0}
                                                  : CursorPosition{position.paragraph, length};
    case Action::ParagraphEndForward:
        if (position.offset < length) {
            return {position.paragraph, length};
        }
        return position.paragraph < lastParagraph
            ? CursorPosition{position.paragraph + 1, paragraphLength(doc, position.paragraph + 1)}
            : position;
    default:
        return position;
    }
}

CursorPosition BookEditor::lineStartOf(const CursorPosition& position) const
{
    const QTextBlock block = m_textBuffer->findBlockByNumber(position.paragraph);
    QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
    if (!layout || layout->lineCount() == 0) {
        return {position.paragraph, 0};
    }
    const QTextLine line = layout->lineForTextPosition(position.offset);
    return {position.paragraph, line.isValid() ? line.textStart() : 0};
}

CursorPosition BookEditor::lineEndOf(const CursorPosition& position) const
{
    const QTextBlock block = m_textBuffer->findBlockByNumber(position.paragraph);
    const int length = std::max(0, block.length() - 1);
    QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
    if (!layout || layout->lineCount() == 0) {
        return {position.paragraph, length};
    }
    const QTextLine line = layout->lineForTextPosition(position.offset);
    if (!line.isValid() || line.lineNumber() == layout->lineCount() - 1) {
        return {position.paragraph, length};
    }
    // The end of a wrapped line is the start of the next one: the cursor stays on the line
    // before the space the line breaks at, as in QTextCursor
    int end = line.textStart() + line.textLength();
    if (end > line.textStart() && block.text().at(end - 1).isSpace()) {
        --end;
    }
    return {position.paragraph, end};
}

void BookEditor::scrollByViewHeight(double direction)
{
    if (!m_viewportManager) {
        return;
    }
    scrollTo(scrollOffset() + direction * m_viewportManager->visibleDocumentHeight(), false);
}

void BookEditor::centerCursorInView()
{
    if (!m_textBuffer || !m_viewportManager || !m_renderPipeline) {
        return;
    }
    const QRectF caret = m_renderPipeline->caretRect(m_cursorPosition);
    if (caret.isNull()) {
        return;
    }
    // In document units: where the cursor's line is and where the middle of the view is
    const double caretMiddle = m_renderPipeline->widgetToDocument(caret.center()).y();
    const double viewMiddle =
        m_renderPipeline->widgetToDocument(QPointF(0.0, 0.0)).y() +
        m_viewportManager->visibleDocumentHeight() / 2.0;
    scrollTo(scrollOffset() + caretMiddle - viewMiddle, false);
}

}  // namespace kalahari::editor
