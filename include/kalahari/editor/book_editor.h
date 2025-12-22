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
#include <kalahari/editor/kml_comment.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/layout_manager.h>
#include <kalahari/editor/page_layout_manager.h>
#include <kalahari/editor/view_modes.h>
#include <kalahari/editor/virtual_scroll_manager.h>
#include <QWidget>
#include <QList>
#include <memory>

class QInputMethodEvent;
class QKeyEvent;
class QMouseEvent;
class QPropertyAnimation;
class QScrollBar;
class QTimer;
class QUndoStack;

namespace kalahari::editor {

/// @brief Custom text editor widget for KML documents
///
/// BookEditor is a QWidget-based text editor designed for rendering and
/// editing KML (Kalahari Markup Language) documents. It uses virtual
/// scrolling to efficiently handle documents of any size.
///
/// The editor coordinates three main components:
/// - KmlDocument: The document model
/// - LayoutManager: Handles paragraph layout and text wrapping
/// - VirtualScrollManager: Manages efficient scrolling
///
/// Usage:
/// @code
/// auto editor = new BookEditor(parentWidget);
/// editor->setDocument(&document);
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
    // Document Management
    // =========================================================================

    /// @brief Set the document to edit
    /// @param document Pointer to the document (not owned, must outlive editor)
    ///
    /// When a document is set, the editor:
    /// 1. Registers as an observer on the document
    /// 2. Initializes the scroll manager and layout manager
    /// 3. Triggers a repaint
    ///
    /// Pass nullptr to clear the current document.
    void setDocument(KmlDocument* document);

    /// @brief Get the current document
    /// @return Pointer to the document, or nullptr if not set
    KmlDocument* document() const;

    // =========================================================================
    // Layout Configuration
    // =========================================================================

    /// @brief Get the layout manager
    /// @return Reference to the layout manager
    LayoutManager& layoutManager();

    /// @brief Get the layout manager (const)
    /// @return Const reference to the layout manager
    const LayoutManager& layoutManager() const;

    /// @brief Get the scroll manager
    /// @return Reference to the virtual scroll manager
    VirtualScrollManager& scrollManager();

    /// @brief Get the scroll manager (const)
    /// @return Const reference to the virtual scroll manager
    const VirtualScrollManager& scrollManager() const;

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

    /// @brief Force cursor to visible state and restart blink timer
    ///
    /// Call this after any cursor movement to ensure the cursor
    /// is visible immediately after the user action.
    void ensureCursorVisible();

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

    /// @brief Move cursor one page up (Page Up)
    ///
    /// Moves approximately one viewport height up.
    void moveCursorPageUp();

    /// @brief Move cursor one page down (Page Down)
    ///
    /// Moves approximately one viewport height down.
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
    /// After deletion, the cursor is positioned at the start of the former selection.
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

    /// @brief Get the undo stack for this editor
    /// @return Pointer to the undo stack
    QUndoStack* undoStack() const;

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
    /// Copies selection to clipboard and deletes it.
    /// Does nothing if no selection.
    void cut();

    /// @brief Paste content from clipboard (Ctrl+V)
    ///
    /// Inserts clipboard content at cursor position.
    /// If there is a selection, it is deleted first.
    /// Supports KML, HTML, and plain text formats.
    void paste();

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
    /// @note Currently a stub - full implementation requires KmlParagraph format runs
    void toggleBold();

    /// @brief Toggle italic formatting on selection or at cursor
    ///
    /// If text is selected, toggles italic on the selection.
    /// If no selection, toggles italic mode for next typed characters.
    /// @note Currently a stub - full implementation requires KmlParagraph format runs
    void toggleItalic();

    /// @brief Toggle underline formatting on selection or at cursor
    ///
    /// If text is selected, toggles underline on the selection.
    /// If no selection, toggles underline mode for next typed characters.
    /// @note Currently a stub - full implementation requires KmlParagraph format runs
    void toggleUnderline();

    /// @brief Toggle strikethrough formatting on selection or at cursor
    ///
    /// If text is selected, toggles strikethrough on the selection.
    /// If no selection, toggles strikethrough mode for next typed characters.
    /// @note Currently a stub - full implementation requires KmlParagraph format runs
    void toggleStrikethrough();

    /// @brief Check if current selection/cursor position has bold formatting
    /// @return true if text at cursor/selection is bold
    /// @note Currently returns false - full implementation requires KmlParagraph format runs
    bool isBold() const;

    /// @brief Check if current selection/cursor position has italic formatting
    /// @return true if text at cursor/selection is italic
    /// @note Currently returns false - full implementation requires KmlParagraph format runs
    bool isItalic() const;

    /// @brief Check if current selection/cursor position has underline formatting
    /// @return true if text at cursor/selection is underlined
    /// @note Currently returns false - full implementation requires KmlParagraph format runs
    bool isUnderline() const;

    /// @brief Check if current selection/cursor position has strikethrough formatting
    /// @return true if text at cursor/selection has strikethrough
    /// @note Currently returns false - full implementation requires KmlParagraph format runs
    bool isStrikethrough() const;

    // =========================================================================
    // Comments (Phase 7.9)
    // =========================================================================

    /// @brief Insert a comment at the current selection
    ///
    /// Opens a dialog to enter comment text, then creates a KmlComment
    /// attached to the selected text range. Does nothing if no selection.
    void insertComment();

    /// @brief Delete a comment by ID
    /// @param commentId The ID of the comment to delete
    ///
    /// Searches all paragraphs for the comment and removes it.
    void deleteComment(const QString& commentId);

    /// @brief Edit an existing comment's text
    /// @param commentId The ID of the comment to edit
    ///
    /// Opens a dialog to edit the comment text.
    void editComment(const QString& commentId);

    /// @brief Get all comments in the current paragraph
    /// @return List of comments in the paragraph containing the cursor
    QList<KmlComment> commentsInCurrentParagraph() const;

    /// @brief Navigate to a specific comment
    /// @param paragraphIndex Paragraph containing the comment
    /// @param commentId ID of the comment to navigate to
    ///
    /// Moves cursor to the start of the commented text and scrolls
    /// to make it visible.
    void navigateToComment(int paragraphIndex, const QString& commentId);

    // =========================================================================
    // View Mode (Phase 5.1)
    // =========================================================================

    /// @brief Get the current view mode
    /// @return Current view mode
    ViewMode viewMode() const;

    /// @brief Set the view mode
    /// @param mode The new view mode
    ///
    /// Emits viewModeChanged if mode changes. Triggers repaint.
    void setViewMode(ViewMode mode);

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

    /// @brief Set the appearance settings
    /// @param appearance The new appearance configuration
    ///
    /// Emits appearanceChanged if appearance changes. Triggers repaint.
    void setAppearance(const EditorAppearance& appearance);

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
    /// @brief Emitted when the document changes
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

    /// @brief Emitted when appearance settings change
    void appearanceChanged();

    /// @brief Emitted when the current page changes (Page Mode)
    /// @param page The new current page number (1-based)
    void currentPageChanged(int page);

    /// @brief Emitted when the total page count changes (Page Mode)
    /// @param pages The new total page count
    void totalPagesChanged(int pages);

    /// @brief Emitted when distraction-free mode is toggled
    /// @param enabled true if distraction-free mode is now active
    void distractionFreeModeChanged(bool enabled);

    /// @brief Emitted when a comment is added to the document
    /// @param paragraphIndex Index of the paragraph containing the new comment
    void commentAdded(int paragraphIndex);

    /// @brief Emitted when a comment is removed from the document
    /// @param paragraphIndex Index of the paragraph from which comment was removed
    /// @param commentId ID of the removed comment
    void commentRemoved(int paragraphIndex, const QString& commentId);

    /// @brief Emitted when a comment is selected (e.g., by clicking in margin)
    /// @param paragraphIndex Index of the paragraph containing the comment
    /// @param commentId ID of the selected comment
    void commentSelected(int paragraphIndex, const QString& commentId);

protected:
    // =========================================================================
    // Event Handlers
    // =========================================================================

    /// @brief Paint event handler
    /// @param event The paint event
    ///
    /// Renders the document content:
    /// - Fills background with palette window color
    /// - Layouts visible paragraphs using LayoutManager
    /// - Draws each paragraph using ParagraphLayout::draw()
    /// - Applies scroll offset for virtual scrolling
    void paintEvent(QPaintEvent* event) override;

    /// @brief Resize event handler
    /// @param event The resize event
    ///
    /// Updates the layout width and viewport height when the widget resizes.
    void resizeEvent(QResizeEvent* event) override;

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
    /// triple-click to select paragraph.
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Mouse move event handler (Phase 3.10)
    /// @param event The mouse event
    ///
    /// Handles drag selection when mouse button is pressed.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Mouse release event handler (Phase 3.10)
    /// @param event The mouse event
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// @brief Mouse double-click event handler (Phase 3.11)
    /// @param event The mouse event
    ///
    /// Handles double-click to select word.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

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

    /// @brief Handle scroll animation value change
    /// @param value Current animation value
    void onScrollAnimationValueChanged(const QVariant& value);

    /// @brief Handle cursor blink timer timeout
    void onCursorBlinkTimeout();

private:
    /// @brief Setup internal components
    void setupComponents();

    /// @brief Update layout manager width from widget width
    void updateLayoutWidth();

    /// @brief Update scroll manager viewport from widget size
    void updateViewport();

    /// @brief Setup the vertical scrollbar
    void setupScrollBar();

    /// @brief Update scrollbar range based on content height
    void updateScrollBarRange();

    /// @brief Sync scrollbar value with scroll manager (without triggering signals)
    void syncScrollBarValue();

    /// @brief Start smooth scroll animation to target offset
    /// @param targetOffset Target scroll offset
    void startScrollAnimation(qreal targetOffset);

    /// @brief Stop any running scroll animation
    void stopScrollAnimation();

    /// @brief Update scroll position for typewriter mode
    ///
    /// In typewriter mode, keeps the cursor at a fixed vertical position
    /// (m_appearance.typewriter.focusPosition). Uses smooth scrolling if enabled.
    void updateTypewriterScroll();

    /// @brief Get the Y coordinate of the cursor in document coordinates
    /// @return The Y position of the cursor line in the document
    qreal getCursorDocumentY() const;

    /// @brief Validate and clamp cursor position to valid range
    /// @param position The position to validate
    /// @return Validated position within document bounds
    CursorPosition validateCursorPosition(const CursorPosition& position) const;

    /// @brief Calculate cursor rectangle in widget coordinates
    /// @return Cursor rectangle, or empty rect if cursor not in visible area
    QRectF calculateCursorRect() const;

    /// @brief Draw the cursor at current position
    /// @param painter The painter to draw with
    void drawCursor(QPainter* painter);

    /// @brief Setup cursor blink timer
    void setupCursorBlinkTimer();

    /// @brief Convert widget point to cursor position (Phase 3.9)
    /// @param widgetPos Point in widget coordinates
    /// @return Cursor position in document, or invalid if outside document
    CursorPosition positionFromPoint(const QPointF& widgetPos) const;

    /// @brief Draw selection highlighting (Phase 3.10)
    /// @param painter The painter to draw with
    void drawSelection(QPainter* painter);

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

    /// @brief Paint the Page Mode view
    /// @param painter The painter to draw with
    ///
    /// Draws page frames with shadows, backgrounds, and borders.
    /// Content painting is delegated to paragraph layouts.
    void paintPageMode(QPainter& painter);

    // =========================================================================
    // Focus Mode (Phase 5.6)
    // =========================================================================

    /// @brief Range of content that is currently focused
    ///
    /// In Focus Mode, content outside this range is dimmed to help
    /// the user concentrate on the focused area.
    struct FocusedRange {
        int startParagraph{0};    ///< First paragraph in focused range
        int endParagraph{0};      ///< Last paragraph in focused range (inclusive)
        int startLine{0};         ///< First line within start paragraph (for Line scope)
        int endLine{0};           ///< Last line within end paragraph (for Line scope)
    };

    /// @brief Calculate the currently focused range based on cursor position
    /// @return Range of paragraphs/lines that should be focused
    ///
    /// The range is determined by m_appearance.focusMode.scope:
    /// - Paragraph: The paragraph containing the cursor
    /// - Line: The specific line containing the cursor
    /// - Sentence: Currently treated same as Paragraph
    FocusedRange getFocusedRange() const;

    /// @brief Paint the focus mode overlay (dimming effect)
    /// @param painter The painter to draw with
    ///
    /// Draws semi-transparent overlays over non-focused content to
    /// create the focus effect. Also draws optional highlight behind
    /// the focused area.
    void paintFocusOverlay(QPainter& painter);

    // =========================================================================
    // Distraction-Free Mode (Phase 5.7)
    // =========================================================================

    /// @brief Paint the distraction-free mode overlay
    /// @param painter The painter to draw with
    ///
    /// Draws word count at bottom center and optional clock at top right.
    /// UI elements fade based on m_uiOpacity.
    void paintDistractionFreeOverlay(QPainter& painter);

    /// @brief Get total word count in the document
    /// @return Word count, or 0 if no document
    int getWordCount() const;

    /// @brief Start UI fade animation
    ///
    /// Sets m_uiOpacity to 1.0 and starts the fade timer.
    /// When timer fires, opacity gradually fades to 0.
    void startUiFade();

    KmlDocument* m_document;                                ///< Document being edited (not owned)
    std::unique_ptr<LayoutManager> m_layoutManager;         ///< Paragraph layout management
    std::unique_ptr<VirtualScrollManager> m_scrollManager;  ///< Virtual scrolling
    std::unique_ptr<PageLayoutManager> m_pageLayoutManager; ///< Page layout for Page Mode

    QScrollBar* m_verticalScrollBar;                        ///< Vertical scrollbar
    QPropertyAnimation* m_scrollAnimation;                  ///< Smooth scroll animation
    QPropertyAnimation* m_typewriterScrollAnimation;        ///< Typewriter mode scroll animation

    bool m_smoothScrollingEnabled;                          ///< Enable smooth scrolling
    int m_smoothScrollDuration;                             ///< Smooth scroll animation duration (ms)
    bool m_updatingScrollBar;                               ///< Flag to prevent scroll signal loops

    // Cursor state (Phase 3.4 + 3.5)
    CursorPosition m_cursorPosition;                        ///< Current cursor position
    QTimer* m_cursorBlinkTimer;                             ///< Timer for cursor blinking
    bool m_cursorVisible;                                   ///< Current blink state (visible/hidden)
    bool m_cursorBlinkingEnabled;                           ///< Enable cursor blinking
    int m_cursorBlinkInterval;                              ///< Blink interval in milliseconds

    // Cursor navigation state (Phase 3.6/3.7/3.8)
    qreal m_preferredCursorX;                               ///< Preferred X position for vertical movement
    bool m_preferredCursorXValid;                           ///< Is m_preferredCursorX valid?

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

    // IME composition state (Phase 4.5/4.6/4.7)
    QString m_preeditString;                                ///< Current IME preedit/composition string
    CursorPosition m_preeditStart;                          ///< Start position of preedit text
    bool m_hasComposition;                                  ///< Is composition in progress?

    // Undo/Redo state (Phase 4.8)
    QUndoStack* m_undoStack;                                ///< Undo stack for editing commands

    // View Mode and Appearance (Phase 5.1)
    ViewMode m_viewMode{ViewMode::Continuous};              ///< Current view mode
    EditorAppearance m_appearance;                          ///< Visual appearance configuration

    // Distraction-Free Mode (Phase 5.7)
    qreal m_uiOpacity{0.0};                                 ///< Opacity for UI overlay elements
    QTimer* m_uiFadeTimer{nullptr};                         ///< Timer for UI fade effect
};

}  // namespace kalahari::editor
