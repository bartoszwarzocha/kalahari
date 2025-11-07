/// @file files_tab.h
/// @brief Files tab for Navigator panel (Task #00020 - Phase 1 PLACEHOLDER)
///
/// Placeholder tab for project files browser.
/// Full implementation in Phase 2.

#pragma once

#include <wx/wx.h>

namespace kalahari {
namespace gui {

/// @brief Files tab - project files browser (PLACEHOLDER)
///
/// **Phase 1 (Task #00020):**
/// - Simple wxPanel with centered placeholder text
/// - Gray background to distinguish from functional tabs
///
/// **Phase 2 (Future):**
/// - wxTreeCtrl showing project file structure
/// - .klh contents (chapters/, media/, metadata.json)
/// - Double-click to open files
/// - Context menu (Open, Rename, Delete, New Folder)
class FilesTab : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (NavigatorPanel's notebook)
    explicit FilesTab(wxWindow* parent);

    /// @brief Destructor
    virtual ~FilesTab() = default;

private:
    wxStaticText* m_placeholder = nullptr; ///< Placeholder text
};

} // namespace gui
} // namespace kalahari
