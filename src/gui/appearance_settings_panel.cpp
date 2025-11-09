/// @file appearance_settings_panel.cpp
/// @brief Implementation of AppearanceSettingsPanel

#include "appearance_settings_panel.h"
#include "settings_dialog.h"
#include <kalahari/core/logger.h>

namespace kalahari {
namespace gui {

// ============================================================================
// Event Table
// ============================================================================

wxBEGIN_EVENT_TABLE(AppearanceSettingsPanel, wxPanel)
    // Event handlers will be added as needed
wxEND_EVENT_TABLE()

// ============================================================================
// Constructor
// ============================================================================

AppearanceSettingsPanel::AppearanceSettingsPanel(wxWindow* parent, SettingsState& state)
    : wxPanel(parent), m_state(state)
{
    core::Logger::getInstance().debug("AppearanceSettingsPanel: Creating panel");

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Create 3 sections
    createThemeSection(mainSizer);
    createIconSection(mainSizer);
    createTypographySection(mainSizer);

    SetSizer(mainSizer);
    core::Logger::getInstance().info("AppearanceSettingsPanel: Panel created with 3 sections");
}

// ============================================================================
// Section Creators
// ============================================================================

void AppearanceSettingsPanel::createThemeSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Theme");

    wxStaticText* description = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Choose the color scheme for the application interface");
    description->SetFont(description->GetFont().MakeItalic());
    box->Add(description, 0, wxALL | wxEXPAND, 5);

    // Theme choice
    wxBoxSizer* themeSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* themeLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Application theme:");
    themeSizer->Add(themeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_themeChoice = new wxChoice(box->GetStaticBox(), wxID_ANY);
    m_themeChoice->Append("System");  // Index 0
    m_themeChoice->Append("Light");   // Index 1
    m_themeChoice->Append("Dark");    // Index 2

    // Select current theme
    if (m_state.themeName == "Light") {
        m_themeChoice->SetSelection(1);
    } else if (m_state.themeName == "Dark") {
        m_themeChoice->SetSelection(2);
    } else {
        m_themeChoice->SetSelection(0); // System (default)
    }

    m_themeChoice->SetToolTip("Choose between Light, Dark, or follow System theme");
    themeSizer->Add(m_themeChoice, 1, wxEXPAND, 0);

    box->Add(themeSizer, 0, wxALL | wxEXPAND, 5);

    // Note about restart
    wxStaticText* restartNote = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Note: Theme changes require application restart to fully apply.");
    wxFont noteFont = restartNote->GetFont();
    noteFont.MakeItalic();
    noteFont.SetPointSize(noteFont.GetPointSize() - 1);
    restartNote->SetFont(noteFont);
    restartNote->SetForegroundColour(wxColour(100, 100, 100));
    box->Add(restartNote, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

void AppearanceSettingsPanel::createIconSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Icons");

    wxStaticText* description = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Configure icon display size throughout the application");
    description->SetFont(description->GetFont().MakeItalic());
    box->Add(description, 0, wxALL | wxEXPAND, 5);

    // Icon size choice
    wxBoxSizer* iconSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* iconLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Icon size:");
    iconSizer->Add(iconLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_iconSizeChoice = new wxChoice(box->GetStaticBox(), wxID_ANY);
    m_iconSizeChoice->Append("Small (16px)");   // Index 0
    m_iconSizeChoice->Append("Medium (24px)");  // Index 1
    m_iconSizeChoice->Append("Large (32px)");   // Index 2
    m_iconSizeChoice->Append("Extra Large (48px)"); // Index 3

    // Select current icon size
    switch (m_state.iconSize) {
        case 16: m_iconSizeChoice->SetSelection(0); break;
        case 24: m_iconSizeChoice->SetSelection(1); break;
        case 32: m_iconSizeChoice->SetSelection(2); break;
        case 48: m_iconSizeChoice->SetSelection(3); break;
        default: m_iconSizeChoice->SetSelection(1); break; // 24px default
    }

    m_iconSizeChoice->SetToolTip("Adjust the size of toolbar and menu icons");
    iconSizer->Add(m_iconSizeChoice, 1, wxEXPAND, 0);

    box->Add(iconSizer, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

void AppearanceSettingsPanel::createTypographySection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Typography");

    wxStaticText* description = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Adjust text size for better readability");
    description->SetFont(description->GetFont().MakeItalic());
    box->Add(description, 0, wxALL | wxEXPAND, 5);

    // Font scaling slider
    wxBoxSizer* fontSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* fontLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Font scaling:");
    fontSizer->Add(fontLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_fontScalingSpinner = new wxSpinCtrlDouble(box->GetStaticBox(), wxID_ANY);
    m_fontScalingSpinner->SetRange(0.8, 1.5);
    m_fontScalingSpinner->SetIncrement(0.05);
    m_fontScalingSpinner->SetDigits(2);
    m_fontScalingSpinner->SetValue(m_state.fontScaling);
    m_fontScalingSpinner->SetToolTip("Scale all UI fonts (0.8x = smaller, 1.5x = larger)");
    fontSizer->Add(m_fontScalingSpinner, 1, wxEXPAND, 0);

    wxStaticText* fontUnit = new wxStaticText(box->GetStaticBox(), wxID_ANY, "x");
    fontSizer->Add(fontUnit, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);

    box->Add(fontSizer, 0, wxALL | wxEXPAND, 5);

    // Example text
    wxStaticText* exampleText = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Example: This is how text will appear at current scaling");
    wxFont exampleFont = exampleText->GetFont();
    exampleFont.SetPointSize(static_cast<int>(exampleFont.GetPointSize() * m_state.fontScaling));
    exampleText->SetFont(exampleFont);
    box->Add(exampleText, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

// ============================================================================
// Public Methods
// ============================================================================

void AppearanceSettingsPanel::saveToState() {
    core::Logger::getInstance().debug("AppearanceSettingsPanel: Saving values to state");

    // Theme
    int themeSelection = m_themeChoice->GetSelection();
    switch (themeSelection) {
        case 0: m_state.themeName = "System"; break;
        case 1: m_state.themeName = "Light"; break;
        case 2: m_state.themeName = "Dark"; break;
        default: m_state.themeName = "System"; break;
    }

    // Icon size
    int iconSelection = m_iconSizeChoice->GetSelection();
    switch (iconSelection) {
        case 0: m_state.iconSize = 16; break;
        case 1: m_state.iconSize = 24; break;
        case 2: m_state.iconSize = 32; break;
        case 3: m_state.iconSize = 48; break;
        default: m_state.iconSize = 24; break;
    }

    // Font scaling
    m_state.fontScaling = m_fontScalingSpinner->GetValue();

    core::Logger::getInstance().info("AppearanceSettingsPanel: Saved 3 settings values (theme={}, iconSize={}, fontScaling={})",
        m_state.themeName.ToStdString(), m_state.iconSize, m_state.fontScaling);
}

} // namespace gui
} // namespace kalahari
