/// @file outline_tab.h
/// @brief Outline tab for Navigator panel (Task #00020)
///
/// Book structure tree view (Book → Parts → Chapters).
/// Full functionality implementation in Phase 2.

#pragma once

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <string>

// Forward declarations
namespace kalahari::core {
    class Document;
}

namespace kalahari {
namespace gui {

/// @brief Outline tab - hierarchical book structure view
///
/// **Phase 1 (Task #00020):**
/// - Basic structure with placeholder text
///
/// **Phase 2 (Task #00020):**
/// - wxTreeCtrl with Book → Parts → Chapters hierarchy
/// - Icons for Book/Part/Chapter nodes (from IconRegistry)
/// - Node selection → load chapter in EditorPanel
/// - Two-way sync: editor ↔ navigator
/// - Context menu CRUD operations (Add/Rename/Delete/Move)
class OutlineTab : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (NavigatorPanel's notebook)
    /// @param document Pointer to Document model (nullptr if none)
    OutlineTab(wxWindow* parent, core::Document* document);

    /// @brief Destructor
    virtual ~OutlineTab();

    // ========================================================================
    // Public API (Phase 2)
    // ========================================================================

    /// @brief Populate tree from document
    ///
    /// Clears tree and rebuilds from Document model.
    /// Called when document loaded or structure changes.
    void populateTree();

    /// @brief Select specific chapter in tree
    /// @param chapterId Chapter UUID to select
    ///
    /// Enables two-way sync: editor → navigator.
    void selectChapter(const std::string& chapterId);

    /// @brief Set active document
    /// @param document Pointer to Document model (nullptr to clear)
    ///
    /// Updates tree with new document structure.
    void setDocument(core::Document* document);

private:
    // ========================================================================
    // UI Components (Phase 2)
    // ========================================================================

    wxTreeCtrl* m_tree = nullptr;           ///< Tree control
    wxImageList* m_imageList = nullptr;     ///< Icons for tree nodes
    wxStaticText* m_placeholder = nullptr;  ///< Phase 1 placeholder

    // ========================================================================
    // Data
    // ========================================================================

    core::Document* m_document = nullptr;   ///< Current document

    // ========================================================================
    // Event Handlers (Phase 2)
    // ========================================================================

    /// @brief Handle tree item selection
    void onTreeItemActivated(wxTreeEvent& event);

    /// @brief Handle tree right-click (context menu)
    void onTreeItemRightClick(wxTreeEvent& event);

    /// @brief Handle tree selection change
    void onTreeSelectionChanged(wxTreeEvent& event);

    // Context menu handlers (Phase 2)
    void onAddChapter(wxCommandEvent& event);
    void onRenameChapter(wxCommandEvent& event);
    void onDeleteChapter(wxCommandEvent& event);
    void onMoveChapterUp(wxCommandEvent& event);
    void onMoveChapterDown(wxCommandEvent& event);

    /// @brief Show context menu at position
    /// @param item Tree item clicked
    /// @param pos Mouse position
    void showContextMenu(wxTreeItemId item, const wxPoint& pos);

    // ========================================================================
    // Helper Methods (Phase 2)
    // ========================================================================

    /// @brief Find tree item by chapter/part/book ID (recursive)
    /// @param parent Parent item to start search from
    /// @param id ID to search for
    /// @return Tree item if found, invalid wxTreeItemId otherwise
    wxTreeItemId findItemById(wxTreeItemId parent, const std::string& id);

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
