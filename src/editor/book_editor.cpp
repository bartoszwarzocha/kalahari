/// @file book_editor.cpp
/// @brief BookEditor implementation (OpenSpec #00042 Phase 3.1-3.5)

#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/text_source_adapter.h>  // Phase 12.3: Text source adapters
#include <kalahari/editor/render_context.h>       // Phase 12.3: RenderContext, RenderMargins
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kml_comment.h>
#include <kalahari/editor/kml_element.h>
#include <kalahari/editor/kml_parser.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/gui/find_replace_bar.h>
#include <QAbstractTextDocumentLayout>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDateTime>
#include <QTextLine>  // Phase 11.10: For view mode cursor rendering
#include <QEasingCurve>
#include <QClipboard>
#include <QGuiApplication>
#include <QInputDialog>
#include <QFocusEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QVariantAnimation>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTimer>
#include <QUndoStack>
#include <QWheelEvent>
#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>

namespace kalahari::editor {

// Default smooth scroll duration in milliseconds
constexpr int DEFAULT_SMOOTH_SCROLL_DURATION = 150;

// Wheel scroll step in pixels (approximate line height)
constexpr qreal WHEEL_SCROLL_STEP = 60.0;

// View pixels one mouse wheel notch scrolls
constexpr qreal WHEEL_PIXELS_PER_NOTCH = 40.0;

// Zoom range and the factor of one zoom step (Ctrl+wheel notch, Zoom In/Out)
constexpr double MIN_ZOOM_FACTOR = 0.25;
constexpr double MAX_ZOOM_FACTOR = 4.0;
constexpr double ZOOM_STEP = 1.1;

// Default cursor blink interval in milliseconds
constexpr int DEFAULT_CURSOR_BLINK_INTERVAL = 500;

// Automatic scrolling while selecting with the mouse or dragging text: one step per
// interval, longer the further the mouse is past the edge (or into the edge band, for
// dragged text)
constexpr int AUTO_SCROLL_INTERVAL = 25;           // ms
constexpr qreal AUTO_SCROLL_DROP_BAND = 24.0;      // px at the top and bottom edges
constexpr qreal AUTO_SCROLL_MIN_STEP = 2.0;        // px
constexpr qreal AUTO_SCROLL_MAX_STEP = 60.0;       // px

// Phase 12.6: Margins now configurable via m_appearance.viewMargins and m_appearance.pageMargins
// Phase 12.5: Removed CURSOR_WIDTH (now handled by EditorRenderPipeline)
// Removed hardcoded LEFT_MARGIN and TOP_MARGIN constants

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

namespace {

/// @brief Undo item that calls back when its step is undone or redone
class CallbackUndoItem final : public QAbstractUndoItem {
public:
    explicit CallbackUndoItem(std::function<void()> callback)
        : m_callback(std::move(callback)) {}

    void undo() override { m_callback(); }
    void redo() override { m_callback(); }

private:
    std::function<void()> m_callback;
};

/// @brief Typography settings as the layout applies them (pixels at 100% zoom)
LayoutTypography layoutTypography(const EditorTypography& typography) {
    LayoutTypography result;
    result.lineSpacing = typography.lineHeight;
    result.paragraphSpacing = typography.paragraphSpacing;
    result.firstLineIndent = typography.firstLineIndent ? typography.indentSize : 0.0;
    return result;
}

/// @brief Per-paragraph cache attached to each block of the edit buffer
///
/// Document statistics are sums of per-paragraph counts, so after an edit only the
/// paragraphs it touched are counted again.
class ParagraphCache : public QTextBlockUserData {
public:
    core::TextCounts counts;
    bool countsValid = false;
};

/// @brief Word and character counts of the whole document, from the paragraph caches
core::TextCounts countDocument(const QTextDocument* doc) {
    core::TextCounts total;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        auto* cache = static_cast<ParagraphCache*>(block.userData());
        if (!cache) {
            cache = new ParagraphCache;
            block.setUserData(cache);  // the document takes ownership
        }
        if (!cache->countsValid) {
            cache->counts = core::countText(block.text());
            cache->countsValid = true;
        }
        total.words += cache->counts.words;
        total.nonSpaceCharacters += cache->counts.nonSpaceCharacters;
    }
    return total;
}

/// @brief Drop the cached counts of the paragraphs a content change touched
///
/// Same range as KalahariTextDocumentLayout::documentChanged(): every block from the one
/// holding @p from to the one holding the first character after the change.
void invalidateParagraphCounts(const QTextDocument* doc, int from, int charsAdded) {
    const QTextBlock last = doc->findBlock(from + charsAdded);
    for (QTextBlock block = doc->findBlock(from); block.isValid(); block = block.next()) {
        if (auto* cache = static_cast<ParagraphCache*>(block.userData())) {
            cache->countsValid = false;
        }
        if (block == last) {
            break;
        }
    }
}

/// @brief Fill a document from a parsed KML model, starting at the cursor's (empty) block
///
/// Each paragraph gets zero margins and its own alignment, if it has one (without one it
/// is shown with the default), and its text on a clean base format with the run formats
/// on top. Shared by loading a chapter and pasting Kalahari content,
/// so both read KML the same way.
void appendParagraphs(QTextCursor& cursor, const KmlDocumentModel& model) {
    QTextBlockFormat zeroMarginFormat;
    zeroMarginFormat.setTopMargin(0);
    zeroMarginFormat.setBottomMargin(0);

    for (size_t i = 0; i < model.paragraphCount(); ++i) {
        QTextBlockFormat blockFormat = zeroMarginFormat;
        if (const Qt::Alignment alignment = model.paragraphAlignment(i); alignment) {
            blockFormat.setAlignment(alignment);
        }
        if (i > 0) {
            cursor.insertBlock(blockFormat);
        } else {
            cursor.setBlockFormat(blockFormat);
        }

        // An EXPLICIT default char format, so the text does not take the format the cursor
        // still carries from the previous paragraph's last run (formatting bled into every
        // following paragraph on reload); the runs then format only their own ranges.
        const int blockStart = cursor.position();
        cursor.insertText(model.paragraphText(i), QTextCharFormat());
        for (const auto& run : model.paragraphFormats(i)) {
            cursor.setPosition(blockStart + static_cast<int>(run.start));
            cursor.setPosition(blockStart + static_cast<int>(run.end), QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(run.format);
        }
        cursor.movePosition(QTextCursor::EndOfBlock);
    }
}

/// @brief Clipboard text as the editor can store and save it
///
/// Line and paragraph breaks of every platform become paragraph breaks. Characters XML
/// cannot hold (control characters other than tab, unpaired surrogates, U+FFFE, U+FFFF)
/// are dropped, so pasted text cannot make the chapter file unreadable.
QString pastedPlainText(const QString& text) {
    QString result;
    result.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const char16_t code = ch.unicode();
        if (code == u'\r' && i + 1 < text.size() && text.at(i + 1) == u'\n') {
            continue;  // the \n that follows makes the break
        }
        if (code == u'\n' || code == u'\r' || code == u'\v' || code == u'\f' ||
            code == QChar::LineSeparator || code == QChar::ParagraphSeparator) {
            result += u'\n';
        } else if (ch.isHighSurrogate() && i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
            result += ch;
            result += text.at(++i);
        } else if ((code >= 0x20 || code == u'\t') && !ch.isSurrogate() && code != 0xFFFE &&
                   code != 0xFFFF) {
            result += ch;
        }
    }
    return result;
}

/// @brief Insert a document at the cursor, replacing its selection, as one edit block
///
/// Each fragment keeps its character format. Paragraphs inserted whole keep their block
/// format; the paragraph the document goes into keeps its own, also on the text after the
/// insertion point. The cursor ends after the inserted text.
void insertDocument(QTextCursor& cursor, const QTextDocument& source) {
    cursor.beginEditBlock();
    cursor.removeSelectedText();

    const QTextBlockFormat targetFormat = cursor.blockFormat();
    const bool atParagraphStart = cursor.atBlockStart();
    const bool atParagraphEnd = cursor.atBlockEnd();
    const int insertionStart = cursor.position();
    const QTextBlock firstBlock = source.firstBlock();
    const QTextBlock lastBlock = source.lastBlock();

    for (QTextBlock block = firstBlock; block.isValid(); block = block.next()) {
        if (block != firstBlock) {
            // The last inserted paragraph also holds the text after the insertion point,
            // unless there is none
            const bool whole = block != lastBlock || (atParagraphEnd && block.length() > 1);
            cursor.insertBlock(whole ? block.blockFormat() : targetFormat, block.charFormat());
        }
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            cursor.insertText(fragment.text(), fragment.charFormat());
        }
    }

    // The first inserted paragraph is whole when it starts a paragraph and more follow
    if (firstBlock != lastBlock && atParagraphStart && firstBlock.length() > 1) {
        QTextCursor first(cursor.document());
        first.setPosition(insertionStart);
        first.setBlockFormat(firstBlock.blockFormat());
    }
    cursor.endEditBlock();
}

/// @brief Insert MIME data at the cursor, replacing its selection, as one edit block
///
/// Kalahari content (KML) is parsed like a chapter file, so formatting and alignment
/// survive; text from other programs takes the formatting of the insertion point. Only the
/// document changes: the editor's cursor and view follow once the outermost edit block has
/// ended. The cursor ends after the inserted text.
void insertMimeData(QTextCursor& cursor, const QMimeData& source) {
    if (source.hasFormat(QString::fromLatin1(MIME_KML))) {
        KmlDocumentModel model;
        if (model.loadKml(QString::fromUtf8(source.data(QString::fromLatin1(MIME_KML)))) &&
            model.paragraphCount() > 0) {
            QTextDocument content;
            content.setUndoRedoEnabled(false);
            QTextCursor contentCursor(&content);
            appendParagraphs(contentCursor, model);
            insertDocument(cursor, content);
            return;
        }
        core::Logger::getInstance().warn("BookEditor: unreadable KML, inserting the text");
    }

    cursor.beginEditBlock();  // the replaced selection and the text: one undo step
    cursor.insertText(pastedPlainText(source.text()));
    cursor.endEditBlock();
}

}  // anonymous namespace

// =============================================================================
// Construction / Destruction
// =============================================================================

BookEditor::BookEditor(QWidget* parent)
    : QWidget(parent)
    // Phase 11: Removed old architecture (KmlDocument, LayoutManager, VirtualScrollManager, PageLayoutManager)
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
    // Phase 8: New performance-optimized components (OpenSpec #00043)
    // Phase 11.10: KmlDocumentModel for fast loading + lazy rendering
    , m_documentModel(std::make_unique<KmlDocumentModel>(this))
    // Phase 11.6: QTextDocument for editing - created on-demand (see ensureEditMode())
    , m_textBuffer(nullptr)
    , m_isEditMode(false)
    // Phase 11.6: Removed m_metadataLayer - markers stored in QTextCharFormat::UserProperty
{
    // Enable input method support
    setAttribute(Qt::WA_InputMethodEnabled, true);

    // Create UI fade timer for distraction-free mode
    m_uiFadeTimer = new QTimer(this);
    m_uiFadeTimer->setSingleShot(true);
    connect(m_uiFadeTimer, &QTimer::timeout, this, [this]() {
        // Fade out UI opacity
        m_uiOpacity = 0.0;
        update();
    });

    // Phase 11.6: m_textBuffer created on-demand in ensureEditMode()
    // m_textCursor initialized when m_textBuffer is created

    // Create ViewportManager (initially without document - set in fromKml())
    m_viewportManager = std::make_unique<ViewportManager>(this);
    // Note: setDocument() called in fromKml() after loading

    // The scrollbar range follows the document height: edits, re-wrapping after a width
    // change, font and zoom changes all end up here.
    connect(m_viewportManager.get(), &ViewportManager::documentHeightChanged,
            this, [this]([[maybe_unused]] double newHeight) {
        updateScrollBarRange();
        updatePageInfo();  // the page count follows the height in page mode
    });

    // The page of the cursor, for the status bar
    connect(this, &BookEditor::cursorPositionChanged, this, [this]() { updatePageInfo(); });

    // Blocks laid out on demand, or wrapped again at a new width, change height while the
    // content stays the same. The viewport keeps the text at its top in place by moving
    // the scroll position; the scroll bar and the painted position follow it.
    connect(m_viewportManager.get(), &ViewportManager::scrollPositionAnchored,
            this, [this](double position) {
        syncScrollBarValue();
        updatePipelineScroll();
        emit scrollOffsetChanged(position);
    });

    // Phase 12.3: Create EditorRenderPipeline (unified rendering)
    m_renderPipeline = std::make_unique<EditorRenderPipeline>(this);
    m_renderPipeline->setViewportManager(m_viewportManager.get());
    // Note: setSearchEngine() called in setupFindReplace() after search engine creation

    // Configure initial pipeline context
    RenderContext ctx;
    ctx.font = m_appearance.typography.textFont;
    ctx.colors.text = m_appearance.colors.textColor(m_appearance.colorMode);
    ctx.colors.background = m_appearance.colors.background(m_appearance.colorMode);
    ctx.colors.cursor = m_appearance.cursor.useCustomColor
        ? m_appearance.cursor.customColor
        : m_appearance.colors.textColor(m_appearance.colorMode);
    ctx.colors.selection = m_appearance.colors.selection;
    // Phase 15: Use centralized margin calculation (converts mm to pixels for Page Mode)
    auto margins = calculateEffectiveMargins();
    ctx.margins = margins;
    ctx.textWidth = static_cast<double>(width());
    ctx.viewMode = m_viewMode;
    // Set initial DPI (will be updated in showEvent when screen is available)
    ctx.screenDpi = DEFAULT_DPI;
    m_renderPipeline->setContext(ctx);

    // Connect pipeline repaint signal
    connect(m_renderPipeline.get(), &EditorRenderPipeline::repaintRequested,
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
    if (m_uiFadeTimer != nullptr) {
        m_uiFadeTimer->stop();
    }
}

// Phase 11: Old architecture methods removed (setDocument, document, layoutManager, scrollManager)
// Use fromKml()/toKml() for document operations
// Use ViewportManager for scroll operations

// =============================================================================
// Scrolling
// =============================================================================

QScrollBar* BookEditor::verticalScrollBar() const
{
    return m_verticalScrollBar;
}

qreal BookEditor::scrollOffset() const
{
    // Phase 11.10: In view mode, use direct scroll offset
    if (!m_isEditMode && m_documentModel && m_documentModel->paragraphCount() > 0) {
        return m_viewModeScrollOffset;
    }
    // Phase 11: Use ViewportManager for edit mode
    return m_viewportManager ? m_viewportManager->scrollPosition() : 0.0;
}

void BookEditor::setScrollOffset(qreal offset)
{
    // Phase 11.10: In view mode, manage scroll directly
    if (!m_isEditMode && m_documentModel && m_documentModel->paragraphCount() > 0) {
        auto [topMargin, bottomMargin] = getScrollPadding();
        double maxScroll = std::max(0.0, m_documentModel->totalHeight() + topMargin + bottomMargin - static_cast<double>(height()));
        double newOffset = std::clamp(static_cast<double>(offset), 0.0, maxScroll);
        if (std::abs(m_viewModeScrollOffset - newOffset) > 0.001) {
            m_viewModeScrollOffset = newOffset;
            syncScrollBarValue();
            emit scrollOffsetChanged(newOffset);
            updatePipelineScroll();  // Phase 14: lightweight scroll only
            update();
            resetCursorBlink();
        }
        return;
    }

    // Edit mode: use ViewportManager
    if (!m_viewportManager) return;

    qreal oldOffset = m_viewportManager->scrollPosition();
    m_viewportManager->setScrollPosition(offset);
    qreal newOffset = m_viewportManager->scrollPosition();

    if (oldOffset != newOffset) {
        syncScrollBarValue();
        emit scrollOffsetChanged(newOffset);
        updatePipelineScroll();  // Phase 14: lightweight scroll only
        update();  // Request repaint
        // Wherever the view stops, the cursor shows at once instead of in the middle of a
        // blink
        resetCursorBlink();
    }
}

void BookEditor::scrollBy(qreal delta, bool animated)
{
    qreal targetOffset = scrollOffset() + delta;
    scrollTo(targetOffset, animated);
}

void BookEditor::scrollTo(qreal offset, bool animated)
{
    // Phase 11: Use ViewportManager for max scroll
    qreal maxScroll = m_viewportManager ? m_viewportManager->maxScrollPosition() : 0.0;
    offset = qBound(0.0, offset, maxScroll);

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
        // Track old position for focus mode optimization
        CursorPosition oldPos = m_cursorPosition;
        m_cursorPosition = validatedPos;

        ensureCursorVisible();
        emit cursorPositionChanged(m_cursorPosition);

        // Phase 11.11: Optimized cursor sync - only update cursor, not full state
        syncPipelineCursor();

        // Targeted repaint for cursor movement
        if (m_renderPipeline) {
            // For focus mode, repaint old and new paragraphs
            if (m_appearance.focusMode.enabled && oldPos.paragraph != validatedPos.paragraph) {
                // Mark old and new paragraphs dirty (pipeline handles this now)
                update();  // Full update needed for focus mode paragraph change
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
        return;
    }

    // Scroll viewport to make cursor visible (only when line is partially clipped)
    if (!m_isEditMode || !m_textBuffer || !m_viewportManager) {
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

    // The band of the view the line must be within: the view without its vertical view
    // margins, as document y relative to the scroll position (the scroll position is drawn
    // at the view's top inset, and page mode zooms by the view scale)
    const qreal scrollY = m_viewportManager->scrollPosition();
    const qreal scale = m_viewportManager->viewScale();
    const qreal inset = m_viewportManager->viewTopInset();
    const qreal viewMargin = m_appearance.viewMargins.vertical;
    const qreal bandTop = (viewMargin - inset) / scale;
    const qreal bandBottom = (static_cast<qreal>(height()) - viewMargin - inset) / scale;

    // Scroll only if line is NOT fully visible
    if (lineTop() < scrollY + bandTop) {
        // Line is clipped at top - scroll up to show full line
        setScrollOffset(lineTop() - bandTop);
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
        ++newPos.offset;
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

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;
    QTextBlock block = m_textBuffer->findBlockByNumber(newPos.paragraph);
    if (!block.isValid()) {
        return;
    }

    QString text = block.text();

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
        QTextBlock prevBlock = m_textBuffer->findBlockByNumber(newPos.paragraph);
        newPos.offset = prevBlock.isValid() ? prevBlock.length() - 1 : 0;
        if (newPos.offset < 0) newPos.offset = 0;
    }

    setCursorPosition(newPos);
}

void BookEditor::moveCursorWordRight()
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Invalidate preferred X position
    m_preferredCursorXValid = false;

    CursorPosition newPos = m_cursorPosition;
    QTextBlock block = m_textBuffer->findBlockByNumber(newPos.paragraph);
    if (!block.isValid()) {
        return;
    }

    QString text = block.text();
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
    } else if (newPos.paragraph + 1 < m_textBuffer->blockCount()) {
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
    // NOTE: ensureCursorVisible() is already called inside setCursorPosition()
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
    // NOTE: ensureCursorVisible() is already called inside setCursorPosition()
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

    // One view height in document units (page mode zooms). The view and the cursor move
    // together: the cursor keeps its place in the view and its column, as in word processors.
    const double step = direction * m_viewportManager->visibleDocumentHeight();
    if (step == 0.0) {
        return;
    }

    // The goal in the document: the column vertical moves keep and the middle of the cursor's
    // line. Presses in a row go on from the previous goal rather than from the line and the
    // character it hit, so that Page Down and Page Up bring the cursor back where it was.
    const QRectF caret = m_renderPipeline->caretRect(m_cursorPosition);
    QPointF goal = m_renderPipeline->widgetToDocument(caret.topLeft());
    goal.ry() += caret.height() / m_viewportManager->viewScale() / 2.0;
    if (m_preferredCursorXValid && m_preferredCursorXPosition == m_cursorPosition) {
        goal.setX(m_preferredCursorX);
    }
    // The previous goal holds while it still leads to the cursor: no other move took the
    // cursor away and the text did not wrap anew (a goal between pages leads to a line nearby)
    if (m_pageMoveCursor == m_cursorPosition &&
        positionFromPoint(m_renderPipeline->documentToWidget(
            QPointF(goal.x(), m_pageMoveGoalY))) == m_cursorPosition) {
        goal.setY(m_pageMoveGoalY);
    }

    setScrollOffset(scrollOffset() + step);

    // The text now in view laid out, the cursor goes to the same place in the view; at the
    // start or the end of the chapter, where the view stops, it goes on by the rest
    m_renderPipeline->ensureVisibleLaidOut();
    goal.ry() += step;
    setCursorPosition(positionFromPoint(m_renderPipeline->documentToWidget(goal)));

    // A goal past the start or the end of the chapter is kept at its edge, so that presses
    // there do not pile up
    m_pageMoveGoalY = std::clamp(goal.y(), 0.0, m_viewportManager->totalDocumentHeight());
    m_pageMoveCursor = m_cursorPosition;
    m_preferredCursorX = goal.x();
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

    // Phase 11.10: Validate against document (m_textBuffer or m_documentModel)
    bool hasContent = (m_isEditMode && m_textBuffer && m_textBuffer->blockCount() > 0) ||
                      (!m_isEditMode && m_documentModel && m_documentModel->paragraphCount() > 0);

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

    // Phase 11.10: Use m_textBuffer in edit mode, m_documentModel in view mode
    if (m_isEditMode && m_textBuffer) {
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
    } else if (m_documentModel) {
        for (int paraIdx = sel.start.paragraph; paraIdx <= sel.end.paragraph; ++paraIdx) {
            if (static_cast<size_t>(paraIdx) >= m_documentModel->paragraphCount()) {
                continue;
            }

            QString text = m_documentModel->paragraphText(static_cast<size_t>(paraIdx));
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

// =============================================================================
// Text Input (Phase 4.1 - 4.4)
// =============================================================================

void BookEditor::insertText(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }

    // Phase 11.10: Ensure we're in edit mode before modifying
    ensureEditMode();

    if (!m_textBuffer) {
        return;
    }

    // Direct QTextCursor edit — recorded by QTextDocument's native undo.
    QTextCursor cursor(m_textBuffer.get());
    if (hasSelection()) {
        SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        cursor.beginEditBlock();  // one undo step for the replace (delete + insert)
        cursor.insertText(text);  // replaces the selection
        cursor.endEditBlock();
        clearSelection();
    } else {
        cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
        cursor.insertText(text);
    }

    // Mirror the resulting QTextCursor into the editor's cursor model.
    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit paragraphModified(m_cursorPosition.paragraph);
}

bool BookEditor::deleteSelectedText()
{
    if (!hasSelection()) {
        return false;
    }

    // Phase 11.10: Ensure we're in edit mode before modifying
    ensureEditMode();

    if (!m_textBuffer) {
        return false;
    }

    SelectionRange sel = m_selection.normalized();

    // Direct QTextCursor delete — recorded by QTextDocument's native undo.
    QTextCursor cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
    cursor.removeSelectedText();

    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();
    clearSelection();

    update();
    emit contentChanged();
    return true;
}

void BookEditor::insertNewline()
{
    // Phase 11.10: Ensure we're in edit mode before modifying
    ensureEditMode();

    if (!m_textBuffer) {
        return;
    }

    // Split the paragraph via a direct QTextCursor insertBlock() — recorded by
    // QTextDocument's native undo, together with the replaced selection as one step.
    // insertBlock() inherits the current block format (zero margins + alignment), so the
    // new paragraph keeps the same layout.
    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        clearSelection();
    }
    cursor.beginEditBlock();
    cursor.removeSelectedText();
    cursor.insertBlock();
    cursor.endEditBlock();

    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit paragraphInserted(m_cursorPosition.paragraph);
}

void BookEditor::deleteBackward()
{
    // Phase 11.10: Ensure we're in edit mode before modifying
    ensureEditMode();

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

    const int oldPara = m_cursorPosition.paragraph;
    const bool wasAtBlockStart = (m_cursorPosition.offset == 0);

    // deletePreviousChar() removes the previous character, OR merges with the previous
    // paragraph when at the start of a block. Recorded by QTextDocument's native undo.
    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    cursor.deletePreviousChar();

    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    if (wasAtBlockStart) {
        emit paragraphRemoved(oldPara);
    } else {
        emit paragraphModified(m_cursorPosition.paragraph);
    }
}

void BookEditor::deleteForward()
{
    // Phase 11.10: Ensure we're in edit mode before modifying
    ensureEditMode();

    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    if (hasSelection()) {
        deleteSelectedText();
        return;
    }

    const int paraLen = paragraphText(m_textBuffer.get(), m_cursorPosition.paragraph).length();
    const bool atBlockEnd = (m_cursorPosition.offset >= paraLen);
    const bool hasNextBlock = (m_cursorPosition.paragraph + 1 < m_textBuffer->blockCount());

    if (!atBlockEnd || hasNextBlock) {
        // deleteChar() removes the character at the cursor, OR merges with the next
        // paragraph at the end of a block. Recorded by QTextDocument's native undo.
        QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
        cursor.deleteChar();

        m_cursorPosition.paragraph = cursor.blockNumber();
        m_cursorPosition.offset = cursor.positionInBlock();

        syncPipelineCursor();
        update();
        emit contentChanged();
        if (atBlockEnd) {
            emit paragraphRemoved(m_cursorPosition.paragraph + 1);
        } else {
            emit paragraphModified(m_cursorPosition.paragraph);
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

bool BookEditor::canUndo() const
{
    return m_textBuffer && m_textBuffer->isUndoAvailable();
}

bool BookEditor::canRedo() const
{
    return m_textBuffer && m_textBuffer->isRedoAvailable();
}

void BookEditor::undo()
{
    if (!m_isEditMode || !m_textBuffer || !m_textBuffer->isUndoAvailable()) {
        return;
    }

    // QTextDocument's native undo is the single source of truth for BOTH text and
    // formatting. undo(&cursor) also positions the cursor at the change — mirror it
    // into the editor's own cursor model.
    m_stepCursor.reset();
    QTextCursor cursor(m_textBuffer.get());
    m_textBuffer->undo(&cursor);
    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();
    clearSelection();
    restoreStepCursor();

    syncPipelineCursor();
    ensureCursorVisible();
    update();
    emit contentChanged();
    emit cursorPositionChanged(m_cursorPosition);
}

void BookEditor::redo()
{
    if (!m_isEditMode || !m_textBuffer || !m_textBuffer->isRedoAvailable()) {
        return;
    }

    m_stepCursor.reset();
    QTextCursor cursor(m_textBuffer.get());
    m_textBuffer->redo(&cursor);
    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();
    clearSelection();
    restoreStepCursor();

    syncPipelineCursor();
    ensureCursorVisible();
    update();
    emit contentChanged();
    emit cursorPositionChanged(m_cursorPosition);
}

void BookEditor::clearUndoStack()
{
    if (m_textBuffer) {
        m_textBuffer->clearUndoRedoStacks();
    }
}

// =============================================================================
// Clipboard (Phase 4.13-4.16)
// =============================================================================

void BookEditor::copy()
{
    if (std::unique_ptr<QMimeData> mimeData = createMimeDataFromSelection()) {
        QGuiApplication::clipboard()->setMimeData(mimeData.release());  // clipboard takes ownership
    }
}

void BookEditor::cut()
{
    if (!hasSelection() || !m_textBuffer) {
        return;
    }

    // Copy first
    copy();

    // Then delete selection (one undo step)
    deleteSelectedText();
}

void BookEditor::paste()
{
    insertFromMimeData(QGuiApplication::clipboard()->mimeData());
}

std::unique_ptr<QMimeData> BookEditor::createMimeDataFromSelection() const
{
    if (!hasSelection() || !m_textBuffer) {
        return nullptr;
    }

    const SelectionRange sel = m_selection.normalized();
    const QTextCursor range = createCursor(m_textBuffer.get(), sel.start, sel.end);
    const QString kml =
        KmlSerializer().toKml(m_textBuffer.get(), range.selectionStart(), range.selectionEnd());

    // Paragraphs separated by line breaks
    QString text = range.selectedText();
    text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));

    auto mimeData = std::make_unique<QMimeData>();
    mimeData->setText(text);
    mimeData->setHtml(ClipboardHandler::kmlToHtml(kml));
    mimeData->setData(QString::fromLatin1(MIME_KML), kml.toUtf8());
    return mimeData;
}

bool BookEditor::canInsertFromMimeData(const QMimeData* source)
{
    return source != nullptr &&
           (source->hasFormat(QString::fromLatin1(MIME_KML)) || source->hasText());
}

void BookEditor::insertFromMimeData(const QMimeData* source)
{
    if (!canInsertFromMimeData(source)) {
        return;
    }
    // Text from other programs is cleaned first: nothing to insert, nothing replaced
    if (!source->hasFormat(QString::fromLatin1(MIME_KML)) &&
        pastedPlainText(source->text()).isEmpty()) {
        return;
    }
    ensureEditMode();
    if (!m_textBuffer) {
        return;
    }

    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        clearSelection();
    }
    insertMimeData(cursor, *source);
    finishEdit(cursor);
}

bool BookEditor::dropMimeData(const QMimeData* source, const CursorPosition& position,
                              bool moveSelection)
{
    if (!canInsertFromMimeData(source)) {
        return false;
    }
    ensureEditMode();
    if (!m_textBuffer || (moveSelection && (!hasSelection() || isInSelection(position)))) {
        return false;
    }

    const SelectionRange moved = m_selection.normalized();
    clearSelection();

    // One undo step: moved text leaves its place and lands at the drop point. Both
    // cursors stay at the same text while the other one edits the document.
    QTextCursor cursor = createCursor(m_textBuffer.get(), validateCursorPosition(position));
    QTextCursor oldPlace;
    cursor.beginEditBlock();
    if (moveSelection) {
        oldPlace = createCursor(m_textBuffer.get(), moved.start, moved.end);
        oldPlace.removeSelectedText();
    }
    const int insertionStart = cursor.position();
    insertMimeData(cursor, *source);
    cursor.endEditBlock();

    // The dropped text is selected, with the cursor at its end
    finishEdit(cursor);
    if (!oldPlace.isNull() && oldPlace.blockNumber() != m_cursorPosition.paragraph) {
        emit paragraphModified(oldPlace.blockNumber());
    }
    const QTextBlock startBlock = m_textBuffer->findBlock(insertionStart);
    m_selectionAnchor = {startBlock.blockNumber(), insertionStart - startBlock.position()};
    setSelection({m_selectionAnchor, m_cursorPosition});
    return true;
}

void BookEditor::finishEdit(const QTextCursor& cursor)
{
    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit paragraphModified(m_cursorPosition.paragraph);
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
    setParagraphAlignment(Qt::AlignLeft);
}

void BookEditor::setAlignCenter()
{
    setParagraphAlignment(Qt::AlignHCenter);
}

void BookEditor::setAlignRight()
{
    setParagraphAlignment(Qt::AlignRight);
}

void BookEditor::setAlignJustify()
{
    setParagraphAlignment(Qt::AlignJustify);
}

void BookEditor::setParagraphAlignment(Qt::Alignment alignment)
{
    if (!m_textBuffer) {
        return;
    }

    // Phase 11: Use QTextBlockFormat for paragraph alignment
    int startPara = m_cursorPosition.paragraph;
    int endPara = m_cursorPosition.paragraph;

    if (hasSelection()) {
        SelectionRange normRange = m_selection.normalized();
        startPara = normRange.start.paragraph;
        endPara = normRange.end.paragraph;
    }

    // All the paragraphs in one undo step. Undoing or redoing it brings back the cursor and
    // selection it was made with: QTextDocument would put the cursor after the last
    // paragraph changed, so the next paragraph's alignment would show.
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    for (int i = startPara; i <= endPara; ++i) {
        QTextBlock block = m_textBuffer->findBlockByNumber(i);
        if (block.isValid()) {
            cursor.setPosition(block.position());
            QTextBlockFormat format = block.blockFormat();
            format.setAlignment(alignment);
            cursor.setBlockFormat(format);
        }
    }
    cursor.endEditBlock();

    // Phase 12.3: Mark pipeline dirty for relayout
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }

    emit contentChanged();
    update();
}

void BookEditor::restoreStepCursor()
{
    // The step changed no text, so its cursor and selection fit the text on both sides of it
    if (m_stepCursor) {
        m_cursorPosition = validateCursorPosition(m_stepCursor->cursor);
        setSelection(m_stepCursor->selection);
        m_stepCursor.reset();
    }
}

Qt::Alignment BookEditor::currentAlignment() const
{
    if (!m_textBuffer) {
        return DEFAULT_PARAGRAPH_ALIGNMENT;
    }

    QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (block.isValid()) {
        return effectiveAlignment(ownAlignment(block.blockFormat()));
    }
    return DEFAULT_PARAGRAPH_ALIGNMENT;
}

void BookEditor::toggleFormat(ElementType formatType)
{
    core::Logger::getInstance().debug("BookEditor::toggleFormat() called - "
        "type={}, hasSelection={}, cursor=({}, {})",
        elementTypeToString(formatType).toStdString(),
        hasSelection(), m_cursorPosition.paragraph, m_cursorPosition.offset);

    if (!m_textBuffer) {
        return;
    }

    if (hasSelection()) {
        // Phase 11: Use QTextCursor for formatting selection
        SelectionRange normRange = m_selection.normalized();
        bool alreadyHasFormat = hasFormat(formatType);

        // Create QTextCursor with selection
        QTextBlock startBlock = m_textBuffer->findBlockByNumber(normRange.start.paragraph);
        QTextBlock endBlock = m_textBuffer->findBlockByNumber(normRange.end.paragraph);
        if (!startBlock.isValid() || !endBlock.isValid()) return;

        int startPos = startBlock.position() + normRange.start.offset;
        int endPos = endBlock.position() + normRange.end.offset;

        QTextCursor cursor(m_textBuffer.get());
        cursor.setPosition(startPos);
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);

        // Create format to apply/remove
        QTextCharFormat fmt;
        switch (formatType) {
            case ElementType::Bold:
                fmt.setFontWeight(alreadyHasFormat ? QFont::Normal : QFont::Bold);
                break;
            case ElementType::Italic:
                fmt.setFontItalic(!alreadyHasFormat);
                break;
            case ElementType::Underline:
                fmt.setFontUnderline(!alreadyHasFormat);
                break;
            case ElementType::Strikethrough:
                fmt.setFontStrikeOut(!alreadyHasFormat);
                break;
            default:
                break;
        }

        // Apply format (QTextDocument handles undo/redo)
        cursor.mergeCharFormat(fmt);

        emit contentChanged();
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
    if (!m_textBuffer) {
        return false;
    }

    // Phase 11: Check QTextCharFormat for formatting
    auto checkCharFormat = [formatType](const QTextCharFormat& fmt) -> bool {
        switch (formatType) {
            case ElementType::Bold:
                return fmt.fontWeight() >= QFont::Bold;
            case ElementType::Italic:
                return fmt.fontItalic();
            case ElementType::Underline:
                return fmt.fontUnderline();
            case ElementType::Strikethrough:
                return fmt.fontStrikeOut();
            default:
                return false;
        }
    };

    if (hasSelection()) {
        // Check if ALL text in the selection carries this format. Iterate the
        // document's text FRAGMENTS (runs of uniform formatting) instead of
        // constructing one QTextCursor per character — the old per-character loop
        // was O(N) cursor allocations and made "select all + bold" take ~10 s on a
        // large chapter. Fragment iteration is O(runs), effectively instant.
        SelectionRange normRange = m_selection.normalized();

        for (int i = normRange.start.paragraph; i <= normRange.end.paragraph; ++i) {
            QTextBlock block = m_textBuffer->findBlockByNumber(i);
            if (!block.isValid()) continue;

            int start = (i == normRange.start.paragraph) ? normRange.start.offset : 0;
            int end = (i == normRange.end.paragraph) ? normRange.end.offset : block.length() - 1;
            if (end < 0) end = 0;
            if (start >= end) continue;  // nothing selected in this block

            // Document-absolute bounds of the selected range within this block.
            const int selFrom = block.position() + start;
            const int selTo = block.position() + end;

            for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
                QTextFragment frag = it.fragment();
                if (!frag.isValid()) continue;

                const int fragFrom = frag.position();
                const int fragTo = fragFrom + frag.length();
                // Skip fragments outside the selected range.
                if (fragTo <= selFrom || fragFrom >= selTo) continue;

                if (!checkCharFormat(frag.charFormat())) {
                    return false;
                }
            }
        }
        return true;
    } else {
        // Check format at cursor position
        QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
        if (!block.isValid()) return false;

        // If cursor is at end of paragraph, check previous character
        int checkOffset = m_cursorPosition.offset;
        if (checkOffset > 0) {
            checkOffset--;
        }

        QTextCursor cursor(m_textBuffer.get());
        cursor.setPosition(block.position() + checkOffset);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        return checkCharFormat(cursor.charFormat());
    }
}

// =============================================================================
// Font Selection (applies to selection if any, otherwise default font)
// =============================================================================

void BookEditor::setSelectionFontFamily(const QString& family)
{
    if (!m_isEditMode || !m_textBuffer) {
        return;
    }

    if (hasSelection()) {
        // Apply to selection
        SelectionRange normRange = m_selection.normalized();
        QTextBlock startBlock = m_textBuffer->findBlockByNumber(static_cast<int>(normRange.start.paragraph));
        QTextBlock endBlock = m_textBuffer->findBlockByNumber(static_cast<int>(normRange.end.paragraph));

        if (!startBlock.isValid() || !endBlock.isValid()) {
            return;
        }

        int startPos = startBlock.position() + normRange.start.offset;
        int endPos = endBlock.position() + normRange.end.offset;

        QTextCursor cursor(m_textBuffer.get());
        cursor.setPosition(startPos);
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);

        QTextCharFormat fmt;
        fmt.setFontFamilies({family});
        cursor.mergeCharFormat(fmt);

        emit contentChanged();
        update();
    }
    // No selection: the toolbar font combo is a selection-only formatting control,
    // like a classic word processor. With no selection we intentionally do nothing.
    // The editor's global default font is owned solely by the settings dialog
    // (editor.fontFamily); the toolbar must never mutate it here, or saving settings
    // would appear to "revert" the font (it was only ever an unpersisted live change
    // on m_appearance, which applyEditorSettingsToAllPanels then overwrote).
}

void BookEditor::setSelectionFontSize(int pointSize)
{
    if (!m_isEditMode || !m_textBuffer) {
        return;
    }

    if (hasSelection()) {
        // Apply to selection
        SelectionRange normRange = m_selection.normalized();
        QTextBlock startBlock = m_textBuffer->findBlockByNumber(static_cast<int>(normRange.start.paragraph));
        QTextBlock endBlock = m_textBuffer->findBlockByNumber(static_cast<int>(normRange.end.paragraph));

        if (!startBlock.isValid() || !endBlock.isValid()) {
            return;
        }

        int startPos = startBlock.position() + normRange.start.offset;
        int endPos = endBlock.position() + normRange.end.offset;

        QTextCursor cursor(m_textBuffer.get());
        cursor.setPosition(startPos);
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);

        QTextCharFormat fmt;
        fmt.setFontPointSize(pointSize);
        cursor.mergeCharFormat(fmt);

        emit contentChanged();
        update();
    }
    // No selection: selection-only control, same as setSelectionFontFamily above.
    // The global default font size is owned by the settings dialog (editor.fontSize).
}

QString BookEditor::currentFontFamily() const
{
    if (!m_isEditMode || !m_textBuffer) {
        return m_appearance.typography.textFont.family();
    }

    // Get font at cursor position
    QTextBlock block = m_textBuffer->findBlockByNumber(static_cast<int>(m_cursorPosition.paragraph));
    if (!block.isValid()) {
        return m_appearance.typography.textFont.family();
    }

    QTextCursor cursor(m_textBuffer.get());
    cursor.setPosition(block.position() + m_cursorPosition.offset);
    QVariant families = cursor.charFormat().fontFamilies();
    if (families.isValid() && families.toStringList().size() > 0) {
        return families.toStringList().first();
    }
    return m_appearance.typography.textFont.family();
}

int BookEditor::currentFontSize() const
{
    if (!m_isEditMode || !m_textBuffer) {
        return m_appearance.typography.textFont.pointSize();
    }

    // Get font at cursor position
    QTextBlock block = m_textBuffer->findBlockByNumber(static_cast<int>(m_cursorPosition.paragraph));
    if (!block.isValid()) {
        return m_appearance.typography.textFont.pointSize();
    }

    QTextCursor cursor(m_textBuffer.get());
    cursor.setPosition(block.position() + m_cursorPosition.offset);
    int size = static_cast<int>(cursor.charFormat().fontPointSize());
    return size > 0 ? size : m_appearance.typography.textFont.pointSize();
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

        // The pipeline recomputes the view (margins, pages, zoom mode, scroll padding) and
        // lays the text out again; scroll anchoring keeps the text at the top of the view,
        // also between the scroll modes and the pages
        if (m_renderPipeline) {
            m_renderPipeline->setConfigViewMode(mode, getZoomModeForViewMode());
        }
        updateScrollBarRange();
        updatePageInfo();

        // Ensure cursor is visible after view mode change
        ensureCursorVisible();
        update();
    }
}

// =============================================================================
// Zoom Control
// =============================================================================

ZoomMode BookEditor::getZoomModeForViewMode() const {
    // Pages zoom as a whole (their line breaks stay); the scroll modes lay the text out
    // again at the zoomed font size, wrapped to the view
    return m_viewMode == ViewMode::Page ? ZoomMode::PageScaling : ZoomMode::FontScaling;
}

double BookEditor::zoomFactor() const {
    if (m_renderPipeline) {
        return m_renderPipeline->zoomFactor();
    }
    return 1.0;
}

void BookEditor::setZoomFactor(double factor) {
    // Zooming keeps the middle of the view on the same text
    applyZoom(factor, QPointF(width() / 2.0, height() / 2.0));
}

void BookEditor::applyZoom(double factor, const QPointF& fixedPoint) {
    if (!m_renderPipeline) {
        return;
    }
    factor = qBound(MIN_ZOOM_FACTOR, factor, MAX_ZOOM_FACTOR);
    const ZoomMode mode = getZoomModeForViewMode();
    const QPointF docPoint = m_renderPipeline->widgetToDocument(fixedPoint);

    // Font scaling wraps only the visible paragraphs before the next paint (scroll
    // anchoring keeps the text at the top of the view), page scaling only changes the
    // painter scale
    m_renderPipeline->setConfigZoom(factor, mode);
    updateScrollBarRange();

    if (mode == ZoomMode::PageScaling) {
        // The document point under fixedPoint stays there
        const RenderContext& ctx = m_renderPipeline->context();
        const double scale = ctx.computed.viewScale;
        m_renderPipeline->setConfigScrollX(
            (ctx.pageMode.pageSpacing + ctx.computed.marginLeft + docPoint.x()) * scale -
            fixedPoint.x());
        setScrollOffset(docPoint.y() - (fixedPoint.y() - ctx.computed.originY) / scale);
        updateHorizontalScrollBar();
    }
    // Typewriter scrolling keeps the cursor line at the focus height instead
    updateTypewriterScroll(false);
    update();
    emit zoomChanged(factor);
}

void BookEditor::zoomIn() {
    setZoomFactor(zoomFactor() * ZOOM_STEP);
}

void BookEditor::zoomOut() {
    setZoomFactor(zoomFactor() / ZOOM_STEP);
}

void BookEditor::zoomReset() {
    setZoomFactor(1.0);
}

// =============================================================================
// Page Navigation (Phase 5.3-5.5)
// =============================================================================

int BookEditor::currentPage() const
{
    // The page holding the cursor, as word processors count it
    if (!m_textBuffer || m_viewMode != ViewMode::Page || !m_renderPipeline) {
        return 0;
    }
    return m_renderPipeline->pageAtDocumentY(getCursorDocumentY()) + 1;
}

int BookEditor::totalPages() const
{
    if (!m_textBuffer || m_viewMode != ViewMode::Page || !m_renderPipeline) {
        return 0;
    }
    return m_renderPipeline->pageCount();
}

void BookEditor::goToPage(int page)
{
    if (!m_textBuffer || m_viewMode != ViewMode::Page || !m_renderPipeline) {
        return;
    }
    if (page < 1 || page > totalPages()) {
        return;
    }

    // Estimated heights above a page shift its text: the blocks down to the page are laid
    // out first, so the page is where its text will stay
    if (auto* layout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())) {
        layout->ensureLaidOutTo(m_renderPipeline->pageTextTop(page - 1));
    }
    page = std::min(page, totalPages());

    // The cursor goes to the first line of the page, and the sheet's top to the top of the
    // view
    const double textTop = m_renderPipeline->pageTextTop(page - 1);
    const int position = m_textBuffer->documentLayout()->hitTest(QPointF(0.0, textTop),
                                                                   Qt::FuzzyHit);
    const QTextBlock block = m_textBuffer->findBlock(std::max(0, position));
    if (block.isValid()) {
        setCursorPosition({block.blockNumber(), std::max(0, position - block.position())});
    }
    // Sheet top at the top of the view (typewriter scrolling has put the cursor line at
    // its focus height instead)
    if (!m_appearance.typewriter.enabled) {
        scrollToPageTop(page);
    }
    updatePageInfo();
    update();
}

void BookEditor::scrollToPageTop(int page)
{
    if (!m_renderPipeline || m_viewMode != ViewMode::Page) {
        return;
    }
    // The sheet's top at the gap below the view's top edge, as the first page shows at the
    // start of the chapter
    const RenderContext& ctx = m_renderPipeline->context();
    setScrollOffset(m_renderPipeline->pageTextTop(page - 1) - ctx.computed.marginTop -
                    ctx.pageMode.pageSpacing + ctx.computed.originY / ctx.computed.viewScale);
}

void BookEditor::zoomToPageWidth()
{
    if (!m_renderPipeline || m_viewMode != ViewMode::Page) {
        return;
    }
    // The page with the gap on both sides fills the width left of the scroll bar
    const RenderContext& ctx = m_renderPipeline->context();
    const double pagesWidth = ctx.computed.pageWidthPixels + 2.0 * ctx.pageMode.pageSpacing;
    if (pagesWidth > 0.0) {
        applyZoom((width() - ctx.scrollBarWidth) / pagesWidth,
                  QPointF(width() / 2.0, height() / 2.0));
    }
}

void BookEditor::zoomToWholePage()
{
    if (!m_renderPipeline || m_viewMode != ViewMode::Page) {
        return;
    }
    // The page with the gaps around it fits the view; the cursor's page is shown
    const RenderContext& ctx = m_renderPipeline->context();
    const double gaps = 2.0 * ctx.pageMode.pageSpacing;
    const double pageWidth = ctx.computed.pageWidthPixels + gaps;
    const double pageHeight = ctx.computed.pageHeightPixels + gaps;
    if (pageWidth <= 0.0 || pageHeight <= 0.0) {
        return;
    }
    const int page = std::max(1, currentPage());
    applyZoom(std::min((width() - ctx.scrollBarWidth) / pageWidth, height() / pageHeight),
              QPointF(width() / 2.0, height() / 2.0));
    if (!m_appearance.typewriter.enabled) {
        scrollToPageTop(page);
    }
}

void BookEditor::updatePageInfo()
{
    const int current = currentPage();
    const int total = totalPages();
    if (total != m_lastTotalPages) {
        m_lastTotalPages = total;
        emit totalPagesChanged(total);
    }
    if (current != m_lastCurrentPage) {
        m_lastCurrentPage = current;
        emit currentPageChanged(current);
    }
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

    // The render pipeline owns the document font: it applies the zoom-scaled effective
    // font as the document's default font (one relayout, only when it changes).
    // Character formats carry only explicit styling - no font is baked into them here,
    // or it would be saved with the chapter and stop following the settings and zoom.
    if (m_renderPipeline) {
        m_renderPipeline->setConfigFont(m_appearance.typography.textFont);
        // Line spacing, paragraph spacing and indent are a view setting of the layout,
        // like the font: never stored in the document or its undo history
        m_renderPipeline->setConfigTypography(layoutTypography(m_appearance.typography));
    }

    // Apply cursor settings
    setCursorBlinkingEnabled(m_appearance.cursor.blinking);
    setCursorBlinkInterval(m_appearance.cursor.blinkInterval);
    if (m_renderPipeline) {
        m_renderPipeline->setCursorStyle(m_appearance.cursor.style);
        m_renderPipeline->setCursorWidth(m_appearance.cursor.lineWidth);
    }

    emit appearanceChanged();

    // Phase 15: granular setters for appearance changes
    if (m_renderPipeline) {
        RenderColors colors;
        colors.text = m_appearance.colors.textColor(m_appearance.colorMode);
        colors.background = m_appearance.colors.background(m_appearance.colorMode);
        colors.cursor = m_appearance.cursor.useCustomColor
            ? m_appearance.cursor.customColor
            : m_appearance.colors.textColor(m_appearance.colorMode);
        colors.selection = m_appearance.colors.selection;
        colors.inactiveText = m_appearance.colors.focusInactiveColor(m_appearance.colorMode);
        m_renderPipeline->setConfigColors(colors);

        // Margins using centralized calculation
        auto margins = calculateEffectiveMargins();
        m_renderPipeline->setConfigMargins(margins.left, margins.top, margins.right, margins.bottom);
        applyPageLayout();
    }

    // The margins and the page set the scroll range (the pipeline gives the viewport the
    // scroll padding)
    updateScrollBarRange();
    applyTypewriter();
    updatePageInfo();
    update();
}

// =============================================================================
// Editor Color Mode (Light/Dark Toggle)
// =============================================================================

void BookEditor::toggleEditorColorMode()
{
    EditorColorMode newMode = (m_appearance.colorMode == EditorColorMode::Light)
        ? EditorColorMode::Dark
        : EditorColorMode::Light;
    setEditorColorMode(newMode);
}

void BookEditor::setEditorColorMode(EditorColorMode mode)
{
    if (m_appearance.colorMode != mode) {
        m_appearance.colorMode = mode;

        auto& logger = core::Logger::getInstance();
        logger.info("BookEditor::setEditorColorMode: {}",
                    mode == EditorColorMode::Light ? "Light" : "Dark");

        emit editorColorModeChanged(mode);
        emit appearanceChanged();

        // Phase 15: granular setter for color change only
        if (m_renderPipeline) {
            RenderColors colors;
            colors.text = m_appearance.colors.textColor(mode);
            colors.background = m_appearance.colors.background(mode);
            colors.cursor = m_appearance.cursor.useCustomColor
                ? m_appearance.cursor.customColor
                : m_appearance.colors.textColor(mode);
            colors.selection = m_appearance.colors.selection;
            colors.inactiveText = m_appearance.colors.focusInactiveColor(mode);
            m_renderPipeline->setConfigColors(colors);
        }

        update();
    }
}

// =============================================================================
// Event Handlers
// =============================================================================

void BookEditor::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // =========================================================================
    // Phase 13.4/13.5: UNIFIED EditorRenderPipeline for ALL view modes
    // Pipeline handles everything: background, pages, text, cursor, selection, overlays
    // =========================================================================
    Q_ASSERT(m_renderPipeline && "RenderPipeline must always exist!");

    // Single render call handles everything for all view modes
    m_renderPipeline->render(&painter, event->rect());

    // Distraction-free mode overlay (not yet migrated to pipeline)
    paintDistractionFreeOverlay(painter);

    event->accept();
}

void BookEditor::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Notify ViewportManager of size changes
    updateViewport();

    // Position FindReplaceBar at top if visible
    if (m_findReplaceBar && m_findReplaceBar->isVisible()) {
        int scrollBarWidth = m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
        m_findReplaceBar->setGeometry(0, 0, width() - scrollBarWidth, m_findReplaceBar->sizeHint().height());
    }

    // The pipeline owns the wrap width; in the scroll modes it follows the viewport width.
    // A new width wraps only the visible paragraphs before the next paint (the others in
    // the background), so it is applied at once, also while the window edge is dragged.
    if (m_renderPipeline) {
        m_renderPipeline->setConfigViewportSize(QSizeF(size()));
    }

    updateScrollBarRange();
    update();
}

void BookEditor::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    // Show the cursor and start blinking from the visible phase
    resetCursorBlink();
}

void BookEditor::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    // Hide the cursor; it does not blink without focus
    if (m_cursorBlinkTimer != nullptr) {
        m_cursorBlinkTimer->stop();
    }
    if (m_renderPipeline) {
        m_renderPipeline->setCursorVisible(false);
    }
    updateCursorArea();
}

void BookEditor::wheelEvent(QWheelEvent* event)
{
    QPoint angleDelta = event->angleDelta();
    if (!angleDelta.isNull()) {
        // Ctrl+scroll = zoom, applied at every notch; pages zoom around the mouse pointer
        if (event->modifiers() & Qt::ControlModifier) {
            if (angleDelta.y() != 0) {
                const qreal zoomDelta = angleDelta.y() > 0 ? ZOOM_STEP : (1.0 / ZOOM_STEP);
                applyZoom(zoomFactor() * zoomDelta, event->position());
            }
            event->accept();
            return;
        }

        // Sideways (a horizontal wheel, or Shift with a vertical one): pages wider than
        // the view
        const int sideways = angleDelta.x() != 0 ? angleDelta.x()
                             : (event->modifiers() & Qt::ShiftModifier) ? angleDelta.y()
                                                                       : 0;
        if (sideways != 0) {
            if (m_renderPipeline && m_renderPipeline->maxScrollX() > 0.0) {
                setHorizontalScrollOffset(m_renderPipeline->context().scrollX -
                                          sideways / 8.0 / 15.0 * WHEEL_PIXELS_PER_NOTCH);
            }
            event->accept();
            return;
        }

        // Standard wheel scroll: 1 notch = 15 degrees, a fixed number of view pixels
        // (document units divide by the view scale: page mode zooms)
        const qreal scale = m_viewportManager ? m_viewportManager->viewScale() : 1.0;
        const qreal delta = -angleDelta.y() / 8.0 / 15.0 * WHEEL_PIXELS_PER_NOTCH / scale;
        setScrollOffset(scrollOffset() + delta);
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
        case Qt::Key_PageDown:
            // One view height in every view mode (Ctrl+PageUp/PageDown would be the page
            // jumps of a word processor); Shift extends the selection
            if (shift && m_selection.isEmpty()) {
                m_selectionAnchor = m_cursorPosition;
            }
            if (event->key() == Qt::Key_PageUp) {
                moveCursorPageUp();
            } else {
                moveCursorPageDown();
            }
            if (shift) {
                extendSelection(m_cursorPosition);
            } else {
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
    if (!m_textBuffer) {
        event->ignore();
        return;
    }

    // Phase 11: Use QTextCursor for IME operations
    if (!m_textBuffer) return;

    // Handle committed text (final input)
    const QString& commitString = event->commitString();
    if (!commitString.isEmpty()) {
        // If we had composition, it's been replaced by commit
        if (m_hasComposition) {
            // The preedit text is already in the document, delete it first
            if (!m_preeditString.isEmpty()) {
                QTextBlock block = m_textBuffer->findBlockByNumber(m_preeditStart.paragraph);
                if (block.isValid()) {
                    QTextCursor cursor(m_textBuffer.get());
                    int startPos = block.position() + m_preeditStart.offset;
                    cursor.setPosition(startPos);
                    cursor.setPosition(startPos + m_preeditString.length(), QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                }
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
            QTextBlock block = m_textBuffer->findBlockByNumber(m_preeditStart.paragraph);
            if (block.isValid()) {
                QTextCursor cursor(m_textBuffer.get());
                int startPos = block.position() + m_preeditStart.offset;
                cursor.setPosition(startPos);
                cursor.setPosition(startPos + m_preeditString.length(), QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
            setCursorPosition(m_preeditStart);
        }
    }

    if (!preeditString.isEmpty()) {
        // Store composition state
        m_preeditStart = m_cursorPosition;
        m_preeditString = preeditString;
        m_hasComposition = true;

        // Insert preedit text using QTextCursor
        QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
        if (block.isValid()) {
            QTextCursor cursor(m_textBuffer.get());
            cursor.setPosition(block.position() + m_cursorPosition.offset);
            cursor.insertText(preeditString);
        }

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
            // Cursor rectangle for IME positioning, from the pipeline that paints the cursor
            const QRectF rect = m_renderPipeline ? m_renderPipeline->cursorRect() : QRectF();
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
            // Phase 11: Return text of current paragraph using QTextBlock
            if (m_textBuffer) {
                QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
                if (block.isValid()) {
                    return block.text();
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

    // Sync to RenderPipeline (Phase 12 fix)
    if (m_renderPipeline) {
        m_renderPipeline->setCursorBlinkState(m_cursorVisible);
    }

    // Only repaint the cursor area instead of the entire widget
    updateCursorArea();
}

// =============================================================================
// Private Methods
// =============================================================================

void BookEditor::setupComponents()
{
    // Phase 11: No m_layoutManager/m_scrollManager - using QTextDocument + ViewportManager

    // Enable focus for keyboard input
    setFocusPolicy(Qt::StrongFocus);

    // An I-beam over the text (an arrow over the selection, see mouseMoveEvent()), which
    // takes dropped text
    setCursor(Qt::IBeamCursor);
    setMouseTracking(true);
    setAcceptDrops(true);

    // Setup scrollbar
    setupScrollBar();

    // Setup cursor blink timer
    setupCursorBlinkTimer();

    // Sync to RenderPipeline (Phase 12 fix): the cursor shows once the editor has focus
    if (m_renderPipeline) {
        m_renderPipeline->setCursorVisible(hasFocus());
        m_renderPipeline->setCursorBlinkState(true);
    }

    updateViewport();
}


void BookEditor::updateViewport()
{
    // Update scroll manager with current viewport dimensions
    m_viewportManager->setViewportSize(size()); // Phase 11: was m_scrollManager->setViewportHeight(static_cast<qreal>(height()));
}

void BookEditor::setupScrollBar()
{
    // Create vertical scrollbar (with an arrow pointer, not the editor's I-beam)
    m_verticalScrollBar = new QScrollBar(Qt::Vertical, this);
    m_verticalScrollBar->setCursor(Qt::ArrowCursor);
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

    // The pages keep clear of the scroll bar over the right edge
    if (m_renderPipeline) {
        m_renderPipeline->setConfigScrollBarWidth(m_verticalScrollBar->sizeHint().width());
    }

    // Horizontal scrollbar: page mode, while the zoomed pages are wider than the view
    m_horizontalScrollBar = new QScrollBar(Qt::Horizontal, this);
    m_horizontalScrollBar->setCursor(Qt::ArrowCursor);
    m_horizontalScrollBar->setRange(0, 0);
    m_horizontalScrollBar->setSingleStep(static_cast<int>(WHEEL_SCROLL_STEP));
    m_horizontalScrollBar->hide();
    connect(m_horizontalScrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        if (!m_updatingScrollBar) {
            setHorizontalScrollOffset(value);
        }
    });

    // Note: Scroll animation is created lazily in startScrollAnimation()
}

void BookEditor::updateScrollBarRange()
{
    if (m_verticalScrollBar == nullptr) {
        return;
    }

    // Scroll range in document units: page mode shows height / zoom of the document
    // Phase 11.10: In view mode, use KmlDocumentModel's height
    double maxOffset = 0.0;
    double pageStep = static_cast<double>(height());
    if (!m_isEditMode && m_documentModel && m_documentModel->paragraphCount() > 0) {
        auto [topPadding, bottomPadding] = getScrollPadding();
        maxOffset = std::max(0.0, m_documentModel->totalHeight() + topPadding + bottomPadding -
                                      static_cast<double>(height()));
    } else if (m_viewportManager) {
        maxOffset = m_viewportManager->maxScrollPosition();
        pageStep = m_viewportManager->visibleDocumentHeight();
    }

    // Update scrollbar without triggering signals
    m_updatingScrollBar = true;
    m_verticalScrollBar->setMaximum(static_cast<int>(std::ceil(maxOffset)));
    m_verticalScrollBar->setPageStep(std::max(1, static_cast<int>(pageStep)));
    m_updatingScrollBar = false;

    // Position scrollbar on right edge (also update position on resize)
    int scrollBarWidth = m_verticalScrollBar->sizeHint().width();
    m_verticalScrollBar->setGeometry(
        width() - scrollBarWidth,
        0,
        scrollBarWidth,
        height()
    );

    // A shorter range (zoom out, a taller window) leaves no space below the end of the text
    if (m_isEditMode && scrollOffset() > maxOffset) {
        setScrollOffset(maxOffset);
    }
    syncScrollBarValue();
    updateHorizontalScrollBar();
}

void BookEditor::updateHorizontalScrollBar()
{
    if (m_horizontalScrollBar == nullptr || !m_renderPipeline) {
        return;
    }

    // Shown while the zoomed pages are wider than the view
    const double maxX = m_renderPipeline->maxScrollX();
    const bool needed = m_viewMode == ViewMode::Page && maxX >= 1.0;
    m_updatingScrollBar = true;
    m_horizontalScrollBar->setRange(0, needed ? static_cast<int>(std::ceil(maxX)) : 0);
    m_horizontalScrollBar->setPageStep(std::max(1, width()));
    m_horizontalScrollBar->setValue(static_cast<int>(std::lround(m_renderPipeline->context().scrollX)));
    m_updatingScrollBar = false;

    const int barHeight = m_horizontalScrollBar->sizeHint().height();
    const int barRight = m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
    m_horizontalScrollBar->setGeometry(0, height() - barHeight, std::max(0, width() - barRight),
                                       barHeight);
    m_horizontalScrollBar->setVisible(needed);
}

void BookEditor::setHorizontalScrollOffset(double x)
{
    if (!m_renderPipeline || m_viewMode != ViewMode::Page) {
        return;
    }
    const double oldX = m_renderPipeline->context().scrollX;
    m_renderPipeline->setConfigScrollX(x);  // clamps it
    if (std::abs(m_renderPipeline->context().scrollX - oldX) > 0.001) {
        updateHorizontalScrollBar();
        update();
    }
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

void BookEditor::syncPipelineState()
{
    // Phase 15: Setup text source then apply initial config
    setupPipelineTextSource();
    if (m_renderPipeline) {
        // Step 1: Set all config values on pipeline
        m_renderPipeline->setViewMode(m_viewMode);
        // Logical DPI: the one Qt converts the font's points with (display scaling is
        // applied on top through the device pixel ratio)
        m_renderPipeline->setScreenDpi(screen() ? screen()->logicalDotsPerInch() : DEFAULT_DPI);
        m_renderPipeline->setViewportSize(QSizeF(width(), height()));
        m_renderPipeline->setZoom(m_appearance.pageLayout.zoomLevel, getZoomModeForViewMode());
        m_renderPipeline->setFont(m_appearance.typography.textFont);
        m_renderPipeline->setConfigTypography(layoutTypography(m_appearance.typography));

        // Set margins using centralized calculation
        auto margins = calculateEffectiveMargins();
        m_renderPipeline->setConfigMargins(margins.left, margins.top, margins.right, margins.bottom);
        applyPageLayout();

        // Step 2: Pipeline computes all derived values and applies them to the text source
        m_renderPipeline->applyInitialConfig();
    }
}

void BookEditor::syncPipelineCursor()
{
    syncSearchOrigin();

    // Lightweight sync - only cursor and selection
    if (!m_renderPipeline) {
        return;
    }

    // Update cursor position and selection
    m_renderPipeline->setCursorPosition(m_cursorPosition);
    // Shown while focused; m_cursorVisible is the blink phase (always on without blinking)
    m_renderPipeline->setCursorVisible(hasFocus());
    m_renderPipeline->setCursorBlinkState(m_cursorVisible);

    if (hasSelection()) {
        m_renderPipeline->setSelection(m_selection);
    } else {
        m_renderPipeline->clearSelection();
    }
}

void BookEditor::applyPageLayout()
{
    if (!m_renderPipeline) {
        return;
    }

    // Page size in points, margins in millimetres. With mirror margins every page has the
    // first page's margins for now: the layout gives all pages one text width.
    const QSizeF sizeMm = m_appearance.pageLayout.pageSizeMm();
    const QSizeF sizePoints(sizeMm.width() * POINTS_PER_INCH / MM_PER_INCH,
                            sizeMm.height() * POINTS_PER_INCH / MM_PER_INCH);
    const PageMarginsConfig& margins = m_appearance.pageMargins;
    m_renderPipeline->setConfigPageLayout(
        sizePoints,
        QMarginsF(margins.effectiveLeft(1), margins.top, margins.effectiveRight(1), margins.bottom),
        m_appearance.pageLayout.pageGap);
    m_renderPipeline->setConfigShowPageNumbers(m_appearance.pageLayout.showPageNumbers);
}

void BookEditor::updatePipelineScroll()
{
    if (!m_renderPipeline) return;
    m_renderPipeline->updateScroll(scrollOffset());
}

RenderMargins BookEditor::calculateEffectiveMargins() const
{
    // The view margins of the scroll modes (pixels); page mode takes the page's margins
    // from the page layout (applyPageLayout())
    return RenderMargins{
        m_appearance.viewMargins.horizontal,
        m_appearance.viewMargins.vertical,
        m_appearance.viewMargins.horizontal,
        m_appearance.viewMargins.vertical
    };
}

std::pair<double, double> BookEditor::getScrollPadding() const
{
    // The room above and below the text (document units): the margins, the gap around the
    // pages and the typewriter room, computed by the pipeline with the view mapping
    if (m_renderPipeline) {
        const auto& computed = m_renderPipeline->context().computed;
        return {computed.scrollPaddingTop, computed.scrollPaddingBottom};
    }
    return {m_appearance.viewMargins.vertical, m_appearance.viewMargins.vertical};
}

void BookEditor::setupPipelineTextSource()
{
    // Phase 14: Set text source ONCE when document changes
    if (!m_renderPipeline) return;

    if (!m_isEditMode && m_documentModel && m_documentModel->paragraphCount() > 0) {
        // View mode: use KmlDocumentModel
        auto* existingSource = dynamic_cast<KmlDocumentModelSource*>(m_renderPipeline->textSource());
        if (!existingSource || existingSource->model() != m_documentModel.get()) {
            m_renderPipeline->setTextSource(
                std::make_unique<KmlDocumentModelSource>(m_documentModel.get()));
        }
    } else if (m_isEditMode && m_textBuffer) {
        // Edit mode: use QTextDocument
        auto* existingSource = dynamic_cast<QTextDocumentSource*>(m_renderPipeline->textSource());
        if (!existingSource || existingSource->document() != m_textBuffer.get()) {
            m_renderPipeline->setTextSource(
                std::make_unique<QTextDocumentSource>(m_textBuffer.get()));
        }
    }
}

void BookEditor::startScrollAnimation(qreal targetOffset, int durationMs)
{
    // Lazily create the animation on first use
    if (m_scrollAnimation == nullptr) {
        // A plain value animation: a QPropertyAnimation needs a target object and
        // asserts in debug Qt builds without one
        m_scrollAnimation = new QVariantAnimation(this);
        m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);

        // Connect animation value changes
        connect(m_scrollAnimation, &QVariantAnimation::valueChanged,
                this, &BookEditor::onScrollAnimationValueChanged);
    }

    // Clamp target to valid range
    targetOffset = qBound(0.0, targetOffset, m_viewportManager->maxScrollPosition());

    // Stop any existing animation
    m_scrollAnimation->stop();

    // Configure animation
    m_scrollAnimation->setDuration(durationMs >= 0 ? durationMs : m_smoothScrollDuration);
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

    // Phase 11.10: Use m_documentModel when not in edit mode
    if (m_isEditMode && m_textBuffer && m_textBuffer->blockCount() > 0) {
        int maxParagraph = m_textBuffer->blockCount() - 1;
        result.paragraph = qBound(0, result.paragraph, maxParagraph);

        int maxOffset = paragraphLength(m_textBuffer.get(), result.paragraph);
        result.offset = qBound(0, result.offset, maxOffset);
        return result;
    }

    // View mode: validate against m_documentModel
    if (m_documentModel && m_documentModel->paragraphCount() > 0) {
        int maxParagraph = static_cast<int>(m_documentModel->paragraphCount()) - 1;
        result.paragraph = qBound(0, result.paragraph, maxParagraph);

        int maxOffset = static_cast<int>(m_documentModel->paragraphLength(static_cast<size_t>(result.paragraph)));
        result.offset = qBound(0, result.offset, maxOffset);
        return result;
    }

    return {0, 0};
}

// Phase 13.5: drawCursor() removed - cursor rendering unified in EditorRenderPipeline::renderCursor()

void BookEditor::setupCursorBlinkTimer()
{
    m_cursorBlinkTimer = new QTimer(this);

    connect(m_cursorBlinkTimer, &QTimer::timeout,
            this, &BookEditor::onCursorBlinkTimeout);

    // The timer runs only while the editor has focus (see focusInEvent/focusOutEvent)
}

// =============================================================================
// Mouse Event Handlers (Phase 3.9/3.10/3.11)
// =============================================================================

void BookEditor::mousePressEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

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
    } else if (!(event->modifiers() & Qt::ShiftModifier) && isOverSelectedText(clickPos)) {
        // A press on the selected text drags it once the mouse moves far enough; released
        // without moving, it places the cursor (mouseReleaseEvent())
        m_textDragPending = true;
        m_textDragStartPos = clickPos;
    } else {
        // Single click - position cursor
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift+click extends selection
            extendSelection(clickPosition);
            update();
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
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

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
            // With mouse tracking this runs on every move: repaint only to show faded UI
            const bool wasFaded = m_uiOpacity < 1.0;
            m_uiOpacity = 1.0;
            startUiFade();
            if (wasFaded) {
                update();
            }
        }
    }

    const QPointF pos = event->position();
    if (!(event->buttons() & Qt::LeftButton)) {
        // An arrow over the selected text, which can be dragged; an I-beam elsewhere
        const Qt::CursorShape shape = isOverSelectedText(pos) ? Qt::ArrowCursor : Qt::IBeamCursor;
        if (cursor().shape() != shape) {
            setCursor(shape);
        }
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (m_textDragPending) {
        if ((pos - m_textDragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_textDragPending = false;
            startTextDrag();
        }
        event->accept();
        return;
    }

    if (!m_isDragging) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // The selection follows the mouse; past the top or bottom edge the view scrolls
    extendMouseSelection(pos);
    ensureCursorVisible();
    updateAutoScroll(pos, false);

    event->accept();
}

void BookEditor::mouseReleaseEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    stopAutoScroll();
    m_isDragging = false;
    if (m_textDragPending) {
        // A click on the selected text without dragging places the cursor there
        m_textDragPending = false;
        clearSelection();
        const CursorPosition position = positionFromPoint(event->position());
        m_selectionAnchor = position;
        setCursorPosition(position);
    }
    event->accept();
}

void BookEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

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
// Drag and drop of text (stage 3)
// =============================================================================

void BookEditor::dragEnterEvent(QDragEnterEvent* event)
{
    if (!m_textBuffer || !canInsertFromMimeData(event->mimeData())) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
}

void BookEditor::dragMoveEvent(QDragMoveEvent* event)
{
    if (!m_textBuffer || !canInsertFromMimeData(event->mimeData())) {
        event->ignore();
        return;
    }

    const QPointF pos = event->position();
    updateAutoScroll(pos, true);

    // The selected text is not moved onto itself
    const CursorPosition position = positionFromPoint(pos);
    if (event->source() == this && event->proposedAction() == Qt::MoveAction &&
        isInSelection(position)) {
        m_renderPipeline->setDropCaret(std::nullopt);
        event->ignore();
        return;
    }

    m_renderPipeline->setDropCaret(position);
    event->acceptProposedAction();
}

void BookEditor::dragLeaveEvent(QDragLeaveEvent* event)
{
    stopAutoScroll();
    m_renderPipeline->setDropCaret(std::nullopt);
    event->accept();
}

void BookEditor::dropEvent(QDropEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    stopAutoScroll();
    m_renderPipeline->setDropCaret(std::nullopt);

    const bool moveSelection = event->source() == this && event->dropAction() == Qt::MoveAction;
    if (!dropMimeData(event->mimeData(), positionFromPoint(event->position()), moveSelection)) {
        event->ignore();
        return;
    }
    setFocus();
    event->accept();
}

bool BookEditor::isOverSelectedText(const QPointF& widgetPos) const
{
    if (!hasSelection() || !m_textBuffer) {
        return false;
    }

    // The character under the point: the layout's exact hit, which misses the space around
    // the text and the gaps between pages
    const QPointF docPoint = m_renderPipeline->widgetToDocument(widgetPos);
    const int hit = m_textBuffer->documentLayout()->hitTest(docPoint, Qt::ExactHit);
    if (hit < 0) {
        return false;
    }
    const QTextBlock block = m_textBuffer->findBlock(hit);
    const CursorPosition position{block.blockNumber(), hit - block.position()};

    const SelectionRange sel = m_selection.normalized();
    return sel.start <= position && position < sel.end;
}

bool BookEditor::isInSelection(const CursorPosition& position) const
{
    const SelectionRange sel = m_selection.normalized();
    return hasSelection() && sel.start <= position && position <= sel.end;
}

void BookEditor::startTextDrag()
{
    std::unique_ptr<QMimeData> mimeData = createMimeDataFromSelection();
    if (!mimeData) {
        return;
    }

    // Stays at the dragged text if the drop target edits the document before it
    const SelectionRange sel = m_selection.normalized();
    QTextCursor dragged = createCursor(m_textBuffer.get(), sel.start, sel.end);

    auto* drag = new QDrag(this);  // Qt deletes it once the drag is over
    drag->setMimeData(mimeData.release());
    m_draggingText = true;
    const Qt::DropAction action = drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::MoveAction);
    m_draggingText = false;
    stopAutoScroll();

    // Moved to another widget or program: the text leaves the editor (dropMimeData()
    // moves it within the editor). The editor's own find bar only takes a copy.
    const auto* target = qobject_cast<QWidget*>(drag->target());
    const bool movedAway =
        action == Qt::MoveAction && target != this && (target == nullptr || !isAncestorOf(target));
    if (movedAway && dragged.hasSelection()) {
        clearSelection();
        dragged.removeSelectedText();
        finishEdit(dragged);
    }
}

void BookEditor::extendMouseSelection(const QPointF& widgetPos)
{
    // Past the top or bottom edge, the selection ends in the first or last visible line;
    // automatic scrolling brings the next lines in
    const double y = std::clamp(widgetPos.y(), 0.0, std::max(0.0, height() - 1.0));
    const CursorPosition position = positionFromPoint(QPointF(widgetPos.x(), y));

    m_cursorPosition = position;
    setSelection({m_selectionAnchor, position});
    syncPipelineCursor();
    update();
}

void BookEditor::updateAutoScroll(const QPointF& widgetPos, bool forDrop)
{
    m_autoScrollPos = widgetPos;
    m_autoScrollForDrop = forDrop;
    if (autoScrollStep() == 0.0) {
        stopAutoScroll();
        return;
    }

    if (m_autoScrollTimer == nullptr) {
        m_autoScrollTimer = new QTimer(this);
        m_autoScrollTimer->setInterval(AUTO_SCROLL_INTERVAL);
        connect(m_autoScrollTimer, &QTimer::timeout, this, &BookEditor::onAutoScrollTimeout);
    }
    if (!m_autoScrollTimer->isActive()) {
        m_autoScrollTimer->start();
    }
}

void BookEditor::stopAutoScroll()
{
    if (m_autoScrollTimer != nullptr) {
        m_autoScrollTimer->stop();
    }
}

double BookEditor::autoScrollStep() const
{
    // Dragged text scrolls the view in a band along the top and bottom edges, a mouse
    // selection past them
    const double band = m_autoScrollForDrop ? AUTO_SCROLL_DROP_BAND : 0.0;
    const double y = m_autoScrollPos.y();
    double distance = 0.0;
    if (y < band) {
        distance = y - band;
    } else if (y > height() - band) {
        distance = y - (height() - band);
    }
    if (distance == 0.0) {
        return 0.0;
    }
    const double step =
        std::clamp(std::abs(distance) / 2.0, AUTO_SCROLL_MIN_STEP, AUTO_SCROLL_MAX_STEP);
    return distance < 0.0 ? -step : step;
}

void BookEditor::onAutoScrollTimeout()
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    // The step is in view pixels (page mode zooms the document)
    const double scale = m_viewportManager ? m_viewportManager->viewScale() : 1.0;
    const double step = autoScrollStep() / scale;
    const qreal before = scrollOffset();
    if (step != 0.0) {
        scrollBy(step, false);
    }
    if (scrollOffset() == before) {
        stopAutoScroll();  // the mouse is back inside, or the view is at the end
        return;
    }

    // The text under the mouse has moved: the selection or the drop caret follows it
    if (m_autoScrollForDrop) {
        const CursorPosition position = positionFromPoint(m_autoScrollPos);
        if (m_draggingText && isInSelection(position)) {
            m_renderPipeline->setDropCaret(std::nullopt);
        } else {
            m_renderPipeline->setDropCaret(position);
        }
    } else if (m_isDragging) {
        extendMouseSelection(m_autoScrollPos);
    }
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

// Phase 13.5: positionFromPointPageMode() removed - hit testing unified in EditorRenderPipeline::positionFromPoint()
// Phase 13.5: drawSelection() removed - selection rendering unified in EditorRenderPipeline::renderSelection()

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
// Selection-aware Cursor Movement (Phase 3.12)
// =============================================================================

void BookEditor::moveCursorLeftWithSelection(bool extend)
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
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
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
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
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
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
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
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
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
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
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
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
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
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
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
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
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
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
        if (!m_appearance.typewriter.enabled) {
            setScrollOffset(0.0);
        }
        extendSelection(m_cursorPosition);
    } else {
        clearSelection();
        moveCursorToDocStart();
    }
}

void BookEditor::moveCursorToDocEndWithSelection(bool extend)
{
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // Set anchor before moving if starting new selection
    if (extend && m_selection.isEmpty()) {
        m_selectionAnchor = m_cursorPosition;
    }

    // Phase 11: Use QTextBlock
    int lastPara = m_textBuffer->blockCount() - 1;
    QTextBlock lastBlock = m_textBuffer->lastBlock();
    int charCount = lastBlock.isValid() ? lastBlock.length() - 1 : 0;
    if (charCount < 0) charCount = 0;
    CursorPosition newPos = {lastPara, charCount};

    if (extend) {
        m_preferredCursorXValid = false;
        setCursorPosition(newPos);
        if (!m_appearance.typewriter.enabled) {
            setScrollOffset(m_viewportManager->maxScrollPosition());
        }
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
    // Phase 11: Use QTextLayout for cursor position
    if (!m_textBuffer) {
        return 0.0;
    }

    QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid()) {
        return 0.0;
    }

    QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
    if (!layout) {
        return 0.0;
    }

    // Use document layout for correct Y position
    QRectF blockRect = m_textBuffer->documentLayout()->blockBoundingRect(block);
    qreal paraY = blockRect.y();

    int offsetInBlock = qMin(m_cursorPosition.offset, block.length() - 1);
    if (offsetInBlock < 0) offsetInBlock = 0;

    QTextLine line = layout->lineForTextPosition(offsetInBlock);
    if (line.isValid()) {
        paraY += line.y();
    }

    return paraY;
}

void BookEditor::updateTypewriterScroll(bool animate)
{
    if (!m_appearance.typewriter.enabled || !m_isEditMode || !m_textBuffer ||
        !m_renderPipeline || !m_viewportManager) {
        return;
    }

    // The middle of the cursor line goes to the focus height of the view. The scroll
    // padding has room for it above the first line and below the last one.
    const double focusY =
        std::clamp(m_appearance.typewriter.focusPosition, 0.0, 1.0) * static_cast<double>(height());
    const auto targetScroll = [&]() -> std::optional<double> {
        const QRectF caret = m_renderPipeline->caretRect(m_cursorPosition);
        if (caret.isNull()) {
            return std::nullopt;
        }
        return std::clamp(
            scrollOffset() + (caret.center().y() - focusY) / m_viewportManager->viewScale(), 0.0,
            m_viewportManager->maxScrollPosition());
    };
    const std::optional<double> target = targetScroll();
    if (!target) {
        return;
    }
    const double distance = std::abs(*target - scrollOffset());
    if (distance < 0.5) {
        return;
    }

    // A short move glides, a jump (search, Ctrl+End) is immediate
    if (animate && m_appearance.typewriter.smoothScroll &&
        distance <= m_viewportManager->visibleDocumentHeight()) {
        startScrollAnimation(*target, m_appearance.typewriter.scrollDuration);
        return;
    }
    stopScrollAnimation();
    setScrollOffset(*target);

    // A jump may land among paragraphs with estimated heights: once the view there is laid
    // out, the line is placed again
    m_renderPipeline->ensureVisibleLaidOut();
    if (const std::optional<double> corrected = targetScroll()) {
        setScrollOffset(*corrected);
    }
}

bool BookEditor::isTypewriterEnabled() const
{
    return m_appearance.typewriter.enabled;
}

void BookEditor::setTypewriterEnabled(bool enabled)
{
    if (m_appearance.typewriter.enabled == enabled) {
        return;
    }
    m_appearance.typewriter.enabled = enabled;
    applyTypewriter();
    emit typewriterChanged(enabled);
}

void BookEditor::applyTypewriter()
{
    if (!m_renderPipeline) {
        return;
    }

    // The room above the text changes with it, and the scroll position by as much, so
    // the text stays where it is on the screen (the pipeline gives the viewport the new
    // scroll padding). Then the cursor line goes to the focus height at once.
    const double oldOrigin = m_renderPipeline->context().computed.originY;
    m_renderPipeline->setConfigTypewriter(m_appearance.typewriter.enabled,
                                          m_appearance.typewriter.focusPosition);
    const auto& computed = m_renderPipeline->context().computed;
    const double scroll = scrollOffset() + (computed.originY - oldOrigin) / computed.viewScale;
    updateScrollBarRange();
    setScrollOffset(scroll);
    updateTypewriterScroll(false);
    update();
}

// Phase 13.5: paintPageMode() removed - rendering now handled by EditorRenderPipeline
// See render() method in editor_render_pipeline.cpp

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
    QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);

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
    // Phase 12.6: Use computed margins (not input)
    const auto& ctx = m_renderPipeline->context();
    double marginTop = ctx.computed.marginTop;

    // Y position of the focused paragraph, from the document layout
    const double focusY = m_viewportManager->paragraphY(
        static_cast<size_t>(std::max(0, focusedRange.startParagraph)));

    // Get focused paragraph height (or line height if Line scope)
    double focusHeight = 0.0;
    double focusTop = focusY;

    if (focusedRange.startParagraph >= 0 &&
        focusedRange.startParagraph < static_cast<int>(m_textBuffer->blockCount())) {

        QTextBlock block = doc->findBlockByNumber(focusedRange.startParagraph);
        QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);

        if (m_appearance.focusMode.scope == FocusModeSettings::FocusScope::Line) {
            // For line scope, calculate specific line bounds
            if (layout && focusedRange.startLine >= 0 && focusedRange.startLine < layout->lineCount()) {
                QTextLine line = layout->lineAt(focusedRange.startLine);
                if (line.isValid()) {
                    const QRectF box = KalahariTextDocumentLayout::lineBox(
                        line, m_renderPipeline->textSource()->lineSpacing());
                    focusTop = focusY + box.top();
                    focusHeight = box.height();
                }
            }
        } else {
            // For paragraph scope, use entire paragraph
            focusHeight = m_viewportManager->paragraphHeight(
                static_cast<size_t>(focusedRange.startParagraph));
        }
    }

    // Calculate screen positions (apply margins and scroll offset)
    int widgetFocusTop = static_cast<int>(marginTop + focusTop - scrollY);
    int widgetFocusBottom = static_cast<int>(marginTop + focusTop + focusHeight - scrollY);

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
        // Use cached margins from ctx - no DPI query needed
        QColor highlightColor = m_appearance.colors.accent;
        highlightColor.setAlpha(25);  // Very subtle

        QRectF focusRect(static_cast<qreal>(ctx.computed.marginLeft),
                         static_cast<qreal>(widgetFocusTop),
                         static_cast<qreal>(width() - ctx.computed.marginLeft),
                         static_cast<qreal>(widgetFocusBottom - widgetFocusTop));
        painter.fillRect(focusRect, highlightColor);
    }
}

// =============================================================================
// Distraction-Free Mode (Phase 5.7)
// =============================================================================

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
        QString countText = tr("%1 words").arg(wordCount());

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
    if (!m_textBuffer) {
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

    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    // For now, stub out comment functionality
    core::Logger::getInstance().debug("BookEditor::insertComment() - not implemented in Phase 11");
    Q_UNUSED(commentText);
    update();
}

void BookEditor::deleteComment(const QString& commentId)
{
    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    core::Logger::getInstance().debug("BookEditor::deleteComment() - not implemented in Phase 11");
    Q_UNUSED(commentId);
}

void BookEditor::editComment(const QString& commentId)
{
    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    core::Logger::getInstance().debug("BookEditor::editComment() - not implemented in Phase 11");
    Q_UNUSED(commentId);
}

QList<KmlComment> BookEditor::commentsInCurrentParagraph() const
{
    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    // For now, return empty list - comment feature requires Phase 12 implementation
    Q_UNUSED(m_cursorPosition);
    return {};
}

void BookEditor::navigateToComment(int paragraphIndex, const QString& commentId)
{
    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    // For now, just move cursor to paragraph start - full comment navigation requires Phase 12
    if (!m_textBuffer) {
        return;
    }

    // Validate paragraph index
    if (paragraphIndex < 0 || paragraphIndex >= m_textBuffer->blockCount()) {
        return;
    }

    // Move cursor to paragraph start (simplified - no comment offset without new storage)
    CursorPosition newPos{paragraphIndex, 0};
    setCursorPosition(newPos);

    ensureCursorVisible();
    emit commentSelected(paragraphIndex, commentId);
    Q_UNUSED(commentId);
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
    if (m_spellCheckService && m_textBuffer) {
        m_spellCheckService->checkDocumentAsync();
    }
}

void BookEditor::onSpellCheckParagraph(int paragraphIndex, const QList<SpellErrorInfo>& errors)
{
    // Phase 11: Store spell errors for rendering
    // Spell errors are now tracked separately and drawn by RenderPipeline
    Q_UNUSED(paragraphIndex);
    Q_UNUSED(errors);
    // TODO: Implement spell error storage for Phase 11 if spell check is needed
    update();
}

void BookEditor::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_textBuffer) {
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

    // Color mode toggle
    menu.addSeparator();
    QString colorModeText = (m_appearance.colorMode == EditorColorMode::Light)
        ? tr("Switch to Dark Mode")
        : tr("Switch to Light Mode");
    menu.addAction(colorModeText, this, &BookEditor::toggleEditorColorMode);

    menu.exec(event->globalPos());
}

std::tuple<QString, int, int> BookEditor::getMisspelledWordAt(int paraIndex, int offset) const
{
    // Phase 11: Spell errors are not currently stored in new architecture
    // TODO: Implement spell error tracking for Phase 11 if needed
    Q_UNUSED(paraIndex);
    Q_UNUSED(offset);
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
    if (!m_textBuffer || paraIndex >= m_textBuffer->blockCount()) {
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

    // Get text for debug log using QTextDocument
    QString replacedText;
    QTextBlock block = m_textBuffer->findBlockByNumber(paraIndex);
    if (block.isValid()) {
        replacedText = block.text().mid(startOffset, endOffset - startOffset);
    }
    core::Logger::getInstance().debug("BookEditor: Replaced '{}' at ({}, {}-{}) with '{}'",
        replacedText.toStdString(),
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
    if (m_grammarCheckService && m_textBuffer) {
        m_grammarCheckService->checkDocumentAsync();
    }
}

void BookEditor::onGrammarCheckParagraph(int paragraphIndex, const QList<GrammarError>& errors)
{
    // Phase 11: Grammar errors are not currently stored in new architecture
    // TODO: Implement grammar error tracking for Phase 11 if needed
    Q_UNUSED(paragraphIndex);
    Q_UNUSED(errors);
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

    // Phase 12.3: Connect search engine to pipeline
    if (m_renderPipeline) {
        m_renderPipeline->setSearchEngine(m_searchEngine.get());
    }

    // Create FindReplaceBar (will be shown when needed), with an arrow pointer over its
    // buttons instead of the editor's I-beam
    m_findReplaceBar = new gui::FindReplaceBar(this);
    m_findReplaceBar->setCursor(Qt::ArrowCursor);
    m_findReplaceBar->setSearchEngine(m_searchEngine.get());
    // Find/Replace performs its edits directly on the document, which QTextDocument's
    // native undo records — no separate undo stack is needed.
    m_findReplaceBar->setUndoStack(nullptr);
    // Phase 11.6: Removed setFormatLayer - not needed (formatting in QTextCharFormat)
    m_findReplaceBar->hide();

    connect(m_findReplaceBar, &gui::FindReplaceBar::navigateToMatch,
            this, &BookEditor::onNavigateToMatch);
    connect(m_findReplaceBar, &gui::FindReplaceBar::textReplaced,
            this, &BookEditor::onTextReplaced);
    connect(m_findReplaceBar, &gui::FindReplaceBar::closed,
            this, &BookEditor::hideFindReplace);
    connect(m_searchEngine.get(), &SearchEngine::matchesChanged,
            this, [this]() { update(); });  // Repaint on match change
    syncSearchOrigin();
}

SearchEngine* BookEditor::searchEngine() const
{
    return m_searchEngine.get();
}

void BookEditor::syncSearchOrigin()
{
    if (!m_searchEngine || !m_textBuffer) {
        return;
    }

    // Find Next goes on from the selection, or from the cursor without one; a selected
    // match is the current one
    const SelectionRange range = hasSelection() ? m_selection.normalized()
                                                : SelectionRange{m_cursorPosition, m_cursorPosition};
    m_searchEngine->setOrigin(
        static_cast<size_t>(editor::calculateAbsolutePosition(m_textBuffer.get(), range.start)),
        static_cast<size_t>(editor::calculateAbsolutePosition(m_textBuffer.get(), range.end)));
}

void BookEditor::takeSearchTextFromSelection()
{
    // The search goes paragraph by paragraph: text across paragraphs would never be found
    if (hasSelection() && m_selection.start.paragraph == m_selection.end.paragraph) {
        m_findReplaceBar->setSearchText(selectedText());
    }
}

void BookEditor::showFind()
{
    if (!m_findReplaceBar) {
        setupFindReplace();
    }

    takeSearchTextFromSelection();

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

    takeSearchTextFromSelection();

    m_findReplaceBar->showFindReplace();

    // Position at top of editor
    int scrollBarWidth = m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
    m_findReplaceBar->setGeometry(0, 0, width() - scrollBarWidth, m_findReplaceBar->sizeHint().height());

    m_findReplaceBar->show();
    m_findReplaceBar->focusSearchInput();
}

void BookEditor::findNext()
{
    // Without a search term, open the bar to type one
    if (!m_searchEngine || !m_searchEngine->isActive()) {
        showFind();
        return;
    }
    auto match = m_searchEngine->nextMatch();
    if (match.isValid()) {
        onNavigateToMatch(match);
    }
}

void BookEditor::findPrevious()
{
    if (!m_searchEngine || !m_searchEngine->isActive()) {
        showFind();
        return;
    }
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

void BookEditor::onTextReplaced()
{
    // The replaced text may be shorter than the selected match the cursor stood on
    clearSelection();
    m_cursorPosition = validateCursorPosition(m_cursorPosition);

    syncPipelineCursor();
    update();
    emit contentChanged();
    emit cursorPositionChanged(m_cursorPosition);
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

    setMarkerInDocument(m_textBuffer.get(), marker);  // native undo records the char-format change

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

    setMarkerInDocument(m_textBuffer.get(), marker);  // native undo records the char-format change

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
            // Remove the first marker at cursor position (native undo records it).
            removeMarkerFromDocument(m_textBuffer.get(), marker.position, marker.length);
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
            // Toggle the TODO completion state directly (native undo records it).
            TextMarker toggled = marker;
            toggled.completed = !toggled.completed;
            setMarkerInDocument(m_textBuffer.get(), toggled);
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
    // Phase 11.10: If in edit mode, use QTextDocument serialization
    if (m_isEditMode && m_textBuffer && m_textBuffer.get()) {
        KmlSerializer serializer;
        return serializer.toKml(m_textBuffer.get());
    }

    // DEAD PATH / SAFETY NET: fromKml() always calls ensureEditMode(), so the editor is
    // in edit mode for its whole lifetime and the branch above serialises losslessly via
    // KmlSerializer. This fallback only runs if that invariant is ever broken — and it is
    // LOSSY (it drops inline bold/italic/font/colour, emitting plain text). It is kept only
    // so a stray save preserves the TEXT rather than wiping the file; the loud error makes
    // the (unexpected) lossy path non-silent instead of quietly corrupting formatting.
    if (m_documentModel && m_documentModel->paragraphCount() > 0) {
        core::Logger::getInstance().error(
            "BookEditor::toKml() called while NOT in edit mode — falling back to a LOSSY "
            "plain-text reconstruction (inline formatting will be dropped). This path should "
            "be unreachable (ensureEditMode keeps edit mode on); investigate if you see this.");

        QString kml;
        kml.reserve(static_cast<int>(m_documentModel->characterCount() * 2));  // Estimate with markup
        for (size_t i = 0; i < m_documentModel->paragraphCount(); ++i) {
            kml += QStringLiteral("<p>");
            kml += m_documentModel->paragraphText(i).toHtmlEscaped();  // text only (lossy, see above)
            kml += QStringLiteral("</p>\n");
        }
        return kml;
    }

    // Return empty string (not null) for empty documents
    return QStringLiteral("");
}

size_t BookEditor::paragraphCount() const
{
    // Phase 11.10: Use m_textBuffer when in edit mode, m_documentModel otherwise
    if (m_isEditMode && m_textBuffer) {
        return static_cast<size_t>(m_textBuffer->blockCount());
    }
    if (m_documentModel) {
        return m_documentModel->paragraphCount();
    }
    return 0;
}

QString BookEditor::paragraphPlainText(size_t index) const
{
    // Phase 11.10: Use m_textBuffer when in edit mode, m_documentModel otherwise
    if (m_isEditMode && m_textBuffer) {
        QTextBlock block = m_textBuffer->findBlockByNumber(static_cast<int>(index));
        return block.isValid() ? block.text() : QString();
    }
    if (m_documentModel && index < m_documentModel->paragraphCount()) {
        return m_documentModel->paragraphText(index);
    }
    return QString();
}

QString BookEditor::plainText() const
{
    // Phase 11.10: Use m_textBuffer when in edit mode, m_documentModel otherwise
    if (m_isEditMode && m_textBuffer) {
        return m_textBuffer->toPlainText();
    }
    if (m_documentModel) {
        return m_documentModel->plainText();
    }
    return QString();
}

size_t BookEditor::characterCount() const
{
    // Phase 11.10: Use m_textBuffer when in edit mode, m_documentModel otherwise
    if (m_isEditMode && m_textBuffer) {
        // QTextDocument::characterCount() includes trailing block separator, subtract 1
        int count = m_textBuffer->characterCount();
        return static_cast<size_t>(std::max(0, count - 1));
    }
    if (m_documentModel) {
        return m_documentModel->characterCount();
    }
    return 0;
}

size_t BookEditor::wordCount() const
{
    if (m_isEditMode && m_textBuffer) {
        return static_cast<size_t>(countDocument(m_textBuffer.get()).words);
    }
    // Fallback for view mode: use cached count from KmlDocumentModel
    if (m_documentModel) {
        return m_documentModel->wordCount();
    }
    return 0;
}

size_t BookEditor::characterCountNoSpaces() const
{
    if (m_isEditMode && m_textBuffer) {
        return static_cast<size_t>(countDocument(m_textBuffer.get()).nonSpaceCharacters);
    }
    // Fallback for view mode: use cached count from KmlDocumentModel
    if (m_documentModel) {
        return m_documentModel->characterCountNoSpaces();
    }
    return 0;
}

QTextDocument* BookEditor::textDocument() const
{
    // Phase 11: Return underlying QTextDocument for accessibility
    return m_textBuffer.get();
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

    // Phase 11.10: Clear edit mode - m_textBuffer created on-demand
    // IMPORTANT: Clear document pointers BEFORE destroying m_textBuffer to avoid dangling pointers
    if (m_viewportManager) {
        m_viewportManager->setDocument(nullptr);
    }
    if (m_searchEngine) {
        m_searchEngine->setDocument(nullptr);
    }
    m_textCursor = QTextCursor();  // Clear cursor before destroying document
    m_isEditMode = false;
    m_textBuffer.reset();

    if (kml.isEmpty()) {
        logger.debug("BookEditor::fromKml - empty KML, clearing content");
        // Phase 11.10: Clear KmlDocumentModel
        if (m_documentModel) {
            m_documentModel->clear();
        }
        m_cursorPosition = {0, 0};
        clearSelection();
        // Always enter edit mode for consistent behavior
        ensureEditMode();
        update();
        emit contentChanged();
        emit documentChanged();
        return;
    }

    logElapsed("Loading into KmlDocumentModel");

    // Phase 11.10: FAST - Load into KmlDocumentModel (no setHtml, no full layout)
    // This just parses the KML and stores paragraphs + format runs
    if (!m_documentModel->loadKml(kml)) {
        logger.error("BookEditor::fromKml - KmlDocumentModel parse error");
        return;
    }

    logElapsed("KmlDocumentModel loaded");

    // Phase 11.10: Reset scroll position for view mode
    m_viewModeScrollOffset = 0.0;

    // Phase 11.10: Configure viewport for initial display
    if (m_viewportManager) {
        m_viewportManager->setViewportSize(size());
        m_viewportManager->setScrollPosition(0.0);
        auto [topPadding, bottomPadding] = getScrollPadding();
        m_viewportManager->setTopScrollPadding(topPadding);
        m_viewportManager->setBottomScrollPadding(bottomPadding);
    }

    // NOTE: Layout of visible paragraphs is deferred to syncPipelineState()
    // This ensures font and lineWidth are properly set before layout happens
    // (syncPipelineState is called from ensureEditMode() below)

    // Note: Document pointers already cleared at start of fromKml()
    // They will be set to m_textBuffer in ensureEditMode()

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);
    logger.info("BookEditor::fromKml - loaded {} paragraphs in {}ms",
        m_documentModel->paragraphCount(), elapsed.count());

    // Reset editor state
    m_cursorPosition = {0, 0};
    clearSelection();

    // Sync to RenderPipeline (Phase 12 fix)
    if (m_renderPipeline) {
        m_renderPipeline->setCursorBlinkState(true);
    }

    logElapsed("Before update/signals");

    // Phase 11.10 FIX: Always enter edit mode immediately for consistent rendering
    // This eliminates the dual view/edit mode system - document is always editable
    // ensureEditMode() builds the document with undo disabled, so the load itself is
    // not undoable (undo starts fresh from the user's first edit).
    ensureEditMode();
    logElapsed("Edit mode initialized");

    // Update scrollbar range for new document
    updateScrollBarRange();

    update();
    emit contentChanged();
    emit documentChanged();

    logElapsed("DONE");
}

// =============================================================================
// Phase 11.10: Edit Mode Conversion
// =============================================================================

void BookEditor::ensureEditMode()
{
    if (m_isEditMode) {
        return;  // Already in edit mode
    }

    auto& logger = core::Logger::getInstance();
    auto startTime = std::chrono::high_resolution_clock::now();
    logger.info("BookEditor::ensureEditMode - converting to edit mode");

    // Create QTextDocument from KmlDocumentModel for editing. Undo stays off while the
    // document is built: the load itself must not be undoable, and recording an undo
    // command for every insertion is a large part of the build cost.
    m_textBuffer = std::make_unique<QTextDocument>();
    m_textBuffer->setUndoRedoEnabled(false);
    m_textBuffer->setDocumentMargin(0);  // Remove default document margins

    // Use custom layout that positions lines at y=0 without Qt's leading gaps
    m_textBuffer->setDocumentLayout(new KalahariTextDocumentLayout(m_textBuffer.get()));

    m_isEditMode = true;

    // The render pipeline applies the (zoom-scaled) font and the wrap width while
    // the document is still empty, so the content below is laid out exactly once.
    syncPipelineState();

    // Paragraphs touched by an edit are counted again on the next statistics query
    connect(m_textBuffer.get(), &QTextDocument::contentsChange,
            this, [doc = m_textBuffer.get()](int from, int, int charsAdded) {
                invalidateParagraphCounts(doc, from, charsAdded);
            });

    // Build QTextDocument from KmlDocumentModel in a single edit block: Qt then reports
    // one change and the layout runs once, at endEditBlock().
    const size_t paraCount = m_documentModel ? m_documentModel->paragraphCount() : 0;
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    if (m_documentModel) {
        appendParagraphs(cursor, *m_documentModel);
    }
    cursor.endEditBlock();
    m_textBuffer->setUndoRedoEnabled(true);

    // The model only carried the parse result - the QTextDocument now holds the content.
    if (m_documentModel) {
        m_documentModel->clear();
    }

    // Initialize QTextCursor for editing operations
    m_textCursor = QTextCursor(m_textBuffer.get());

    // Connect ViewportManager to QTextDocument (the paint lays out the visible blocks,
    // the layout's background pass the others)
    if (m_viewportManager) {
        m_viewportManager->setDocument(m_textBuffer.get());
        auto [topPadding, bottomPadding] = getScrollPadding();
        m_viewportManager->setTopScrollPadding(topPadding);
        m_viewportManager->setBottomScrollPadding(bottomPadding);
    }

    // Connect SearchEngine to QTextDocument
    if (m_searchEngine) {
        m_searchEngine->setDocument(m_textBuffer.get());
    }

    updateViewport();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime);
    logger.info("BookEditor::ensureEditMode - completed in {}ms ({} paragraphs)",
        elapsed.count(), paraCount);

    // Trigger repaint to use RenderPipeline
    update();
}

}  // namespace kalahari::editor
