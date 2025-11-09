/// @file log_settings_panel.cpp
/// @brief Implementation of LogSettingsPanel

#include "log_settings_panel.h"
#include "settings_dialog.h"
#include <kalahari/core/logger.h>

namespace kalahari {
namespace gui {

// ============================================================================
// Event Table
// ============================================================================

wxBEGIN_EVENT_TABLE(LogSettingsPanel, wxPanel)
    // Event handlers will be added as needed
wxEND_EVENT_TABLE()

// ============================================================================
// Constructor
// ============================================================================

LogSettingsPanel::LogSettingsPanel(wxWindow* parent, SettingsState& state)
    : wxPanel(parent), m_state(state)
{
    core::Logger::getInstance().debug("LogSettingsPanel: Creating panel");

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Create 2 sections
    createBufferSection(mainSizer);
    createAppearanceSection(mainSizer);

    SetSizer(mainSizer);
    core::Logger::getInstance().info("LogSettingsPanel: Panel created with 2 sections");
}

// ============================================================================
// Section Creators
// ============================================================================

void LogSettingsPanel::createBufferSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Buffer Settings");

    wxStaticText* description = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Configure the ring buffer for log entries (older entries are automatically removed)");
    description->SetFont(description->GetFont().MakeItalic());
    box->Add(description, 0, wxALL | wxEXPAND, 5);

    // Buffer size spinner
    wxBoxSizer* bufferSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* bufferLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Buffer size (entries):");
    bufferSizer->Add(bufferLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_bufferSizeSpinner = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY);
    m_bufferSizeSpinner->SetRange(1, 1000);
    m_bufferSizeSpinner->SetValue(m_state.logBufferSize);
    m_bufferSizeSpinner->SetToolTip("Maximum number of log entries to keep in memory (1-1000)");
    bufferSizer->Add(m_bufferSizeSpinner, 1, wxEXPAND, 0);

    box->Add(bufferSizer, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

void LogSettingsPanel::createAppearanceSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Appearance");

    // Background color picker
    wxBoxSizer* bgColorSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* bgColorLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Background color:");
    bgColorSizer->Add(bgColorLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_backgroundColorPicker = new wxColourPickerCtrl(box->GetStaticBox(), wxID_ANY,
        m_state.logBackgroundColor);
    m_backgroundColorPicker->SetToolTip("Background color for the log panel");
    bgColorSizer->Add(m_backgroundColorPicker, 0, wxEXPAND, 0);

    box->Add(bgColorSizer, 0, wxALL | wxEXPAND, 5);

    // Text color picker
    wxBoxSizer* textColorSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* textColorLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Text color:");
    textColorSizer->Add(textColorLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_textColorPicker = new wxColourPickerCtrl(box->GetStaticBox(), wxID_ANY,
        m_state.logTextColor);
    m_textColorPicker->SetToolTip("Text color for log entries");
    textColorSizer->Add(m_textColorPicker, 0, wxEXPAND, 0);

    box->Add(textColorSizer, 0, wxALL | wxEXPAND, 5);

    // Font size spinner
    wxBoxSizer* fontSizeSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* fontSizeLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Font size (pt):");
    fontSizeSizer->Add(fontSizeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_fontSizeSpinner = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY);
    m_fontSizeSpinner->SetRange(6, 20);
    m_fontSizeSpinner->SetValue(m_state.logFontSize);
    m_fontSizeSpinner->SetToolTip("Font size for log text in points (6-20)");
    fontSizeSizer->Add(m_fontSizeSpinner, 1, wxEXPAND, 0);

    box->Add(fontSizeSizer, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

// ============================================================================
// Public Methods
// ============================================================================

void LogSettingsPanel::saveToState() {
    core::Logger::getInstance().debug("LogSettingsPanel: Saving values to state");

    // Buffer Settings
    m_state.logBufferSize = m_bufferSizeSpinner->GetValue();

    // Appearance
    m_state.logBackgroundColor = m_backgroundColorPicker->GetColour();
    m_state.logTextColor = m_textColorPicker->GetColour();
    m_state.logFontSize = m_fontSizeSpinner->GetValue();

    core::Logger::getInstance().info("LogSettingsPanel: Saved {} settings values", 4);
}

} // namespace gui
} // namespace kalahari
