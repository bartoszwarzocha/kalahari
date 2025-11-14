/// @file assistant_panel.cpp
/// @brief Implementation of AssistantPanel

#include "kalahari/gui/panels/assistant_panel.h"
#include <kalahari/core/logger.h>

namespace kalahari {
namespace gui {

enum {
    ID_SEND_BUTTON = wxID_HIGHEST + 200
};

wxBEGIN_EVENT_TABLE(AssistantPanel, wxPanel)
    EVT_BUTTON(ID_SEND_BUTTON, AssistantPanel::onSend)
wxEND_EVENT_TABLE()

AssistantPanel::AssistantPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    core::Logger::getInstance().info("Creating Assistant panel (stub)");
    setupLayout();
}

void AssistantPanel::setupLayout() {
    core::Logger::getInstance().debug("AssistantPanel::setupLayout() - Creating main sizer");
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    core::Logger::getInstance().debug("AssistantPanel::setupLayout() - Creating text control");
    // Simple placeholder text
    wxStaticText* placeholder = new wxStaticText(this, wxID_ANY,
        "AI Writing Assistant\n\n(Full interface coming in Phase 2)");
    mainSizer->Add(placeholder, 1, wxALL | wxALIGN_CENTER, 20);

    core::Logger::getInstance().debug("AssistantPanel::setupLayout() - Setting sizer");
    SetSizer(mainSizer);
    core::Logger::getInstance().debug("AssistantPanel::setupLayout() - DONE");
}

void AssistantPanel::onSend([[maybe_unused]] wxCommandEvent& event) {
    wxString message = m_inputCtrl->GetValue();
    if (message.IsEmpty()) {
        return;
    }

    core::Logger::getInstance().info("Assistant message: {}", message.ToStdString());

    // Add to chat display
    m_chatDisplay->AppendText(wxString::Format("\n\nYou: %s\n", message));
    m_chatDisplay->AppendText("Lion: AI integration coming in Phase 2!\n");

    m_inputCtrl->Clear();
}

} // namespace gui
} // namespace kalahari
