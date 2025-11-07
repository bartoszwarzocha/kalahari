/// @file libraries_tab.h
/// @brief Libraries tab for Navigator panel (Task #00020 - Phase 1 PLACEHOLDER)
///
/// Placeholder tab for Character/Place/Item libraries.
/// Full implementation in Phase 2+.

#pragma once

#include <wx/wx.h>

namespace kalahari {
namespace gui {

/// @brief Libraries tab - Character/Place/Item resources (PLACEHOLDER)
///
/// **Phase 1 (Task #00020):**
/// - Simple wxPanel with centered placeholder text
/// - Gray background to distinguish from functional tabs
///
/// **Phase 2+ (Future):**
/// - Grid/List view of library items (Characters, Places, Items)
/// - Icons for each item (custom or generated)
/// - Double-click → open in editor with tabs (Description, Timeline, Notes)
/// - Context menu (New, Edit, Delete, Export)
/// - Configurable libraries per book genre (novel vs. reportage)
class LibrariesTab : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (NavigatorPanel's notebook)
    explicit LibrariesTab(wxWindow* parent);

    /// @brief Destructor
    virtual ~LibrariesTab() = default;

private:
    wxStaticText* m_placeholder = nullptr; ///< Placeholder text
};

} // namespace gui
} // namespace kalahari
