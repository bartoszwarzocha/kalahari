/// @file book_editor.cpp
/// @brief BookEditor implementation (OpenSpec #00042 Phase 3.1-3.5)

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kml_commands.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/paragraph_layout.h>
#include <QEasingCurve>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyleHints>
#include <QTimer>
#include <QUndoStack>
#include <QWheelEvent>

namespace kalahari::editor {

// Default smooth scroll duration in milliseconds
constexpr int DEFAULT_SMOOTH_SCROLL_DURATION = 150;

// Wheel scroll step in pixels (approximate line height)
constexpr qreal WHEEL_SCROLL_STEP = 60.0;

// Default cursor blink interval in milliseconds
constexpr int DEFAULT_CURSOR_BLINK_INTERVAL = 500;

// Cursor width in pixels
constexpr qreal CURSOR_WIDTH = 2.0;

// Content margins (must match paintEvent)
constexpr qreal LEFT_MARGIN = 10.0;
constexpr qreal TOP_MARGIN = 10.0;

// =============================================================================
// Construction / Destruction
// =============================================================================

BookEditor::BookEditor(QWidget* parent)
    : QWidget(parent)
    , m_document(nullptr)
    , m_layoutManager(std::make_unique<LayoutManager>())
    , m_scrollManager(std::make_unique<VirtualScrollManager>())
    , m_verticalScrollBar(nullptr)
    , m_scrollAnimation(nullptr)
    , m_smoothScrollingEnabled(false)  // Disabled by default for stability in tests
    , m_smoothScrollDuration(DEFAULT_SMOOTH_SCROLL_DURATION)
    , m_updatingScrollBar(false)
    , m_cursorPosition{0, 0}
    , m_cursorBlinkTimer(nullptr)
    , m_cursorVisible(true)
    , m_cursorBlinkingEnabled(true)
    , m_cursorBlinkInterval(DEFAULT_CURSOR_BLINK_INTERVAL)
    , m_preferredCursorX(0.0)
    , m_preferredCursorXValid(false)
    , m_selection{}
    , m_selectionAnchor{0, 0}
    , m_isDragging(false)
    , m_clickTimer(nullptr)
    , m_clickCount(0)
    , m_lastClickPos(0.0, 0.0)
    , m_preeditString()
    , m_preeditStart{0, 0}
    , m_hasComposition(false)
    , m_undoStack(nullptr)
{
    // Enable input method support
    setAttribute(Qt::WA_InputMethodEnabled, true);

    // Create undo stack
    m_undoStack = new QUndoStack(this);

    setupComponents();
}

BookEditor::~BookEditor()
{
    // Stop timers before destruction to prevent callbacks during cleanup
    if (m_cursorBlinkTimer != nullptr) {
        m_cursorBlinkTimer->stop();
    }
    if (m_clickTimer != nullptr) {
        m_clickTimer->stop();
    }
    if (m_scrollAnimation != nullptr) {
        m_scrollAnimation->stop();
    }
}

// =============================================================================
// Document Management
// =============================================================================

void BookEditor::setDocument(KmlDocument* document)
{
    if (m_document == document) {
        return;
    }

    m_document = document;

    // Update managers with new document
    m_scrollManager->setDocument(document);
    m_layoutManager->setDocument(document);

    // Update viewport and layout width
    updateViewport();
    updateLayoutWidth();

    // Update scrollbar range for new document
    updateScrollBarRange();

    emit documentChanged();
    update();  // Request repaint
}

KmlDocument* BookEditor::document() const
{
    return m_document;
}

// =============================================================================
// Layout Configuration
// =============================================================================

LayoutManager& BookEditor::layoutManager()
{
    return *m_layoutManager;
}

const LayoutManager& BookEditor::layoutManager() const
{
    return *m_layoutManager;
}

VirtualScrollManager& BookEditor::scrollManager()
{
    return *m_scrollManager;
}

const VirtualScrollManager& BookEditor::scrollManager() const
{
    return *m_scrollManager;
}

// =============================================================================
// Scrolling
// =============================================================================

QScrollBar* BookEditor::verticalScrollBar() const
{
    return m_verticalScrollBar;
}

qreal BookEditor::scrollOffset() const
{
    return m_scrollManager->scrollOffset();
}

void BookEditor::setScrollOffset(qreal offset)
{
    qreal oldOffset = m_scrollManager->scrollOffset();
    m_scrollManager->setScrollOffset(offset);
    qreal newOffset = m_scrollManager->scrollOffset();

    if (oldOffset != newOffset) {
        syncScrollBarValue();
        emit scrollOffsetChanged(newOffset);
        update();  // Request repaint
    }
}

void BookEditor::scrollBy(qreal delta, bool animated)
{
    qreal targetOffset = scrollOffset() + delta;
    scrollTo(targetOffset, animated);
}

void BookEditor::scrollTo(qreal offset, bool animated)
{
    // Clamp the offset to valid range
    offset = qBound(0.0, offset, m_scrollManager->maxScrollOffset());

    if (animated && m_smoothScrollingEnabled) {
        startScrollAnimation(offset);
    } else {
        stopScrollAnimation();
        setScrollOffset(offset);
    }
}

bool BookEditor::isSmoothScrollingEnabled() const
{
    return m_smoothScrollingEnabled;
}

void BookEditor::setSmoothScrollingEnabled(bool enabled)
{
    m_smoothScrollingEnabled = enabled;
    if (!enabled) {
        stopScrollAnimation();
    }
}

int BookEditor::smoothScrollDuration() const
{
    return m_smoothScrollDuration;
}

void BookEditor::setSmoothScrollDuration(int duration)
{
    m_smoothScrollDuration = qMax(0, duration);
}

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
        m_cursorPosition = validatedPos;
        ensureCursorVisible();
        emit cursorPositionChanged(m_cursorPosition);
        update();  // Request repaint
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
        if (m_cursorBlinkTimer != nullptr) {
            m_cursorBlinkTimer->start(m_cursorBlinkInterval);
        }
    } else {
        // Stop blinking and keep cursor visible
        if (m_cursorBlinkTimer != nullptr) {
            m_cursorBlinkTimer->stop();
        }
        if (!m_cursorVisible) {
            m_cursorVisible = true;
            update();
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

void BookEditor::ensureCursorVisible()
{
    // Reset blink state to visible
    m_cursorVisible = true;

    // Restart blink timer if blinking is enabled
    if (m_cursorBlinkTimer != nullptr && m_cursorBlinkingEnabled) {
        m_cursorBlinkTimer->start(m_cursorBlinkInterval);
    }
}

// =============================================================================
// Cursor Navigation (Phase 3.6/3.7/3.8)
// =============================================================================

void BookEditor::moveCursorLeft()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Invalidate preferred X position (horizontal movement resets it)
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;

    if (newPos.offset > 0) {
        // Move within current paragraph
        --newPos.offset;
    } else if (newPos.paragraph > 0) {
        // Move to end of previous paragraph
        --newPos.paragraph;
        const KmlParagraph* para = m_document->paragraph(newPos.paragraph);
        newPos.offset = para ? para->characterCount() : 0;
    }
    // else: already at document start, do nothing

    setCursorPosition(newPos);
}

void BookEditor::moveCursorRight()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Invalidate preferred X position (horizontal movement resets it)
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;
    const KmlParagraph* currentPara = m_document->paragraph(newPos.paragraph);
    int currentLength = currentPara ? currentPara->characterCount() : 0;

    if (newPos.offset < currentLength) {
        // Move within current paragraph
        ++newPos.offset;
    } else if (newPos.paragraph < m_document->paragraphCount() - 1) {
        // Move to start of next paragraph
        ++newPos.paragraph;
        newPos.offset = 0;
    }
    // else: already at document end, do nothing

    setCursorPosition(newPos);
}

void BookEditor::moveCursorUp()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Get current layout for hit testing
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    if (layout == nullptr) {
        m_layoutManager->layoutParagraph(m_cursorPosition.paragraph);
        layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    }
    if (layout == nullptr) {
        return;
    }

    // Get current line index
    int currentLine = layout->lineForPosition(m_cursorPosition.offset);
    if (currentLine < 0) {
        currentLine = 0;
    }

    // Get or calculate preferred X position
    if (!m_preferredCursorXValid) {
        QRectF cursorRect = layout->cursorRect(m_cursorPosition.offset);
        m_preferredCursorX = cursorRect.x();
        m_preferredCursorXValid = true;
    }

    CursorPosition newPos = m_cursorPosition;

    if (currentLine > 0) {
        // Move to previous line in same paragraph
        QRectF lineRect = layout->lineRect(currentLine - 1);
        QPointF hitPoint(m_preferredCursorX, lineRect.center().y());
        int newOffset = layout->positionAt(hitPoint);
        if (newOffset >= 0) {
            newPos.offset = newOffset;
        }
    } else if (m_cursorPosition.paragraph > 0) {
        // Move to previous paragraph
        newPos.paragraph = m_cursorPosition.paragraph - 1;

        // Layout previous paragraph if needed
        ParagraphLayout* prevLayout = m_layoutManager->paragraphLayout(newPos.paragraph);
        if (prevLayout == nullptr) {
            m_layoutManager->layoutParagraph(newPos.paragraph);
            prevLayout = m_layoutManager->paragraphLayout(newPos.paragraph);
        }

        if (prevLayout != nullptr && prevLayout->lineCount() > 0) {
            // Go to last line of previous paragraph at preferred X
            int lastLine = prevLayout->lineCount() - 1;
            QRectF lineRect = prevLayout->lineRect(lastLine);
            QPointF hitPoint(m_preferredCursorX, lineRect.center().y());
            int newOffset = prevLayout->positionAt(hitPoint);
            newPos.offset = (newOffset >= 0) ? newOffset : prevLayout->text().length();
        } else {
            const KmlParagraph* para = m_document->paragraph(newPos.paragraph);
            newPos.offset = para ? para->characterCount() : 0;
        }
    }
    // else: already at first line of first paragraph, do nothing

    setCursorPosition(newPos);
}

void BookEditor::moveCursorDown()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Get current layout for hit testing
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    if (layout == nullptr) {
        m_layoutManager->layoutParagraph(m_cursorPosition.paragraph);
        layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    }
    if (layout == nullptr) {
        return;
    }

    // Get current line index
    int currentLine = layout->lineForPosition(m_cursorPosition.offset);
    if (currentLine < 0) {
        currentLine = 0;
    }

    // Get or calculate preferred X position
    if (!m_preferredCursorXValid) {
        QRectF cursorRect = layout->cursorRect(m_cursorPosition.offset);
        m_preferredCursorX = cursorRect.x();
        m_preferredCursorXValid = true;
    }

    CursorPosition newPos = m_cursorPosition;
    int lineCount = layout->lineCount();

    if (currentLine < lineCount - 1) {
        // Move to next line in same paragraph
        QRectF lineRect = layout->lineRect(currentLine + 1);
        QPointF hitPoint(m_preferredCursorX, lineRect.center().y());
        int newOffset = layout->positionAt(hitPoint);
        if (newOffset >= 0) {
            newPos.offset = newOffset;
        }
    } else if (m_cursorPosition.paragraph < m_document->paragraphCount() - 1) {
        // Move to next paragraph
        newPos.paragraph = m_cursorPosition.paragraph + 1;

        // Layout next paragraph if needed
        ParagraphLayout* nextLayout = m_layoutManager->paragraphLayout(newPos.paragraph);
        if (nextLayout == nullptr) {
            m_layoutManager->layoutParagraph(newPos.paragraph);
            nextLayout = m_layoutManager->paragraphLayout(newPos.paragraph);
        }

        if (nextLayout != nullptr && nextLayout->lineCount() > 0) {
            // Go to first line of next paragraph at preferred X
            QRectF lineRect = nextLayout->lineRect(0);
            QPointF hitPoint(m_preferredCursorX, lineRect.center().y());
            int newOffset = nextLayout->positionAt(hitPoint);
            newPos.offset = (newOffset >= 0) ? newOffset : 0;
        } else {
            newPos.offset = 0;
        }
    }
    // else: already at last line of last paragraph, do nothing

    setCursorPosition(newPos);
}

void BookEditor::moveCursorWordLeft()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;
    const KmlParagraph* para = m_document->paragraph(newPos.paragraph);
    if (para == nullptr) {
        return;
    }

    QString text = para->plainText();

    if (newPos.offset > 0) {
        // Move backwards, skipping whitespace first
        int pos = newPos.offset - 1;

        // Skip trailing whitespace/punctuation
        while (pos >= 0 && !text.at(pos).isLetterOrNumber()) {
            --pos;
        }

        // Skip word characters to find start of word
        while (pos >= 0 && text.at(pos).isLetterOrNumber()) {
            --pos;
        }

        newPos.offset = pos + 1;
    } else if (newPos.paragraph > 0) {
        // Move to end of previous paragraph
        --newPos.paragraph;
        const KmlParagraph* prevPara = m_document->paragraph(newPos.paragraph);
        newPos.offset = prevPara ? prevPara->characterCount() : 0;
    }

    setCursorPosition(newPos);
}

void BookEditor::moveCursorWordRight()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;
    const KmlParagraph* para = m_document->paragraph(newPos.paragraph);
    if (para == nullptr) {
        return;
    }

    QString text = para->plainText();
    int textLen = text.length();

    if (newPos.offset < textLen) {
        int pos = newPos.offset;

        // Skip current word characters
        while (pos < textLen && text.at(pos).isLetterOrNumber()) {
            ++pos;
        }

        // Skip whitespace/punctuation to reach next word
        while (pos < textLen && !text.at(pos).isLetterOrNumber()) {
            ++pos;
        }

        newPos.offset = pos;
    } else if (newPos.paragraph < m_document->paragraphCount() - 1) {
        // Move to start of next paragraph
        ++newPos.paragraph;
        newPos.offset = 0;
    }

    setCursorPosition(newPos);
}

void BookEditor::moveCursorToLineStart()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    // Get current layout
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    if (layout == nullptr) {
        m_layoutManager->layoutParagraph(m_cursorPosition.paragraph);
        layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    }

    CursorPosition newPos = m_cursorPosition;

    if (layout != nullptr) {
        int currentLine = layout->lineForPosition(m_cursorPosition.offset);
        if (currentLine >= 0 && currentLine < layout->lineCount()) {
            // Get start of current line using QTextLine
            const QTextLayout& textLayout = layout->textLayout();
            if (currentLine < textLayout.lineCount()) {
                QTextLine line = textLayout.lineAt(currentLine);
                newPos.offset = line.textStart();
            }
        } else {
            // Fallback: move to paragraph start
            newPos.offset = 0;
        }
    } else {
        // No layout: move to paragraph start
        newPos.offset = 0;
    }

    setCursorPosition(newPos);
}

void BookEditor::moveCursorToLineEnd()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    // Get current layout
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    if (layout == nullptr) {
        m_layoutManager->layoutParagraph(m_cursorPosition.paragraph);
        layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    }

    CursorPosition newPos = m_cursorPosition;
    const KmlParagraph* para = m_document->paragraph(m_cursorPosition.paragraph);
    int paraLength = para ? para->characterCount() : 0;

    if (layout != nullptr) {
        int currentLine = layout->lineForPosition(m_cursorPosition.offset);
        if (currentLine >= 0 && currentLine < layout->lineCount()) {
            const QTextLayout& textLayout = layout->textLayout();
            if (currentLine < textLayout.lineCount()) {
                QTextLine line = textLayout.lineAt(currentLine);
                // End of line is textStart + textLength, but clamp to paragraph length
                int lineEnd = line.textStart() + line.textLength();
                // Don't include trailing newline if present
                if (currentLine < layout->lineCount() - 1) {
                    // This is not the last line, line ends at text boundary
                    newPos.offset = qMin(lineEnd, paraLength);
                } else {
                    // Last line: go to paragraph end
                    newPos.offset = paraLength;
                }
            }
        } else {
            // Fallback: move to paragraph end
            newPos.offset = paraLength;
        }
    } else {
        // No layout: move to paragraph end
        newPos.offset = paraLength;
    }

    setCursorPosition(newPos);
}

void BookEditor::moveCursorToDocStart()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    setCursorPosition({0, 0});

    // Scroll to top
    setScrollOffset(0.0);
}

void BookEditor::moveCursorToDocEnd()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    int lastPara = m_document->paragraphCount() - 1;
    const KmlParagraph* para = m_document->paragraph(lastPara);
    int lastOffset = para ? para->characterCount() : 0;

    setCursorPosition({lastPara, lastOffset});

    // Scroll to bottom
    setScrollOffset(m_scrollManager->maxScrollOffset());
}

void BookEditor::moveCursorPageUp()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Get viewport height for page calculation
    qreal pageHeight = static_cast<qreal>(height());
    if (pageHeight <= 0) {
        return;
    }

    // Get current cursor Y position
    qreal cursorY = m_layoutManager->paragraphY(m_cursorPosition.paragraph);
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    if (layout != nullptr) {
        QRectF cursorRect = layout->cursorRect(m_cursorPosition.offset);
        cursorY += cursorRect.y();
    }

    // Calculate target Y position (one page up)
    qreal targetY = qMax(0.0, cursorY - pageHeight);

    // Find paragraph at target Y
    int targetPara = m_scrollManager->paragraphAtY(targetY);
    if (targetPara < 0) {
        targetPara = 0;
    }

    // Layout target paragraph if needed
    ParagraphLayout* targetLayout = m_layoutManager->paragraphLayout(targetPara);
    if (targetLayout == nullptr) {
        m_layoutManager->layoutParagraph(targetPara);
        targetLayout = m_layoutManager->paragraphLayout(targetPara);
    }

    // Get or calculate preferred X position
    if (!m_preferredCursorXValid && layout != nullptr) {
        QRectF cursorRect = layout->cursorRect(m_cursorPosition.offset);
        m_preferredCursorX = cursorRect.x();
        m_preferredCursorXValid = true;
    }

    CursorPosition newPos;
    newPos.paragraph = targetPara;

    if (targetLayout != nullptr && targetLayout->lineCount() > 0) {
        // Find position at preferred X within target paragraph
        qreal targetParaY = m_layoutManager->paragraphY(targetPara);
        qreal relativeY = targetY - targetParaY;
        QPointF hitPoint(m_preferredCursorX, relativeY);
        int newOffset = targetLayout->positionAt(hitPoint);
        newPos.offset = (newOffset >= 0) ? newOffset : 0;
    } else {
        newPos.offset = 0;
    }

    setCursorPosition(newPos);

    // Scroll by same amount
    qreal newScrollOffset = qMax(0.0, scrollOffset() - pageHeight);
    setScrollOffset(newScrollOffset);
}

void BookEditor::moveCursorPageDown()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Get viewport height for page calculation
    qreal pageHeight = static_cast<qreal>(height());
    if (pageHeight <= 0) {
        return;
    }

    // Get current cursor Y position
    qreal cursorY = m_layoutManager->paragraphY(m_cursorPosition.paragraph);
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    if (layout != nullptr) {
        QRectF cursorRect = layout->cursorRect(m_cursorPosition.offset);
        cursorY += cursorRect.y();
    }

    // Calculate target Y position (one page down)
    qreal maxY = m_scrollManager->totalHeight();
    qreal targetY = qMin(maxY, cursorY + pageHeight);

    // Find paragraph at target Y
    int targetPara = m_scrollManager->paragraphAtY(targetY);
    if (targetPara < 0 || targetPara >= m_document->paragraphCount()) {
        targetPara = m_document->paragraphCount() - 1;
    }

    // Layout target paragraph if needed
    ParagraphLayout* targetLayout = m_layoutManager->paragraphLayout(targetPara);
    if (targetLayout == nullptr) {
        m_layoutManager->layoutParagraph(targetPara);
        targetLayout = m_layoutManager->paragraphLayout(targetPara);
    }

    // Get or calculate preferred X position
    if (!m_preferredCursorXValid && layout != nullptr) {
        QRectF cursorRect = layout->cursorRect(m_cursorPosition.offset);
        m_preferredCursorX = cursorRect.x();
        m_preferredCursorXValid = true;
    }

    CursorPosition newPos;
    newPos.paragraph = targetPara;

    if (targetLayout != nullptr && targetLayout->lineCount() > 0) {
        // Find position at preferred X within target paragraph
        qreal targetParaY = m_layoutManager->paragraphY(targetPara);
        qreal relativeY = targetY - targetParaY;
        QPointF hitPoint(m_preferredCursorX, relativeY);
        int newOffset = targetLayout->positionAt(hitPoint);
        if (newOffset >= 0) {
            newPos.offset = newOffset;
        } else {
            const KmlParagraph* para = m_document->paragraph(targetPara);
            newPos.offset = para ? para->characterCount() : 0;
        }
    } else {
        const KmlParagraph* para = m_document->paragraph(targetPara);
        newPos.offset = para ? para->characterCount() : 0;
    }

    setCursorPosition(newPos);

    // Scroll by same amount
    qreal newScrollOffset = qMin(m_scrollManager->maxScrollOffset(), scrollOffset() + pageHeight);
    setScrollOffset(newScrollOffset);
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

    // Validate against document
    if (m_document != nullptr && m_document->paragraphCount() > 0) {
        // Clamp start
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
        update();
    }
}

bool BookEditor::hasSelection() const
{
    return !m_selection.isEmpty();
}

QString BookEditor::selectedText() const
{
    if (m_document == nullptr || m_selection.isEmpty()) {
        return QString();
    }

    SelectionRange sel = m_selection.normalized();
    QString result;

    for (int paraIdx = sel.start.paragraph; paraIdx <= sel.end.paragraph; ++paraIdx) {
        const KmlParagraph* para = m_document->paragraph(paraIdx);
        if (para == nullptr) {
            continue;
        }

        QString text = para->plainText();
        int startOffset = (paraIdx == sel.start.paragraph) ? sel.start.offset : 0;
        int endOffset = (paraIdx == sel.end.paragraph) ? sel.end.offset : text.length();

        result += text.mid(startOffset, endOffset - startOffset);

        // Add paragraph separator for multi-paragraph selection
        if (paraIdx < sel.end.paragraph) {
            result += QChar::ParagraphSeparator;
        }
    }

    return result;
}

void BookEditor::selectAll()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    SelectionRange range;
    range.start = {0, 0};

    int lastPara = m_document->paragraphCount() - 1;
    const KmlParagraph* para = m_document->paragraph(lastPara);
    range.end = {lastPara, para ? para->characterCount() : 0};

    // Set anchor and cursor position
    m_selectionAnchor = range.start;
    m_cursorPosition = range.end;

    setSelection(range);
}

// =============================================================================
// Text Input (Phase 4.1 - 4.4)
// =============================================================================

void BookEditor::insertText(const QString& text)
{
    if (m_document == nullptr || text.isEmpty()) {
        return;
    }

    // If there's a selection, delete it first (replace behavior)
    if (hasSelection()) {
        deleteSelectedText();
    }

    // Insert text at cursor position
    if (m_document->insertText(m_cursorPosition, text)) {
        // Move cursor to end of inserted text
        CursorPosition newPos = m_cursorPosition;
        newPos.offset += text.length();

        // Validate against paragraph length
        const KmlParagraph* para = m_document->paragraph(newPos.paragraph);
        if (para != nullptr) {
            newPos.offset = qMin(newPos.offset, para->characterCount());
        }

        setCursorPosition(newPos);
        ensureCursorVisible();
    }
}

bool BookEditor::deleteSelectedText()
{
    if (m_document == nullptr || !hasSelection()) {
        return false;
    }

    // Normalize selection range
    SelectionRange range = m_selection.normalized();

    // Delete the selected text
    if (m_document->deleteText(range.start, range.end)) {
        // Position cursor at start of former selection
        setCursorPosition(range.start);

        // Clear selection
        clearSelection();
        ensureCursorVisible();
        return true;
    }

    return false;
}

void BookEditor::insertNewline()
{
    if (m_document == nullptr) {
        return;
    }

    // If there's a selection, delete it first
    if (hasSelection()) {
        deleteSelectedText();
    }

    // Split paragraph at cursor position
    if (m_document->splitParagraph(m_cursorPosition)) {
        // Move cursor to start of new paragraph
        CursorPosition newPos;
        newPos.paragraph = m_cursorPosition.paragraph + 1;
        newPos.offset = 0;

        setCursorPosition(newPos);
        ensureCursorVisible();
    }
}

void BookEditor::deleteBackward()
{
    if (m_document == nullptr) {
        return;
    }

    // If there's a selection, delete it
    if (hasSelection()) {
        deleteSelectedText();
        return;
    }

    // If at start of document, nothing to delete
    if (m_cursorPosition.paragraph == 0 && m_cursorPosition.offset == 0) {
        return;
    }

    // If at start of paragraph, merge with previous paragraph
    if (m_cursorPosition.offset == 0) {
        // Get length of previous paragraph to know where cursor should go
        const KmlParagraph* prevPara = m_document->paragraph(m_cursorPosition.paragraph - 1);
        if (prevPara == nullptr) {
            return;
        }
        int prevLength = prevPara->characterCount();

        // Merge with previous paragraph
        if (m_document->mergeParagraphWithPrevious(m_cursorPosition.paragraph)) {
            // Cursor moves to end of previous paragraph (now merged)
            CursorPosition newPos;
            newPos.paragraph = m_cursorPosition.paragraph - 1;
            newPos.offset = prevLength;

            setCursorPosition(newPos);
            ensureCursorVisible();
        }
    } else {
        // Delete character before cursor
        CursorPosition deleteStart = m_cursorPosition;
        deleteStart.offset -= 1;

        if (m_document->deleteText(deleteStart, m_cursorPosition)) {
            setCursorPosition(deleteStart);
            ensureCursorVisible();
        }
    }
}

void BookEditor::deleteForward()
{
    if (m_document == nullptr) {
        return;
    }

    // If there's a selection, delete it
    if (hasSelection()) {
        deleteSelectedText();
        return;
    }

    // Check if at end of document
    int lastPara = m_document->paragraphCount() - 1;
    if (lastPara < 0) {
        return;
    }

    const KmlParagraph* para = m_document->paragraph(m_cursorPosition.paragraph);
    if (para == nullptr) {
        return;
    }

    bool atEndOfPara = m_cursorPosition.offset >= para->characterCount();
    bool atLastPara = m_cursorPosition.paragraph == lastPara;

    // If at end of document, nothing to delete
    if (atEndOfPara && atLastPara) {
        return;
    }

    // If at end of paragraph, merge with next paragraph
    if (atEndOfPara) {
        // Merge next paragraph into this one
        // This effectively deletes the paragraph break
        if (m_document->mergeParagraphWithPrevious(m_cursorPosition.paragraph + 1)) {
            // Cursor position stays the same
            ensureCursorVisible();
        }
    } else {
        // Delete character after cursor
        CursorPosition deleteEnd = m_cursorPosition;
        deleteEnd.offset += 1;

        if (m_document->deleteText(m_cursorPosition, deleteEnd)) {
            // Cursor position stays the same
            ensureCursorVisible();
        }
    }
}

// =============================================================================
// Size Hints
// =============================================================================

QSize BookEditor::minimumSizeHint() const
{
    // Minimum size for basic text display
    // Allow at least some text to be visible
    return QSize(200, 100);
}

QSize BookEditor::sizeHint() const
{
    // Comfortable editing size
    // Approximately 80 characters wide at typical font sizes
    return QSize(600, 400);
}

// =============================================================================
// Undo/Redo (Phase 4.8)
// =============================================================================

QUndoStack* BookEditor::undoStack() const
{
    return m_undoStack;
}

bool BookEditor::canUndo() const
{
    return m_undoStack != nullptr && m_undoStack->canUndo();
}

bool BookEditor::canRedo() const
{
    return m_undoStack != nullptr && m_undoStack->canRedo();
}

void BookEditor::undo()
{
    if (m_undoStack == nullptr) {
        return;
    }

    m_undoStack->undo();

    // Update cursor position from the undone command
    // (The command stores cursor positions, so editor needs to update)
    ensureCursorVisible();
    update();
}

void BookEditor::redo()
{
    if (m_undoStack == nullptr) {
        return;
    }

    m_undoStack->redo();
    ensureCursorVisible();
    update();
}

void BookEditor::clearUndoStack()
{
    if (m_undoStack != nullptr) {
        m_undoStack->clear();
    }
}

// =============================================================================
// Event Handlers
// =============================================================================

void BookEditor::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Fill background with palette background color
    painter.fillRect(event->rect(), palette().window());

    // If no document, nothing more to render
    if (m_document == nullptr) {
        return;
    }

    // Layout visible paragraphs before rendering
    m_layoutManager->layoutVisibleParagraphs();

    // Get visible range from scroll manager
    auto [firstVisible, lastVisible] = m_scrollManager->visibleRange();
    if (firstVisible < 0 || lastVisible < 0) {
        return;
    }

    // Get current scroll offset
    qreal scrollY = m_scrollManager->scrollOffset();

    // Render each visible paragraph
    for (int paraIndex = firstVisible; paraIndex <= lastVisible; ++paraIndex) {
        // Get the layout for this paragraph
        ParagraphLayout* layout = m_layoutManager->paragraphLayout(paraIndex);
        if (layout == nullptr) {
            // Layout should have been created by layoutVisibleParagraphs()
            // If not, skip this paragraph
            continue;
        }

        // Calculate paragraph position in document coordinates
        qreal paraY = m_layoutManager->paragraphY(paraIndex);
        qreal paraHeight = layout->height();

        // Convert to widget coordinates (apply scroll offset)
        qreal widgetY = TOP_MARGIN + paraY - scrollY;

        // Skip if paragraph is entirely above or below visible area
        if (widgetY + paraHeight < 0 || widgetY > height()) {
            continue;
        }

        // Draw paragraph background (subtle alternating for readability)
        // This is optional - can be enabled for debugging or visual separation
        // QRectF paragraphRect(0, widgetY, width(), paraHeight);
        // painter.fillRect(paragraphRect, palette().alternateBase());

        // Draw the paragraph text
        QPointF drawPosition(LEFT_MARGIN, widgetY);
        layout->draw(&painter, drawPosition);
    }

    // Draw selection highlighting (Phase 3.10)
    // Note: ParagraphLayout also draws selection when draw() is called
    // but we draw additional highlighting for visual feedback
    drawSelection(&painter);

    // Draw cursor (Phase 3.5)
    if (m_cursorVisible && hasFocus()) {
        drawCursor(&painter);
    }
}

void BookEditor::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Update layout and scroll managers with new size
    updateLayoutWidth();
    updateViewport();

    // Relayout visible paragraphs with new width
    if (m_document != nullptr) {
        m_layoutManager->layoutVisibleParagraphs();
    }

    // Update scrollbar range as content layout may have changed
    updateScrollBarRange();
}

void BookEditor::wheelEvent(QWheelEvent* event)
{
    // Get the scroll delta in degrees (each step = 15 degrees typically)
    QPoint angleDelta = event->angleDelta();

    // Vertical scrolling
    if (!angleDelta.isNull()) {
        // Calculate scroll delta - invert so scroll up moves content down
        // Qt reports positive Y for scroll up, but we want to decrease offset
        qreal delta = -angleDelta.y() / 8.0;  // Convert from 1/8 degree steps
        delta = delta / 15.0 * WHEEL_SCROLL_STEP;  // Convert degrees to pixels

        if (m_smoothScrollingEnabled) {
            // For smooth scrolling, accumulate with current animation target
            qreal currentTarget = scrollOffset();
            if (m_scrollAnimation != nullptr &&
                m_scrollAnimation->state() == QAbstractAnimation::Running) {
                currentTarget = m_scrollAnimation->endValue().toReal();
            }
            scrollTo(currentTarget + delta, true);
        } else {
            scrollBy(delta, false);
        }

        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void BookEditor::keyPressEvent(QKeyEvent* event)
{
    // Handle cursor navigation keys
    bool handled = false;
    bool ctrl = event->modifiers() & Qt::ControlModifier;
    bool shift = event->modifiers() & Qt::ShiftModifier;

    switch (event->key()) {
        case Qt::Key_Left:
            if (ctrl && shift) {
                moveCursorWordLeftWithSelection(true);
            } else if (ctrl) {
                moveCursorWordLeftWithSelection(false);
            } else if (shift) {
                moveCursorLeftWithSelection(true);
            } else {
                moveCursorLeftWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_Right:
            if (ctrl && shift) {
                moveCursorWordRightWithSelection(true);
            } else if (ctrl) {
                moveCursorWordRightWithSelection(false);
            } else if (shift) {
                moveCursorRightWithSelection(true);
            } else {
                moveCursorRightWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_Up:
            if (shift) {
                moveCursorUpWithSelection(true);
            } else {
                moveCursorUpWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_Down:
            if (shift) {
                moveCursorDownWithSelection(true);
            } else {
                moveCursorDownWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_Home:
            if (ctrl && shift) {
                moveCursorToDocStartWithSelection(true);
            } else if (ctrl) {
                moveCursorToDocStartWithSelection(false);
            } else if (shift) {
                moveCursorToLineStartWithSelection(true);
            } else {
                moveCursorToLineStartWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_End:
            if (ctrl && shift) {
                moveCursorToDocEndWithSelection(true);
            } else if (ctrl) {
                moveCursorToDocEndWithSelection(false);
            } else if (shift) {
                moveCursorToLineEndWithSelection(true);
            } else {
                moveCursorToLineEndWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_PageUp:
            moveCursorPageUp();
            if (!shift) {
                clearSelection();
            }
            handled = true;
            break;

        case Qt::Key_PageDown:
            moveCursorPageDown();
            if (!shift) {
                clearSelection();
            }
            handled = true;
            break;

        case Qt::Key_A:
            if (ctrl) {
                selectAll();
                handled = true;
            }
            break;

        case Qt::Key_Z:
            if (ctrl && !shift) {
                undo();
                handled = true;
            } else if (ctrl && shift) {
                redo();  // Ctrl+Shift+Z is redo on some platforms
                handled = true;
            }
            break;

        case Qt::Key_Y:
            if (ctrl) {
                redo();
                handled = true;
            }
            break;

        case Qt::Key_Return:
        case Qt::Key_Enter:
            insertNewline();
            handled = true;
            break;

        case Qt::Key_Backspace:
            deleteBackward();
            handled = true;
            break;

        case Qt::Key_Delete:
            deleteForward();
            handled = true;
            break;

        default:
            break;
    }

    // Handle printable characters (if not already handled)
    if (!handled && !ctrl && !event->text().isEmpty()) {
        QString text = event->text();
        // Only handle printable characters
        if (!text.isEmpty() && text.at(0).isPrint()) {
            insertText(text);
            handled = true;
        }
    }

    if (handled) {
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void BookEditor::inputMethodEvent(QInputMethodEvent* event)
{
    if (m_document == nullptr) {
        event->ignore();
        return;
    }

    // Handle committed text (final input)
    const QString& commitString = event->commitString();
    if (!commitString.isEmpty()) {
        // If we had composition, it's been replaced by commit
        if (m_hasComposition) {
            // The preedit text is already in the document, delete it first
            if (!m_preeditString.isEmpty()) {
                CursorPosition deleteEnd = m_preeditStart;
                deleteEnd.offset += m_preeditString.length();
                m_document->deleteText(m_preeditStart, deleteEnd);
                setCursorPosition(m_preeditStart);
            }
            m_preeditString.clear();
            m_hasComposition = false;
        }

        // Insert committed text (this handles selection deletion too)
        insertText(commitString);
    }

    // Handle preedit text (composition in progress)
    const QString& preeditString = event->preeditString();
    if (m_hasComposition) {
        // Remove old preedit text
        if (!m_preeditString.isEmpty()) {
            CursorPosition deleteEnd = m_preeditStart;
            deleteEnd.offset += m_preeditString.length();
            m_document->deleteText(m_preeditStart, deleteEnd);
            setCursorPosition(m_preeditStart);
        }
    }

    if (!preeditString.isEmpty()) {
        // Store composition state
        m_preeditStart = m_cursorPosition;
        m_preeditString = preeditString;
        m_hasComposition = true;

        // Insert preedit text
        m_document->insertText(m_cursorPosition, preeditString);

        // Move cursor to end of preedit
        CursorPosition newPos = m_cursorPosition;
        newPos.offset += preeditString.length();
        setCursorPosition(newPos);
    } else {
        // No preedit, clear composition state
        m_preeditString.clear();
        m_hasComposition = false;
    }

    event->accept();
    update();
}

QVariant BookEditor::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
        case Qt::ImEnabled:
            return true;

        case Qt::ImCursorRectangle: {
            // Return cursor rectangle for IME positioning
            QRectF rect = calculateCursorRect();
            if (rect.isEmpty()) {
                // Default position at top-left with some offset
                return QRectF(10, 10, 2, 20);
            }
            return rect;
        }

        case Qt::ImFont:
            return font();

        case Qt::ImCursorPosition:
            return m_cursorPosition.offset;

        case Qt::ImSurroundingText: {
            // Return text of current paragraph
            if (m_document != nullptr) {
                const KmlParagraph* para = m_document->paragraph(m_cursorPosition.paragraph);
                if (para != nullptr) {
                    return para->plainText();
                }
            }
            return QString();
        }

        case Qt::ImCurrentSelection: {
            if (hasSelection()) {
                return selectedText();
            }
            return QString();
        }

        case Qt::ImAnchorPosition:
            return m_selectionAnchor.offset;

        case Qt::ImHints:
            return static_cast<int>(Qt::ImhMultiLine);

        default:
            break;
    }

    return QWidget::inputMethodQuery(query);
}

// =============================================================================
// Private Slots
// =============================================================================

void BookEditor::onScrollBarValueChanged(int value)
{
    // Avoid recursive updates
    if (m_updatingScrollBar) {
        return;
    }

    // Stop any running animation when user manually scrolls
    stopScrollAnimation();

    // Update scroll manager with new offset
    setScrollOffset(static_cast<qreal>(value));
}

void BookEditor::onScrollAnimationValueChanged(const QVariant& value)
{
    setScrollOffset(value.toReal());
}

void BookEditor::onCursorBlinkTimeout()
{
    m_cursorVisible = !m_cursorVisible;
    update();  // Request repaint to show/hide cursor
}

// =============================================================================
// Private Methods
// =============================================================================

void BookEditor::setupComponents()
{
    // Connect layout manager to scroll manager
    m_layoutManager->setScrollManager(m_scrollManager.get());

    // Set default font
    m_layoutManager->setFont(font());

    // Enable focus for keyboard input
    setFocusPolicy(Qt::StrongFocus);

    // Setup scrollbar
    setupScrollBar();

    // Setup cursor blink timer
    setupCursorBlinkTimer();

    // Set a reasonable initial width
    updateLayoutWidth();
    updateViewport();
}

void BookEditor::updateLayoutWidth()
{
    // Use widget width for layout, with some margin
    constexpr int margin = 20;  // 10px on each side
    qreal layoutWidth = qMax(100.0, static_cast<qreal>(width() - margin));
    m_layoutManager->setWidth(layoutWidth);
}

void BookEditor::updateViewport()
{
    // Update scroll manager with current viewport dimensions
    m_scrollManager->setViewportHeight(static_cast<qreal>(height()));
}

void BookEditor::setupScrollBar()
{
    // Create vertical scrollbar
    m_verticalScrollBar = new QScrollBar(Qt::Vertical, this);
    m_verticalScrollBar->setMinimum(0);
    m_verticalScrollBar->setMaximum(0);
    m_verticalScrollBar->setSingleStep(static_cast<int>(WHEEL_SCROLL_STEP));
    m_verticalScrollBar->setPageStep(height());

    // Position scrollbar on right edge
    // Note: Position will be updated in resizeEvent
    m_verticalScrollBar->setGeometry(
        width() - m_verticalScrollBar->sizeHint().width(),
        0,
        m_verticalScrollBar->sizeHint().width(),
        height()
    );

    // Connect scrollbar value changes
    connect(m_verticalScrollBar, &QScrollBar::valueChanged,
            this, &BookEditor::onScrollBarValueChanged);

    // Note: Scroll animation is created lazily in startScrollAnimation()
    // to avoid potential issues with QPropertyAnimation in test environments
}

void BookEditor::updateScrollBarRange()
{
    if (m_verticalScrollBar == nullptr) {
        return;
    }

    // Calculate total content height
    qreal totalHeight = m_scrollManager->totalHeight();
    qreal viewportHeight = static_cast<qreal>(height());

    // Maximum scroll offset
    qreal maxOffset = qMax(0.0, totalHeight - viewportHeight);

    // Update scrollbar without triggering signals
    m_updatingScrollBar = true;

    m_verticalScrollBar->setMaximum(static_cast<int>(maxOffset));
    m_verticalScrollBar->setPageStep(static_cast<int>(viewportHeight));

    m_updatingScrollBar = false;

    // Position scrollbar on right edge (also update position on resize)
    int scrollBarWidth = m_verticalScrollBar->sizeHint().width();
    m_verticalScrollBar->setGeometry(
        width() - scrollBarWidth,
        0,
        scrollBarWidth,
        height()
    );

    // Sync current value
    syncScrollBarValue();
}

void BookEditor::syncScrollBarValue()
{
    if (m_verticalScrollBar == nullptr) {
        return;
    }

    m_updatingScrollBar = true;
    m_verticalScrollBar->setValue(static_cast<int>(scrollOffset()));
    m_updatingScrollBar = false;
}

void BookEditor::startScrollAnimation(qreal targetOffset)
{
    // Lazily create the animation on first use
    if (m_scrollAnimation == nullptr) {
        m_scrollAnimation = new QPropertyAnimation(this);
        m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);

        // Connect animation value changes
        connect(m_scrollAnimation, &QPropertyAnimation::valueChanged,
                this, &BookEditor::onScrollAnimationValueChanged);
    }

    // Clamp target to valid range
    targetOffset = qBound(0.0, targetOffset, m_scrollManager->maxScrollOffset());

    // Stop any existing animation
    m_scrollAnimation->stop();

    // Configure animation
    m_scrollAnimation->setDuration(m_smoothScrollDuration);
    m_scrollAnimation->setStartValue(scrollOffset());
    m_scrollAnimation->setEndValue(targetOffset);

    // Start animation
    m_scrollAnimation->start();
}

void BookEditor::stopScrollAnimation()
{
    if (m_scrollAnimation != nullptr) {
        m_scrollAnimation->stop();
    }
}

CursorPosition BookEditor::validateCursorPosition(const CursorPosition& position) const
{
    CursorPosition result = position;

    // If no document, return origin position
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return {0, 0};
    }

    // Clamp paragraph index to valid range
    int maxParagraph = m_document->paragraphCount() - 1;
    result.paragraph = qBound(0, result.paragraph, maxParagraph);

    // Clamp offset to paragraph length
    const KmlParagraph* para = m_document->paragraph(result.paragraph);
    if (para != nullptr) {
        int maxOffset = para->characterCount();
        result.offset = qBound(0, result.offset, maxOffset);
    } else {
        result.offset = 0;
    }

    return result;
}

QRectF BookEditor::calculateCursorRect() const
{
    // If no document or cursor paragraph is invalid, return empty rect
    if (m_document == nullptr) {
        return QRectF();
    }

    int paraIndex = m_cursorPosition.paragraph;
    if (paraIndex < 0 || paraIndex >= m_document->paragraphCount()) {
        return QRectF();
    }

    // Get paragraph layout
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(paraIndex);
    if (layout == nullptr) {
        return QRectF();
    }

    // Get cursor rect from layout (in layout coordinates)
    QRectF layoutCursorRect = layout->cursorRect(m_cursorPosition.offset);
    if (layoutCursorRect.isEmpty()) {
        // Fallback for empty paragraph or end of paragraph
        layoutCursorRect = QRectF(0, 0, CURSOR_WIDTH, layout->height() > 0 ? layout->height() : 20.0);
    }

    // Convert to widget coordinates
    qreal paraY = m_layoutManager->paragraphY(paraIndex);
    qreal scrollY = m_scrollManager->scrollOffset();
    qreal widgetY = TOP_MARGIN + paraY - scrollY + layoutCursorRect.y();
    qreal widgetX = LEFT_MARGIN + layoutCursorRect.x();

    return QRectF(widgetX, widgetY, CURSOR_WIDTH, layoutCursorRect.height());
}

void BookEditor::drawCursor(QPainter* painter)
{
    QRectF cursorRect = calculateCursorRect();
    if (cursorRect.isEmpty()) {
        return;
    }

    // Check if cursor is within visible area
    if (cursorRect.bottom() < 0 || cursorRect.top() > height()) {
        return;
    }

    // Draw cursor as a filled rectangle using text color
    QColor cursorColor = palette().color(QPalette::Text);
    painter->fillRect(cursorRect, cursorColor);
}

void BookEditor::setupCursorBlinkTimer()
{
    m_cursorBlinkTimer = new QTimer(this);

    connect(m_cursorBlinkTimer, &QTimer::timeout,
            this, &BookEditor::onCursorBlinkTimeout);

    // Start the timer if blinking is enabled
    if (m_cursorBlinkingEnabled) {
        m_cursorBlinkTimer->start(m_cursorBlinkInterval);
    }
}

// =============================================================================
// Mouse Event Handlers (Phase 3.9/3.10/3.11)
// =============================================================================

void BookEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Set focus on click
    setFocus();

    QPointF clickPos = event->position();

    // Check for multi-click (double/triple)
    bool isMultiClick = false;
    if (m_clickTimer != nullptr && m_clickTimer->isActive()) {
        // Check distance from last click
        qreal distance = (clickPos - m_lastClickPos).manhattanLength();
        if (distance <= MULTI_CLICK_DISTANCE) {
            ++m_clickCount;
            isMultiClick = true;
        } else {
            m_clickCount = 1;
        }
    } else {
        m_clickCount = 1;
    }

    m_lastClickPos = clickPos;

    // Start/restart click timer
    if (m_clickTimer == nullptr) {
        m_clickTimer = new QTimer(this);
        m_clickTimer->setSingleShot(true);
        connect(m_clickTimer, &QTimer::timeout, this, [this]() {
            m_clickCount = 0;
        });
    }
    m_clickTimer->start(MULTI_CLICK_INTERVAL);

    // Convert click to cursor position
    CursorPosition clickPosition = positionFromPoint(clickPos);

    if (m_clickCount == 3) {
        // Triple click - select paragraph
        m_cursorPosition = clickPosition;
        selectParagraphAtCursor();
    } else if (m_clickCount == 2 || isMultiClick) {
        // Double click - select word (handled in mouseDoubleClickEvent)
        // But we still need to set position for the case when double-click
        // is detected through our click counting
        m_cursorPosition = clickPosition;
        selectWordAtCursor();
    } else {
        // Single click - position cursor
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift+click extends selection
            extendSelection(clickPosition);
        } else {
            // Normal click clears selection and positions cursor
            clearSelection();
            m_selectionAnchor = clickPosition;
            setCursorPosition(clickPosition);
        }

        // Start drag selection
        m_isDragging = true;
    }

    event->accept();
}

void BookEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_isDragging || !(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // Get position from mouse
    CursorPosition dragPosition = positionFromPoint(event->position());

    // Update selection from anchor to current position
    SelectionRange newSelection;
    newSelection.start = m_selectionAnchor;
    newSelection.end = dragPosition;

    // Move cursor to drag position
    m_cursorPosition = dragPosition;
    setSelection(newSelection);
    ensureCursorVisible();

    event->accept();
}

void BookEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void BookEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    // Position cursor at double-click location
    CursorPosition clickPosition = positionFromPoint(event->position());
    m_cursorPosition = clickPosition;

    // Select word at cursor
    selectWordAtCursor();

    // Update click count for triple-click detection
    m_clickCount = 2;
    m_lastClickPos = event->position();
    if (m_clickTimer != nullptr) {
        m_clickTimer->start(MULTI_CLICK_INTERVAL);
    }

    event->accept();
}

// =============================================================================
// Private Methods (Phase 3.9/3.10/3.11/3.12)
// =============================================================================

CursorPosition BookEditor::positionFromPoint(const QPointF& widgetPos) const
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return {0, 0};
    }

    // Convert from widget coordinates to document coordinates
    qreal scrollY = m_scrollManager->scrollOffset();
    qreal documentY = widgetPos.y() - TOP_MARGIN + scrollY;
    qreal documentX = widgetPos.x() - LEFT_MARGIN;

    // Find paragraph at this Y position
    int paraIndex = m_scrollManager->paragraphAtY(documentY);
    if (paraIndex < 0) {
        paraIndex = 0;
    } else if (paraIndex >= m_document->paragraphCount()) {
        paraIndex = m_document->paragraphCount() - 1;
    }

    // Get layout for paragraph
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(paraIndex);
    if (layout == nullptr) {
        m_layoutManager->layoutParagraph(paraIndex);
        layout = m_layoutManager->paragraphLayout(paraIndex);
    }

    int offset = 0;
    if (layout != nullptr) {
        // Calculate relative Y within paragraph
        qreal paraY = m_layoutManager->paragraphY(paraIndex);
        QPointF relativePoint(documentX, documentY - paraY);

        // Use layout hit testing
        offset = layout->positionAt(relativePoint);
        if (offset < 0) {
            offset = 0;
        }
    }

    return {paraIndex, offset};
}

void BookEditor::drawSelection(QPainter* painter)
{
    if (m_selection.isEmpty() || m_document == nullptr) {
        return;
    }

    SelectionRange sel = m_selection.normalized();
    QColor selectionColor = palette().color(QPalette::Highlight);
    selectionColor.setAlpha(128);  // Semi-transparent

    qreal scrollY = m_scrollManager->scrollOffset();

    for (int paraIndex = sel.start.paragraph; paraIndex <= sel.end.paragraph; ++paraIndex) {
        ParagraphLayout* layout = m_layoutManager->paragraphLayout(paraIndex);
        if (layout == nullptr) {
            continue;
        }

        qreal paraY = m_layoutManager->paragraphY(paraIndex);
        qreal widgetY = TOP_MARGIN + paraY - scrollY;

        // Determine selection range within this paragraph
        int startOffset = (paraIndex == sel.start.paragraph) ? sel.start.offset : 0;
        int endOffset = (paraIndex == sel.end.paragraph) ? sel.end.offset : layout->text().length();

        // Get selection rectangles from layout
        const QTextLayout& textLayout = layout->textLayout();
        for (int lineIdx = 0; lineIdx < textLayout.lineCount(); ++lineIdx) {
            QTextLine line = textLayout.lineAt(lineIdx);
            int lineStart = line.textStart();
            int lineEnd = lineStart + line.textLength();

            // Check if this line overlaps with selection
            if (endOffset <= lineStart || startOffset >= lineEnd) {
                continue;
            }

            // Calculate selection x range on this line
            int selStart = qMax(startOffset, lineStart);
            int selEnd = qMin(endOffset, lineEnd);

            qreal x1 = line.cursorToX(selStart);
            qreal x2 = line.cursorToX(selEnd);

            QRectF selRect(
                LEFT_MARGIN + x1,
                widgetY + line.y(),
                x2 - x1,
                line.height()
            );

            painter->fillRect(selRect, selectionColor);
        }
    }
}

void BookEditor::selectWordAtCursor()
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
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
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    const KmlParagraph* para = m_document->paragraph(m_cursorPosition.paragraph);
    if (para == nullptr) {
        return;
    }

    SelectionRange range;
    range.start = {m_cursorPosition.paragraph, 0};
    range.end = {m_cursorPosition.paragraph, para->characterCount()};

    m_selectionAnchor = range.start;
    m_cursorPosition = range.end;

    setSelection(range);
}

std::pair<int, int> BookEditor::findWordBoundaries(int paraIndex, int offset) const
{
    if (m_document == nullptr) {
        return {0, 0};
    }

    const KmlParagraph* para = m_document->paragraph(paraIndex);
    if (para == nullptr) {
        return {0, 0};
    }

    QString text = para->plainText();
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
    if (m_document == nullptr) {
        return;
    }

    SelectionRange sel = m_selection.normalized();

    // Update each visible paragraph layout with its selection range
    auto [firstVisible, lastVisible] = m_scrollManager->visibleRange();
    if (firstVisible < 0) {
        return;
    }

    for (int paraIndex = firstVisible; paraIndex <= lastVisible; ++paraIndex) {
        ParagraphLayout* layout = m_layoutManager->paragraphLayout(paraIndex);
        if (layout == nullptr) {
            continue;
        }

        if (m_selection.isEmpty()) {
            layout->clearSelection();
        } else if (paraIndex < sel.start.paragraph || paraIndex > sel.end.paragraph) {
            layout->clearSelection();
        } else {
            int startOffset = (paraIndex == sel.start.paragraph) ? sel.start.offset : 0;
            int endOffset = (paraIndex == sel.end.paragraph) ? sel.end.offset : layout->text().length();
            layout->setSelection(startOffset, endOffset);
        }
    }
}

// =============================================================================
// Selection-aware Cursor Movement (Phase 3.12)
// =============================================================================

void BookEditor::moveCursorLeftWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // If there's a selection and not extending, collapse to start
    if (!extend && hasSelection()) {
        SelectionRange sel = m_selection.normalized();
        setCursorPosition(sel.start);
        clearSelection();
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // Move cursor
    moveCursorLeft();

    // Extend or clear selection
    if (extend) {
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
    }
}

void BookEditor::moveCursorRightWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // If there's a selection and not extending, collapse to end
    if (!extend && hasSelection()) {
        SelectionRange sel = m_selection.normalized();
        setCursorPosition(sel.end);
        clearSelection();
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // Move cursor
    moveCursorRight();

    // Extend or clear selection
    if (extend) {
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
    }
}

void BookEditor::moveCursorUpWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // Move cursor
    moveCursorUp();

    // Extend or clear selection
    if (extend) {
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
    }
}

void BookEditor::moveCursorDownWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // Move cursor
    moveCursorDown();

    // Extend or clear selection
    if (extend) {
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
    }
}

void BookEditor::moveCursorWordLeftWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // Move cursor
    moveCursorWordLeft();

    // Extend or clear selection
    if (extend) {
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
    }
}

void BookEditor::moveCursorWordRightWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // Move cursor
    moveCursorWordRight();

    // Extend or clear selection
    if (extend) {
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
    }
}

void BookEditor::moveCursorToLineStartWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // Move cursor
    moveCursorToLineStart();

    // Extend or clear selection
    if (extend) {
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
    }
}

void BookEditor::moveCursorToLineEndWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // Move cursor
    moveCursorToLineEnd();

    // Extend or clear selection
    if (extend) {
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
    }
}

void BookEditor::moveCursorToDocStartWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // For doc start/end, we need to set position before calling
    // moveCursorToDocStart since it also scrolls
    CursorPosition newPos = {0, 0};

    if (extend) {
        // Just set cursor position without scrolling first
        m_preferredCursorXValid = false;
        setCursorPosition(newPos);
        setScrollOffset(0.0);
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
        moveCursorToDocStart();
    }
}

void BookEditor::moveCursorToDocEndWithSelection(bool extend)
{
    if (m_document == nullptr || m_document->paragraphCount() == 0) {
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    int lastPara = m_document->paragraphCount() - 1;
    const KmlParagraph* para = m_document->paragraph(lastPara);
    CursorPosition newPos = {lastPara, para ? para->characterCount() : 0};

    if (extend) {
        m_preferredCursorXValid = false;
        setCursorPosition(newPos);
        setScrollOffset(m_scrollManager->maxScrollOffset());
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
        moveCursorToDocEnd();
    }
}

}  // namespace kalahari::editor
