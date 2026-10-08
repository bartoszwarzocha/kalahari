/// @file book_editor.h
/// @brief BookEditor - Custom text editor widget (OpenSpec #00042 Phase 3.1-3.5)
///
/// BookEditor is the main text editing widget for Kalahari. It renders KML
/// documents using virtual scrolling for efficient handling of large texts.
/// This is the foundation for the writer-focused editing experience.
///
/// Key responsibilities:
/// - Render KML documents with efficient virtual scrolling
/// - Manage layout and scroll state
/// - Provide basic widget infrastructure for text editing
/// - Handle scrollbar and mouse wheel scrolling with smooth animation
/// - Cursor position tracking and blinking cursor rendering

#pragma once

#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/spell_check_service.h>  // For SpellErrorInfo
#include <kalahari/editor/grammar_check_service.h> // For GrammarError, GrammarIssueType (Phase 6.17)
#include <kalahari/editor/view_modes.h>
// Phase 11: New 2-step architecture (OpenSpec #00043)
// KML → QTextDocument (with QTextCharFormat) → Render visible fragment
#include <kalahari/editor/viewport_manager.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/editor_render_pipeline.h>  // Phase 12.3: Unified render pipeline
#include <QWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QList>
#include <memory>
#include <optional>
#include <vector>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QInputMethodEvent;
class QKeyEvent;
class QMouseEvent;
class QVariantAnimation;
class QScreen;
class QScrollBar;
class QTimer;
class QMenu;
class QMimeData;

namespace kalahari::editor {

// Forward declarations
class FindReplaceBar;
class KmlDocumentModel;
class SpellCheckService;
class GrammarCheckService;

/// @brief Custom text editor widget for KML documents
///
/// BookEditor is a QWidget-based text editor designed for rendering and
/// editing KML (Kalahari Markup Language) documents. Uses Qt's QTextDocument
/// as the single source of truth (Phase 11 - 2-step architecture).
///
/// Architecture (OpenSpec #00043 Phase 11):
/// - QTextDocument: Document model with inline formatting (QTextCharFormat)
/// - ViewportManager: Visible paragraph range calculation
/// - RenderEngine: Efficient rendering of visible content
///
/// Usage:
/// @code
/// auto editor = new BookEditor(parentWidget);
/// editor->fromKml(kmlContent);  // Load KML content
/// // Document is now rendered in the widget
/// @endcode
///
/// Thread safety: Not thread-safe. Use from GUI thread only.
class BookEditor : public QWidget {
    Q_OBJECT

public:
    /// @brief Construct a BookEditor widget
    /// @param parent Parent widget (optional)
    explicit BookEditor(QWidget* parent = nullptr);

    /// @brief Destructor
    ~BookEditor() override;

    /// @brief Copy constructor (deleted - QWidget cannot be copied)
    BookEditor(const BookEditor&) = delete;

    /// @brief Copy assignment (deleted - QWidget cannot be copied)
    BookEditor& operator=(const BookEditor&) = delete;

    // =========================================================================
    // Document Management (Phase 11: QTextDocument-based)
    // =========================================================================

    /// @brief Get document content as KML markup
    /// @return KML string representing document state
    QString toKml() const;

    /// @brief Load document content from KML markup
    /// @param kml The KML string to load
    ///
    /// Reads the KML (KmlDocumentModel) into a new QTextDocument, with QTextCharFormat
    /// for formatting and metadata. Resets the cursor position and the undo stack.
    /// KML that is not well-formed gives the text read before the error.
    /// @return False when the KML is not well-formed, so part of it was not read
    bool fromKml(const QString& kml);

    /// @brief Replace the document content with KML markup, as one edit
    /// @param kml The KML string to put in place of the current content
    ///
    /// Unlike fromKml(), the document and its undo history stay: one undo step brings the
    /// previous content back (restoring a snapshot). The cursor keeps its position, within
    /// the new text. KML that is not well-formed gives the text read before the error.
    void replaceWithKml(const QString& kml);

    // =========================================================================
    // Content Access (New Architecture API)
    // =========================================================================

    /// @brief Get the number of paragraphs in the document
    /// @return Paragraph count from QTextDocument
    size_t paragraphCount() const;

    /// @brief Get the plain text of a specific paragraph
    /// @param index Paragraph index (0-based)
    /// @return Plain text of the paragraph, or empty if index out of range
    QString paragraphPlainText(size_t index) const;

    /// @brief Get the full plain text of the document
    /// @return Concatenation of all paragraph texts with newlines (no-break spaces stay)
    QString plainText() const;

    /// @brief Get total character count in the document
    /// @return Character count from QTextDocument
    size_t characterCount() const;

    /// @brief Get total word count in the document
    /// @return Word count as defined by core::countText(). Counts are cached per
    ///         paragraph, so only paragraphs edited since the last call are counted.
    size_t wordCount() const;

    /// @brief Get character count without spaces
    /// @return Non-space character count, cached per paragraph like wordCount()
    size_t characterCountNoSpaces() const;

    /// @brief Get the underlying QTextDocument (read-only for accessibility)
    /// @return Pointer to the QTextDocument, or nullptr if not initialized
    /// @note This is primarily for accessibility interfaces (screen readers)
    QTextDocument* textDocument() const;

    // =========================================================================
    // Scrolling
    // =========================================================================

    /// @brief Get the vertical scrollbar
    /// @return Pointer to the vertical scrollbar
    QScrollBar* verticalScrollBar() const;

    /// @brief Get the current scroll offset
    /// @return Current scroll position in pixels
    qreal scrollOffset() const;

    /// @brief Set the scroll offset
    /// @param offset New scroll position in pixels
    ///
    /// The offset is clamped to valid range [0, maxScrollOffset].
    void setScrollOffset(qreal offset);

    /// @brief Scroll by a delta amount with optional smooth animation
    /// @param delta Amount to scroll (positive = down, negative = up)
    /// @param animated If true, use smooth scrolling animation
    void scrollBy(qreal delta, bool animated = false);

    /// @brief Scroll to a specific offset with optional smooth animation
    /// @param offset Target scroll offset
    /// @param animated If true, use smooth scrolling animation
    void scrollTo(qreal offset, bool animated = false);

    /// @brief Check if smooth scrolling is enabled
    /// @return true if smooth scrolling is enabled
    /// @note Smooth scrolling is disabled by default for stability
    bool isSmoothScrollingEnabled() const;

    /// @brief Enable or disable smooth scrolling
    /// @param enabled true to enable smooth scrolling
    void setSmoothScrollingEnabled(bool enabled);

    /// @brief Get smooth scrolling animation duration
    /// @return Animation duration in milliseconds
    int smoothScrollDuration() const;

    /// @brief Set smooth scrolling animation duration
    /// @param duration Animation duration in milliseconds
    void setSmoothScrollDuration(int duration);

    // =========================================================================
    // Cursor Position (Phase 3.4)
    // =========================================================================

    /// @brief Get the current cursor position
    /// @return The cursor position in the document
    CursorPosition cursorPosition() const;

    /// @brief Set the cursor position
    /// @param position The new cursor position
    ///
    /// The position is validated against the document:
    /// - Paragraph index is clamped to valid range
    /// - Character offset is clamped to paragraph length
    /// Emits cursorPositionChanged if position changes.
    void setCursorPosition(const CursorPosition& position);

    /// @brief Check if the cursor is currently visible (blink state)
    /// @return true if cursor should be drawn
    bool isCursorVisible() const;

    /// @brief Enable or disable cursor blinking
    /// @param enabled true to enable blinking
    void setCursorBlinkingEnabled(bool enabled);

    /// @brief Check if cursor blinking is enabled
    /// @return true if cursor blinking is enabled
    bool isCursorBlinkingEnabled() const;

    /// @brief Get the cursor blink interval
    /// @return Blink interval in milliseconds
    int cursorBlinkInterval() const;

    /// @brief Set the cursor blink interval
    /// @param interval Blink interval in milliseconds
    void setCursorBlinkInterval(int interval);

    /// @brief Force cursor to visible state, restart blink timer and scroll to the cursor
    ///
    /// Call this after any cursor movement to ensure the cursor
    /// is visible immediately after the user action. A page wider than the view scrolls
    /// sideways too.
    void ensureCursorVisible();

    /// @brief Reset cursor blink timer without scrolling
    ///
    /// Used when cursor position is set to the same location (click on same spot).
    /// Resets blink state to visible and restarts timer, but does not adjust scroll.
    void resetCursorBlink();

    // =========================================================================
    // Cursor Navigation (Phase 3.6/3.7/3.8)
    // =========================================================================

    /// @brief Move cursor one character to the left
    ///
    /// If at the start of a paragraph (offset=0) and not the first paragraph,
    /// moves to the end of the previous paragraph.
    void moveCursorLeft();

    /// @brief Move cursor one character to the right
    ///
    /// If at the end of a paragraph and not the last paragraph,
    /// moves to the start of the next paragraph.
    void moveCursorRight();

    /// @brief Move cursor one line up
    ///
    /// Attempts to maintain the same visual X position (column).
    /// If at the first line, moves to paragraph start.
    void moveCursorUp();

    /// @brief Move cursor one line down
    ///
    /// Attempts to maintain the same visual X position (column).
    /// If at the last line, moves to paragraph end.
    void moveCursorDown();

    /// @brief Move cursor to previous word boundary (Ctrl+Left)
    ///
    /// Word boundaries are defined by whitespace and punctuation.
    /// If at paragraph start, moves to end of previous paragraph.
    void moveCursorWordLeft();

    /// @brief Move cursor to next word boundary (Ctrl+Right)
    ///
    /// Word boundaries are defined by whitespace and punctuation.
    /// If at paragraph end, moves to start of next paragraph.
    void moveCursorWordRight();

    /// @brief Move cursor to start of current line (Home)
    void moveCursorToLineStart();

    /// @brief Move cursor to end of current line (End)
    void moveCursorToLineEnd();

    /// @brief Move cursor to document start (Ctrl+Home)
    void moveCursorToDocStart();

    /// @brief Move cursor to document end (Ctrl+End)
    void moveCursorToDocEnd();

    /// @brief Move cursor one view height up (Page Up)
    ///
    /// The view scrolls by the same height, so the cursor keeps its place in the view. In
    /// page mode both move by one page: the previous page shows where this one was, the
    /// cursor on the same line of it.
    void moveCursorPageUp();

    /// @brief Move cursor one view height down (Page Down)
    ///
    /// The view scrolls by the same height, so the cursor keeps its place in the view. In
    /// page mode both move by one page: the next page shows where this one was, the cursor
    /// on the same line of it.
    void moveCursorPageDown();

    // =========================================================================
    // Selection (Phase 3.10/3.12)
    // =========================================================================

    /// @brief Get the current selection
    /// @return The selection range (may be empty)
    SelectionRange selection() const;

    /// @brief Set the selection range
    /// @param range The new selection range
    void setSelection(const SelectionRange& range);

    /// @brief Clear the current selection
    void clearSelection();

    /// @brief Check if there is an active selection
    /// @return true if selection is not empty
    bool hasSelection() const;

    /// @brief Get the selected text
    /// @return The selected text, or empty string if no selection
    QString selectedText() const;

    /// @brief Select all text in the document (Ctrl+A)
    void selectAll();

    // =========================================================================
    // Text Input (Phase 4.1 - 4.4)
    // =========================================================================

    /// @brief Insert text at the current cursor position
    /// @param text The text to insert
    ///
    /// If there is an active selection, it is deleted first (replace behavior).
    /// The cursor moves to the end of the inserted text.
    void insertText(const QString& text);

    /// @brief Delete the currently selected text
    /// @return true if text was deleted, false if no selection
    ///
    /// After deletion, the cursor is positioned at the start of the former selection. The
    /// annotations of the deleted text stay on its place.
    bool deleteSelectedText();

    /// @brief Insert a newline, splitting the paragraph at cursor position
    ///
    /// If there is an active selection, it is deleted first.
    /// The cursor moves to the start of the new paragraph.
    void insertNewline();

    /// @brief Delete character before cursor (Backspace)
    ///
    /// If there is a selection, deletes the selection.
    /// If at paragraph start, merges with previous paragraph.
    /// Otherwise, deletes the character before cursor.
    void deleteBackward();

    /// @brief Delete character after cursor (Delete key)
    ///
    /// If there is a selection, deletes the selection.
    /// If at paragraph end, merges with next paragraph.
    /// Otherwise, deletes the character after cursor.
    void deleteForward();

    // =========================================================================
    // Undo/Redo (Phase 4.8)
    // =========================================================================

    /// @brief Check if undo is available
    /// @return true if there are commands to undo
    bool canUndo() const;

    /// @brief Check if redo is available
    /// @return true if there are commands to redo
    bool canRedo() const;

    /// @brief Undo the last command
    void undo();

    /// @brief Redo the last undone command
    void redo();

    /// @brief Clear the undo stack
    void clearUndoStack();

    // =========================================================================
    // Clipboard (Phase 4.13-4.16)
    // =========================================================================

    /// @brief Copy selected content to clipboard (Ctrl+C)
    ///
    /// Copies selection as KML (native), HTML, and plain text formats.
    /// Does nothing if no selection.
    void copy();

    /// @brief Cut selected content to clipboard (Ctrl+X)
    ///
    /// Copies selection to clipboard and deletes it, as one undo step.
    /// Does nothing if no selection.
    void cut();

    /// @brief Paste content from clipboard (Ctrl+V)
    ///
    /// Inserts the clipboard content at the cursor, replacing the selection, as one undo
    /// step - see insertFromMimeData().
    void paste();

    /// @brief Clipboard content of the selection: KML (native), HTML and plain text
    /// @return New MIME data, or nullptr without a selection
    std::unique_ptr<QMimeData> createMimeDataFromSelection() const;

    /// @brief Insert MIME data at the cursor, replacing the selection, as one undo step
    ///
    /// Kalahari content (KML) keeps its formatting. Paragraphs inserted whole keep their
    /// alignment, while the paragraph the content goes into keeps its own. Text from other
    /// programs takes the formatting of the insertion point.
    /// @param source MIME data to insert (nothing happens for nullptr)
    void insertFromMimeData(const QMimeData* source);

    /// @brief Insert dropped MIME data at a position and select it, as one undo step
    ///
    /// The content is inserted as insertFromMimeData() does. With @p moveSelection the
    /// content is the selected text, which leaves its old place in the same undo step.
    /// @param source MIME data to insert
    /// @param position Drop point
    /// @param moveSelection Whether the selected text moves to the drop point
    /// @return false if nothing was dropped: the content cannot be inserted, or the
    ///         selection would move onto itself
    bool dropMimeData(const QMimeData* source, const CursorPosition& position, bool moveSelection);

    /// @brief Check if paste is available
    /// @return true if clipboard has compatible content
    bool canPaste() const;

    // =========================================================================
    // Formatting (Phase 7.2)
    // =========================================================================

    /// @brief Toggle bold formatting on selection or at cursor
    ///
    /// If text is selected, toggles bold on the selection.
    /// If no selection, toggles bold mode for next typed characters.
    void toggleBold();

    /// @brief Toggle italic formatting on selection or at cursor
    ///
    /// If text is selected, toggles italic on the selection.
    /// If no selection, toggles italic mode for next typed characters.
    void toggleItalic();

    /// @brief Toggle underline formatting on selection or at cursor
    ///
    /// If text is selected, toggles underline on the selection.
    /// If no selection, toggles underline mode for next typed characters.
    void toggleUnderline();

    /// @brief Toggle strikethrough formatting on selection or at cursor
    ///
    /// If text is selected, toggles strikethrough on the selection.
    /// If no selection, toggles strikethrough mode for next typed characters.
    void toggleStrikethrough();

    /// @brief Check if current selection/cursor position has bold formatting
    /// @return true if text at cursor/selection is bold
    bool isBold() const;

    /// @brief Check if current selection/cursor position has italic formatting
    /// @return true if text at cursor/selection is italic
    bool isItalic() const;

    /// @brief Check if current selection/cursor position has underline formatting
    /// @return true if text at cursor/selection is underlined
    bool isUnderline() const;

    /// @brief Check if current selection/cursor position has strikethrough formatting
    /// @return true if text at cursor/selection has strikethrough
    bool isStrikethrough() const;

    // =========================================================================
    // Font Selection (applies to selection if any, otherwise default font)
    // =========================================================================

    /// @brief Set font family for selection or default font if no selection
    /// @param family The font family name
    void setSelectionFontFamily(const QString& family);

    /// @brief Set font size for selection or default font if no selection
    /// @param pointSize The font size in points
    void setSelectionFontSize(int pointSize);

    /// @brief Get font family at current cursor position
    /// @return Font family name at cursor
    QString currentFontFamily() const;

    /// @brief Get font size at current cursor position
    /// @return Font size in points at cursor
    int currentFontSize() const;

    // =========================================================================
    // Paragraph Alignment
    // =========================================================================

    /// @brief Set left alignment on current paragraph
    void setAlignLeft();

    /// @brief Set center alignment on current paragraph
    void setAlignCenter();

    /// @brief Set right alignment on current paragraph
    void setAlignRight();

    /// @brief Set justify alignment on current paragraph
    void setAlignJustify();

    /// @brief Get the alignment the current paragraph is shown with
    /// @return Its own alignment, or DEFAULT_PARAGRAPH_ALIGNMENT without one
    Qt::Alignment currentAlignment() const;

    // =========================================================================
    // Annotations: comments, TODOs and notes
    // =========================================================================

    /// @brief The annotations of the chapter, in text order (by where they end)
    std::vector<AnnotationPlace> annotations() const;

    /// @brief Anchor a new annotation to the selection, or to the cursor's place without one
    ///
    /// One undo step. The selection stays.
    /// @param kind What it is
    /// @param text Its text
    /// @param author Who makes it
    /// @return The new annotation, with its id and the time it was made
    Annotation addAnnotation(AnnotationKind kind, const QString& text, const QString& author);

    /// @brief Give an annotation new data (kind, text, state...), one undo step
    /// @return false when the chapter has no annotation with its id
    bool updateAnnotation(const Annotation& annotation);

    /// @brief Take an annotation off the text, one undo step
    /// @return false when the chapter has no annotation with this id
    bool removeAnnotation(const QString& id);

    /// @brief Go to an annotation: select its fragment, or put the cursor on its place
    /// @return false when the chapter has no annotation with this id
    bool goToAnnotation(const QString& id);

    /// @brief Go to the next TODO not done yet, after the cursor
    /// @return false when there is none after the cursor
    bool goToNextTodo();

    /// @brief Go to the previous TODO not done yet, before the cursor
    /// @return false when there is none before the cursor
    bool goToPreviousTodo();

    // =========================================================================
    // View Mode (Phase 5.1)
    // =========================================================================

    /// @brief Get the current view mode
    /// @return Current view mode
    ViewMode viewMode() const;

    /// @brief Set the view mode
    /// @param mode The new view mode
    ///
    /// Every view has the page's width, so the line breaks stay; the cursor line keeps its
    /// place on the screen while it is in the view. Emits viewModeChanged if mode changes.
    /// Triggers repaint.
    void setViewMode(ViewMode mode);

    /// @brief Whether typewriter scrolling is on (in any view mode)
    bool isTypewriterEnabled() const;

    /// @brief Turn typewriter scrolling on or off
    ///
    /// While it is on, the line with the cursor stays at the focus height of the view
    /// (appearance().typewriter.focusPosition) as the text is typed or the cursor moved
    /// with the keyboard. The text still starts at the top of the view: the first lines
    /// stay above the focus height. Mouse clicks and manual scrolling leave the view where
    /// it is.
    /// Emits typewriterChanged if the state changes.
    void setTypewriterEnabled(bool enabled);

    /// @brief Whether Focus is on (in any view mode)
    bool isFocusModeEnabled() const;

    /// @brief Turn Focus on or off
    ///
    /// While it is on, every paragraph but the one with the cursor is dimmed, and the
    /// bright paragraph follows the cursor. The view mode, its pages and the layout stay
    /// as they are.
    /// Emits focusModeChanged if the state changes.
    void setFocusModeEnabled(bool enabled);

    /// @brief Whether Distraction-Free writing is on (in any view mode)
    bool isDistractionFree() const;

    /// @brief Turn Distraction-Free writing on or off
    ///
    /// A toggle on top of the view mode, like Focus: the view mode, its pages and the
    /// layout stay as they are. While it is on, the scroll bars are hidden, the sides of
    /// the view darken toward its edges, and the word count, the hint and the clock (if
    /// appearance() shows it) appear at the edges of the view: at first and whenever the
    /// mouse comes near an edge, fading out after appearance().distractionFree.uiFadeTimeout.
    /// The window around the editor hides its own parts.
    /// Emits distractionFreeModeChanged if the state changes.
    /// @param enabled true to turn it on
    /// @param hint A line shown at the top of the view (e.g. how to leave); empty for none
    void setDistractionFree(bool enabled, const QString& hint = QString());

    // =======================================================================
    // Zoom Control
    // =======================================================================

    /// @brief Get current zoom factor
    /// @return Zoom factor (1.0 = 100%)
    double zoomFactor() const;

    /// @brief Set zoom factor
    /// @param factor Zoom factor (1.0 = 100%, range 0.25-4.0)
    void setZoomFactor(double factor);

    /// @brief Widget pixels per layout pixel at zoom 100%, in every view. With the screen's
    ///        paperScaleOf(), 100% shows the page at its size on paper; 1 (the default)
    ///        gives the size of the system's display scaling.
    void setPaperScale(double scale);

    /// @brief Widget pixels per layout pixel at zoom 100% (setPaperScale())
    double paperScale() const;

    /// @brief The paper scale of a screen: its physical DPI over its logical DPI (the one
    ///        the text is laid out with), or 1 where its reported size gives no likely ratio
    static double paperScaleOf(const QScreen* screen);

    /// @brief The paper scale for a physical and a logical DPI (see paperScaleOf())
    static double paperScaleFor(double physicalDpi, double logicalDpi);

    /// @brief Zoom the page (the pages, or the endless page of the continuous views) to
    ///        fill the width of the view
    void zoomToPageWidth();

    /// @brief Zoom so that a whole page fits the view: the Page Layout view shows the
    ///        cursor's page, the continuous views take the same zoom
    void zoomToWholePage();

    /// @brief Zoom in by one step (+10%)
    void zoomIn();

    /// @brief Zoom out by one step (-10%)
    void zoomOut();

    /// @brief Reset zoom to 100%
    void zoomReset();

    // =========================================================================
    // Page Navigation (Phase 5.3-5.5)
    // =========================================================================

    /// @brief Get the current page number (1-based)
    /// @return Current page number, or 0 if no document
    int currentPage() const;

    /// @brief Get the total number of pages
    /// @return Total page count, or 0 if no document
    int totalPages() const;

    /// @brief Navigate to a specific page
    /// @param page The page number (1-based)
    void goToPage(int page);

    /// @brief Navigate to the next page
    void nextPage();

    /// @brief Navigate to the previous page
    void previousPage();

    // =========================================================================
    // Appearance (Phase 5.1)
    // =========================================================================

    /// @brief Get the current appearance settings
    /// @return Current editor appearance configuration
    const EditorAppearance& appearance() const;

    // =========================================================================
    // Spell Check Integration (Phase 6.9)
    // =========================================================================

    /// @brief Set the spell check service to use
    /// @param service Pointer to SpellCheckService (not owned, must outlive editor)
    ///
    /// Connects the service's paragraphChecked signal to update paragraph layouts
    /// with spell error underlines. Pass nullptr to disable spell checking.
    void setSpellCheckService(SpellCheckService* service);

    /// @brief Get the current spell check service
    /// @return Pointer to the service, or nullptr if not set
    SpellCheckService* spellCheckService() const;

    /// @brief Request spell check for entire document
    ///
    /// Triggers asynchronous spell checking of all paragraphs.
    /// Results are received via paragraphChecked signal and rendered automatically.
    void requestSpellCheck();

    // =========================================================================
    // Grammar Check Integration (Phase 6.17)
    // =========================================================================

    /// @brief Set the grammar check service to use
    /// @param service Pointer to GrammarCheckService (not owned, must outlive editor)
    ///
    /// Connects the service's paragraphChecked signal to update paragraph layouts
    /// with grammar error underlines. Pass nullptr to disable grammar checking.
    void setGrammarCheckService(GrammarCheckService* service);

    /// @brief Get the current grammar check service
    /// @return Pointer to the service, or nullptr if not set
    GrammarCheckService* grammarCheckService() const;

    /// @brief Request grammar check for entire document
    ///
    /// Triggers asynchronous grammar checking of all paragraphs.
    /// Results are received via paragraphChecked signal and rendered automatically.
    void requestGrammarCheck();

    // =========================================================================
    // Reading Aloud
    // =========================================================================

    /// @brief Highlight the word being read aloud
    /// @param paragraph Paragraph of the word
    /// @param offset First character of the word in the paragraph
    /// @param length Length of the word; 0 clears the highlight
    void setSpokenWord(int paragraph, int offset, int length);

    // =========================================================================
    // Find/Replace (Phase 9.4-9.6)
    // =========================================================================

    /// @brief Get the search engine for find/replace operations
    /// @return Pointer to the SearchEngine
    SearchEngine* searchEngine() const;

    /// @brief Show the find bar (find-only mode)
    ///
    /// Text selected within one paragraph becomes the search text.
    void showFind();

    /// @brief Show the find/replace bar
    ///
    /// Text selected within one paragraph becomes the search text.
    void showFindReplace();

    /// @brief Navigate to the next search match
    void findNext();

    /// @brief Navigate to the previous search match
    void findPrevious();

    /// @brief Hide the find/replace bar and clear search highlights
    void hideFindReplace();

    /// @brief Set the appearance settings
    /// @param appearance The new appearance configuration
    ///
    /// Emits appearanceChanged if appearance changes. Triggers repaint.
    void setAppearance(const EditorAppearance& appearance);

    /// @brief Toggle editor color mode between light and dark
    ///
    /// Switches the editor's color mode independently from the application theme.
    /// Emits editorColorModeChanged signal.
    void toggleEditorColorMode();

    /// @brief Set editor color mode
    /// @param mode The color mode to set (Light or Dark)
    void setEditorColorMode(EditorColorMode mode);

    /// @brief Get current editor color mode
    /// @return Current color mode
    EditorColorMode editorColorMode() const { return m_appearance.colorMode; }

    // =========================================================================
    // Size Hints
    // =========================================================================

    /// @brief Get minimum size hint
    /// @return Minimum recommended size for the widget
    ///
    /// Returns a reasonable minimum size that allows basic text display
    /// (approximately 200x100 pixels).
    QSize minimumSizeHint() const override;

    /// @brief Get preferred size hint
    /// @return Preferred size for the widget
    ///
    /// Returns a comfortable editing size (approximately 600x400 pixels).
    QSize sizeHint() const override;

signals:
    /// @brief Emitted when the document content changes
    ///
    /// This signal is emitted whenever text is inserted, deleted, or modified.
    /// Use this to track content changes (e.g., for statistics, auto-save).
    void contentChanged();

    /// @brief Emitted when the document reference changes
    ///
    /// This signal is emitted whenever the document reference changes
    /// (not when document content changes).
    void documentChanged();

    /// @brief Emitted when scroll offset changes
    /// @param offset New scroll offset in pixels
    void scrollOffsetChanged(qreal offset);

    /// @brief Emitted when cursor position changes
    /// @param position The new cursor position
    void cursorPositionChanged(const CursorPosition& position);

    /// @brief Emitted when selection changes
    void selectionChanged();

    /// @brief Emitted when view mode changes
    /// @param mode The new view mode
    void viewModeChanged(ViewMode mode);

    /// @brief Emitted when typewriter scrolling is turned on or off
    void typewriterChanged(bool enabled);

    /// @brief Emitted when Focus is turned on or off
    void focusModeChanged(bool enabled);

    /// @brief Emitted when zoom factor changes
    void zoomChanged(double factor);

    /// @brief Emitted when appearance settings change
    void appearanceChanged();

    /// @brief Emitted when editor color mode changes (light/dark toggle)
    /// @param mode The new color mode
    void editorColorModeChanged(EditorColorMode mode);

    /// @brief Emitted when the current page changes (Page Mode)
    /// @param page The new current page number (1-based)
    void currentPageChanged(int page);

    /// @brief Emitted when the total page count changes (Page Mode)
    /// @param pages The new total page count
    void totalPagesChanged(int pages);

    /// @brief Emitted when Distraction-Free writing is turned on or off
    /// @param enabled true if it is now on
    void distractionFreeModeChanged(bool enabled);

    /// @brief Emitted when a paragraph is modified (text inserted/deleted)
    /// @param paragraphIndex Index of the modified paragraph
    void paragraphModified(int paragraphIndex);

    /// @brief Emitted when a new paragraph is inserted (after newline)
    /// @param paragraphIndex Index of the newly inserted paragraph
    void paragraphInserted(int paragraphIndex);

    /// @brief Emitted when a paragraph is removed (merged with adjacent)
    /// @param paragraphIndex Index of the removed paragraph
    void paragraphRemoved(int paragraphIndex);

protected:
    // =========================================================================
    // Event Handlers
    // =========================================================================

    /// @brief Paint event handler
    /// @param event The paint event
    ///
    /// The render pipeline draws the view (background, pages, text, cursor, selection and
    /// highlights); the distraction-free overlay is drawn on top.
    void paintEvent(QPaintEvent* event) override;

    /// @brief Resize event handler
    /// @param event The resize event
    ///
    /// Updates the layout width and viewport height when the widget resizes.
    void resizeEvent(QResizeEvent* event) override;

    /// @brief Focus in event handler
    /// @param event The focus event
    ///
    /// Triggers repaint to show cursor when editor gains focus.
    void focusInEvent(QFocusEvent* event) override;

    /// @brief Focus out event handler
    /// @param event The focus event
    ///
    /// Triggers repaint to hide cursor when editor loses focus.
    void focusOutEvent(QFocusEvent* event) override;

    /// @brief Mouse wheel event handler
    /// @param event The wheel event
    ///
    /// Handles mouse wheel scrolling. Uses smooth scrolling if enabled.
    void wheelEvent(QWheelEvent* event) override;

    /// @brief Key press event handler
    /// @param event The key event
    ///
    /// Handles keyboard navigation (arrow keys, Home, End, Page Up/Down).
    void keyPressEvent(QKeyEvent* event) override;

    /// @brief Mouse press event handler (Phase 3.9/3.11)
    /// @param event The mouse event
    ///
    /// Handles click to position cursor, double-click to select word,
    /// triple-click to select paragraph. A press on the selected text may start
    /// dragging it (see mouseMoveEvent()).
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Mouse move event handler (Phase 3.10)
    /// @param event The mouse event
    ///
    /// Extends the selection while the button is held, scrolling when the mouse is
    /// beyond the top or bottom edge; starts dragging the selected text once the mouse
    /// moves far enough from a press on it. Without a button, shows an arrow over the
    /// selected text and an I-beam elsewhere.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Mouse release event handler (Phase 3.10)
    /// @param event The mouse event
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// @brief Mouse double-click event handler (Phase 3.11)
    /// @param event The mouse event
    ///
    /// Handles double-click to select word.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /// @brief Accepts dragged content the editor can insert (Kalahari text or plain text)
    void dragEnterEvent(QDragEnterEvent* event) override;

    /// @brief Shows where dragged text would land; scrolls near the top and bottom edges
    void dragMoveEvent(QDragMoveEvent* event) override;

    /// @brief Hides the drop caret when the drag leaves the editor
    void dragLeaveEvent(QDragLeaveEvent* event) override;

    /// @brief Inserts dropped text at the drop point and selects it
    ///
    /// Text moved within the editor leaves its old place in the same undo step.
    void dropEvent(QDropEvent* event) override;

    /// @brief Context menu event handler (Phase 6.9)
    /// @param event The context menu event
    ///
    /// Shows context menu with spell check suggestions if over a misspelled word,
    /// otherwise shows default editing menu.
    void contextMenuEvent(QContextMenuEvent* event) override;

    /// @brief Input method event handler (Phase 4.5/4.6)
    /// @param event The input method event
    ///
    /// Handles IME composition and commit for CJK input.
    void inputMethodEvent(QInputMethodEvent* event) override;

public:
    /// @brief Input method query handler (Phase 4.7)
    /// @param query The query type
    /// @return Value for the queried property
    ///
    /// Provides information to IME about cursor position, selection, etc.
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private slots:
    /// @brief Handle scrollbar value change
    /// @param value New scrollbar value
    void onScrollBarValueChanged(int value);

    /// @brief Handle spell check results for a paragraph (Phase 6.9)
    /// @param paragraphIndex The paragraph index
    /// @param errors List of spelling errors found
    void onSpellCheckParagraph(int paragraphIndex, const QList<SpellErrorInfo>& errors);

    /// @brief Handle grammar check results for a paragraph (Phase 6.17)
    /// @param paragraphIndex The paragraph index
    /// @param errors List of grammar errors found
    void onGrammarCheckParagraph(int paragraphIndex, const QList<GrammarError>& errors);

    /// @brief Handle scroll animation value change
    /// @param value Current animation value
    void onScrollAnimationValueChanged(const QVariant& value);

    /// @brief Handle cursor blink timer timeout
    void onCursorBlinkTimeout();

private:
    /// @brief Setup internal components
    void setupComponents();

    /// @brief Put the editor's cursor where an edit of the document ended, show it, and
    /// report the change
    void finishEdit(const QTextCursor& cursor);

    /// @brief Remove the selected text
    /// @param keepAnnotations true: its annotations stay on its place; false: they go with
    ///        it (to the clipboard)
    /// @return false without a selection
    bool removeSelection(bool keepAnnotations);

    /// @brief Align the paragraph at the cursor, or the selected ones, as one undo step
    void setParagraphAlignment(Qt::Alignment alignment);

    /// @brief Put back the cursor and selection a paragraph format step just undone or
    /// redone was made with
    void restoreStepCursor();

    /// @brief Update scroll manager viewport from widget size
    void updateViewport();

    /// @brief Setup the vertical scrollbar
    void setupScrollBar();

    /// @brief Update scrollbar range based on content height
    void updateScrollBarRange();

    /// @brief Show the horizontal scrollbar while the zoomed pages are wider than the view
    void updateHorizontalScrollBar();

    /// @brief Scroll the pages sideways (page mode; clamped to the pipeline's range)
    void setHorizontalScrollOffset(double x);

    /// @brief Move the cursor and the view by about one view height (-1 up, 1 down), the
    ///        cursor's line staying in its row of the view; in page mode by one page, the
    ///        cursor to the same line of the next (previous) page
    void moveCursorByViewHeight(double direction);

    /// @brief Zoom to a factor, keeping the document point under a widget point in place
    void applyZoom(double factor, const QPointF& fixedPoint);

    /// @brief Give the pipeline the page size, margins, gap and page numbers
    void applyPageLayout();

    /// @brief Give the pipeline the typewriter state, keeping the text in place on the
    ///        screen, and put the cursor line at the focus height when it is on
    void applyTypewriter();

    /// @brief Emit currentPageChanged / totalPagesChanged when the numbers change
    void updatePageInfo();

    /// @brief Scroll so that a page's sheet starts at the top of the view (1-based page)
    void scrollToPageTop(int page);

    /// @brief Sync scrollbar value with scroll manager (without triggering signals)
    void syncScrollBarValue();

    /// @brief Sync pipeline state from BookEditor (Phase 12.3)
    ///
    /// Updates the render pipeline with current BookEditor state:
    /// - Text source (the QTextDocument)
    /// - Scroll position
    /// - Viewport size
    /// - Cursor position and selection
    /// - Appearance settings (colors, font, view mode)
    void syncPipelineState();
    void syncPipelineCursor();  ///< Lightweight cursor-only sync

    /// @brief Setup text source when document changes
    /// Call this ONCE when a new document is created
    void setupPipelineTextSource();

    /// @brief Update only scroll position (lightweight)
    void updatePipelineScroll();

    /// @brief Scroll room above and below the text, in document units
    /// @return {topPadding, bottomPadding}, as computed by the render pipeline
    std::pair<double, double> getScrollPadding() const;

    /// @brief Start smooth scroll animation to target offset
    /// @param targetOffset Target scroll offset
    /// @param durationMs Animation length; the smooth scrolling duration when negative
    void startScrollAnimation(qreal targetOffset, int durationMs = -1);

    /// @brief Stop any running scroll animation
    void stopScrollAnimation();

    /// @brief Scroll the cursor line fully into the view, off its top and bottom edges
    ///
    /// The first line scrolls to the top of the document, showing the page's top margin.
    void scrollToCursorLine();

    /// @brief Scroll a page wider than the view sideways to show the cursor
    void scrollSidewaysToCursor();

    /// @brief Scroll the line with the cursor to the typewriter focus height
    /// @param animate Animate a short scroll (when smooth typewriter scrolling is on); a
    ///        jump of more than a view height is never animated
    void updateTypewriterScroll(bool animate = true);

    /// @brief Get the Y coordinate of the cursor in document coordinates
    /// @return The Y position of the cursor line in the document
    qreal getCursorDocumentY() const;

    /// @brief Validate and clamp cursor position to valid range
    /// @param position The position to validate
    /// @return Validated position within document bounds
    CursorPosition validateCursorPosition(const CursorPosition& position) const;

    /// @brief Repaint the area of the text cursor (the whole widget in Page Mode)
    void updateCursorArea();

    /// @brief Setup cursor blink timer
    void setupCursorBlinkTimer();

    /// @brief Convert widget point to cursor position
    /// @param widgetPos Point in widget coordinates
    /// @return Cursor position using QTextDocument block lookup
    ///
    /// Uses QTextDocument and ViewportManager for efficient position calculation.
    /// Page Mode delegates to EditorRenderPipeline::positionFromPoint() (Phase 13.5)
    CursorPosition positionFromPoint(const QPointF& widgetPos) const;

    // Drag and drop of text (stage 3)

    /// @brief Whether a widget point is over the selected text (a press there may drag it)
    bool isOverSelectedText(const QPointF& widgetPos) const;

    /// @brief Whether a position lies in the selection, its ends included
    bool isInSelection(const CursorPosition& position) const;

    /// @brief Whether insertFromMimeData() can insert the content
    static bool canInsertFromMimeData(const QMimeData* source);

    /// @brief Drag the selected text (copy or move)
    ///
    /// Text moved to another widget or program is removed here once dropped; a move
    /// within the editor is done by dropEvent().
    void startTextDrag();

    /// @brief Extend the mouse selection to a point (past the top or bottom edge: to the
    /// first or last visible line)
    void extendMouseSelection(const QPointF& widgetPos);

    /// @brief Scroll while the mouse is beyond the top or bottom edge during selection,
    /// or near it while text is dragged over the editor
    void updateAutoScroll(const QPointF& widgetPos, bool forDrop);

    /// @brief Stop scrolling started by updateAutoScroll()
    void stopAutoScroll();

    /// @brief Pixels to scroll at the last mouse position (negative: up; 0: none)
    double autoScrollStep() const;

    /// @brief Scroll one step and follow the mouse with the selection or the drop caret
    void onAutoScrollTimeout();

    /// @brief Update paragraph layouts with current selection state
    void updateSelectionInLayouts();

    /// @brief Select word at cursor position (Phase 3.11)
    void selectWordAtCursor();

    /// @brief Select paragraph at cursor position (Phase 3.11)
    void selectParagraphAtCursor();

    /// @brief Find word boundaries at given position
    /// @param paraIndex Paragraph index
    /// @param offset Character offset within paragraph
    /// @return Pair of (start, end) offsets for the word
    std::pair<int, int> findWordBoundaries(int paraIndex, int offset) const;

    /// @brief Extend selection with shift key (Phase 3.12)
    /// @param newCursor The new cursor position
    void extendSelection(const CursorPosition& newCursor);

    /// @brief Move cursor with optional selection extension
    /// @param extend If true, extend selection rather than clear it
    void moveCursorLeftWithSelection(bool extend);
    void moveCursorRightWithSelection(bool extend);
    void moveCursorUpWithSelection(bool extend);
    void moveCursorDownWithSelection(bool extend);
    void moveCursorWordLeftWithSelection(bool extend);
    void moveCursorWordRightWithSelection(bool extend);
    void moveCursorToLineStartWithSelection(bool extend);
    void moveCursorToLineEndWithSelection(bool extend);
    void moveCursorToDocStartWithSelection(bool extend);
    void moveCursorToDocEndWithSelection(bool extend);

    // =========================================================================
    // Distraction-Free writing
    // =========================================================================

    /// @brief Paint the Distraction-Free overlay (while it is on)
    /// @param painter The painter to draw with
    ///
    /// Darkens the sides of the view toward its edges and draws the word count at the
    /// bottom center, the hint at the top center and the optional clock at the top right.
    /// The texts fade with m_uiOpacity.
    void paintDistractionFreeOverlay(QPainter& painter);

    /// @brief Start UI fade animation
    ///
    /// Sets m_uiOpacity to 1.0 and starts the fade timer.
    /// When timer fires, opacity gradually fades to 0.
    void startUiFade();

    // =========================================================================
    // Formatting Helpers (Phase 7.2)
    // =========================================================================

    /// @brief Inline formatting the bold, italic, underline and strikethrough commands toggle
    enum class InlineFormat { Bold, Italic, Underline, Strikethrough };

    /// @brief Toggle inline formatting on selection or set pending format
    /// @param formatType The type of formatting
    ///
    /// If text is selected, toggles the format on the selection.
    /// If no selection, toggles the pending format state for next typed text.
    void toggleFormat(InlineFormat formatType);

    /// @brief Check if text at cursor/selection has specific formatting
    /// @param formatType The type of formatting to check
    /// @return true if current position has the specified formatting
    bool hasFormat(InlineFormat formatType) const;

    QScrollBar* m_verticalScrollBar;                        ///< Vertical scrollbar
    QScrollBar* m_horizontalScrollBar = nullptr;            ///< Horizontal scrollbar (zoomed pages)
    int m_lastCurrentPage = -1;                             ///< Page number last emitted
    int m_lastTotalPages = -1;                              ///< Page count last emitted
    QVariantAnimation* m_scrollAnimation;                   ///< Smooth scroll animation
    bool m_pointerMovesCursor = false;                      ///< A mouse or drop event moves the cursor
                                                            ///< (typewriter scrolling leaves the view)

    bool m_smoothScrollingEnabled;                          ///< Enable smooth scrolling
    int m_smoothScrollDuration;                             ///< Smooth scroll animation duration (ms)
    bool m_updatingScrollBar;                               ///< Flag to prevent scroll signal loops
    bool m_paintingWholeView = false;                       ///< A paint of the whole view runs

    // Cursor state (Phase 3.4 + 3.5)
    CursorPosition m_cursorPosition;                        ///< Current cursor position
    QTimer* m_cursorBlinkTimer;                             ///< Timer for cursor blinking
    bool m_cursorVisible;                                   ///< Current blink state (visible/hidden)
    bool m_cursorBlinkingEnabled;                           ///< Enable cursor blinking
    int m_cursorBlinkInterval;                              ///< Blink interval in milliseconds

    // Cursor navigation state (Phase 3.6/3.7/3.8)
    qreal m_preferredCursorX;                               ///< Preferred X position for vertical movement
    bool m_preferredCursorXValid;                           ///< Is m_preferredCursorX valid?
    CursorPosition m_preferredCursorXPosition;              ///< Cursor position the last vertical move gave
                                                            ///< (m_preferredCursorX holds only there)

    /// @brief A Page Up/Down move: where it found the cursor, and the cursor's row then (how
    /// far below the top of the view its line starts, in document units)
    struct PageMove {
        CursorPosition cursor;
        double row = 0.0;
        int pageLine = 0;  ///< Page mode: the cursor's line on its page, from 0
    };
    std::vector<PageMove> m_pageMoves;                      ///< Page Up/Down moves in a row, one way
    double m_pageMovesDirection = 0.0;                      ///< Their way: 1 down, -1 up
    CursorPosition m_pageMoveCursor{-1, -1};                ///< Cursor position the last of them gave

    // Selection state (Phase 3.10)
    SelectionRange m_selection;                             ///< Current selection range
    CursorPosition m_selectionAnchor;                       ///< Anchor point for selection
    bool m_isDragging;                                      ///< Is mouse drag selection in progress?

    // Click tracking for double/triple click (Phase 3.11)
    QTimer* m_clickTimer;                                   ///< Timer for click counting
    int m_clickCount;                                       ///< Number of consecutive clicks
    QPointF m_lastClickPos;                                 ///< Position of last click
    static constexpr int MULTI_CLICK_INTERVAL = 400;        ///< Max interval between clicks (ms)
    static constexpr qreal MULTI_CLICK_DISTANCE = 5.0;      ///< Max distance for multi-click

    // Drag and drop of text (stage 3)
    bool m_textDragPending = false;     ///< Press on the selection: drags once the mouse moves far enough
    QPointF m_textDragStartPos;         ///< Where that press was
    QTimer* m_autoScrollTimer = nullptr;  ///< Scrolls while selecting or dragging near the edges
    QPointF m_autoScrollPos;            ///< Last mouse position for automatic scrolling
    bool m_autoScrollForDrop = false;   ///< Scrolling for dragged text (else: for a mouse selection)
    bool m_draggingText = false;        ///< The selected text is being dragged (startTextDrag())

    // IME composition state (Phase 4.5/4.6/4.7)
    QString m_preeditString;                                ///< Current IME preedit/composition string
    CursorPosition m_preeditStart;                          ///< Start position of preedit text
    bool m_hasComposition;                                  ///< Is composition in progress?

    // Undo/Redo: QTextDocument's native undo is the single source of truth
    // (text AND formatting) — see undo()/redo(). No separate QUndoStack.

    // Pending format state (Phase 7.2)
    bool m_pendingBold{false};                              ///< Apply bold to next typed text
    bool m_pendingItalic{false};                            ///< Apply italic to next typed text
    bool m_pendingUnderline{false};                         ///< Apply underline to next typed text
    bool m_pendingStrikethrough{false};                     ///< Apply strikethrough to next typed text

    // View Mode and Appearance (Phase 5.1)
    ViewMode m_viewMode{ViewMode::Continuous};              ///< Current view mode
    EditorAppearance m_appearance;                          ///< Visual appearance configuration

    // Phase 13.5: Pagination moved to EditorRenderPipeline - see editor_render_pipeline.h

    // Distraction-Free writing (a toggle on top of the view mode)
    bool m_distractionFree{false};                          ///< Distraction-Free is on
    QString m_distractionFreeHint;                          ///< Line at the top of the view
    qreal m_uiOpacity{0.0};                                 ///< Opacity for UI overlay elements
    QTimer* m_uiFadeTimer{nullptr};                         ///< Timer for UI fade effect

    // Spell Check (Phase 6.9)
    SpellCheckService* m_spellCheckService{nullptr};        ///< Spell check service (not owned)

    /// @brief Find misspelled word at given position
    /// @param paraIndex Paragraph index
    /// @param offset Character offset within paragraph
    /// @return The misspelled word and its range, or empty if no error at position
    std::tuple<QString, int, int> getMisspelledWordAt(int paraIndex, int offset) const;

    /// @brief Create context menu for spell check
    /// @param word The misspelled word
    /// @param paraIndex Paragraph index
    /// @param startOffset Start position of word
    /// @param endOffset End position of word
    /// @return Context menu with suggestions
    QMenu* createSpellCheckContextMenu(const QString& word, int paraIndex, int startOffset, int endOffset);

    /// @brief Replace word in document
    /// @param paraIndex Paragraph index
    /// @param startOffset Start position of word
    /// @param endOffset End position of word
    /// @param replacement Replacement text
    void replaceWord(int paraIndex, int startOffset, int endOffset, const QString& replacement);

    // Grammar Check (Phase 6.17)
    GrammarCheckService* m_grammarCheckService{nullptr};    ///< Grammar check service (not owned)

    /// @brief Find grammar error at given position
    /// @param paraIndex Paragraph index
    /// @param offset Character offset within paragraph
    /// @return The grammar error info if found, or nullopt if no error at position
    std::optional<GrammarError> getGrammarErrorAt(int paraIndex, int offset) const;

    /// @brief Create context menu for grammar check
    /// @param error The grammar error
    /// @param paraIndex Paragraph index
    /// @return Context menu with suggestions and explanation
    QMenu* createGrammarContextMenu(const GrammarError& error, int paraIndex);

    // =========================================================================
    // Document, viewport and rendering
    // =========================================================================

    /// @brief The document: text, formatting and undo history
    /// @note Created by fromKml(), or empty by the first edit (see ensureDocument())
    std::unique_ptr<QTextDocument> m_textBuffer;

    /// @brief QTextCursor for direct cursor operations (Phase 11.6)
    QTextCursor m_textCursor;

    /// @brief Cursor and selection a paragraph format step was made with
    struct StepCursor {
        CursorPosition cursor;
        SelectionRange selection;
    };

    /// @brief Set by the undo item of a paragraph format step being undone or redone
    /// (see setParagraphAlignment())
    std::optional<StepCursor> m_stepCursor;

    /// @brief Create the document with the given content and connect it to the view
    ///
    /// Builds a new QTextDocument (with the editor's layout) with undo disabled, so
    /// the content is not an undo step, and connects the render pipeline, the viewport
    /// and the search engine to it.
    /// @param content Paragraphs read from KML
    void createDocument(const KmlDocumentModel& content);

    /// @brief Create an empty document if there is none yet (before the first edit)
    void ensureDocument();

    /// @brief Viewport manager for scroll and visibility coordination (Task 8.4)
    std::unique_ptr<ViewportManager> m_viewportManager;

    /// @brief Render pipeline: draws the view in every view mode
    std::unique_ptr<EditorRenderPipeline> m_renderPipeline;

    /// @brief Calculate absolute character position from cursor position
    /// @param pos Cursor position (paragraph + offset)
    /// @return Absolute character offset in the document (0-based)
    int calculateAbsolutePosition(const CursorPosition& pos) const;

    /// @brief Calculate cursor position from absolute character position
    /// @param absolutePos Absolute character offset in the document (0-based)
    /// @return Cursor position (paragraph + offset)
    CursorPosition calculateCursorPosition(int absolutePos) const;

    // =========================================================================
    // Find/Replace (Phase 9.4-9.6)
    // =========================================================================

    /// @brief Search engine for find/replace operations
    std::unique_ptr<SearchEngine> m_searchEngine;

    /// @brief Find/replace bar widget
    FindReplaceBar* m_findReplaceBar = nullptr;

    /// @brief Setup find/replace components
    void setupFindReplace();

    /// @brief Tell the search engine where the cursor and the selection are
    void syncSearchOrigin();

    /// @brief Make the selected text the search text, if it lies in one paragraph
    void takeSearchTextFromSelection();

    /// @brief Navigate cursor to a search match
    /// @param match The search match to navigate to
    void onNavigateToMatch(const SearchMatch& match);

    /// @brief Report an edit made by the find/replace bar
    ///
    /// Replacements edit the document directly, outside the editor's own editing
    /// operations: keep the cursor inside the changed text and emit contentChanged().
    void onTextReplaced();
};

}  // namespace kalahari::editor
