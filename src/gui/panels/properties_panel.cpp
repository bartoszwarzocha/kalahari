/// @file properties_panel.cpp
/// @brief Implementation of PropertiesPanel

#include "kalahari/gui/panels/properties_panel.h"
#include <kalahari/core/logger.h>

namespace kalahari {
namespace gui {

wxBEGIN_EVENT_TABLE(PropertiesPanel, wxPanel)
wxEND_EVENT_TABLE()

PropertiesPanel::PropertiesPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    core::Logger::getInstance().info("Creating Properties panel (stub)");
    setupLayout();
}

void PropertiesPanel::setupLayout() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Placeholder content with wxStaticBoxSizer
    wxStaticBoxSizer* metadataBox = new wxStaticBoxSizer(wxVERTICAL, this, "Chapter Properties");

    wxStaticText* titleLabel = new wxStaticText(metadataBox->GetStaticBox(), wxID_ANY, "Title:");
    metadataBox->Add(titleLabel, 0, wxALL, 5);

    wxTextCtrl* titleCtrl = new wxTextCtrl(metadataBox->GetStaticBox(), wxID_ANY, "Chapter 1");
    metadataBox->Add(titleCtrl, 0, wxALL | wxEXPAND, 5);

    wxStaticText* notesLabel = new wxStaticText(metadataBox->GetStaticBox(), wxID_ANY, "Notes:");
    metadataBox->Add(notesLabel, 0, wxALL, 5);

    wxTextCtrl* notesCtrl = new wxTextCtrl(metadataBox->GetStaticBox(), wxID_ANY,
        "This is a placeholder for chapter metadata.\n\nFull wxPropertyGrid implementation coming in Phase 1.",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE);
    metadataBox->Add(notesCtrl, 1, wxALL | wxEXPAND, 5);

    mainSizer->Add(metadataBox, 1, wxALL | wxEXPAND, 5);
    SetSizer(mainSizer);
}

} // namespace gui
} // namespace kalahari
