/// @file editor_panel.h
/// @brief Rich text editor panel (placeholder for Phase 1)
///
/// Main content area for book writing. Currently a simple wxTextCtrl placeholder.
/// Phase 1 will implement: wxRichTextCtrl or custom wxGraphicsContext-based editor.
/// Implemented in Task #00013.

#pragma once

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <filesystem>

// Forward declaration
namespace kalahari::core {
    class BookElement;
}

namespace kalahari {
namespace gui {

/// @brief Editor panel for rich text editing (placeholder)
///
/// Main content area where users write their books.
/// Current state: Simple wxTextCtrl placeholder
/// Phase 1 planned features:
/// - wxRichTextCtrl or custom wxGraphicsContext editor
/// - Page-like view with A4 pages on gray background
/// - Load/Save from BookElement HTML files
/// - Real-time word count
/// - Full formatting (Bold, Italic, etc.)
/// - Undo/Redo support
class EditorPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (usually MainWindow)
    explicit EditorPanel(wxWindow* parent);

    /// @brief Destructor
    virtual ~EditorPanel();

    // ========================================================================
    // Public API for MainWindow integration
    // ========================================================================

    /// @brief Load chapter content from BookElement
    /// @param element BookElement with HTML file path
    /// @return true on success, false on error
    ///
    /// If HTML file doesn't exist, clears editor (new chapter).
    /// Updates m_currentElement pointer for save operations.
    bool loadChapter(const core::BookElement& element);

    /// @brief Save current content to BookElement HTML file
    /// @param element BookElement to save to
    /// @return true on success, false on error
    ///
    /// Creates directory structure if missing.
    /// Updates element's wordCount and modified timestamp.
    bool saveChapter(core::BookElement& element);

    /// @brief Get current word count
    /// @return Number of words in editor
    int getWordCount() const;

    /// @brief Clear editor content
    void clearContent();

    /// @brief Check if content has unsaved changes
    /// @return true if modified since last save
    bool hasUnsavedChanges() const;

    // ========================================================================
    // Format Event Handlers (public - called from MainWindow)
    // ========================================================================
    // TODO: Implement in Phase 1 with rich text editor

    /// @brief Apply bold formatting to selection
    /// @param event Command event from Format menu
    void onFormatBold(wxCommandEvent& event);

    /// @brief Apply italic formatting to selection
    /// @param event Command event from Format menu
    void onFormatItalic(wxCommandEvent& event);

    /// @brief Apply underline formatting to selection
    /// @param event Command event from Format menu
    void onFormatUnderline(wxCommandEvent& event);

    /// @brief Show font selection dialog
    /// @param event Command event from Format menu
    void onFormatFont(wxCommandEvent& event);

    /// @brief Clear all formatting from selection
    /// @param event Command event from Format menu
    void onFormatClear(wxCommandEvent& event);

private:
    // ========================================================================
    // UI Components
    // ========================================================================

    /// @brief Text control (placeholder for Phase 1 rich editor)
    wxTextCtrl* m_textCtrl = nullptr;

    /// @brief Word count timer (500ms debounce for status bar updates)
    wxTimer* m_wordCountTimer = nullptr;

    /// @brief Current loaded chapter (non-owning pointer)
    core::BookElement* m_currentElement = nullptr;

    /// @brief Flag tracking if editor content was modified
    bool m_isModified = false;

    /// @brief Cached word count
    mutable int m_cachedWordCount = 0;

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /// @brief Setup the panel layout
    void setupLayout();

    // ========================================================================
    // Event Handlers
    // ========================================================================

    /// @brief Handle word count timer (update StatusBar)
    /// @param event wxTimerEvent after 500ms debounce
    void onWordCountTimer(wxTimerEvent& event);

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
