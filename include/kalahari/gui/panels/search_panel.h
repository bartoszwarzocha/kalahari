/// @file search_panel.h
/// @brief Search and replace panel (wxAUI panel)
///
/// Find and replace functionality across documents.
/// Stub implementation - full features in Phase 1.

#pragma once

#include <wx/wx.h>

namespace kalahari {
namespace gui {

/// @brief Search panel for find and replace
///
/// Provides search and replace functionality across documents.
/// This is a stub implementation - full features in later tasks.
class SearchPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (usually MainWindow)
    explicit SearchPanel(wxWindow* parent);

    /// @brief Destructor
    virtual ~SearchPanel() = default;

private:
    /// @brief Search text control
    wxTextCtrl* m_searchCtrl = nullptr;

    /// @brief Replace text control
    wxTextCtrl* m_replaceCtrl = nullptr;

    /// @brief Setup the panel layout
    void setupLayout();

    /// @brief Handle find button click
    void onFind(wxCommandEvent& event);

    /// @brief Handle replace button click
    void onReplace(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
