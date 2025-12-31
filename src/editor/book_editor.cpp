/// @file book_editor.cpp
/// @brief BookEditor implementation (OpenSpec #00042 Phase 3.1-3.5)

#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kml_commands.h>
#include <kalahari/editor/kml_comment.h>
#include <kalahari/editor/kml_element.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_parser.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/paragraph_layout.h>
#include <kalahari/gui/find_replace_bar.h>
#include <QAbstractTextDocumentLayout>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QEasingCurve>
#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyleHints>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QUndoStack>
#include <QWheelEvent>
#include <chrono>

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
// Phase 11.6: Helper functions for QTextDocument paragraph operations
// These replace TextBuffer methods with direct QTextDocument access
// =============================================================================

/// @brief Get paragraph length (excluding block separator)
/// @param doc QTextDocument pointer
/// @param index Block/paragraph index
/// @return Character count in paragraph (length - 1 to exclude block separator)
inline int paragraphLength(QTextDocument* doc, int index) {
    if (!doc) return 0;
    QTextBlock block = doc->findBlockByNumber(index);
    return block.isValid() ? (block.length() - 1) : 0;
}

/// @brief Get paragraph text
/// @param doc QTextDocument pointer
/// @param index Block/paragraph index
/// @return Text content of the paragraph
inline QString paragraphText(QTextDocument* doc, int index) {
    if (!doc) return QString();
    QTextBlock block = doc->findBlockByNumber(index);
    return block.isValid() ? block.text() : QString();
}

/// @brief Get Y position of paragraph using block layout
/// @param doc QTextDocument pointer
/// @param index Block/paragraph index
/// @return Y position in document coordinates
inline double getParagraphY(QTextDocument* doc, int index) {
    if (!doc) return 0.0;
    QTextBlock block = doc->findBlockByNumber(index);
    if (block.isValid() && block.layout()) {
        return block.layout()->position().y();
    }
    return 0.0;
}

/// @brief Find paragraph at Y position using document layout hit test
/// @param doc QTextDocument pointer
/// @param y Y coordinate in document
/// @return Block/paragraph index
inline int getParagraphAtY(QTextDocument* doc, double y) {
    if (!doc) return 0;
    if (auto* layout = doc->documentLayout()) {
        int pos = layout->hitTest(QPointF(0, y), Qt::FuzzyHit);
        if (pos >= 0) {
            QTextBlock block = doc->findBlock(pos);
            return block.isValid() ? block.blockNumber() : 0;
        }
    }
    return 0;
}

// =============================================================================
// Construction / Destruction
// =============================================================================

BookEditor::BookEditor(QWidget* parent)
    : QWidget(parent)
    , m_document(nullptr)
    , m_layoutManager(std::make_unique<LayoutManager>())
    , m_scrollManager(std::make_unique<VirtualScrollManager>())
    , m_pageLayoutManager(std::make_unique<PageLayoutManager>())
    , m_verticalScrollBar(nullptr)
    , m_scrollAnimation(nullptr)
    , m_typewriterScrollAnimation(nullptr)
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
    // Phase 8: New performance-optimized components (OpenSpec #00043)
    // Phase 11.6: Removed FormatLayer, LazyLayoutManager, MetadataLayer, TextBuffer - use QTextDocument directly
    , m_textBuffer(std::make_unique<QTextDocument>())
    // Phase 11.6: Removed m_metadataLayer - markers stored in QTextCharFormat::UserProperty
{
    // Enable input method support
    setAttribute(Qt::WA_InputMethodEnabled, true);

    // Create undo stack
    m_undoStack = new QUndoStack(this);

    // Create UI fade timer for distraction-free mode
    m_uiFadeTimer = new QTimer(this);
    m_uiFadeTimer->setSingleShot(true);
    connect(m_uiFadeTimer, &QTimer::timeout, this, [this]() {
        // Fade out UI opacity
        m_uiOpacity = 0.0;
        update();
    });

    // Phase 11.6: Direct QTextDocument usage (no wrapper layers)
    // Set document width for text wrapping
    if (m_textBuffer) {
        m_textBuffer->setTextWidth(static_cast<double>(width()) - LEFT_MARGIN * 2);
    }

    // Phase 11.6: Initialize QTextCursor for direct cursor operations
    m_textCursor = QTextCursor(m_textBuffer.get());

    // Create ViewportManager and connect to document
    m_viewportManager = std::make_unique<ViewportManager>(this);
    m_viewportManager->setDocument(m_textBuffer.get());

    // Create RenderEngine and connect all dependencies
    m_renderEngine = std::make_unique<RenderEngine>(this);
    m_renderEngine->setDocument(m_textBuffer.get());
    m_renderEngine->setViewportManager(m_viewportManager.get());

    // Connect RenderEngine repaint signal
    connect(m_renderEngine.get(), &RenderEngine::repaintRequested,
            this, [this](const QRegion& region) {
        update(region.boundingRect());
    });

    // Connect ViewportManager signals
    connect(m_viewportManager.get(), &ViewportManager::viewportChanged,
            this, [this]() {
        update();
    });

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
    if (m_typewriterScrollAnimation != nullptr) {
        m_typewriterScrollAnimation->stop();
    }
    if (m_uiFadeTimer != nullptr) {
        m_uiFadeTimer->stop();
    }
}

// =============================================================================
// Document Management
// =============================================================================

void BookEditor::setDocument(KmlDocument* document)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("BookEditor::setDocument - start, document={}", (void*)document);

    if (m_document == document) {
        logger.debug("BookEditor::setDocument - same document, returning");
        return;
    }

    m_document = document;
    logger.debug("BookEditor::setDocument - document assigned");

    // Update managers with new document
    logger.debug("BookEditor::setDocument - calling scrollManager->setDocument...");
    m_scrollManager->setDocument(document);
    logger.debug("BookEditor::setDocument - calling layoutManager->setDocument...");
    m_layoutManager->setDocument(document);
    logger.debug("BookEditor::setDocument - managers updated");

    // Update viewport and layout width
    logger.debug("BookEditor::setDocument - calling updateViewport...");
    updateViewport();
    logger.debug("BookEditor::setDocument - calling updateLayoutWidth...");
    updateLayoutWidth();
    logger.debug("BookEditor::setDocument - viewport/layout updated");

    // Update scrollbar range for new document
    logger.debug("BookEditor::setDocument - calling updateScrollBarRange...");
    updateScrollBarRange();
    logger.debug("BookEditor::setDocument - scrollbar updated");

    // Sync to QTextDocument for new architecture (backward compatibility with tests)
    if (m_textBuffer && document) {
        QStringList paragraphs;
        int paraCount = static_cast<int>(document->paragraphCount());
        for (int i = 0; i < paraCount; ++i) {
            const KmlParagraph* para = document->paragraph(i);
            if (para) {
                paragraphs.append(para->plainText());
            }
        }
        QString fullText = paragraphs.join(QChar::ParagraphSeparator);
        // Phase 11.6: Set text using QTextCursor instead of TextBuffer::setPlainText
        QTextCursor cursor(m_textBuffer.get());
        cursor.select(QTextCursor::Document);
        cursor.insertText(fullText);
        logger.debug("BookEditor::setDocument - QTextDocument synced ({} paragraphs)", paraCount);
        // Note: QTextDocument handles layout refresh automatically
    }

    // Sync cursor to RenderEngine (Phase 8 fix)
    // Note: We don't reset cursor position here - caller may want to preserve it
    // The cursor position is validated lazily on next setCursorPosition call
    if (m_renderEngine) {
        m_renderEngine->setCursorPosition(m_cursorPosition);
        m_renderEngine->setCursorVisible(true);
        if (m_cursorBlinkingEnabled) {
            m_renderEngine->startCursorBlink();
        }
    }

    emit documentChanged();
    update();  // Request repaint
    logger.debug("BookEditor::setDocument - complete");
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

        // Release distant layouts to bound memory usage
        auto [firstVisible, lastVisible] = m_scrollManager->visibleRange();
        m_layoutManager->releaseDistantLayouts(firstVisible, lastVisible);

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

        // Sync cursor position to RenderEngine (Phase 8 fix)
        if (m_renderEngine) {
            m_renderEngine->setCursorPosition(validatedPos);
        }

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
        // Sync to RenderEngine (Phase 8 fix)
        if (m_renderEngine) {
            m_renderEngine->startCursorBlink();
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
        // Sync to RenderEngine (Phase 8 fix)
        if (m_renderEngine) {
            m_renderEngine->stopCursorBlink();
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

    // Sync to RenderEngine (Phase 8 fix)
    if (m_renderEngine) {
        m_renderEngine->setCursorBlinkInterval(m_cursorBlinkInterval);
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

    // Sync visibility to RenderEngine (Phase 8 fix)
    if (m_renderEngine) {
        m_renderEngine->setCursorVisible(true);
        if (m_cursorBlinkingEnabled) {
            m_renderEngine->startCursorBlink();
        }
    }

    // In Typewriter mode, update scroll to keep cursor at focus position
    if (m_viewMode == ViewMode::Typewriter) {
        updateTypewriterScroll();
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
        --newPos.offset;
    } else if (newPos.paragraph > 0) {
        --newPos.paragraph;
        newPos.offset = paragraphLength(m_textBuffer.get(), newPos.paragraph);
    }
    // else: already at document start, do nothing

    setCursorPosition(newPos);
    ensureCursorVisible();
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
        ++newPos.offset;
    } else if (static_cast<size_t>(newPos.paragraph) < m_textBuffer->blockCount() - 1) {
        ++newPos.paragraph;
        newPos.offset = 0;
    }
    // else: already at document end, do nothing

    setCursorPosition(newPos);
    ensureCursorVisible();
}

void BookEditor::moveCursorUp()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    CursorPosition newPos = m_cursorPosition;

    if (newPos.paragraph > 0) {
        // Move to end of previous paragraph
        --newPos.paragraph;
        newPos.offset = paragraphLength(m_textBuffer.get(), newPos.paragraph);
    } else {
        // At first paragraph: move to start
        newPos.offset = 0;
    }

    setCursorPosition(newPos);
    ensureCursorVisible();
}

void BookEditor::moveCursorDown()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    CursorPosition newPos = m_cursorPosition;

    if (static_cast<size_t>(newPos.paragraph) < m_textBuffer->blockCount() - 1) {
        // Move to start of next paragraph
        ++newPos.paragraph;
        newPos.offset = 0;
    } else {
        // At last paragraph: move to end
        newPos.offset = paragraphLength(m_textBuffer.get(), newPos.paragraph);
    }

    setCursorPosition(newPos);
    ensureCursorVisible();
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
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;
    newPos.offset = 0;  // Move to paragraph start

    setCursorPosition(newPos);
    ensureCursorVisible();
}

void BookEditor::moveCursorToLineEnd()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;
    newPos.offset = paragraphLength(m_textBuffer.get(), newPos.paragraph);

    setCursorPosition(newPos);
    ensureCursorVisible();
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

        if (m_renderEngine) {
            m_renderEngine->clearSelection();
        }

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

    if (m_renderEngine) {
        m_renderEngine->setSelection(range);
    }

    update();
}

// =============================================================================
// Text Input (Phase 4.1 - 4.4)
// =============================================================================

void BookEditor::insertText(const QString& text)
{
    if (!m_textBuffer || text.isEmpty()) {
        return;
    }

    CursorPosition cursorBefore = m_cursorPosition;

    // Handle selection replacement
    if (hasSelection()) {
        SelectionRange sel = m_selection.normalized();

        // Get text being deleted for undo
        QString deletedText;
        for (int paraIdx = sel.start.paragraph; paraIdx <= sel.end.paragraph; ++paraIdx) {
            QString paraText = paragraphText(m_textBuffer.get(), paraIdx);
            int startOff = (paraIdx == sel.start.paragraph) ? sel.start.offset : 0;
            int endOff = (paraIdx == sel.end.paragraph) ? sel.end.offset : paraText.length();
            deletedText += paraText.mid(startOff, endOff - startOff);
            if (paraIdx < sel.end.paragraph) {
                deletedText += QChar::ParagraphSeparator;
            }
        }

        // Create composite command for delete + insert
        auto* composite = new CompositeDocumentCommand(
            m_textBuffer.get(), cursorBefore, tr("Replace"));

        // Delete selection first
        composite->addCommand(std::make_unique<TextDeleteCommand>(
            m_textBuffer.get(), sel.start, sel.end, deletedText));

        // Then insert text at selection start
        composite->addCommand(std::make_unique<TextInsertCommand>(
            m_textBuffer.get(), sel.start, text));

        m_undoStack->push(composite);  // push() calls redo() automatically

        // Update cursor position (redo already executed)
        m_cursorPosition = sel.start;
        m_cursorPosition.offset += text.length();
        clearSelection();
    } else {
        // Simple insert - push single command
        m_undoStack->push(new TextInsertCommand(
            m_textBuffer.get(), m_cursorPosition, text));

        // Update cursor position (redo already executed)
        m_cursorPosition.offset += text.length();
    }


    ensureCursorVisible();
    update();
    emit contentChanged();
    emit paragraphModified(m_cursorPosition.paragraph);
}

bool BookEditor::deleteSelectedText()
{
    if (!hasSelection() || !m_textBuffer) {
        return false;
    }

    SelectionRange sel = m_selection.normalized();
    CursorPosition cursorBefore = m_cursorPosition;

    // Get text being deleted for undo
    QString deletedText;
    for (int paraIdx = sel.start.paragraph; paraIdx <= sel.end.paragraph; ++paraIdx) {
        QString paraText = paragraphText(m_textBuffer.get(), paraIdx);
        int startOff = (paraIdx == sel.start.paragraph) ? sel.start.offset : 0;
        int endOff = (paraIdx == sel.end.paragraph) ? sel.end.offset : paraText.length();
        deletedText += paraText.mid(startOff, endOff - startOff);
        if (paraIdx < sel.end.paragraph) {
            deletedText += QChar::ParagraphSeparator;
        }
    }

    // Push delete command (push() calls redo() automatically)
    m_undoStack->push(new TextDeleteCommand(
        m_textBuffer.get(), sel.start, sel.end, deletedText));

    // Move cursor to start of deleted range (redo already executed)
    m_cursorPosition = sel.start;
    clearSelection();

    update();
    emit contentChanged();
    return true;
}

void BookEditor::insertNewline()
{
    if (!m_textBuffer) {
        return;
    }

    if (hasSelection()) {
        deleteSelectedText();
    }

    CursorPosition cursorBefore = m_cursorPosition;

    // Push paragraph split command (push() calls redo() automatically)
    m_undoStack->push(new ParagraphSplitCommand(
        m_textBuffer.get(), m_cursorPosition));

    // Move cursor to start of new paragraph (redo already executed)
    m_cursorPosition.paragraph++;
    m_cursorPosition.offset = 0;

    ensureCursorVisible();
    update();
    emit contentChanged();
    emit paragraphInserted(m_cursorPosition.paragraph);
}

void BookEditor::deleteBackward()
{
    if (!m_textBuffer) {
        return;
    }

    if (hasSelection()) {
        deleteSelectedText();
        return;
    }

    // If at start of document, nothing to delete
    if (m_cursorPosition.paragraph == 0 && m_cursorPosition.offset == 0) {
        return;
    }

    CursorPosition cursorBefore = m_cursorPosition;

    if (m_cursorPosition.offset > 0) {
        // Delete character before cursor using TextDeleteCommand
        QString paraText = paragraphText(m_textBuffer.get(), m_cursorPosition.paragraph);
        QString deletedChar = paraText.mid(m_cursorPosition.offset - 1, 1);

        CursorPosition deleteStart = m_cursorPosition;
        deleteStart.offset = m_cursorPosition.offset - 1;

        // Push delete command (push() calls redo() automatically)
        m_undoStack->push(new TextDeleteCommand(
            m_textBuffer.get(), deleteStart, m_cursorPosition, deletedChar));

        // Update cursor (redo already executed)
        m_cursorPosition.offset--;
        ensureCursorVisible();
        update();
        emit contentChanged();
        emit paragraphModified(m_cursorPosition.paragraph);
    } else if (m_cursorPosition.paragraph > 0) {
        // Merge with previous paragraph using ParagraphMergeCommand
        int mergeFromIndex = m_cursorPosition.paragraph;
        QString mergedContent = paragraphText(m_textBuffer.get(), mergeFromIndex);
        int prevParaLen = paragraphLength(m_textBuffer.get(), m_cursorPosition.paragraph - 1);

        // Push merge command (push() calls redo() automatically)
        m_undoStack->push(new ParagraphMergeCommand(
            m_textBuffer.get(), cursorBefore, mergeFromIndex, mergedContent));

        // Update cursor (redo already executed)
        m_cursorPosition.paragraph--;
        m_cursorPosition.offset = prevParaLen;

        // Qt's QTextDocument handles layout invalidation automatically
        ensureCursorVisible();
        update();
        emit contentChanged();
        emit paragraphRemoved(mergeFromIndex);
    }
}

void BookEditor::deleteForward()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    if (hasSelection()) {
        deleteSelectedText();
        return;
    }

    QString paraText = paragraphText(m_textBuffer.get(), m_cursorPosition.paragraph);
    int paraLen = paraText.length();

    CursorPosition cursorBefore = m_cursorPosition;

    if (m_cursorPosition.offset < paraLen) {
        // Delete character at cursor using TextDeleteCommand
        QString deletedChar = paraText.mid(m_cursorPosition.offset, 1);

        CursorPosition deleteEnd = m_cursorPosition;
        deleteEnd.offset = m_cursorPosition.offset + 1;

        // Push delete command (push() calls redo() automatically)
        m_undoStack->push(new TextDeleteCommand(
            m_textBuffer.get(), m_cursorPosition, deleteEnd, deletedChar));

        // Cursor position stays the same after forward delete
        // Qt's QTextDocument handles layout invalidation automatically
        update();
        emit contentChanged();
        emit paragraphModified(m_cursorPosition.paragraph);
    } else if (static_cast<size_t>(m_cursorPosition.paragraph) <
               m_textBuffer->blockCount() - 1) {
        // Merge with next paragraph using ParagraphMergeCommand
        int mergeFromIndex = m_cursorPosition.paragraph + 1;
        QString mergedContent = paragraphText(m_textBuffer.get(), mergeFromIndex);

        // Push merge command (push() calls redo() automatically)
        m_undoStack->push(new ParagraphMergeCommand(
            m_textBuffer.get(), cursorBefore, mergeFromIndex, mergedContent));

        // Cursor position stays the same after merge
        // Qt's QTextDocument handles layout invalidation automatically
        update();
        emit contentChanged();
        emit paragraphRemoved(mergeFromIndex);
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
// Clipboard (Phase 4.13-4.16)
// =============================================================================

void BookEditor::copy()
{
    if (!hasSelection() || !m_textBuffer) {
        return;
    }

    SelectionRange sel = m_selection.normalized();

    // Build plain text from selection
    QString plainText;
    for (int p = sel.start.paragraph; p <= sel.end.paragraph; ++p) {
        QString paraText = paragraphText(m_textBuffer.get(), p);
        int startOffset = (p == sel.start.paragraph) ? sel.start.offset : 0;
        int endOffset = (p == sel.end.paragraph) ? sel.end.offset : paraText.length();

        if (p > sel.start.paragraph) {
            plainText += '\n';  // Paragraph separator
        }
        plainText += paraText.mid(startOffset, endOffset - startOffset);
    }

    // Set clipboard
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(plainText);
}

void BookEditor::cut()
{
    if (!hasSelection() || !m_textBuffer) {
        return;
    }

    // Copy first
    copy();

    // Then delete selection
    deleteSelectedText();
}

void BookEditor::paste()
{
    if (!m_textBuffer) {
        return;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    QString text = clipboard->text();

    if (text.isEmpty()) {
        return;
    }

    // Delete selection if any
    if (hasSelection()) {
        deleteSelectedText();
    }

    // Insert the pasted text (insertText handles newlines)
    insertText(text);
}

bool BookEditor::canPaste() const
{
    return ClipboardHandler::canPaste();
}

// =============================================================================
// Formatting (Phase 7.2)
// =============================================================================

void BookEditor::toggleBold()
{
    toggleFormat(ElementType::Bold);
}

void BookEditor::toggleItalic()
{
    toggleFormat(ElementType::Italic);
}

void BookEditor::toggleUnderline()
{
    toggleFormat(ElementType::Underline);
}

void BookEditor::toggleStrikethrough()
{
    toggleFormat(ElementType::Strikethrough);
}

bool BookEditor::isBold() const
{
    // If no selection, check pending state or cursor position
    if (!hasSelection()) {
        if (m_pendingBold) {
            return true;
        }
    }
    return hasFormat(ElementType::Bold);
}

bool BookEditor::isItalic() const
{
    if (!hasSelection()) {
        if (m_pendingItalic) {
            return true;
        }
    }
    return hasFormat(ElementType::Italic);
}

bool BookEditor::isUnderline() const
{
    if (!hasSelection()) {
        if (m_pendingUnderline) {
            return true;
        }
    }
    return hasFormat(ElementType::Underline);
}

bool BookEditor::isStrikethrough() const
{
    if (!hasSelection()) {
        if (m_pendingStrikethrough) {
            return true;
        }
    }
    return hasFormat(ElementType::Strikethrough);
}

// =============================================================================
// Paragraph Alignment
// =============================================================================

void BookEditor::setAlignLeft()
{
    if (!m_document) {
        return;
    }

    // Apply alignment to all paragraphs in selection (or just cursor paragraph if no selection)
    int startPara = m_cursorPosition.paragraph;
    int endPara = m_cursorPosition.paragraph;

    if (hasSelection()) {
        SelectionRange normRange = m_selection.normalized();
        startPara = normRange.start.paragraph;
        endPara = normRange.end.paragraph;
    }

    for (int i = startPara; i <= endPara; ++i) {
        KmlParagraph* para = m_document->paragraph(i);
        if (para) {
            para->setAlignment(Qt::AlignLeft);
            m_document->notifyParagraphModified(i);
        }
    }
    update();
}

void BookEditor::setAlignCenter()
{
    if (!m_document) {
        return;
    }

    // Apply alignment to all paragraphs in selection (or just cursor paragraph if no selection)
    int startPara = m_cursorPosition.paragraph;
    int endPara = m_cursorPosition.paragraph;

    if (hasSelection()) {
        SelectionRange normRange = m_selection.normalized();
        startPara = normRange.start.paragraph;
        endPara = normRange.end.paragraph;
    }

    for (int i = startPara; i <= endPara; ++i) {
        KmlParagraph* para = m_document->paragraph(i);
        if (para) {
            para->setAlignment(Qt::AlignHCenter);
            m_document->notifyParagraphModified(i);
        }
    }
    update();
}

void BookEditor::setAlignRight()
{
    if (!m_document) {
        return;
    }

    // Apply alignment to all paragraphs in selection (or just cursor paragraph if no selection)
    int startPara = m_cursorPosition.paragraph;
    int endPara = m_cursorPosition.paragraph;

    if (hasSelection()) {
        SelectionRange normRange = m_selection.normalized();
        startPara = normRange.start.paragraph;
        endPara = normRange.end.paragraph;
    }

    for (int i = startPara; i <= endPara; ++i) {
        KmlParagraph* para = m_document->paragraph(i);
        if (para) {
            para->setAlignment(Qt::AlignRight);
            m_document->notifyParagraphModified(i);
        }
    }
    update();
}

void BookEditor::setAlignJustify()
{
    if (!m_document) {
        return;
    }

    // Apply alignment to all paragraphs in selection (or just cursor paragraph if no selection)
    int startPara = m_cursorPosition.paragraph;
    int endPara = m_cursorPosition.paragraph;

    if (hasSelection()) {
        SelectionRange normRange = m_selection.normalized();
        startPara = normRange.start.paragraph;
        endPara = normRange.end.paragraph;
    }

    for (int i = startPara; i <= endPara; ++i) {
        KmlParagraph* para = m_document->paragraph(i);
        if (para) {
            para->setAlignment(Qt::AlignJustify);
            m_document->notifyParagraphModified(i);
        }
    }
    update();
}

Qt::Alignment BookEditor::currentAlignment() const
{
    if (!m_document) {
        return Qt::AlignLeft;
    }

    const KmlParagraph* para = m_document->paragraph(m_cursorPosition.paragraph);
    if (para) {
        return para->alignment();
    }
    return Qt::AlignLeft;
}

void BookEditor::toggleFormat(ElementType formatType)
{
    core::Logger::getInstance().debug("BookEditor::toggleFormat() called - "
        "type={}, hasSelection={}, cursor=({}, {})",
        elementTypeToString(formatType).toStdString(),
        hasSelection(), m_cursorPosition.paragraph, m_cursorPosition.offset);

    if (!m_document) {
        return;
    }

    if (hasSelection()) {
        // Apply/remove formatting to selection
        SelectionRange normRange = m_selection.normalized();

        // Check if selection already has this format
        bool alreadyHasFormat = true;
        for (int i = normRange.start.paragraph; i <= normRange.end.paragraph; ++i) {
            const KmlParagraph* para = m_document->paragraph(i);
            if (!para) {
                continue;
            }

            int start = (i == normRange.start.paragraph) ? normRange.start.offset : 0;
            int end = (i == normRange.end.paragraph) ? normRange.end.offset : para->characterCount();

            if (!para->hasFormatInRange(start, end, formatType)) {
                alreadyHasFormat = false;
                break;
            }
        }

        // Store old KML for undo
        QString oldKml;
        for (int i = normRange.start.paragraph; i <= normRange.end.paragraph; ++i) {
            const KmlParagraph* para = m_document->paragraph(i);
            if (para) {
                oldKml += para->toKml();
            }
        }

        // Create and execute the command
        auto* cmd = new ToggleFormatCommand(
            m_document,
            m_selection,
            formatType,
            !alreadyHasFormat,  // Apply if not already formatted, remove if formatted
            oldKml
        );
        m_undoStack->push(cmd);

        // Invalidate layouts for affected paragraphs
        for (int i = normRange.start.paragraph; i <= normRange.end.paragraph; ++i) {
            m_layoutManager->invalidateLayout(i);
        }

        // Trigger repaint
        update();

        core::Logger::getInstance().debug("BookEditor::toggleFormat() - formatting {} {}",
            alreadyHasFormat ? "removed" : "applied",
            elementTypeToString(formatType).toStdString());
    } else {
        // Toggle pending format for next typed characters
        switch (formatType) {
            case ElementType::Bold:
                m_pendingBold = !m_pendingBold;
                core::Logger::getInstance().debug("BookEditor::toggleFormat() - "
                    "pending bold={}", m_pendingBold);
                break;
            case ElementType::Italic:
                m_pendingItalic = !m_pendingItalic;
                core::Logger::getInstance().debug("BookEditor::toggleFormat() - "
                    "pending italic={}", m_pendingItalic);
                break;
            case ElementType::Underline:
                m_pendingUnderline = !m_pendingUnderline;
                core::Logger::getInstance().debug("BookEditor::toggleFormat() - "
                    "pending underline={}", m_pendingUnderline);
                break;
            case ElementType::Strikethrough:
                m_pendingStrikethrough = !m_pendingStrikethrough;
                core::Logger::getInstance().debug("BookEditor::toggleFormat() - "
                    "pending strikethrough={}", m_pendingStrikethrough);
                break;
            default:
                break;
        }
    }
}

bool BookEditor::hasFormat(ElementType formatType) const
{
    if (!m_document) {
        return false;
    }

    if (hasSelection()) {
        // Check if ALL text in selection has this format
        SelectionRange normRange = m_selection.normalized();

        for (int i = normRange.start.paragraph; i <= normRange.end.paragraph; ++i) {
            const KmlParagraph* para = m_document->paragraph(i);
            if (!para) {
                continue;
            }

            int start = (i == normRange.start.paragraph) ? normRange.start.offset : 0;
            int end = (i == normRange.end.paragraph) ? normRange.end.offset : para->characterCount();

            if (!para->hasFormatInRange(start, end, formatType)) {
                return false;
            }
        }
        return true;
    } else {
        // Check format at cursor position
        const KmlParagraph* para = m_document->paragraph(m_cursorPosition.paragraph);
        if (!para) {
            return false;
        }

        // If cursor is at end of paragraph, check previous character
        int checkOffset = m_cursorPosition.offset;
        if (checkOffset > 0) {
            checkOffset--;  // Check character before cursor
        }

        return para->hasFormatAt(checkOffset, formatType);
    }
}

// =============================================================================
// View Mode (Phase 5.1)
// =============================================================================

ViewMode BookEditor::viewMode() const
{
    return m_viewMode;
}

void BookEditor::setViewMode(ViewMode mode)
{
    auto& logger = core::Logger::getInstance();

    if (m_viewMode != mode) {
        ViewMode oldMode = m_viewMode;
        m_viewMode = mode;

        // Log view mode transition (OpenSpec #00042 Task 7.19 Issue #6)
        logger.info("BookEditor::setViewMode: {} -> {}",
                    static_cast<int>(oldMode), static_cast<int>(mode));

        // When entering Typewriter mode, update scroll to focus position
        if (mode == ViewMode::Typewriter) {
            logger.debug("BookEditor: Entering Typewriter mode, updating scroll");
            updateTypewriterScroll();
        }

        // When entering Distraction-Free mode, show UI initially
        if (mode == ViewMode::DistractionFree) {
            m_uiOpacity = 1.0;
            startUiFade();
            emit distractionFreeModeChanged(true);
        } else if (oldMode == ViewMode::DistractionFree) {
            // Leaving distraction-free mode
            if (m_uiFadeTimer != nullptr) {
                m_uiFadeTimer->stop();
            }
            emit distractionFreeModeChanged(false);
        }

        emit viewModeChanged(mode);
        update();
    }
}

// =============================================================================
// Page Navigation (Phase 5.3-5.5)
// =============================================================================

int BookEditor::currentPage() const
{
    if (m_document == nullptr || m_viewMode != ViewMode::Page) {
        return 0;
    }
    return m_pageLayoutManager->pageForPosition(m_cursorPosition);
}

int BookEditor::totalPages() const
{
    if (m_document == nullptr) {
        return 0;
    }
    return m_pageLayoutManager->totalPages();
}

void BookEditor::goToPage(int page)
{
    if (m_document == nullptr || m_viewMode != ViewMode::Page) {
        return;
    }

    int total = m_pageLayoutManager->totalPages();
    if (page < 1 || page > total) {
        return;
    }

    int oldPage = currentPage();

    // Scroll to the page
    qreal pageY = m_pageLayoutManager->pageY(page);
    setScrollOffset(pageY);

    // Emit signal if page changed
    if (page != oldPage) {
        emit currentPageChanged(page);
    }

    update();
}

void BookEditor::nextPage()
{
    int current = currentPage();
    int total = totalPages();
    if (current < total) {
        goToPage(current + 1);
    }
}

void BookEditor::previousPage()
{
    int current = currentPage();
    if (current > 1) {
        goToPage(current - 1);
    }
}

// =============================================================================
// Appearance (Phase 5.1)
// =============================================================================

const EditorAppearance& BookEditor::appearance() const
{
    return m_appearance;
}

void BookEditor::setAppearance(const EditorAppearance& appearance)
{
    m_appearance = appearance;

    // Update PageLayoutManager with new page layout settings
    m_pageLayoutManager->setPageLayout(m_appearance.pageLayout);

    // Update LayoutManager font when typography changes (OpenSpec #00042 Phase 7.2)
    if (m_layoutManager) {
        m_layoutManager->setFont(m_appearance.typography.textFont);
    }

    emit appearanceChanged();
    update();
}

// =============================================================================
// Event Handlers
// =============================================================================

void BookEditor::paintEvent(QPaintEvent* event)
{
    if (!m_renderEngine || !m_textBuffer) {
        // Fallback: just fill background
        QPainter painter(this);
        painter.fillRect(rect(), m_appearance.colors.editorBackground);
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Background
    painter.fillRect(rect(), m_appearance.colors.editorBackground);

    // Page Mode uses separate rendering path
    if (m_viewMode == ViewMode::Page) {
        paintPageMode(painter);
        event->accept();
        return;
    }

    // Configure render engine with current appearance settings
    m_renderEngine->setBackgroundColor(m_appearance.colors.editorBackground);
    m_renderEngine->setTextColor(m_appearance.colors.text);

    // Delegate painting to RenderEngine
    m_renderEngine->paint(&painter, event->rect(), size());

    // Additional overlays for view modes
    if (m_appearance.focusMode.enabled) {
        paintFocusOverlay(painter);
    }

    // Distraction-free mode overlay
    paintDistractionFreeOverlay(painter);

    event->accept();
}

void BookEditor::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Notify ViewportManager of size changes
    if (m_viewportManager) {
        m_viewportManager->setViewportSize(event->size());
    }

    // Update legacy components for compatibility
    updateLayoutWidth();
    updateViewport();
    updateScrollBarRange();

    // Position FindReplaceBar at top if visible
    if (m_findReplaceBar && m_findReplaceBar->isVisible()) {
        int scrollBarWidth = m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
        m_findReplaceBar->setGeometry(0, 0, width() - scrollBarWidth, m_findReplaceBar->sizeHint().height());
    }
}

void BookEditor::wheelEvent(QWheelEvent* event)
{
    if (!m_viewportManager) {
        QWidget::wheelEvent(event);
        return;
    }

    QPoint angleDelta = event->angleDelta();
    if (!angleDelta.isNull()) {
        // Standard wheel scroll: 1 step = 15 degrees, 8 degrees per line
        qreal delta = -angleDelta.y() / 8.0 / 15.0 * 40.0;  // 40 pixels per step
        double newPos = m_viewportManager->scrollPosition() + delta;
        m_viewportManager->setScrollPosition(newPos);
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
            // In Page Mode, navigate between pages
            if (m_viewMode == ViewMode::Page) {
                previousPage();
            } else {
                moveCursorPageUp();
                if (!shift) {
                    clearSelection();
                }
            }
            handled = true;
            break;

        case Qt::Key_PageDown:
            // In Page Mode, navigate between pages
            if (m_viewMode == ViewMode::Page) {
                nextPage();
            } else {
                moveCursorPageDown();
                if (!shift) {
                    clearSelection();
                }
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

        case Qt::Key_C:
            if (ctrl) {
                copy();
                handled = true;
            }
            break;

        case Qt::Key_X:
            if (ctrl) {
                cut();
                handled = true;
            }
            break;

        case Qt::Key_V:
            if (ctrl) {
                paste();
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
    // Note: On Windows, AltGr sends Ctrl+Alt, so we must allow text when both are pressed
    // Only block Ctrl-only combinations (real shortcuts like Ctrl+C)
    bool alt = event->modifiers() & Qt::AltModifier;
    bool ctrlOnly = ctrl && !alt;
    if (!handled && !ctrlOnly && !event->text().isEmpty()) {
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

    // Only repaint the cursor area instead of the entire widget
    // This significantly reduces CPU usage during cursor blink
    if (m_document && m_document->paragraphCount() > 0) {
        QRectF cursorRect = calculateCursorRect();
        if (!cursorRect.isEmpty()) {
            // Add small margin around cursor for antialiasing artifacts
            update(cursorRect.toRect().adjusted(-2, -2, 2, 2));
            return;
        }
    }

    // Fallback to full update if cursor rect unavailable
    update();
}

// =============================================================================
// Private Methods
// =============================================================================

void BookEditor::setupComponents()
{
    // Connect layout manager to scroll manager
    m_layoutManager->setScrollManager(m_scrollManager.get());

    // Set font from appearance (OpenSpec #00042 Phase 7.2)
    m_layoutManager->setFont(m_appearance.typography.textFont);

    // Enable focus for keyboard input
    setFocusPolicy(Qt::StrongFocus);

    // Setup scrollbar
    setupScrollBar();

    // Setup cursor blink timer
    setupCursorBlinkTimer();

    // Initialize RenderEngine cursor (Phase 8 fix: sync cursor with new architecture)
    if (m_renderEngine) {
        m_renderEngine->setCursorPosition(m_cursorPosition);
        m_renderEngine->setCursorVisible(true);
        m_renderEngine->setCursorBlinkInterval(m_cursorBlinkInterval);
        m_renderEngine->setCursorWidth(CURSOR_WIDTH);
        if (m_cursorBlinkingEnabled) {
            m_renderEngine->startCursorBlink();
        }
    }

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

    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return {0, 0};
    }

    int maxParagraph = m_textBuffer->blockCount() - 1;
    result.paragraph = qBound(0, result.paragraph, maxParagraph);

    int maxOffset = paragraphLength(m_textBuffer.get(), result.paragraph);
    result.offset = qBound(0, result.offset, maxOffset);
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

            if (m_renderEngine && hasSelection()) {
                m_renderEngine->setSelection(m_selection);
                update();
            }
        } else {
            // Normal click clears selection and positions cursor
            clearSelection();
            m_selectionAnchor = clickPosition;
            setCursorPosition(clickPosition);

            if (m_renderEngine) {
                m_renderEngine->clearSelection();
            }
        }

        // Start drag selection
        m_isDragging = true;
    }

    event->accept();
}

void BookEditor::mouseMoveEvent(QMouseEvent* event)
{
    // In Distraction-Free mode, show UI on mouse movement
    if (m_viewMode == ViewMode::DistractionFree) {
        // Check if mouse is near edges for fade trigger
        QPointF pos = event->position();
        qreal edgeThreshold = 50.0;  // Pixels from edge
        bool nearEdge = (pos.y() < edgeThreshold ||
                         pos.y() > height() - edgeThreshold ||
                         pos.x() < edgeThreshold ||
                         pos.x() > width() - edgeThreshold);

        if (nearEdge && m_appearance.distractionFree.fadeOnMouseMove) {
            m_uiOpacity = 1.0;
            startUiFade();
            update();
        }
    }

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

    if (m_renderEngine) {
        m_renderEngine->setSelection(newSelection);
        update();  // Trigger repaint for selection rendering
    }

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
    // Phase 11.6: Position calculation using QTextDocument layout
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return {0, 0};
    }

    // Convert widget Y to document Y (accounting for scroll and margins)
    double docY = m_viewportManager->scrollPosition() + widgetPos.y() - TOP_MARGIN;
    if (docY < 0) {
        docY = 0;
    }

    // Find paragraph at Y using QTextDocument layout hit test
    int paraIndex = getParagraphAtY(m_textBuffer.get(), docY);
    if (paraIndex >= m_textBuffer->blockCount()) {
        paraIndex = m_textBuffer->blockCount() - 1;
    }

    // Phase 11.6: Get layout directly from QTextDocument block
    QTextDocument* doc = m_textBuffer.get();
    if (!doc) {
        return {paraIndex, 0};
    }
    QTextBlock block = doc->findBlockByNumber(paraIndex);
    if (!block.isValid()) {
        return {paraIndex, 0};
    }
    QTextLayout* layout = block.layout();
    if (!layout) {
        return {paraIndex, 0};
    }

    // Convert to paragraph-relative coordinates
    double paraY = getParagraphY(m_textBuffer.get(), paraIndex);
    double localY = docY - paraY;
    double localX = widgetPos.x() - LEFT_MARGIN;
    if (localX < 0) {
        localX = 0;
    }

    // Hit test within layout to find character offset
    int offset = 0;
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        if (localY >= line.y() && localY < line.y() + line.height()) {
            offset = line.xToCursor(localX, QTextLine::CursorBetweenCharacters);
            break;
        }
        // If below all lines, use last line
        if (i == layout->lineCount() - 1) {
            offset = line.xToCursor(localX, QTextLine::CursorBetweenCharacters);
        }
    }

    return {static_cast<int>(paraIndex), offset};
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

    // Phase 8.14: Update render engine selection
    if (m_renderEngine) {
        m_renderEngine->setSelection(range);
    }
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

// =============================================================================
// Typewriter Mode (Phase 5.2)
// =============================================================================

qreal BookEditor::getCursorDocumentY() const
{
    if (m_document == nullptr) {
        return 0.0;
    }

    // Get paragraph Y position
    qreal paraY = m_layoutManager->paragraphY(m_cursorPosition.paragraph);

    // Get layout for the paragraph to find line offset within paragraph
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(m_cursorPosition.paragraph);
    if (layout != nullptr) {
        QRectF cursorRect = layout->cursorRect(m_cursorPosition.offset);
        paraY += cursorRect.y();
    }

    return paraY;
}

void BookEditor::updateTypewriterScroll()
{
    auto& logger = core::Logger::getInstance();

    if (m_viewMode != ViewMode::Typewriter) {
        return;
    }

    if (m_document == nullptr) {
        logger.debug("BookEditor::updateTypewriterScroll: No document");
        return;
    }

    // Get cursor Y position in document coordinates
    qreal cursorY = getCursorDocumentY();

    // Calculate target scroll position to keep cursor at focus position
    // Use widget height as viewport height (BookEditor is a QWidget, not QAbstractScrollArea)
    qreal viewportHeight = static_cast<qreal>(height());
    qreal focusY = viewportHeight * m_appearance.typewriter.focusPosition;
    qreal targetScrollY = cursorY - focusY;

    // Clamp to valid range
    qreal maxScroll = m_scrollManager->maxScrollOffset();
    int targetValue = qBound(0, static_cast<int>(targetScrollY), static_cast<int>(maxScroll));

    // Log typewriter scroll parameters (OpenSpec #00042 Task 7.19 Issue #6)
    logger.debug("BookEditor::updateTypewriterScroll: cursorY={:.1f}, viewportH={:.1f}, focusPos={:.2f}, targetScroll={}",
                 cursorY, viewportHeight, m_appearance.typewriter.focusPosition, targetValue);

    QScrollBar* vbar = verticalScrollBar();
    if (vbar == nullptr) {
        // Fallback if no scrollbar - use setScrollOffset directly
        setScrollOffset(static_cast<qreal>(targetValue));
        return;
    }

    if (m_appearance.typewriter.smoothScroll) {
        // Use smooth scrolling animation
        if (m_typewriterScrollAnimation == nullptr) {
            m_typewriterScrollAnimation = new QPropertyAnimation(vbar, "value", this);
            m_typewriterScrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
        }

        // Stop any running animation
        m_typewriterScrollAnimation->stop();

        // Only animate if there's a significant difference
        int currentValue = vbar->value();
        if (qAbs(currentValue - targetValue) > 1) {
            m_typewriterScrollAnimation->setDuration(m_appearance.typewriter.scrollDuration);
            m_typewriterScrollAnimation->setStartValue(currentValue);
            m_typewriterScrollAnimation->setEndValue(targetValue);
            m_typewriterScrollAnimation->start();
        }
    } else {
        // Immediate scroll
        vbar->setValue(targetValue);
    }
}

// =============================================================================
// Page Mode Rendering
// =============================================================================

void BookEditor::paintPageMode(QPainter& painter)
{
    // Phase 11.6: Page Mode rendering using QTextDocument directly

    if (!m_textBuffer || !m_viewportManager) {
        return;
    }
    QTextDocument* doc = m_textBuffer.get();
    if (!doc) {
        return;
    }

    auto& logger = core::Logger::getInstance();

    // Get page layout settings
    const PageLayout& pageLayout = m_appearance.pageLayout;

    // Calculate page dimensions in pixels (96 DPI assumed)
    constexpr qreal dpi = 96.0;
    QSizeF pagePixels = pageLayout.pageSizePixels(dpi);
    qreal pageWidth = pagePixels.width();
    qreal pageHeight = pagePixels.height();

    // Calculate margins in pixels
    constexpr qreal mmToInch = 1.0 / 25.4;
    QMarginsF marginsPixels(
        pageLayout.margins.left() * mmToInch * dpi * pageLayout.zoomLevel,
        pageLayout.margins.top() * mmToInch * dpi * pageLayout.zoomLevel,
        pageLayout.margins.right() * mmToInch * dpi * pageLayout.zoomLevel,
        pageLayout.margins.bottom() * mmToInch * dpi * pageLayout.zoomLevel
    );

    qreal textAreaWidth = pageWidth - marginsPixels.left() - marginsPixels.right();
    qreal textAreaHeight = pageHeight - marginsPixels.top() - marginsPixels.bottom();

    if (textAreaWidth < 1.0) textAreaWidth = 1.0;
    if (textAreaHeight < 1.0) textAreaHeight = 1.0;

    // Calculate page centering
    qreal viewportWidth = static_cast<qreal>(width());
    qreal centerOffset = (viewportWidth > pageWidth)
        ? (viewportWidth - pageWidth) / 2.0
        : 0.0;

    // Get scroll offset
    double scrollY = m_viewportManager->scrollPosition();

    // Calculate page breaks and build page list
    // This is a simplified pagination that doesn't use widow/orphan control
    struct PageContent {
        qreal pageY;
        QRectF pageRect;
        QRectF textRect;
        std::vector<std::pair<size_t, qreal>> paragraphs; // (paraIndex, yOffset)
    };
    std::vector<PageContent> pages;

    size_t paraCount = m_textBuffer->blockCount();
    if (paraCount == 0) {
        // Draw at least one empty page
        PageContent page;
        page.pageY = 0.0;
        page.pageRect = QRectF(centerOffset, 0.0, pageWidth, pageHeight);
        page.textRect = QRectF(centerOffset + marginsPixels.left(),
                               marginsPixels.top(), textAreaWidth, textAreaHeight);
        pages.push_back(page);
    } else {
        qreal currentPageY = 0.0;
        qreal currentY = 0.0;  // Y within current page's text area

        PageContent currentPage;
        currentPage.pageY = currentPageY;
        currentPage.pageRect = QRectF(centerOffset, currentPageY, pageWidth, pageHeight);
        currentPage.textRect = QRectF(centerOffset + marginsPixels.left(),
                                      currentPageY + marginsPixels.top(),
                                      textAreaWidth, textAreaHeight);

        for (size_t paraIndex = 0; paraIndex < paraCount; ++paraIndex) {
            // Phase 11.6: Get paragraph height from QTextBlock layout
            QTextBlock block = doc->findBlockByNumber(static_cast<int>(paraIndex));
            qreal paraHeight = 0.0;
            if (block.isValid() && block.layout()) {
                paraHeight = block.layout()->boundingRect().height();
            }

            // Check if paragraph fits on current page
            if (currentY + paraHeight > textAreaHeight && currentY > 0) {
                // Paragraph doesn't fit, start new page
                pages.push_back(currentPage);

                currentPageY += pageHeight + pageLayout.pageGap;
                currentY = 0.0;

                currentPage = PageContent();
                currentPage.pageY = currentPageY;
                currentPage.pageRect = QRectF(centerOffset, currentPageY, pageWidth, pageHeight);
                currentPage.textRect = QRectF(centerOffset + marginsPixels.left(),
                                              currentPageY + marginsPixels.top(),
                                              textAreaWidth, textAreaHeight);
            }

            // Add paragraph to current page
            currentPage.paragraphs.emplace_back(paraIndex, currentY);
            currentY += paraHeight;
        }

        // Add final page
        if (!currentPage.paragraphs.empty() || pages.empty()) {
            pages.push_back(currentPage);
        }
    }

    logger.debug("BookEditor::paintPageMode: {} pages, viewport width={}",
                 pages.size(), width());

    // Page styling
    QColor borderColor = m_appearance.colors.text;
    borderColor.setAlpha(30);
    constexpr qreal shadowOffsetX = 4.0;
    constexpr qreal shadowOffsetY = 4.0;
    constexpr qreal shadowBlur = 8.0;

    // Render visible pages
    for (size_t pageIdx = 0; pageIdx < pages.size(); ++pageIdx) {
        const PageContent& page = pages[pageIdx];

        // Convert to widget coordinates
        QRectF pageRect = page.pageRect;
        qreal widgetY = pageRect.top() - scrollY;
        pageRect.moveTop(widgetY);

        // Skip if page is not visible
        if (pageRect.bottom() < 0 || pageRect.top() > height()) {
            continue;
        }

        // Draw page shadow
        if (m_appearance.elements.showPageShadows) {
            QRectF shadowRect = pageRect.translated(shadowOffsetX, shadowOffsetY);

            for (int i = 0; i < 4; ++i) {
                QColor shadow = m_appearance.colors.pageShadow;
                shadow.setAlpha(shadow.alpha() / (i + 1));
                qreal expand = shadowBlur * (i + 1) / 4.0;
                QRectF blurRect = shadowRect.adjusted(-expand, -expand, expand, expand);
                painter.fillRect(blurRect, shadow);
            }
        }

        // Draw page background
        painter.fillRect(pageRect, m_appearance.colors.pageBackground);

        // Draw page border
        if (m_appearance.elements.showPageBorders) {
            QPen borderPen(borderColor);
            borderPen.setWidthF(1.0);
            painter.setPen(borderPen);
            painter.drawRect(pageRect);
        }

        // Draw content
        QRectF textRect = page.textRect;
        textRect.moveTop(textRect.top() - scrollY);

        // Set clip to text area
        painter.save();
        painter.setClipRect(textRect);

        for (const auto& [paraIndex, yOffset] : page.paragraphs) {
            // Phase 11.6: Get layout from QTextBlock
            QTextBlock block = doc->findBlockByNumber(static_cast<int>(paraIndex));
            if (block.isValid() && block.layout()) {
                QPointF drawPos(textRect.left(), textRect.top() + yOffset);
                block.layout()->draw(&painter, drawPos);
            }
        }

        painter.restore();
    }

    // Draw selection highlighting
    // TODO: Implement page-aware selection drawing for new arch
    // drawSelectionNewArch(&painter);

    // Draw cursor
    if (m_cursorVisible && hasFocus()) {
        // For new arch, cursor drawing needs page-aware coordinates
        // TODO: Implement page-aware cursor drawing
        // drawCursorNewArch(&painter);
    }

    // Focus mode overlay
    if (m_appearance.focusMode.enabled) {
        paintFocusOverlay(painter);
    }

    // Distraction-free mode overlay
    paintDistractionFreeOverlay(painter);
}

// =============================================================================
// Focus Mode (Phase 5.6)
// =============================================================================

BookEditor::FocusedRange BookEditor::getFocusedRange() const
{
    FocusedRange range;

    // Phase 11.6: Use QTextDocument directly instead of LazyLayoutManager
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return range;
    }
    QTextDocument* doc = m_textBuffer.get();
    if (!doc) {
        return range;
    }

    int paraIndex = m_cursorPosition.paragraph;
    size_t paraCount = m_textBuffer->blockCount();

    if (paraIndex < 0 || paraIndex >= static_cast<int>(paraCount)) {
        paraIndex = 0;
    }

    // Default to paragraph scope
    range.startParagraph = paraIndex;
    range.endParagraph = paraIndex;
    range.startLine = 0;
    range.endLine = -1;  // Will be set below

    // Get layout from QTextBlock
    QTextBlock block = doc->findBlockByNumber(paraIndex);
    QTextLayout* layout = block.isValid() ? block.layout() : nullptr;

    // Determine range based on focus scope
    switch (m_appearance.focusMode.scope) {
        case FocusModeSettings::FocusScope::Line: {
            // Focus on the specific line containing the cursor
            if (layout && layout->lineCount() > 0) {
                int lineIndex = 0;
                if (m_cursorPosition.offset > 0) {
                    QTextLine line = layout->lineForTextPosition(m_cursorPosition.offset);
                    if (line.isValid()) {
                        lineIndex = line.lineNumber();
                    }
                }
                range.startLine = lineIndex;
                range.endLine = lineIndex;
            } else {
                range.startLine = 0;
                range.endLine = 0;
            }
            break;
        }

        case FocusModeSettings::FocusScope::Sentence:
            // Sentence detection is complex - treat as paragraph for now
            [[fallthrough]];

        case FocusModeSettings::FocusScope::Paragraph:
        default:
            // Focus on the entire paragraph
            if (layout && layout->lineCount() > 0) {
                range.endLine = layout->lineCount() - 1;
            } else {
                range.endLine = 0;
            }
            break;
    }

    return range;
}

void BookEditor::paintFocusOverlay(QPainter& painter)
{
    // Only draw overlay in Focus view mode
    if (m_viewMode != ViewMode::Focus) {
        return;
    }

    // Phase 11.6: Use QTextDocument directly
    if (!m_textBuffer || !m_viewportManager) {
        return;
    }
    QTextDocument* doc = m_textBuffer.get();
    if (!doc || m_textBuffer->blockCount() == 0) {
        return;
    }

    FocusedRange focusedRange = getFocusedRange();

    // Get viewport info
    int viewportHeight = height();
    double scrollY = m_viewportManager->scrollPosition();

    // Calculate Y position of focused paragraph using QTextBlock layouts
    double focusY = 0.0;
    for (int i = 0; i < focusedRange.startParagraph && i < static_cast<int>(m_textBuffer->blockCount()); ++i) {
        QTextBlock block = doc->findBlockByNumber(i);
        if (block.isValid() && block.layout()) {
            focusY += block.layout()->boundingRect().height();
        }
    }

    // Get focused paragraph height (or line height if Line scope)
    double focusHeight = 0.0;
    double focusTop = focusY;

    if (focusedRange.startParagraph >= 0 &&
        focusedRange.startParagraph < static_cast<int>(m_textBuffer->blockCount())) {

        QTextBlock block = doc->findBlockByNumber(focusedRange.startParagraph);
        QTextLayout* layout = (block.isValid()) ? block.layout() : nullptr;

        if (m_appearance.focusMode.scope == FocusModeSettings::FocusScope::Line) {
            // For line scope, calculate specific line bounds
            if (layout && focusedRange.startLine >= 0 && focusedRange.startLine < layout->lineCount()) {
                QTextLine line = layout->lineAt(focusedRange.startLine);
                if (line.isValid()) {
                    focusTop = focusY + line.y();
                    focusHeight = line.height();
                }
            }
        } else {
            // For paragraph scope, use entire paragraph
            if (layout) {
                focusHeight = layout->boundingRect().height();
            }
        }
    }

    // Calculate screen positions (apply TOP_MARGIN and scroll offset)
    int widgetFocusTop = static_cast<int>(TOP_MARGIN + focusTop - scrollY);
    int widgetFocusBottom = static_cast<int>(TOP_MARGIN + focusTop + focusHeight - scrollY);

    // Calculate overlay opacity (inverted: high dimOpacity = more dimming)
    int overlayAlpha = static_cast<int>((1.0 - m_appearance.focusMode.dimOpacity) * 255.0);
    overlayAlpha = qBound(0, overlayAlpha, 255);

    // Create overlay color based on editor background
    QColor overlayColor = m_appearance.colors.editorBackground;
    overlayColor.setAlpha(overlayAlpha);

    // Draw overlay above focused area
    if (widgetFocusTop > 0) {
        QRectF topOverlay(0, 0, width(), widgetFocusTop);
        painter.fillRect(topOverlay, overlayColor);
    }

    // Draw overlay below focused area
    if (widgetFocusBottom < viewportHeight) {
        QRectF bottomOverlay(0, widgetFocusBottom, width(), viewportHeight - widgetFocusBottom);
        painter.fillRect(bottomOverlay, overlayColor);
    }

    // Draw highlight behind focused area (optional)
    if (m_appearance.focusMode.highlightBackground) {
        QColor highlightColor = m_appearance.colors.accent;
        highlightColor.setAlpha(25);  // Very subtle

        QRectF focusRect(LEFT_MARGIN, widgetFocusTop,
                         width() - LEFT_MARGIN, widgetFocusBottom - widgetFocusTop);
        painter.fillRect(focusRect, highlightColor);
    }
}

// =============================================================================
// Distraction-Free Mode (Phase 5.7)
// =============================================================================

int BookEditor::getWordCount() const
{
    int count = 0;
    static const QRegularExpression wordSplitter(QStringLiteral("\\s+"));

    // Prefer new architecture (QTextDocument)
    if (m_textBuffer && m_textBuffer->blockCount() > 0) {
        int paraCount = m_textBuffer->blockCount();
        for (int i = 0; i < paraCount; ++i) {
            QString text = paragraphText(m_textBuffer.get(), i);
            if (!text.isEmpty()) {
                count += text.split(wordSplitter, Qt::SkipEmptyParts).size();
            }
        }
        return count;
    }

    // Fallback to legacy KmlDocument (for backwards compatibility during tests)
    if (m_document && m_document->paragraphCount() > 0) {
        int docParaCount = static_cast<int>(m_document->paragraphCount());
        for (int i = 0; i < docParaCount; ++i) {
            const KmlParagraph* para = m_document->paragraph(i);
            if (para) {
                QString text = para->plainText();
                if (!text.isEmpty()) {
                    count += static_cast<int>(text.split(wordSplitter, Qt::SkipEmptyParts).size());
                }
            }
        }
    }

    return count;
}

void BookEditor::startUiFade()
{
    if (m_uiFadeTimer == nullptr) {
        return;
    }

    // Stop any existing timer
    m_uiFadeTimer->stop();

    // Start fade timer with configured timeout
    int timeout = m_appearance.distractionFree.uiFadeTimeout;
    if (timeout > 0) {
        m_uiFadeTimer->start(timeout);
    }
}

void BookEditor::paintDistractionFreeOverlay(QPainter& painter)
{
    // Only draw overlay in Distraction-Free view mode
    if (m_viewMode != ViewMode::DistractionFree) {
        return;
    }

    // Calculate content area based on textWidth setting
    // (This is preparation for Phase 7 - actual text centering will be done there)
    qreal viewportWidth = static_cast<qreal>(width());
    qreal textWidth = viewportWidth * m_appearance.distractionFree.textWidth;
    qreal sideMargin = (viewportWidth - textWidth) / 2.0;

    // Draw subtle gradient/vignette on sides (optional visual touch)
    if (sideMargin > 0) {
        // Create a subtle vignette effect on the sides
        QColor vignetteColor = m_appearance.colors.editorBackground;
        vignetteColor.setAlpha(30);  // Very subtle

        // Left vignette
        QLinearGradient leftGradient(0, 0, sideMargin, 0);
        leftGradient.setColorAt(0.0, vignetteColor);
        leftGradient.setColorAt(1.0, Qt::transparent);
        painter.fillRect(QRectF(0, 0, sideMargin, height()), leftGradient);

        // Right vignette
        QLinearGradient rightGradient(width() - sideMargin, 0, width(), 0);
        rightGradient.setColorAt(0.0, Qt::transparent);
        rightGradient.setColorAt(1.0, vignetteColor);
        painter.fillRect(QRectF(width() - sideMargin, 0, sideMargin, height()), rightGradient);
    }

    // Only draw overlays if UI is visible (opacity > 0)
    if (m_uiOpacity <= 0.0) {
        return;
    }

    // Set up text color with opacity
    QColor textColor = m_appearance.colors.textSecondary;
    textColor.setAlphaF(m_uiOpacity);

    // Draw word count at bottom center
    if (m_appearance.distractionFree.showWordCount) {
        int wordCount = getWordCount();
        QString countText = tr("%1 words").arg(wordCount);

        // Use UI font, slightly smaller
        QFont countFont = m_appearance.typography.uiFont;
        countFont.setPointSize(10);
        painter.setFont(countFont);
        painter.setPen(textColor);

        // Calculate position - bottom center with some padding
        QFontMetrics countFm(countFont);
        int countTextWidth = countFm.horizontalAdvance(countText);
        int countTextHeight = countFm.height();
        int countPadding = 20;

        QRectF countRect(
            (width() - countTextWidth) / 2.0,
            height() - countTextHeight - countPadding,
            countTextWidth,
            countTextHeight
        );

        painter.drawText(countRect, Qt::AlignCenter, countText);
    }

    // Draw clock at top right
    if (m_appearance.distractionFree.showClock) {
        QString timeText = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));

        // Use UI font
        QFont clockFont = m_appearance.typography.uiFont;
        clockFont.setPointSize(10);
        painter.setFont(clockFont);
        painter.setPen(textColor);

        // Calculate position - top right with some padding
        QFontMetrics clockFm(clockFont);
        int clockTextWidth = clockFm.horizontalAdvance(timeText);
        int clockTextHeight = clockFm.height();
        int clockPadding = 20;

        QRectF clockRect(
            width() - clockTextWidth - clockPadding,
            clockPadding,
            clockTextWidth,
            clockTextHeight
        );

        painter.drawText(clockRect, Qt::AlignCenter, timeText);
    }
}

// =============================================================================
// Comments (Phase 7.9)
// =============================================================================

void BookEditor::insertComment()
{
    if (m_document == nullptr) {
        return;
    }

    // Must have a selection to add a comment
    if (!hasSelection()) {
        core::Logger::getInstance().debug("BookEditor::insertComment() - no selection, cannot add comment");
        return;
    }

    // Get the normalized selection range
    SelectionRange sel = m_selection.normalized();

    // Comments within a single paragraph are simpler
    if (sel.start.paragraph != sel.end.paragraph) {
        core::Logger::getInstance().debug("BookEditor::insertComment() - multi-paragraph selection not supported");
        return;
    }

    // Show input dialog for comment text
    bool ok = false;
    QString commentText = QInputDialog::getMultiLineText(
        this,
        tr("Insert Comment"),
        tr("Enter comment:"),
        QString(),
        &ok
    );

    if (!ok || commentText.isEmpty()) {
        return;
    }

    // Get the paragraph
    KmlParagraph* para = m_document->paragraph(sel.start.paragraph);
    if (para == nullptr) {
        return;
    }

    // Create and add the comment
    KmlComment comment(sel.start.offset, sel.end.offset, commentText);
    para->addComment(comment);

    core::Logger::getInstance().info("BookEditor: Added comment '{}' to paragraph {} at ({}, {})",
        comment.id().toStdString(), sel.start.paragraph, sel.start.offset, sel.end.offset);

    // Phase 11.6: Removed MetadataLayer - comments stored in KmlParagraph (KmlComment)
    // Comment data is serialized to KML via KmlParagraph::comments()

    // Emit signal
    emit commentAdded(sel.start.paragraph);

    // Repaint to show comment highlight
    update();
}

void BookEditor::deleteComment(const QString& commentId)
{
    if (m_document == nullptr || commentId.isEmpty()) {
        return;
    }

    // Search all paragraphs for the comment
    for (int i = 0; i < m_document->paragraphCount(); ++i) {
        KmlParagraph* para = m_document->paragraph(i);
        if (para != nullptr && para->removeComment(commentId)) {
            core::Logger::getInstance().info("BookEditor: Deleted comment '{}' from paragraph {}",
                commentId.toStdString(), i);

            // Phase 11.6: Removed MetadataLayer - comments removed from KmlParagraph only

            // Emit signal
            emit commentRemoved(i, commentId);

            // Repaint
            update();
            return;
        }
    }

    core::Logger::getInstance().debug("BookEditor::deleteComment() - comment '{}' not found",
        commentId.toStdString());
}

void BookEditor::editComment(const QString& commentId)
{
    if (m_document == nullptr || commentId.isEmpty()) {
        return;
    }

    // Find the comment
    for (int i = 0; i < m_document->paragraphCount(); ++i) {
        KmlParagraph* para = m_document->paragraph(i);
        if (para == nullptr) {
            continue;
        }

        KmlComment* comment = para->commentById(commentId);
        if (comment != nullptr) {
            // Show input dialog with current text
            bool ok = false;
            QString newText = QInputDialog::getMultiLineText(
                this,
                tr("Edit Comment"),
                tr("Edit comment:"),
                comment->text(),
                &ok
            );

            if (ok && !newText.isEmpty()) {
                comment->setText(newText);
                core::Logger::getInstance().info("BookEditor: Edited comment '{}'",
                    commentId.toStdString());
                update();
            }
            return;
        }
    }

    core::Logger::getInstance().debug("BookEditor::editComment() - comment '{}' not found",
        commentId.toStdString());
}

QList<KmlComment> BookEditor::commentsInCurrentParagraph() const
{
    if (m_document == nullptr) {
        return {};
    }

    const KmlParagraph* para = m_document->paragraph(m_cursorPosition.paragraph);
    if (para == nullptr) {
        return {};
    }

    return para->comments();
}

void BookEditor::navigateToComment(int paragraphIndex, const QString& commentId)
{
    if (m_document == nullptr) {
        return;
    }

    // Validate paragraph index
    if (paragraphIndex < 0 || paragraphIndex >= m_document->paragraphCount()) {
        return;
    }

    const KmlParagraph* para = m_document->paragraph(paragraphIndex);
    if (para == nullptr) {
        return;
    }

    // Find the comment
    const KmlComment* comment = para->commentById(commentId);
    if (comment == nullptr) {
        return;
    }

    // Move cursor to start of commented text
    CursorPosition newPos{paragraphIndex, comment->startPos()};
    setCursorPosition(newPos);

    // Ensure cursor is visible
    ensureCursorVisible();

    // Emit signal
    emit commentSelected(paragraphIndex, commentId);
}

// =============================================================================
// Spell Check Integration (Phase 6.9)
// =============================================================================

void BookEditor::setSpellCheckService(SpellCheckService* service)
{
    // Disconnect from previous service
    if (m_spellCheckService) {
        disconnect(m_spellCheckService, nullptr, this, nullptr);
        m_spellCheckService->setBookEditor(nullptr);
    }

    m_spellCheckService = service;

    // Connect to new service
    if (m_spellCheckService) {
        connect(m_spellCheckService, &SpellCheckService::paragraphChecked,
                this, &BookEditor::onSpellCheckParagraph);

        // Connect service to this BookEditor for paragraph signals
        m_spellCheckService->setBookEditor(this);

        core::Logger::getInstance().debug("BookEditor: Spell check service connected");
    }
}

SpellCheckService* BookEditor::spellCheckService() const
{
    return m_spellCheckService;
}

void BookEditor::requestSpellCheck()
{
    if (m_spellCheckService && m_document) {
        m_spellCheckService->checkDocumentAsync();
    }
}

void BookEditor::onSpellCheckParagraph(int paragraphIndex, const QList<SpellErrorInfo>& errors)
{
    if (!m_layoutManager) {
        return;
    }

    // Get the paragraph layout
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(paragraphIndex);
    if (!layout) {
        return;
    }

    // Convert SpellErrorInfo to SpellError and set on layout
    layout->clearSpellErrors();
    for (const SpellErrorInfo& error : errors) {
        layout->addSpellError(error.startPos, error.length);
    }

    // Trigger repaint for the affected paragraph
    update();
}

void BookEditor::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_document) {
        QWidget::contextMenuEvent(event);
        return;
    }

    // Convert mouse position to document position
    CursorPosition pos = positionFromPoint(event->pos());
    if (pos.paragraph < 0) {
        QWidget::contextMenuEvent(event);
        return;
    }

    // Check if position is in a misspelled word (spell check takes priority)
    auto [word, startOffset, endOffset] = getMisspelledWordAt(pos.paragraph, pos.offset);

    if (!word.isEmpty()) {
        // Create spell check context menu
        QMenu* menu = createSpellCheckContextMenu(word, pos.paragraph, startOffset, endOffset);
        menu->exec(event->globalPos());
        delete menu;
        return;
    }

    // Check if position is in a grammar error (Phase 6.17)
    auto grammarError = getGrammarErrorAt(pos.paragraph, pos.offset);
    if (grammarError.has_value()) {
        // Create grammar check context menu
        QMenu* menu = createGrammarContextMenu(*grammarError, pos.paragraph);
        menu->exec(event->globalPos());
        delete menu;
        return;
    }

    // Default context menu
    QMenu menu(this);

    if (hasSelection()) {
        menu.addAction(tr("Cut"), this, &BookEditor::cut);
        menu.addAction(tr("Copy"), this, &BookEditor::copy);
    }
    menu.addAction(tr("Paste"), this, &BookEditor::paste);

    if (hasSelection()) {
        menu.addSeparator();
        menu.addAction(tr("Select All"), this, &BookEditor::selectAll);
    }

    menu.exec(event->globalPos());
}

std::tuple<QString, int, int> BookEditor::getMisspelledWordAt(int paraIndex, int offset) const
{
    if (!m_layoutManager) {
        return {QString(), 0, 0};
    }

    ParagraphLayout* layout = m_layoutManager->paragraphLayout(paraIndex);
    if (!layout) {
        return {QString(), 0, 0};
    }

    // Check if offset is within any spell error
    const std::vector<SpellError>& errors = layout->spellErrors();
    for (const SpellError& error : errors) {
        if (offset >= error.start && offset < error.start + error.length) {
            // Get the paragraph text
            if (!m_document || paraIndex >= m_document->paragraphCount()) {
                return {QString(), 0, 0};
            }

            const KmlParagraph* para = m_document->paragraph(paraIndex);
            if (!para) {
                return {QString(), 0, 0};
            }

            QString text = para->plainText();
            if (error.start + error.length <= text.length()) {
                QString word = text.mid(error.start, error.length);
                return {word, error.start, error.start + error.length};
            }
        }
    }

    return {QString(), 0, 0};
}

QMenu* BookEditor::createSpellCheckContextMenu(const QString& word, int paraIndex,
                                                int startOffset, int endOffset)
{
    QMenu* menu = new QMenu(this);

    // Get suggestions from spell check service
    QStringList suggestions;
    if (m_spellCheckService) {
        suggestions = m_spellCheckService->suggestions(word, 5);
    }

    // Add suggestion actions
    if (suggestions.isEmpty()) {
        QAction* noSuggestionsAction = menu->addAction(tr("(No suggestions)"));
        noSuggestionsAction->setEnabled(false);
    } else {
        for (const QString& suggestion : suggestions) {
            QAction* action = menu->addAction(suggestion);
            connect(action, &QAction::triggered, this, [this, paraIndex, startOffset, endOffset, suggestion]() {
                replaceWord(paraIndex, startOffset, endOffset, suggestion);
            });
        }
    }

    menu->addSeparator();

    // Add to dictionary option
    QAction* addToDictAction = menu->addAction(tr("Add to Dictionary"));
    connect(addToDictAction, &QAction::triggered, this, [this, word]() {
        if (m_spellCheckService) {
            m_spellCheckService->addToUserDictionary(word);
            // Re-check affected paragraph
            requestSpellCheck();
        }
    });

    // Ignore option
    QAction* ignoreAction = menu->addAction(tr("Ignore"));
    connect(ignoreAction, &QAction::triggered, this, [this, word]() {
        if (m_spellCheckService) {
            m_spellCheckService->ignoreWord(word);
            // Re-check affected paragraph
            requestSpellCheck();
        }
    });

    return menu;
}

void BookEditor::replaceWord(int paraIndex, int startOffset, int endOffset, const QString& replacement)
{
    if (!m_document || paraIndex >= m_document->paragraphCount()) {
        return;
    }

    // Select the word to replace
    SelectionRange range;
    range.start = CursorPosition{paraIndex, startOffset};
    range.end = CursorPosition{paraIndex, endOffset};
    m_selection = range;

    // Delete selection and insert replacement
    deleteSelectedText();
    insertText(replacement);

    core::Logger::getInstance().debug("BookEditor: Replaced '{}' at ({}, {}-{}) with '{}'",
        m_document->paragraph(paraIndex)->plainText().mid(startOffset, endOffset - startOffset).toStdString(),
        paraIndex, startOffset, endOffset, replacement.toStdString());
}

// =============================================================================
// Grammar Check Integration (Phase 6.17)
// =============================================================================

void BookEditor::setGrammarCheckService(GrammarCheckService* service)
{
    // Disconnect from previous service
    if (m_grammarCheckService) {
        disconnect(m_grammarCheckService, nullptr, this, nullptr);
        m_grammarCheckService->setBookEditor(nullptr);
    }

    m_grammarCheckService = service;

    // Connect to new service
    if (m_grammarCheckService) {
        connect(m_grammarCheckService, &GrammarCheckService::paragraphChecked,
                this, &BookEditor::onGrammarCheckParagraph);

        // Connect service to this BookEditor for paragraph signals
        m_grammarCheckService->setBookEditor(this);

        core::Logger::getInstance().debug("BookEditor: Grammar check service connected");
    }
}

GrammarCheckService* BookEditor::grammarCheckService() const
{
    return m_grammarCheckService;
}

void BookEditor::requestGrammarCheck()
{
    if (m_grammarCheckService && m_document) {
        m_grammarCheckService->checkDocumentAsync();
    }
}

void BookEditor::onGrammarCheckParagraph(int paragraphIndex, const QList<GrammarError>& errors)
{
    if (!m_layoutManager) {
        return;
    }

    // Get the paragraph layout
    ParagraphLayout* layout = m_layoutManager->paragraphLayout(paragraphIndex);
    if (!layout) {
        return;
    }

    // Convert GrammarError to GrammarErrorRange and set on layout
    layout->clearGrammarErrors();
    for (const GrammarError& error : errors) {
        // Convert GrammarIssueType to GrammarErrorType
        GrammarErrorType errorType;
        switch (error.type) {
            case GrammarIssueType::Grammar:
                errorType = GrammarErrorType::Grammar;
                break;
            case GrammarIssueType::Style:
                errorType = GrammarErrorType::Style;
                break;
            case GrammarIssueType::Typography:
                errorType = GrammarErrorType::Typography;
                break;
            default:
                errorType = GrammarErrorType::Grammar;
                break;
        }
        layout->addGrammarError(error.startPos, error.length, errorType);
    }

    // Trigger repaint for the affected paragraph
    update();
}

std::optional<GrammarError> BookEditor::getGrammarErrorAt(int paraIndex, int offset) const
{
    if (!m_grammarCheckService) {
        return std::nullopt;
    }

    // Get cached errors for the paragraph
    QList<GrammarError> errors = m_grammarCheckService->errorsForParagraph(paraIndex);

    for (const GrammarError& error : errors) {
        if (offset >= error.startPos && offset < error.startPos + error.length) {
            return error;
        }
    }

    return std::nullopt;
}

QMenu* BookEditor::createGrammarContextMenu(const GrammarError& error, int paraIndex)
{
    QMenu* menu = new QMenu(this);

    // Show the error message as a disabled item (header)
    QAction* headerAction = menu->addAction(error.shortMessage.isEmpty() ? error.message : error.shortMessage);
    headerAction->setEnabled(false);

    // Show the problematic text
    if (!error.text.isEmpty()) {
        QAction* textAction = menu->addAction(tr("Error: \"%1\"").arg(error.text));
        textAction->setEnabled(false);
    }

    menu->addSeparator();

    // Add suggestions
    if (!error.suggestions.isEmpty()) {
        for (const QString& suggestion : error.suggestions) {
            QAction* action = menu->addAction(suggestion);
            connect(action, &QAction::triggered, this, [this, paraIndex, error, suggestion]() {
                replaceWord(paraIndex, error.startPos, error.startPos + error.length, suggestion);
            });
        }
        menu->addSeparator();
    }

    // Show full explanation if different from short message
    if (!error.message.isEmpty() && error.message != error.shortMessage) {
        QAction* explainAction = menu->addAction(tr("Explanation..."));
        connect(explainAction, &QAction::triggered, this, [error]() {
            QMessageBox::information(nullptr, QObject::tr("Grammar Issue"),
                QObject::tr("<b>%1</b><br><br>%2<br><br><i>Rule: %3 (%4)</i>")
                    .arg(error.shortMessage.isEmpty() ? error.text : error.shortMessage)
                    .arg(error.message)
                    .arg(error.ruleId)
                    .arg(error.category));
        });
    }

    // Ignore rule option
    QAction* ignoreAction = menu->addAction(tr("Ignore this rule"));
    connect(ignoreAction, &QAction::triggered, this, [this, error]() {
        if (m_grammarCheckService) {
            m_grammarCheckService->ignoreRule(error.ruleId);
            requestGrammarCheck();
        }
    });

    return menu;
}

// =============================================================================
// Position Calculation Helpers (OpenSpec #00043)
// =============================================================================

int BookEditor::calculateAbsolutePosition(const CursorPosition& pos) const
{
    if (m_textBuffer == nullptr || m_textBuffer->blockCount() == 0) {
        return 0;
    }

    int absolutePos = 0;
    const int targetPara = std::max(0, pos.paragraph);
    const int paraCount = m_textBuffer->blockCount();

    // Sum lengths of all paragraphs before the target paragraph
    for (int i = 0; i < targetPara && i < paraCount; ++i) {
        absolutePos += paragraphLength(m_textBuffer.get(), i);
        absolutePos += 1;  // Account for newline character between paragraphs
    }

    // Add the character offset within the target paragraph
    if (targetPara < paraCount) {
        const int paraLen = paragraphLength(m_textBuffer.get(), targetPara);
        absolutePos += std::min(pos.offset, paraLen);
    }

    return absolutePos;
}

CursorPosition BookEditor::calculateCursorPosition(int absolutePos) const
{
    if (m_textBuffer == nullptr || m_textBuffer->blockCount() == 0 || absolutePos <= 0) {
        return CursorPosition{0, 0};
    }

    int remaining = absolutePos;
    const int paraCount = m_textBuffer->blockCount();

    for (int i = 0; i < paraCount; ++i) {
        const int paraLen = paragraphLength(m_textBuffer.get(), i);

        if (remaining <= paraLen) {
            // Position is within this paragraph
            return CursorPosition{i, remaining};
        }

        remaining -= paraLen;
        remaining -= 1;  // Account for newline character

        if (remaining < 0) {
            // Position was at the newline between paragraphs
            return CursorPosition{i, paraLen};
        }
    }

    // Position is beyond document end - return end of last paragraph
    const int lastPara = paraCount - 1;
    return CursorPosition{lastPara, paragraphLength(m_textBuffer.get(), lastPara)};
}

// =============================================================================
// Find/Replace (Phase 9.4-9.6)
// =============================================================================

void BookEditor::setupFindReplace()
{
    m_searchEngine = std::make_unique<SearchEngine>(this);
    m_searchEngine->setDocument(m_textBuffer.get());

    // Connect search engine to render engine
    if (m_renderEngine) {
        m_renderEngine->setSearchEngine(m_searchEngine.get());
    }

    // Create FindReplaceBar (will be shown when needed)
    m_findReplaceBar = new gui::FindReplaceBar(this);
    m_findReplaceBar->setSearchEngine(m_searchEngine.get());
    m_findReplaceBar->setUndoStack(m_undoStack);
    // Phase 11.6: Removed setFormatLayer - not needed (formatting in QTextCharFormat)
    m_findReplaceBar->hide();

    connect(m_findReplaceBar, &gui::FindReplaceBar::navigateToMatch,
            this, &BookEditor::onNavigateToMatch);
    connect(m_findReplaceBar, &gui::FindReplaceBar::closed,
            this, &BookEditor::hideFindReplace);
    connect(m_searchEngine.get(), &SearchEngine::matchesChanged,
            this, [this]() { update(); });  // Repaint on match change
}

SearchEngine* BookEditor::searchEngine() const
{
    return m_searchEngine.get();
}

void BookEditor::showFind()
{
    if (!m_findReplaceBar) {
        setupFindReplace();
    }

    // If text selected, use as search text
    if (hasSelection()) {
        m_findReplaceBar->setSearchText(selectedText());
    }

    m_findReplaceBar->showFind();

    // Position at top of editor
    int scrollBarWidth = m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
    m_findReplaceBar->setGeometry(0, 0, width() - scrollBarWidth, m_findReplaceBar->sizeHint().height());

    m_findReplaceBar->show();
    m_findReplaceBar->focusSearchInput();
}

void BookEditor::showFindReplace()
{
    if (!m_findReplaceBar) {
        setupFindReplace();
    }

    if (hasSelection()) {
        m_findReplaceBar->setSearchText(selectedText());
    }

    m_findReplaceBar->showFindReplace();

    // Position at top of editor
    int scrollBarWidth = m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
    m_findReplaceBar->setGeometry(0, 0, width() - scrollBarWidth, m_findReplaceBar->sizeHint().height());

    m_findReplaceBar->show();
    m_findReplaceBar->focusSearchInput();
}

void BookEditor::findNext()
{
    if (!m_searchEngine) return;
    auto match = m_searchEngine->nextMatch();
    if (match.isValid()) {
        onNavigateToMatch(match);
    }
}

void BookEditor::findPrevious()
{
    if (!m_searchEngine) return;
    auto match = m_searchEngine->previousMatch();
    if (match.isValid()) {
        onNavigateToMatch(match);
    }
}

void BookEditor::hideFindReplace()
{
    if (m_findReplaceBar) {
        m_findReplaceBar->hide();
    }
    if (m_searchEngine) {
        m_searchEngine->clear();
    }
    update();  // Clear highlights
    setFocus();
}

void BookEditor::onNavigateToMatch(const SearchMatch& match)
{
    // Move cursor to match position
    CursorPosition newPos{match.paragraph, match.paragraphOffset};
    setCursorPosition(newPos);

    // Select the match
    CursorPosition endPos{match.paragraph, match.paragraphOffset + static_cast<int>(match.length)};
    SelectionRange newSelection{newPos, endPos};
    setSelection(newSelection);

    ensureCursorVisible();
    update();
}

// =============================================================================
// TODO/Note Markers (Phase 9.12)
// =============================================================================

void BookEditor::addTodoAtCursor(const QString& text)
{
    // Phase 11.6: Markers stored in QTextCharFormat::UserProperty
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    // Calculate absolute position
    int absPos = calculateAbsolutePosition(m_cursorPosition);

    TextMarker marker;
    marker.id = TextMarker::generateId();
    marker.position = absPos;
    marker.length = 1;
    marker.text = text.isEmpty() ? tr("TODO") : text;
    marker.type = MarkerType::Todo;
    marker.completed = false;
    marker.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    m_undoStack->push(new MarkerAddCommand(
        m_textBuffer.get(), m_cursorPosition, marker));

    update();
}

void BookEditor::addNoteAtCursor(const QString& text)
{
    // Phase 11.6: Markers stored in QTextCharFormat::UserProperty
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    // Calculate absolute position
    int absPos = calculateAbsolutePosition(m_cursorPosition);

    TextMarker marker;
    marker.id = TextMarker::generateId();
    marker.position = absPos;
    marker.length = 1;
    marker.text = text.isEmpty() ? tr("Note") : text;
    marker.type = MarkerType::Note;
    marker.completed = false;
    marker.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    m_undoStack->push(new MarkerAddCommand(
        m_textBuffer.get(), m_cursorPosition, marker));

    update();
}

void BookEditor::removeMarkerAtCursor()
{
    // Phase 11.6: Use findAllMarkers from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    // Find all markers and filter by position
    auto allMarkers = findAllMarkers(m_textBuffer.get(), std::nullopt);
    for (const auto& marker : allMarkers) {
        if (marker.position == absPos) {
            // Remove the first marker at cursor position
            m_undoStack->push(new MarkerRemoveCommand(
                m_textBuffer.get(), m_cursorPosition, marker));
            update();
            return;
        }
    }
}

void BookEditor::toggleTodoAtCursor()
{
    // Phase 11.6: Use findAllMarkers from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    // Find all markers and filter by position and type
    auto allMarkers = findAllMarkers(m_textBuffer.get(), MarkerType::Todo);
    for (const auto& marker : allMarkers) {
        if (marker.position == absPos) {
            m_undoStack->push(new MarkerToggleCommand(
                m_textBuffer.get(), m_cursorPosition, marker.id,
                marker.position));
            update();
            return;
        }
    }
}

void BookEditor::goToNextTodo()
{
    // Phase 11.6: Use findNextMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto next = findNextMarker(m_textBuffer.get(), absPos, MarkerType::Todo);
    if (next) {
        CursorPosition newPos = calculateCursorPosition(next->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToPreviousTodo()
{
    // Phase 11.6: Use findPreviousMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto prev = findPreviousMarker(m_textBuffer.get(), absPos, MarkerType::Todo);
    if (prev) {
        CursorPosition newPos = calculateCursorPosition(prev->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToNextNote()
{
    // Phase 11.6: Use findNextMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto next = findNextMarker(m_textBuffer.get(), absPos, MarkerType::Note);
    if (next) {
        CursorPosition newPos = calculateCursorPosition(next->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToPreviousNote()
{
    // Phase 11.6: Use findPreviousMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto prev = findPreviousMarker(m_textBuffer.get(), absPos, MarkerType::Note);
    if (prev) {
        CursorPosition newPos = calculateCursorPosition(prev->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToNextMarker()
{
    // Phase 11.6: Use findNextMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto next = findNextMarker(m_textBuffer.get(), absPos, std::nullopt);  // Any type
    if (next) {
        CursorPosition newPos = calculateCursorPosition(next->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToPreviousMarker()
{
    // Phase 11.6: Use findPreviousMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto prev = findPreviousMarker(m_textBuffer.get(), absPos, std::nullopt);  // Any type
    if (prev) {
        CursorPosition newPos = calculateCursorPosition(prev->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

QString BookEditor::toKml() const
{
    // Phase 11.6: Use KmlSerializer directly with QTextDocument
    if (m_textBuffer && m_textBuffer.get()) {
        KmlSerializer serializer;
        return serializer.toKml(m_textBuffer.get());
    }
    return QString();
}

size_t BookEditor::paragraphCount() const
{
    // Phase 11.6: Use QTextDocument directly
    if (m_textBuffer && m_textBuffer.get()) {
        return static_cast<size_t>(m_textBuffer.get()->blockCount());
    }
    return 0;
}

QString BookEditor::paragraphPlainText(size_t index) const
{
    // Phase 11.6: Use QTextDocument directly
    if (!m_textBuffer || !m_textBuffer.get()) {
        return QString();
    }
    QTextBlock block = m_textBuffer.get()->findBlockByNumber(static_cast<int>(index));
    return block.isValid() ? block.text() : QString();
}

QString BookEditor::plainText() const
{
    // Phase 11.6: Use QTextDocument directly
    if (m_textBuffer && m_textBuffer.get()) {
        return m_textBuffer.get()->toPlainText();
    }
    return QString();
}

size_t BookEditor::characterCount() const
{
    // Phase 11.6: Use QTextDocument directly
    if (m_textBuffer && m_textBuffer.get()) {
        return static_cast<size_t>(m_textBuffer.get()->characterCount());
    }
    return 0;
}

void BookEditor::fromKml(const QString& kml)
{
    auto& logger = core::Logger::getInstance();
    auto startTime = std::chrono::high_resolution_clock::now();
    auto logElapsed = [&](const char* step) {
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        logger.info("BookEditor::fromKml [{}ms] {}", ms, step);
    };

    logElapsed("START");

    // Clear undo stack first
    if (m_undoStack) {
        m_undoStack->clear();
    }

    if (kml.isEmpty()) {
        logger.debug("BookEditor::fromKml - empty KML, clearing content");
        // Phase 11.6: Clear content using QTextDocument directly
        if (m_textBuffer && m_textBuffer.get()) {
            QTextCursor clearCursor(m_textBuffer.get());
            clearCursor.select(QTextCursor::Document);
            clearCursor.removeSelectedText();
        }
        // Phase 11.6: Removed FormatLayer and MetadataLayer - data stored in QTextCharFormat
        m_cursorPosition = {0, 0};
        m_textCursor = QTextCursor(m_textBuffer.get());
        clearSelection();
        update();
        emit contentChanged();
        emit documentChanged();
        return;
    }

    logElapsed("Before parseKml");

    // Phase 11.6: Use KmlParser to parse KML directly into QTextDocument
    KmlParser parser;
    QTextDocument* newDoc = parser.parseKml(kml);

    logElapsed("After parseKml");

    if (!newDoc) {
        logger.error("BookEditor::fromKml - parse error: {} (line {}, col {})",
            parser.lastError().toStdString(), parser.lastErrorLine(), parser.lastErrorColumn());
        return;
    }

    logElapsed("Disconnecting observers");

    // Phase 11.6: Disconnect ViewportManager before modifying buffer
    if (m_viewportManager && m_textBuffer) {
        m_viewportManager->setDocument(nullptr);
    }
    // Phase 11.6: Removed FormatLayer and LazyLayoutManager

    logElapsed("Copying document content");

    // Phase 11.6: Copy content from parsed document to m_textBuffer (QTextDocument)
    // This preserves QTextCharFormat (inline formatting like bold, italic, etc.)
    if (m_textBuffer) {
        QTextCursor destCursor(m_textBuffer.get());
        destCursor.select(QTextCursor::Document);
        destCursor.removeSelectedText();

        // Copy blocks from parsed document preserving all formatting
        QTextCursor srcCursor(newDoc);
        srcCursor.select(QTextCursor::Document);
        destCursor.insertFragment(srcCursor.selection());
    }

    // Clean up parsed document
    delete newDoc;

    // Phase 11.6: Removed FormatLayer and MetadataLayer
    // - Formatting stored in QTextCharFormat
    // - Markers (TODO/Note) stored in QTextCharFormat::UserProperty (KmlPropTodo)

    // Phase 11.6: Removed LazyLayoutManager - Qt handles layout automatically

    logElapsed("Setting up RenderEngine");

    if (m_renderEngine) {
        m_renderEngine->setDocument(m_textBuffer.get());
    }

    logElapsed("Setting up ViewportManager");

    if (m_viewportManager) {
        // Set viewport size and connect to document
        m_viewportManager->setViewportSize(size());
        m_viewportManager->setDocument(m_textBuffer.get());
        m_viewportManager->setScrollPosition(0.0);

        logElapsed("ViewportManager initialized");
    }

    // Update SearchEngine with buffer (if it exists)
    if (m_searchEngine) {
        m_searchEngine->setDocument(m_textBuffer.get());
    }

    // Phase 11.6: Removed setFormatLayer - SearchEngine doesn't use it

    logElapsed("Loaded paragraphs");
    logger.debug("BookEditor::fromKml - loaded {} paragraphs",
        m_textBuffer ? m_textBuffer.get()->blockCount() : 0);

    // Reset editor state
    m_cursorPosition = {0, 0};
    m_textCursor = QTextCursor(m_textBuffer.get());
    clearSelection();

    // Sync cursor to RenderEngine (Phase 8 fix)
    if (m_renderEngine) {
        m_renderEngine->setCursorPosition(m_cursorPosition);
        m_renderEngine->setCursorVisible(true);
        if (m_cursorBlinkingEnabled) {
            m_renderEngine->startCursorBlink();
        }
    }

    // Invalidate layouts for old architecture
    if (m_layoutManager) {
        m_layoutManager->invalidateAllLayouts();
    }

    logElapsed("Before update/signals");

    update();
    emit contentChanged();
    emit documentChanged();

    logElapsed("DONE");
}

}  // namespace kalahari::editor
