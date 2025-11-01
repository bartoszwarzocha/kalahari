/// @file manage_perspectives_dialog.h
/// @brief Dialog for managing saved perspectives
///
/// Allows users to load, delete, and rename custom perspectives.
/// Default perspectives (Default, Writing, Editing, Research) are protected.

#pragma once

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <vector>
#include <string>

namespace kalahari {
namespace gui {

/// @brief Dialog for managing saved perspectives
///
/// Features:
/// - List of all saved perspectives
/// - Load button (loads selected perspective)
/// - Delete button (removes custom perspectives, default ones protected)
/// - Rename button (renames custom perspectives)
/// - Close button
class ManagePerspectivesDialog : public wxDialog {
public:
    /// @brief Constructor
    /// @param parent Parent window (usually MainWindow)
    explicit ManagePerspectivesDialog(wxWindow* parent);

    /// @brief Destructor
    virtual ~ManagePerspectivesDialog() = default;

    /// @brief Get the name of the perspective selected for loading
    /// @return Perspective name, or empty string if none selected
    std::string getSelectedPerspective() const { return m_selectedPerspective; }

    /// @brief Check if user wants to load the selected perspective
    /// @return true if Load button was clicked
    bool shouldLoadPerspective() const { return m_shouldLoad; }

private:
    /// @brief List control showing all perspectives
    wxListCtrl* m_listCtrl = nullptr;

    /// @brief Load button
    wxButton* m_loadButton = nullptr;

    /// @brief Delete button
    wxButton* m_deleteButton = nullptr;

    /// @brief Rename button
    wxButton* m_renameButton = nullptr;

    /// @brief Selected perspective name
    std::string m_selectedPerspective;

    /// @brief Flag indicating if Load was clicked
    bool m_shouldLoad = false;

    /// @brief Default perspective names (protected from deletion)
    static const std::vector<std::string> DEFAULT_PERSPECTIVES;

    /// @brief Setup the dialog layout
    void setupLayout();

    /// @brief Refresh the list of perspectives
    void refreshList();

    /// @brief Check if a perspective is a default (protected) one
    /// @param name Perspective name
    /// @return true if it's a default perspective
    bool isDefaultPerspective(const std::string& name) const;

    /// @brief Handle Load button click
    void onLoad(wxCommandEvent& event);

    /// @brief Handle Delete button click
    void onDelete(wxCommandEvent& event);

    /// @brief Handle Rename button click
    void onRename(wxCommandEvent& event);

    /// @brief Handle Close button click
    void onClose(wxCommandEvent& event);

    /// @brief Handle list item selection
    void onListItemSelected(wxListEvent& event);

    /// @brief Handle list item double-click (same as Load)
    void onListItemActivated(wxListEvent& event);

    /// @brief Update button states based on selection
    void updateButtonStates();

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
