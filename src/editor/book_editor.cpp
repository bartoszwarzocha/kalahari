/// @file book_editor.cpp
/// @brief BookEditor implementation (OpenSpec #00042 Phase 3.1-3.5)

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/core/logger.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/text_source_adapter.h>
#include <kalahari/editor/render_context.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/paragraph_data.h>
#include <kalahari/editor/find_replace_bar.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QFocusEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
#include <algorithm>
#include <chrono>
#include <utility>

namespace kalahari::editor {

// Default smooth scroll duration in milliseconds
constexpr int DEFAULT_SMOOTH_SCROLL_DURATION = 150;

// Default cursor blink interval in milliseconds
constexpr int DEFAULT_CURSOR_BLINK_INTERVAL = 500;

namespace {

/// @brief Typography settings as the layout applies them (pixels at 100% zoom)
LayoutTypography layoutTypography(const EditorTypography& typography) {
    LayoutTypography result;
    result.lineSpacing = typography.lineHeight;
    result.paragraphSpacing = typography.paragraphSpacing;
    result.firstLineIndent = typography.firstLineIndent ? typography.indentSize : 0.0;
    return result;
}

/// @brief Word and character counts of the whole document, from the paragraphs' counts
///
/// Document statistics are sums of per-paragraph counts, so after an edit only the
/// paragraphs it touched are counted again.
core::TextCounts countDocument(const QTextDocument* doc) {
    core::TextCounts total;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        ParagraphData* data = ParagraphData::of(block);
        if (!data->countsValid) {
            data->counts = core::countText(block.text());
            data->countsValid = true;
        }
        total.words += data->counts.words;
        total.nonSpaceCharacters += data->counts.nonSpaceCharacters;
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
        if (ParagraphData* data = ParagraphData::find(block)) {
            data->countsValid = false;
        }
        if (block == last) {
            break;
        }
    }
}

}  // anonymous namespace

// =============================================================================
// Construction / Destruction
// =============================================================================

BookEditor::BookEditor(QWidget* parent)
    : QWidget(parent)
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
    // The QTextDocument is created by fromKml(), or empty by the first edit
    , m_textBuffer(nullptr)
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

    // m_textBuffer and m_textCursor are created by createDocument()

    // Create ViewportManager (initially without document - set in fromKml())
    m_viewportManager = std::make_unique<ViewportManager>(this);
    // Note: setDocument() called in fromKml() after loading

    // The scrollbar range follows the document height: edits and re-wrapping after a font
    // or page change end up here (a zoom sets the range itself).
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
        // In page mode the text below the changed blocks is broken into pages anew: the
        // text at the top stays, the page breaks around it move. The whole view is painted
        // again (a partial repaint, such as the cursor blink, would mix the new layout into
        // the old picture), unless a paint of the whole view is laying out right now.
        if (m_viewMode == ViewMode::Page && !m_paintingWholeView) {
            update();
        }
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
// Appearance (Phase 5.1)
// =============================================================================

const EditorAppearance& BookEditor::appearance() const
{
    return m_appearance;
}

void BookEditor::setAppearance(const EditorAppearance& appearance)
{
    m_appearance = appearance;

    // The render pipeline owns the document font: it applies the font of the settings as
    // the document's default font (one relayout, only when it changes; the zoom scales the
    // painter). Character formats carry only explicit styling - no font is baked into them
    // here, or it would be saved with the chapter and stop following the settings.
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

        // The page's size and margins, in every view
        applyPageLayout();
    }

    // The page's margins set the scroll range (the pipeline gives the viewport the scroll
    // padding)
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
    const QScopedValueRollback<bool> paintingWholeView(m_paintingWholeView,
                                                       event->region().contains(rect()));
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

    // The text keeps the page's width in every view: a new size only moves the page in the
    // view, without a relayout
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
        m_renderPipeline->setConfigZoom(m_appearance.pageLayout.zoomLevel);
        m_renderPipeline->setFont(m_appearance.typography.textFont);
        m_renderPipeline->setConfigTypography(layoutTypography(m_appearance.typography));
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

void BookEditor::updatePipelineScroll()
{
    if (!m_renderPipeline) return;
    m_renderPipeline->updateScroll(scrollOffset());
}

std::pair<double, double> BookEditor::getScrollPadding() const
{
    // The room above and below the text (document units): the page's margins, the gap
    // around the page and the typewriter room below, computed by the pipeline with the
    // view mapping
    if (m_renderPipeline) {
        const auto& computed = m_renderPipeline->context().computed;
        return {computed.scrollPaddingTop, computed.scrollPaddingBottom};
    }
    return {0.0, 0.0};
}

void BookEditor::setupPipelineTextSource()
{
    // Phase 14: Set text source ONCE when document changes
    if (!m_renderPipeline) return;

    if (m_textBuffer) {
        auto* existingSource = dynamic_cast<QTextDocumentSource*>(m_renderPipeline->textSource());
        if (!existingSource || existingSource->document() != m_textBuffer.get()) {
            m_renderPipeline->setTextSource(
                std::make_unique<QTextDocumentSource>(m_textBuffer.get()));
        }
    }
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

QString BookEditor::toKml() const
{
    if (m_textBuffer) {
        KmlSerializer serializer;
        return serializer.toKml(m_textBuffer.get());
    }

    // Return empty string (not null) for empty documents
    return QStringLiteral("");
}

size_t BookEditor::paragraphCount() const
{
    return m_textBuffer ? static_cast<size_t>(m_textBuffer->blockCount()) : 0;
}

QString BookEditor::paragraphPlainText(size_t index) const
{
    if (!m_textBuffer) {
        return QString();
    }
    QTextBlock block = m_textBuffer->findBlockByNumber(static_cast<int>(index));
    return block.isValid() ? block.text() : QString();
}

QString BookEditor::plainText() const
{
    if (!m_textBuffer) {
        return QString();
    }
    // As toPlainText(), which would also turn no-break spaces into spaces
    QString text = m_textBuffer->toRawText();
    for (QChar& ch : text) {
        switch (ch.unicode()) {
        case QChar::ParagraphSeparator:
        case QChar::LineSeparator:
        case 0xFDD0:  // QTextBeginningOfFrame
        case 0xFDD1:  // QTextEndOfFrame
            ch = QLatin1Char('\n');
            break;
        default:
            break;
        }
    }
    return text;
}

size_t BookEditor::characterCount() const
{
    if (!m_textBuffer) {
        return 0;
    }
    // QTextDocument::characterCount() includes trailing block separator, subtract 1
    int count = m_textBuffer->characterCount();
    return static_cast<size_t>(std::max(0, count - 1));
}

size_t BookEditor::wordCount() const
{
    return m_textBuffer ? static_cast<size_t>(countDocument(m_textBuffer.get()).words) : 0;
}

size_t BookEditor::characterCountNoSpaces() const
{
    return m_textBuffer ? static_cast<size_t>(countDocument(m_textBuffer.get()).nonSpaceCharacters) : 0;
}

QTextDocument* BookEditor::textDocument() const
{
    // Phase 11: Return underlying QTextDocument for accessibility
    return m_textBuffer.get();
}

bool BookEditor::fromKml(const QString& kml)
{
    auto& logger = core::Logger::getInstance();
    const auto startTime = std::chrono::high_resolution_clock::now();
    auto logElapsed = [&](const char* step) {
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        logger.info("BookEditor::fromKml [{}ms] {}", ms, step);
    };

    logElapsed("START");

    // The text gets a new document. The pointers to the old one are cleared BEFORE it is
    // destroyed, so none is left dangling.
    if (m_viewportManager) {
        m_viewportManager->setDocument(nullptr);
    }
    if (m_searchEngine) {
        m_searchEngine->setDocument(nullptr);
    }
    m_textCursor = QTextCursor();
    m_textBuffer.reset();
    m_pageMoves.clear();  // Page Up/Down start anew in the new text
    m_pageMoveCursor = {-1, -1};

    // Unreadable KML gives the paragraphs read before the error
    KmlDocumentModel content;
    const bool complete = content.loadKml(kml);
    if (!complete) {
        logger.error("BookEditor::fromKml - unreadable KML, {} paragraphs read before the error",
                     content.paragraphCount());
    }
    logElapsed("KML read");

    // The new text starts at the top
    if (m_viewportManager) {
        m_viewportManager->setViewportSize(size());
        m_viewportManager->setScrollPosition(0.0);
        auto [topPadding, bottomPadding] = getScrollPadding();
        m_viewportManager->setTopScrollPadding(topPadding);
        m_viewportManager->setBottomScrollPadding(bottomPadding);
    }

    m_cursorPosition = {0, 0};
    clearSelection();
    if (m_renderPipeline) {
        m_renderPipeline->setCursorBlinkState(true);
    }

    // The document is built with undo disabled, so the load itself is not undoable
    // (undo starts fresh from the user's first edit)
    createDocument(content);
    updateScrollBarRange();
    logElapsed("Document created");

    update();
    emit contentChanged();
    emit documentChanged();

    logElapsed("DONE");
    return complete;
}

void BookEditor::replaceWithKml(const QString& kml)
{
    KmlDocumentModel content;
    if (!content.loadKml(kml)) {
        core::Logger::getInstance().error(
            "BookEditor::replaceWithKml - unreadable KML, {} paragraphs read before the error",
            content.paragraphCount());
    }
    ensureDocument();

    // The whole text in one undo step. Undoing or redoing it brings back the cursor and
    // selection it was made with: QTextDocument would put the cursor at the end of the
    // text it put back.
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    cursor.select(QTextCursor::Document);
    cursor.removeSelectedText();
    cursor.setBlockCharFormat(QTextCharFormat());  // nothing left of the old first paragraph
    appendParagraphs(cursor, content);
    cursor.endEditBlock();

    m_pageMoves.clear();  // Page Up/Down start anew in the new text
    m_pageMoveCursor = {-1, -1};
    clearSelection();
    m_cursorPosition = validateCursorPosition(m_cursorPosition);

    syncPipelineCursor();
    ensureCursorVisible();
    update();
    emit contentChanged();
    emit cursorPositionChanged(m_cursorPosition);
}

// =============================================================================
// Document
// =============================================================================

void BookEditor::createDocument(const KmlDocumentModel& content)
{
    auto& logger = core::Logger::getInstance();
    const auto startTime = std::chrono::high_resolution_clock::now();

    // Undo stays off while the document is built: the content must not be undoable,
    // and recording an undo command for every insertion is a large part of the build cost.
    m_textBuffer = std::make_unique<QTextDocument>();
    m_textBuffer->setUndoRedoEnabled(false);
    m_textBuffer->setDocumentMargin(0);  // Remove default document margins

    // Use custom layout that positions lines at y=0 without Qt's leading gaps
    m_textBuffer->setDocumentLayout(new KalahariTextDocumentLayout(m_textBuffer.get()));

    // The render pipeline applies the font and the wrap width while the document is
    // still empty, so the content below is laid out exactly once.
    syncPipelineState();

    // Paragraphs touched by an edit are counted again on the next statistics query
    connect(m_textBuffer.get(), &QTextDocument::contentsChange,
            this, [doc = m_textBuffer.get()](int from, int, int charsAdded) {
                invalidateParagraphCounts(doc, from, charsAdded);
            });

    // The content goes in in a single edit block: Qt then reports one change and the
    // layout runs once, at endEditBlock().
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    appendParagraphs(cursor, content);
    cursor.endEditBlock();
    m_textBuffer->setUndoRedoEnabled(true);

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
    logger.info("BookEditor::createDocument - {} paragraphs in {}ms",
        content.paragraphCount(), elapsed.count());

    // Trigger repaint to use RenderPipeline
    update();
}

void BookEditor::ensureDocument()
{
    if (!m_textBuffer) {
        createDocument(KmlDocumentModel{});
    }
}

}  // namespace kalahari::editor
