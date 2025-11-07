/// @file navigator_panel.h
/// @brief Navigator panel with tabbed interface (Task #00020)
///
/// Left sidebar panel containing wxAuiNotebook with 3 tabs:
/// - Outline: Book structure (Book → Parts → Chapters) - FULL FUNCTIONALITY
/// - Files: Project files browser - PLACEHOLDER (Phase 2)
/// - Libraries: Character/Place/Item resources - PLACEHOLDER (Phase 2+)

#pragma once

#include <wx/wx.h>
#include <wx/aui/auibook.h>

// Forward declarations
namespace kalahari::core {
    class Document;
}

namespace kalahari {
namespace gui {

// Forward declarations
class OutlineTab;
class FilesTab;
class LibrariesTab;

/// @brief Navigator panel with tabbed interface for project navigation
///
/// Dockable wxAUI panel on the left side (280px default width).
/// Contains wxAuiNotebook with 3 tabs for different navigation modes.
///
/// **Phase 1 (Task #00020):**
/// - Outline tab: Full wxTreeCtrl functionality (Book → Parts → Chapters)
/// - Files tab: Placeholder (Phase 2)
/// - Libraries tab: Placeholder (Phase 2+)
///
/// **Future enhancements:**
/// - Configurable tabs (enable/disable via Settings)
/// - Additional tabs: Research, Timeline, Scenes, Bibliography
/// - Tab reordering (wxAUI_NB_TAB_MOVE)
class NavigatorPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (usually MainWindow)
    /// @param document Pointer to Document model (nullptr if no document loaded)
    NavigatorPanel(wxWindow* parent, core::Document* document);

    /// @brief Destructor
    virtual ~NavigatorPanel();

    // ========================================================================
    // Public API for MainWindow integration
    // ========================================================================

    /// @brief Update all tabs when document changes
    ///
    /// Called when document structure changes (add/delete/rename chapters).
    /// Refreshes Outline tab tree, updates Files tab, etc.
    void refresh();

    /// @brief Highlight specific chapter in Outline tab
    /// @param chapterId Chapter UUID to select
    ///
    /// Called from EditorPanel when chapter loaded.
    /// Enables two-way sync: editor ↔ navigator.
    void selectChapter(const std::string& chapterId);

    /// @brief Set active document
    /// @param document Pointer to Document model (nullptr to clear)
    ///
    /// Called when user opens/closes document.
    /// Updates all tabs with new document data.
    void setDocument(core::Document* document);

private:
    // ========================================================================
    // UI Components
    // ========================================================================

    wxAuiNotebook* m_notebook = nullptr;  ///< Tab container
    OutlineTab* m_outlineTab = nullptr;   ///< Book structure tab (Phase 1)
    FilesTab* m_filesTab = nullptr;       ///< Files browser tab (Phase 2)
    LibrariesTab* m_librariesTab = nullptr; ///< Libraries tab (Phase 2+)

    // ========================================================================
    // Data
    // ========================================================================

    core::Document* m_document = nullptr; ///< Current document (nullptr if none)

    // ========================================================================
    // Setup Methods
    // ========================================================================

    /// @brief Create notebook and all tabs
    ///
    /// Called from constructor. Creates:
    /// 1. wxAuiNotebook with top tabs
    /// 2. OutlineTab (functional)
    /// 3. FilesTab (placeholder)
    /// 4. LibrariesTab (placeholder)
    void createTabs();

    /// @brief Setup tab icons from IconRegistry
    ///
    /// Uses placeholder icons (Phase 1):
    /// - Outline: FOLDER (will be account_tree later)
    /// - Files: FILE_OPEN (will be folder_open later)
    /// - Libraries: FOLDER (will be library_books later)
    void setupIcons();
};

} // namespace gui
} // namespace kalahari
