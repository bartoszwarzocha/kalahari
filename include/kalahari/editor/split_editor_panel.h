/// @file split_editor_panel.h
/// @brief SplitEditorPanel - Container for split view editing (OpenSpec #00042)
///
/// SplitEditorPanel manages one or two BookEditor instances in a split view
/// configuration. All editors share the same KmlDocument but maintain independent
/// scroll positions, cursor positions, and selections.
///
/// Features:
/// - Single editor or horizontal/vertical split
/// - Active editor tracking with visual indicator
/// - Shared document, independent view state
/// - State save/restore for session persistence

#pragma once

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/view_modes.h>
#include <QWidget>

class QFocusEvent;
class QKeyEvent;
class QSplitter;
class QVBoxLayout;

namespace kalahari::editor {

// =============================================================================
// Split Orientation
// =============================================================================

/// @brief Orientation of the editor split
enum class SplitOrientation {
    None,        ///< Single editor (no split)
    Horizontal,  ///< Side by side (Ctrl+Backslash)
    Vertical     ///< Stacked top/bottom (Ctrl+Shift+Backslash)
};

// =============================================================================
// SplitEditorPanel
// =============================================================================

/// @brief Container widget for split view editing
///
/// SplitEditorPanel provides the ability to view and edit the same document
/// in up to two panes simultaneously. Each pane contains a BookEditor instance
/// that shares the underlying KmlDocument.
///
/// Usage:
/// @code
/// auto panel = new SplitEditorPanel(this);
/// panel->setDocument(&document);
///
/// // Split horizontally
/// panel->splitHorizontal();
///
/// // Work with editors
/// auto active = panel->activeEditor();
/// active->insertText("Hello");
///
/// // Close split
/// panel->closeSplit();
/// @endcode
///
/// Keyboard shortcuts:
/// - Ctrl+Backslash: Split horizontally
/// - Ctrl+Shift+Backslash: Split vertically
/// - Ctrl+W: Close split (secondary editor)
class SplitEditorPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Construct a SplitEditorPanel
    /// @param parent Parent widget (optional)
    explicit SplitEditorPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~SplitEditorPanel() override;

    /// @brief Copy constructor (deleted - QWidget cannot be copied)
    SplitEditorPanel(const SplitEditorPanel&) = delete;

    /// @brief Copy assignment (deleted - QWidget cannot be copied)
    SplitEditorPanel& operator=(const SplitEditorPanel&) = delete;

    // =========================================================================
    // Document Management
    // =========================================================================

    /// @brief Set the document to edit
    /// @param document Pointer to the document (not owned, must outlive panel)
    ///
    /// The document is shared with all editor instances in the split.
    void setDocument(KmlDocument* document);

    /// @brief Get the current document
    /// @return Pointer to the document, or nullptr if not set
    KmlDocument* document() const;

    // =========================================================================
    // Split Operations
    // =========================================================================

    /// @brief Get the current split orientation
    /// @return Current split orientation
    SplitOrientation splitOrientation() const;

    /// @brief Check if the view is currently split
    /// @return true if there are two editors visible
    bool isSplit() const;

    /// @brief Split the view horizontally (side by side)
    /// @return true if split was successful, false if already split
    ///
    /// Creates a second editor to the right of the primary editor.
    /// Both editors share the same document.
    /// Shortcut: Ctrl+Backslash
    bool splitHorizontal();

    /// @brief Split the view vertically (stacked)
    /// @return true if split was successful, false if already split
    ///
    /// Creates a second editor below the primary editor.
    /// Both editors share the same document.
    /// Shortcut: Ctrl+Shift+Backslash
    bool splitVertical();

    /// @brief Close the split view
    /// @return true if split was closed, false if not split
    ///
    /// Removes the secondary editor and restores single-editor view.
    /// Shortcut: Ctrl+W
    bool closeSplit();

    /// @brief Close a specific editor by index
    /// @param index Editor index (0 = primary, 1 = secondary)
    /// @return true if editor was closed, false if invalid index
    bool closeSplit(int index);

    // =========================================================================
    // Editor Access
    // =========================================================================

    /// @brief Get the currently active editor
    /// @return Pointer to the active editor, never nullptr
    BookEditor* activeEditor() const;

    /// @brief Get editor by index
    /// @param index Editor index (0 = primary, 1 = secondary)
    /// @return Pointer to editor, or nullptr if index invalid
    BookEditor* editor(int index) const;

    /// @brief Get the number of visible editors
    /// @return 1 if not split, 2 if split
    int editorCount() const;

    /// @brief Set the active editor by index
    /// @param index Editor index (0 = primary, 1 = secondary)
    void setActiveEditor(int index);

    // =========================================================================
    // Appearance
    // =========================================================================

    /// @brief Set the appearance for all editors
    /// @param appearance The appearance configuration
    void setAppearance(const EditorAppearance& appearance);

    /// @brief Get the current appearance configuration
    /// @return Current editor appearance
    EditorAppearance appearance() const;

    // =========================================================================
    // View Mode
    // =========================================================================

    /// @brief Set the view mode for all editors
    /// @param mode The view mode to set
    ///
    /// View mode is shared across all editors in the split.
    void setViewMode(ViewMode mode);

    /// @brief Get the current view mode
    /// @return Current view mode
    ViewMode viewMode() const;

    // =========================================================================
    // State Persistence
    // =========================================================================

    /// @brief Save the current panel state
    /// @return Serialized state data
    ///
    /// Saves:
    /// - Split orientation
    /// - Splitter sizes
    /// - Active editor index
    QByteArray saveState() const;

    /// @brief Restore panel state
    /// @param state Previously saved state data
    /// @return true if state was restored successfully
    bool restoreState(const QByteArray& state);

signals:
    /// @brief Emitted when split orientation changes
    /// @param orientation The new orientation
    void splitChanged(SplitOrientation orientation);

    /// @brief Emitted when the active editor changes
    /// @param editor The new active editor
    void activeEditorChanged(BookEditor* editor);

    /// @brief Emitted when cursor position changes in active editor
    /// @param position The new cursor position
    void cursorPositionChanged(const CursorPosition& position);

    /// @brief Emitted when selection changes in active editor
    void selectionChanged();

    /// @brief Emitted when view mode changes
    /// @param mode The new view mode
    void viewModeChanged(ViewMode mode);

protected:
    /// @brief Handle key press events
    /// @param event The key event
    ///
    /// Handles split-related shortcuts:
    /// - Ctrl+Backslash: Split horizontal
    /// - Ctrl+Shift+Backslash: Split vertical
    /// - Ctrl+W: Close split
    void keyPressEvent(QKeyEvent* event) override;

    /// @brief Event filter to track focus changes
    /// @param watched The watched object
    /// @param event The event
    /// @return true if event was handled
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Create a new editor instance
    /// @return Newly created editor
    BookEditor* createEditor();

    /// @brief Setup signal connections for an editor
    /// @param editor The editor to connect
    void setupEditorConnections(BookEditor* editor);

    /// @brief Update visual indicators for active/inactive editors
    void updateActiveIndicators();

    /// @brief Create split with given orientation
    /// @param orientation The split orientation
    /// @return true if successful
    bool createSplit(SplitOrientation orientation);

    /// @brief Setup the main layout
    void setupLayout();

    // =========================================================================
    // Member Variables
    // =========================================================================

    KmlDocument* m_document{nullptr};          ///< Shared document (not owned)
    QSplitter* m_splitter{nullptr};            ///< Split container (created on split)
    QVBoxLayout* m_layout{nullptr};            ///< Main layout

    BookEditor* m_primaryEditor{nullptr};      ///< Primary (always visible) editor
    BookEditor* m_secondaryEditor{nullptr};    ///< Secondary (split only) editor
    BookEditor* m_activeEditor{nullptr};       ///< Currently active editor

    SplitOrientation m_orientation{SplitOrientation::None};  ///< Current split orientation
    EditorAppearance m_appearance;             ///< Shared appearance configuration
    ViewMode m_viewMode{ViewMode::Continuous}; ///< Shared view mode
};

}  // namespace kalahari::editor
