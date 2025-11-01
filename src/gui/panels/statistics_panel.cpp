/// @file statistics_panel.cpp
/// @brief Implementation of StatisticsPanel

#include "kalahari/gui/panels/statistics_panel.h"
#include <kalahari/core/logger.h>

namespace kalahari {
namespace gui {

wxBEGIN_EVENT_TABLE(StatisticsPanel, wxPanel)
wxEND_EVENT_TABLE()

StatisticsPanel::StatisticsPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    core::Logger::getInstance().info("Creating Statistics panel (stub)");
    setupLayout();
}

void StatisticsPanel::setupLayout() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Placeholder statistics
    wxStaticBoxSizer* statsBox = new wxStaticBoxSizer(wxVERTICAL, this, "Writing Statistics");

    wxStaticText* wordsLabel = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY, "Word Count: 1,234");
    statsBox->Add(wordsLabel, 0, wxALL, 5);

    wxStaticText* charsLabel = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY, "Characters: 7,890");
    statsBox->Add(charsLabel, 0, wxALL, 5);

    wxStaticText* pagesLabel = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY, "Pages: ~5");
    statsBox->Add(pagesLabel, 0, wxALL, 5);

    wxStaticText* readTimeLabel = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY, "Reading Time: ~6 min");
    statsBox->Add(readTimeLabel, 0, wxALL, 5);

    statsBox->Add(0, 10, 0, 0, 0); // Spacer

    wxStaticText* infoLabel = new wxStaticText(statsBox->GetStaticBox(), wxID_ANY,
        "Full statistics implementation\nwith charts coming in Phase 1.");
    statsBox->Add(infoLabel, 0, wxALL, 5);

    mainSizer->Add(statsBox, 0, wxALL | wxEXPAND, 5);
    mainSizer->AddStretchSpacer(1); // Push content to top
    SetSizer(mainSizer);
}

} // namespace gui
} // namespace kalahari
