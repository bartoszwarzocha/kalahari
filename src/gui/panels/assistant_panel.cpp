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
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer* assistantBox = new wxStaticBoxSizer(wxVERTICAL, this, "AI Writing Assistant");

    // Chat display area
    m_chatDisplay = new wxTextCtrl(assistantBox->GetStaticBox(), wxID_ANY,
        "🦁 Lion: Welcome to Kalahari Writer's IDE!\n\n"
        "This is the AI Assistant panel stub.\n\n"
        "In Phase 2, you'll be able to:\n"
        "- Ask writing questions\n"
        "- Get character development suggestions\n"
        "- Brainstorm plot ideas\n"
        "- Receive grammar tips\n\n"
        "Available assistants:\n"
        "🦁 Lion - General writing mentor\n"
        "🐱 Meerkat - Detail-oriented editor\n"
        "🐘 Elephant - Plot and structure expert\n"
        "🐆 Cheetah - Speed writing coach",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
    assistantBox->Add(m_chatDisplay, 1, wxALL | wxEXPAND, 5);

    // Input area
    wxBoxSizer* inputSizer = new wxBoxSizer(wxHORIZONTAL);
    m_inputCtrl = new wxTextCtrl(assistantBox->GetStaticBox(), wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    inputSizer->Add(m_inputCtrl, 1, wxALL | wxEXPAND, 0);

    wxButton* sendBtn = new wxButton(assistantBox->GetStaticBox(), ID_SEND_BUTTON, "Send");
    inputSizer->Add(sendBtn, 0, wxLEFT, 5);

    assistantBox->Add(inputSizer, 0, wxALL | wxEXPAND, 5);

    mainSizer->Add(assistantBox, 1, wxALL | wxEXPAND, 5);
    SetSizer(mainSizer);
}

void AssistantPanel::onSend([[maybe_unused]] wxCommandEvent& event) {
    wxString message = m_inputCtrl->GetValue();
    if (message.IsEmpty()) {
        return;
    }

    core::Logger::getInstance().info("Assistant message: {}", message.ToStdString());

    // Add to chat display
    m_chatDisplay->AppendText(wxString::Format("\n\nYou: %s\n", message));
    m_chatDisplay->AppendText("🦁 Lion: AI integration coming in Phase 2!\n");

    m_inputCtrl->Clear();
}

} // namespace gui
} // namespace kalahari
