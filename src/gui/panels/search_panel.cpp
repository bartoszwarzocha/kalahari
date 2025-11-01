/// @file search_panel.cpp
/// @brief Implementation of SearchPanel

#include "kalahari/gui/panels/search_panel.h"
#include <kalahari/core/logger.h>

namespace kalahari {
namespace gui {

enum {
    ID_FIND_BUTTON = wxID_HIGHEST + 100,
    ID_REPLACE_BUTTON
};

wxBEGIN_EVENT_TABLE(SearchPanel, wxPanel)
    EVT_BUTTON(ID_FIND_BUTTON, SearchPanel::onFind)
    EVT_BUTTON(ID_REPLACE_BUTTON, SearchPanel::onReplace)
wxEND_EVENT_TABLE()

SearchPanel::SearchPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    core::Logger::getInstance().info("Creating Search panel (stub)");
    setupLayout();
}

void SearchPanel::setupLayout() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer* searchBox = new wxStaticBoxSizer(wxVERTICAL, this, "Find and Replace");

    // Find controls
    wxStaticText* findLabel = new wxStaticText(searchBox->GetStaticBox(), wxID_ANY, "Find:");
    searchBox->Add(findLabel, 0, wxALL, 5);

    wxBoxSizer* findSizer = new wxBoxSizer(wxHORIZONTAL);
    m_searchCtrl = new wxTextCtrl(searchBox->GetStaticBox(), wxID_ANY, "");
    findSizer->Add(m_searchCtrl, 1, wxALL | wxEXPAND, 0);

    wxButton* findBtn = new wxButton(searchBox->GetStaticBox(), ID_FIND_BUTTON, "Find");
    findSizer->Add(findBtn, 0, wxLEFT, 5);

    searchBox->Add(findSizer, 0, wxALL | wxEXPAND, 5);

    // Replace controls
    wxStaticText* replaceLabel = new wxStaticText(searchBox->GetStaticBox(), wxID_ANY, "Replace with:");
    searchBox->Add(replaceLabel, 0, wxALL, 5);

    wxBoxSizer* replaceSizer = new wxBoxSizer(wxHORIZONTAL);
    m_replaceCtrl = new wxTextCtrl(searchBox->GetStaticBox(), wxID_ANY, "");
    replaceSizer->Add(m_replaceCtrl, 1, wxALL | wxEXPAND, 0);

    wxButton* replaceBtn = new wxButton(searchBox->GetStaticBox(), ID_REPLACE_BUTTON, "Replace");
    replaceSizer->Add(replaceBtn, 0, wxLEFT, 5);

    searchBox->Add(replaceSizer, 0, wxALL | wxEXPAND, 5);

    // Info text
    wxStaticText* infoLabel = new wxStaticText(searchBox->GetStaticBox(), wxID_ANY,
        "Full search/replace implementation\ncoming in Phase 1.");
    searchBox->Add(infoLabel, 0, wxALL, 10);

    mainSizer->Add(searchBox, 0, wxALL | wxEXPAND, 5);
    SetSizer(mainSizer);
}

void SearchPanel::onFind([[maybe_unused]] wxCommandEvent& event) {
    wxString searchText = m_searchCtrl->GetValue();
    if (searchText.IsEmpty()) {
        wxMessageBox("Please enter text to search for.", "Search", wxOK | wxICON_INFORMATION, this);
        return;
    }
    core::Logger::getInstance().info("Search requested: {}", searchText.ToStdString());
    wxMessageBox(wxString::Format("Search for '%s' - Not implemented yet.", searchText),
        "Search Stub", wxOK | wxICON_INFORMATION, this);
}

void SearchPanel::onReplace([[maybe_unused]] wxCommandEvent& event) {
    wxString searchText = m_searchCtrl->GetValue();
    wxString replaceText = m_replaceCtrl->GetValue();
    core::Logger::getInstance().info("Replace requested: '{}' -> '{}'",
        searchText.ToStdString(), replaceText.ToStdString());
    wxMessageBox(wxString::Format("Replace '%s' with '%s' - Not implemented yet.", searchText, replaceText),
        "Replace Stub", wxOK | wxICON_INFORMATION, this);
}

} // namespace gui
} // namespace kalahari
