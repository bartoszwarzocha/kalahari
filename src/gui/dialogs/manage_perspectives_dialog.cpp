/// @file manage_perspectives_dialog.cpp
/// @brief Implementation of ManagePerspectivesDialog

#include "kalahari/gui/dialogs/manage_perspectives_dialog.h"
#include "kalahari/gui/perspective_manager.h"
#include <kalahari/core/logger.h>
#include <wx/statline.h>

namespace kalahari {
namespace gui {

// Default perspectives (protected from deletion/rename)
const std::vector<std::string> ManagePerspectivesDialog::DEFAULT_PERSPECTIVES = {
    "Default",
    "Writing",
    "Editing",
    "Research"
};

enum {
    ID_LOAD = wxID_HIGHEST + 300,
    ID_DELETE,
    ID_RENAME
};

wxBEGIN_EVENT_TABLE(ManagePerspectivesDialog, wxDialog)
    EVT_BUTTON(ID_LOAD, ManagePerspectivesDialog::onLoad)
    EVT_BUTTON(ID_DELETE, ManagePerspectivesDialog::onDelete)
    EVT_BUTTON(ID_RENAME, ManagePerspectivesDialog::onRename)
    EVT_BUTTON(wxID_CLOSE, ManagePerspectivesDialog::onClose)
    EVT_LIST_ITEM_SELECTED(wxID_ANY, ManagePerspectivesDialog::onListItemSelected)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, ManagePerspectivesDialog::onListItemActivated)
wxEND_EVENT_TABLE()

ManagePerspectivesDialog::ManagePerspectivesDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Manage Perspectives"),
               wxDefaultPosition, wxSize(500, 400),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    setupLayout();
    refreshList();
    updateButtonStates();
}

void ManagePerspectivesDialog::setupLayout() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Info text
    wxStaticText* infoText = new wxStaticText(this, wxID_ANY,
        _("Manage your saved panel layouts. Default perspectives cannot be deleted."));
    mainSizer->Add(infoText, 0, wxALL, 10);

    // List control with columns
    m_listCtrl = new wxListCtrl(this, wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);

    m_listCtrl->AppendColumn(_("Perspective Name"), wxLIST_FORMAT_LEFT, 250);
    m_listCtrl->AppendColumn(_("Type"), wxLIST_FORMAT_LEFT, 150);

    mainSizer->Add(m_listCtrl, 1, wxALL | wxEXPAND, 10);

    // Buttons row
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    m_loadButton = new wxButton(this, ID_LOAD, _("&Load"));
    m_loadButton->SetToolTip(_("Load the selected perspective"));
    buttonSizer->Add(m_loadButton, 0, wxALL, 5);

    m_deleteButton = new wxButton(this, ID_DELETE, _("&Delete"));
    m_deleteButton->SetToolTip(_("Delete the selected custom perspective"));
    buttonSizer->Add(m_deleteButton, 0, wxALL, 5);

    m_renameButton = new wxButton(this, ID_RENAME, _("&Rename"));
    m_renameButton->SetToolTip(_("Rename the selected custom perspective"));
    buttonSizer->Add(m_renameButton, 0, wxALL, 5);

    buttonSizer->AddStretchSpacer(1);

    wxButton* closeButton = new wxButton(this, wxID_CLOSE, _("&Close"));
    buttonSizer->Add(closeButton, 0, wxALL, 5);

    mainSizer->Add(buttonSizer, 0, wxALL | wxEXPAND, 10);

    SetSizer(mainSizer);
}

void ManagePerspectivesDialog::refreshList() {
    m_listCtrl->DeleteAllItems();

    auto& perspMgr = PerspectiveManager::getInstance();
    std::vector<std::string> perspectives = perspMgr.listPerspectives();

    for (size_t i = 0; i < perspectives.size(); ++i) {
        const std::string& name = perspectives[i];

        long itemIndex = m_listCtrl->InsertItem(i, wxString::FromUTF8(name));

        // Mark default vs custom
        if (isDefaultPerspective(name)) {
            m_listCtrl->SetItem(itemIndex, 1, _("Default (Protected)"));
        } else {
            m_listCtrl->SetItem(itemIndex, 1, _("Custom"));
        }
    }
}

bool ManagePerspectivesDialog::isDefaultPerspective(const std::string& name) const {
    return std::find(DEFAULT_PERSPECTIVES.begin(), DEFAULT_PERSPECTIVES.end(), name)
           != DEFAULT_PERSPECTIVES.end();
}

void ManagePerspectivesDialog::updateButtonStates() {
    long selectedIndex = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    bool hasSelection = (selectedIndex != -1);

    m_loadButton->Enable(hasSelection);

    if (hasSelection) {
        wxListItem item;
        item.SetId(selectedIndex);
        item.SetColumn(0);
        item.SetMask(wxLIST_MASK_TEXT);
        m_listCtrl->GetItem(item);

        std::string selectedName = item.GetText().ToStdString();
        bool isDefault = isDefaultPerspective(selectedName);

        m_deleteButton->Enable(!isDefault);
        m_renameButton->Enable(!isDefault);
    } else {
        m_deleteButton->Enable(false);
        m_renameButton->Enable(false);
    }
}

void ManagePerspectivesDialog::onListItemSelected([[maybe_unused]] wxListEvent& event) {
    updateButtonStates();
}

void ManagePerspectivesDialog::onListItemActivated([[maybe_unused]] wxListEvent& event) {
    // Double-click = Load
    wxCommandEvent dummyEvent;
    onLoad(dummyEvent);
}

void ManagePerspectivesDialog::onLoad([[maybe_unused]] wxCommandEvent& event) {
    long selectedIndex = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

    if (selectedIndex == -1) {
        wxMessageBox(_("Please select a perspective to load."),
            _("No Selection"), wxOK | wxICON_INFORMATION, this);
        return;
    }

    wxListItem item;
    item.SetId(selectedIndex);
    item.SetColumn(0);
    item.SetMask(wxLIST_MASK_TEXT);
    m_listCtrl->GetItem(item);

    m_selectedPerspective = item.GetText().ToStdString();
    m_shouldLoad = true;

    core::Logger::getInstance().info("User selected perspective to load: {}", m_selectedPerspective);

    EndModal(wxID_OK);
}

void ManagePerspectivesDialog::onDelete([[maybe_unused]] wxCommandEvent& event) {
    long selectedIndex = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

    if (selectedIndex == -1) {
        return;
    }

    wxListItem item;
    item.SetId(selectedIndex);
    item.SetColumn(0);
    item.SetMask(wxLIST_MASK_TEXT);
    m_listCtrl->GetItem(item);

    std::string perspectiveName = item.GetText().ToStdString();

    // Extra safety check
    if (isDefaultPerspective(perspectiveName)) {
        wxMessageBox(_("Default perspectives cannot be deleted."),
            _("Protected Perspective"), wxOK | wxICON_WARNING, this);
        return;
    }

    // Confirm deletion
    int result = wxMessageBox(
        wxString::Format(_("Are you sure you want to delete the perspective '%s'?"), perspectiveName),
        _("Confirm Deletion"),
        wxYES_NO | wxICON_QUESTION, this);

    if (result != wxYES) {
        return;
    }

    // Delete perspective
    auto& perspMgr = PerspectiveManager::getInstance();
    if (perspMgr.deletePerspective(perspectiveName)) {
        core::Logger::getInstance().info("Deleted perspective: {}", perspectiveName);
        wxMessageBox(wxString::Format(_("Perspective '%s' deleted successfully."), perspectiveName),
            _("Deleted"), wxOK | wxICON_INFORMATION, this);

        refreshList();
        updateButtonStates();
    } else {
        wxMessageBox(wxString::Format(_("Failed to delete perspective '%s'."), perspectiveName),
            _("Error"), wxOK | wxICON_ERROR, this);
    }
}

void ManagePerspectivesDialog::onRename([[maybe_unused]] wxCommandEvent& event) {
    long selectedIndex = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

    if (selectedIndex == -1) {
        return;
    }

    wxListItem item;
    item.SetId(selectedIndex);
    item.SetColumn(0);
    item.SetMask(wxLIST_MASK_TEXT);
    m_listCtrl->GetItem(item);

    std::string oldName = item.GetText().ToStdString();

    // Extra safety check
    if (isDefaultPerspective(oldName)) {
        wxMessageBox(_("Default perspectives cannot be renamed."),
            _("Protected Perspective"), wxOK | wxICON_WARNING, this);
        return;
    }

    // Ask for new name
    wxTextEntryDialog dialog(this,
        wxString::Format(_("Enter a new name for '%s':"), oldName),
        _("Rename Perspective"),
        wxString::FromUTF8(oldName),
        wxOK | wxCANCEL);

    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    std::string newName = dialog.GetValue().ToStdString();

    if (newName.empty()) {
        wxMessageBox(_("Perspective name cannot be empty."),
            _("Invalid Name"), wxOK | wxICON_ERROR, this);
        return;
    }

    if (newName == oldName) {
        return; // No change
    }

    // Rename perspective
    auto& perspMgr = PerspectiveManager::getInstance();
    if (perspMgr.renamePerspective(oldName, newName)) {
        core::Logger::getInstance().info("Renamed perspective: '{}' -> '{}'", oldName, newName);
        wxMessageBox(wxString::Format(_("Perspective renamed to '%s'."), newName),
            _("Renamed"), wxOK | wxICON_INFORMATION, this);

        refreshList();
        updateButtonStates();
    } else {
        wxMessageBox(wxString::Format(_("Failed to rename perspective. Name '%s' may already exist."), newName),
            _("Error"), wxOK | wxICON_ERROR, this);
    }
}

void ManagePerspectivesDialog::onClose([[maybe_unused]] wxCommandEvent& event) {
    EndModal(wxID_CANCEL);
}

} // namespace gui
} // namespace kalahari
