/// @file navigator_panel.cpp
/// @brief Implementation of NavigatorPanel

#include "kalahari/gui/panels/navigator_panel.h"
#include <kalahari/core/logger.h>

namespace kalahari {
namespace gui {

wxBEGIN_EVENT_TABLE(NavigatorPanel, wxPanel)
wxEND_EVENT_TABLE()

NavigatorPanel::NavigatorPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    core::Logger::getInstance().info("Creating Navigator panel (stub)");
    setupLayout();
}

void NavigatorPanel::setupLayout() {
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // Placeholder tree control
    m_treeCtrl = new wxTreeCtrl(this, wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);

    // Add placeholder root
    wxTreeItemId root = m_treeCtrl->AddRoot("Project");
    m_treeCtrl->AppendItem(root, "Part 1: Introduction");
    m_treeCtrl->AppendItem(root, "Part 2: Main Content");
    m_treeCtrl->AppendItem(root, "Part 3: Conclusion");
    m_treeCtrl->ExpandAll();

    sizer->Add(m_treeCtrl, 1, wxALL | wxEXPAND, 5);
    SetSizer(sizer);
}

} // namespace gui
} // namespace kalahari
