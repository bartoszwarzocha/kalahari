/// @file navigator_panel.h
/// @brief Navigator panel for document structure (wxAUI panel)
///
/// Displays hierarchical document structure using wxTreeCtrl.
/// Full implementation in Task #00015.

#pragma once

#include <wx/wx.h>
#include <wx/treectrl.h>

namespace kalahari {
namespace gui {

/// @brief Navigator panel showing document hierarchy
///
/// Displays book structure (Parts, Chapters) in a tree view.
/// This is a stub implementation - full features in Task #00015.
class NavigatorPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (usually MainWindow)
    explicit NavigatorPanel(wxWindow* parent);

    /// @brief Destructor
    virtual ~NavigatorPanel() = default;

private:
    /// @brief Tree control showing document structure
    wxTreeCtrl* m_treeCtrl = nullptr;

    /// @brief Setup the panel layout
    void setupLayout();

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
