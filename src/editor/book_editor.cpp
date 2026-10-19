/// @file book_editor.cpp
/// @brief BookEditor implementation (OpenSpec #00042 Phase 3.1-3.5)

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/core/logger.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/text_source_adapter.h>
#include <kalahari/editor/render_context.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/paragraph_data.h>
#include <kalahari/editor/find_replace_bar.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QFocusEvent>
#include <QHelpEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QToolTip>
#include <QVariantAnimation>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace kalahari::editor {

// Default smooth scroll duration in milliseconds
constexpr int DEFAULT_SMOOTH_SCROLL_DURATION = 150;

// Default cursor blink interval in milliseconds
constexpr int DEFAULT_CURSOR_BLINK_INTERVAL = 500;

// The longest text of an annotation its mark's tooltip shows
constexpr qsizetype MARK_TOOLTIP_LENGTH = 600;

// How long the editing pauses before a content as long as the saved one is compared with it
constexpr int MODIFIED_CHECK_DELAY_MS = 150;

// Two typewriter heights closer than this are the same (as for the render pipeline)
constexpr double TYPEWRITER_HEIGHT_EPSILON = 0.0001;

namespace {

/// @brief The colors the render pipeline draws with, on the paper of a mode
RenderColors renderColorsFor(const EditorAppearance& appearance, EditorColorMode mode) {
    RenderColors colors;
    colors.text = appearance.colors.textColor(mode);
    colors.background = appearance.colors.background(mode);
    colors.cursor = appearance.cursor.useCustomColor ? appearance.cursor.customColor
                                                     : appearance.colors.textColor(mode);
    colors.selection = appearance.colors.selection;
    colors.inactiveText = appearance.colors.focusInactiveColor(mode);
    const EditorColors::AnnotationColors& annotations = appearance.colors.annotations(mode);
    colors.annotationComment = annotations.comment;
    colors.annotationTodo = annotations.todo;
    colors.annotationNote = annotations.note;
    const EditorColors::IssueColors& issues = appearance.colors.issues(mode);
    if (issues.spelling.isValid()) {
        colors.spellError = issues.spelling;
    }
    if (issues.grammar.isValid()) {
        colors.grammarWarning = issues.grammar;
    }
    return colors;
}

/// @brief Typography settings as the layout applies them (pixels at 100% zoom)
LayoutTypography layoutTypography(const EditorTypography& typography) {
    LayoutTypography result;
    result.lineSpacing = typography.lineHeight;
    result.paragraphSpacing = typography.paragraphSpacing;
    result.firstLineIndent = typography.firstLineIndent ? typography.indentSize : 0.0;
    return result;
}

/// @brief Add the counts @p part to @p total
void addCounts(core::TextCounts& total, const core::TextCounts& part) {
    total.words += part.words;
    total.characters += part.characters;
    total.nonSpaceCharacters += part.nonSpaceCharacters;
    total.paragraphs += part.paragraphs;
    total.dashes += part.dashes;
}

/// @brief The counts of a paragraph, counted when its cached counts are stale
const core::TextCounts& paragraphCounts(const QTextBlock& block,
                                        const core::WordCountRules& rules) {
    ParagraphData* data = ParagraphData::of(block);
    if (!data->countsValid) {
        data->counts = core::countText(block.text(), rules);
        data->countsValid = true;
    }
    return data->counts;
}

/// @brief Word, character and paragraph counts of the whole document, from the paragraphs'
///        counts
///
/// Document statistics are sums of per-paragraph counts, so after an edit only the
/// paragraphs it touched are counted again. A word never goes on past the end of its
/// paragraph, so the sums are the counts of the whole text.
core::TextCounts countDocument(const QTextDocument* doc, const core::WordCountRules& rules) {
    core::TextCounts total;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        addCounts(total, paragraphCounts(block, rules));
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

    // A content as long as the saved one is compared with it when the editing pauses
    m_modifiedCheck = new QTimer(this);
    m_modifiedCheck->setSingleShot(true);
    m_modifiedCheck->setInterval(MODIFIED_CHECK_DELAY_MS);
    connect(m_modifiedCheck, &QTimer::timeout, this,
            [this]() { setModifiedState(differsFromSaved()); });

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

    // A new text has issues of its own (none until it is checked)
    connect(this, &BookEditor::documentChanged, this, &BookEditor::noteIssuesChanged);

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
    // The checking of the grammar outlives the editor: its requests are dropped
    dropGrammarRequests();

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
    // Typewriter scrolling places the cursor line when it is turned on or its height
    // changes; the other settings (and the switches of the Tools menu, which change settings
    // too) leave the view where it is
    const bool typewriterChanged =
        appearance.typewriter.enabled != m_appearance.typewriter.enabled ||
        std::abs(appearance.typewriter.focusPosition - m_appearance.typewriter.focusPosition) >=
            TYPEWRITER_HEIGHT_EPSILON;
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
        m_renderPipeline->setConfigColors(renderColorsFor(m_appearance, m_appearance.colorMode));
        m_renderPipeline->setConfigFocus(m_appearance.focusMode.enabled);
        m_renderPipeline->setConfigAnnotationMarkScale(m_appearance.annotationMarkScale);
        m_renderPipeline->setConfigTextFrameBorder(m_appearance.textFrameBorder.show,
                                                   m_appearance.textFrameBorder.color,
                                                   m_appearance.textFrameBorder.width);

        // The page's size and margins, in every view
        applyPageLayout();
    }

    // The page's margins set the scroll range (the pipeline gives the viewport the scroll
    // padding)
    updateScrollBarRange();
    if (typewriterChanged) {
        applyTypewriter();
    }
    updatePageInfo();
    applyFirstShowShrink();  // the page's new size, until the first paint
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
            m_renderPipeline->setConfigColors(renderColorsFor(m_appearance, mode));
        }

        update();
    }
}

// =============================================================================
// Event Handlers
// =============================================================================

bool BookEditor::event(QEvent* event)
{
    // The tooltip of an annotation's mark: who made the annotation, in bold, and its text,
    // as it is written
    if (event->type() == QEvent::ToolTip && m_renderPipeline) {
        const auto* help = static_cast<QHelpEvent*>(event);
        const std::optional<AnnotationMark> mark =
            m_renderPipeline->annotationMarkDetailsAt(QPointF(help->pos()));
        QString text = mark && !mark->text.trimmed().isEmpty() ? mark->text : QString();
        const QString author = mark ? mark->author.trimmed() : QString();
        if (text.isEmpty() && author.isEmpty()) {
            QToolTip::hideText();
            event->ignore();
            return true;
        }
        if (text.size() > MARK_TOOLTIP_LENGTH) {
            text = text.left(MARK_TOOLTIP_LENGTH) + QStringLiteral("...");
        }
        QStringList lines;
        if (!author.isEmpty()) {
            lines << QStringLiteral("<b>%1</b>").arg(author.toHtmlEscaped());
        }
        if (!text.isEmpty()) {
            lines << text.toHtmlEscaped();
        }
        QToolTip::showText(help->globalPos(),
                           QStringLiteral("<p style='white-space:pre-wrap'>%1</p>")
                               .arg(lines.join(QStringLiteral("<br>"))),
                           this);
        return true;
    }
    // The rows of the find/replace bar changed (a longer count of the matches goes on in the
    // next row of a narrow bar): it gets the height they need
    if (event->type() == QEvent::LayoutRequest && m_findReplaceBar &&
        !m_findReplaceBar->isHidden()) {
        placeFindReplaceBar();
    }
    return QWidget::event(event);
}

void BookEditor::paintEvent(QPaintEvent* event)
{
    // The zoom and the cursor line are on the screen now: from here on they change only when
    // asked for
    m_shrinkOnFirstShow = false;
    m_cursorLineHeightOnFirstShow = -1.0;

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

    // Position FindReplaceBar at top if open (also in a tab behind the others)
    if (m_findReplaceBar && !m_findReplaceBar->isHidden()) {
        placeFindReplaceBar();
    }

    // The text keeps the page's width in every view: a new size only moves the page in the
    // view, without a relayout
    if (m_renderPipeline) {
        m_renderPipeline->setConfigViewportSize(QSizeF(size()));
    }
    applyFirstShowShrink();  // the view's new width, until the first paint

    updateScrollBarRange();
    applyFirstShowCursorLine();  // the view's new size, until the first paint
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

    // A format given for the text typed next stays at its place: typing there (the text has
    // it, and what is typed after it follows it), moving the cursor or selecting drops it.
    // Text composed with an input method moves the cursor, but is typed there.
    if (!m_hasComposition && (hasSelection() || m_cursorPosition != m_pendingFormatAt)) {
        m_pendingFormat = QTextCharFormat();
    }

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
    // As toPlainText(), which would also turn no-break spaces into spaces and the line
    // breaks in the paragraphs into newlines
    QString text = m_textBuffer->toRawText();
    for (QChar& ch : text) {
        switch (ch.unicode()) {
        case QChar::ParagraphSeparator:
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
    return static_cast<size_t>(textCounts().characters);
}

size_t BookEditor::wordCount() const
{
    return static_cast<size_t>(textCounts().words);
}

size_t BookEditor::characterCountNoSpaces() const
{
    return static_cast<size_t>(textCounts().nonSpaceCharacters);
}

core::TextCounts BookEditor::textCounts() const
{
    return m_textBuffer ? countDocument(m_textBuffer.get(), m_wordCountRules) : core::TextCounts{};
}

core::TextCounts BookEditor::selectionCounts() const
{
    core::TextCounts total;
    if (!m_textBuffer || m_selection.isEmpty()) {
        return total;
    }

    // The paragraphs selected whole have their cached counts; the first and the last one
    // may be selected in part
    const SelectionRange range = m_selection.normalized();
    int number = range.start.paragraph;
    for (QTextBlock block = m_textBuffer->findBlockByNumber(number);
         block.isValid() && number <= range.end.paragraph; block = block.next(), ++number) {
        const int length = block.length() - 1;  // without the paragraph's end
        const int start =
            number == range.start.paragraph ? std::clamp(range.start.offset, 0, length) : 0;
        const int end =
            number == range.end.paragraph ? std::clamp(range.end.offset, 0, length) : length;
        if (start == 0 && end == length) {
            addCounts(total, paragraphCounts(block, m_wordCountRules));
        } else if (end > start) {
            const QString text = block.text();
            addCounts(total,
                      core::countText(QStringView(text).mid(start, end - start), m_wordCountRules));
        }
    }
    return total;
}

void BookEditor::setWordCountRules(const core::WordCountRules& rules)
{
    if (rules == m_wordCountRules) {
        return;
    }
    m_wordCountRules = rules;

    // Every paragraph is counted again, by the new rules, when the counts are asked for
    if (m_textBuffer) {
        for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
            if (ParagraphData* paragraph = ParagraphData::find(block)) {
                paragraph->countsValid = false;
            }
        }
    }
    update();  // the word count of Distraction-Free writing
    emit countsChanged();
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

    // The text gets a new document. The selection goes while the old one is there (the
    // cursor is drawn from it), and the pointers to it are cleared BEFORE it is destroyed,
    // so none is left dangling.
    m_cursorPosition = {0, 0};
    clearSelection();
    if (m_viewportManager) {
        m_viewportManager->setDocument(nullptr);
    }
    if (m_searchEngine) {
        m_searchEngine->setDocument(nullptr);
    }
    m_textCursor = QTextCursor();
    dropGrammarRequests();  // their places are in the old document
    m_textBuffer.reset();
    m_pageMoves.clear();  // Page Up/Down start anew in the new text
    m_pageMoveCursor = {-1, -1};
    m_pendingFormat = QTextCharFormat();  // it was given for the old text

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

    if (m_renderPipeline) {
        m_renderPipeline->setCursorBlinkState(true);
    }

    // The document is built with undo disabled, so the load itself is not undoable
    // (undo starts fresh from the user's first edit)
    createDocument(content);
    startFromSaved(kml);  // what isModified() compares the content with
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
    if (m_readOnly) {
        return;
    }
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
    m_pendingFormat = QTextCharFormat();  // it was given for the old text
    clearSelection();
    m_cursorPosition = validateCursorPosition(m_cursorPosition);

    syncPipelineCursor();
    ensureCursorVisible();
    update();
    emit contentChanged();
    emit cursorPositionChanged(m_cursorPosition);
}

void BookEditor::replaceMarkupWithKml(const QString& kml)
{
    if (m_readOnly) {
        return;
    }
    if (!m_textBuffer || !m_renderPipeline || !m_viewportManager) {
        replaceWithKml(kml);
        return;
    }

    // The characters stay one for one: the cursor and the text at the top of the view keep
    // their places, counted from the start of the text
    const int cursorPlace = calculateAbsolutePosition(m_cursorPosition);
    const CursorPosition top = positionFromPoint(QPointF(width() / 2.0, 0.0));
    const double topLine = m_renderPipeline->caretRect(top).top();
    const int topPlace = calculateAbsolutePosition(top);
    const double scrollX = m_renderPipeline->context().scrollX;

    replaceWithKml(kml);
    setCursorPosition(absoluteToCursorPosition(m_textBuffer.get(), cursorPlace));

    // Both went to the cursor: the view goes back to its text
    stopScrollAnimation();
    placeLineAt(absoluteToCursorPosition(m_textBuffer.get(), topPlace), topLine);
    setHorizontalScrollOffset(scrollX);
    update();
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
    setKeepsAnnotations(*m_textBuffer, m_annotationsAllowed);  // see setAnnotationsAllowed()

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

    // and their spelling is checked again (the waves of the words left as they were stay)
    connect(m_textBuffer.get(), &QTextDocument::contentsChange, this,
            &BookEditor::adjustSpellingToEdit);

    // The content goes in in a single edit block: Qt then reports one change and the
    // layout runs once, at endEditBlock().
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    appendParagraphs(cursor, content);
    cursor.endEditBlock();
    m_textBuffer->setUndoRedoEnabled(true);

    // The grammar of the paragraphs edited is checked when the writer stops typing (the
    // content put in above is not an edit: it is checked at once)
    connect(m_textBuffer.get(), &QTextDocument::contentsChange, this,
            &BookEditor::adjustGrammarToEdit);

    // From now on an edit can make the content differ from the saved one, or bring it back
    connect(m_textBuffer.get(), &QTextDocument::contentsChanged, this,
            &BookEditor::checkModified);

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

    // The new text has the zoom of the settings (syncPipelineState())
    applyFirstShowShrink();

    // Its spelling and grammar are checked from the paragraphs in view
    requestSpellCheck();
    m_grammarEditClock.invalidate();
    requestGrammarCheck();

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
        setUnmodified();  // the empty text is the saved one
    }
}

// =============================================================================
// Saved content
// =============================================================================

bool BookEditor::isModified() const
{
    // A comparison waiting for the editing to pause is made now
    return m_modifiedCheck->isActive() ? differsFromSaved() : m_modified;
}

void BookEditor::setUnmodified()
{
    if (m_textBuffer) {
        startFromSaved(QString());
        m_savedKml = toKml();  // the content now: there is nothing to read
    }
}

void BookEditor::startFromSaved(const QString& kml)
{
    m_textBuffer->setModified(false);  // undoing back to here brings the saved content back
    m_savedSource = kml;
    m_savedKml.reset();
    m_savedLength = m_textBuffer->characterCount();
    m_modifiedCheck->stop();
    setModifiedState(false);
}

void BookEditor::checkModified()
{
    if (m_textBuffer->isModified() && m_textBuffer->characterCount() == m_savedLength) {
        m_modifiedCheck->start();
    } else {
        m_modifiedCheck->stop();
        setModifiedState(m_textBuffer->isModified());
    }
}

bool BookEditor::differsFromSaved() const
{
    if (!m_textBuffer || !m_textBuffer->isModified()) {
        return false;
    }
    return m_textBuffer->characterCount() != m_savedLength || toKml() != savedKml();
}

const QString& BookEditor::savedKml() const
{
    if (!m_savedKml) {
        // Read as fromKml() reads it, and written as toKml() writes the content
        KmlDocumentModel content;
        content.loadKml(m_savedSource);
        QTextDocument document;
        document.setUndoRedoEnabled(false);
        QTextCursor cursor(&document);
        cursor.beginEditBlock();
        appendParagraphs(cursor, content);
        cursor.endEditBlock();
        m_savedKml = KmlSerializer().toKml(&document);
        m_savedSource.clear();
    }
    return *m_savedKml;
}

void BookEditor::setModifiedState(bool modified)
{
    if (modified != m_modified) {
        m_modified = modified;
        emit modifiedChanged(modified);
    }
}

}  // namespace kalahari::editor
