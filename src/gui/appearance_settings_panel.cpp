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
    EVT_SPINCTRLDOUBLE(wxID_ANY, AppearanceSettingsPanel::onFontScalingChanged)
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

    // Store description for dynamic wrapping
    m_themeDescription = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Choose the color scheme for the application interface");
    m_themeDescription->SetFont(m_themeDescription->GetFont().MakeItalic());
    box->Add(m_themeDescription, 0, wxALL | wxEXPAND, 5);

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

    // Note about restart (store for dynamic wrapping)
    m_restartNote = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Note: Theme changes require application restart to fully apply.");
    wxFont noteFont = m_restartNote->GetFont();
    noteFont.MakeItalic();
    noteFont.SetPointSize(noteFont.GetPointSize() - 1);
    m_restartNote->SetFont(noteFont);
    m_restartNote->SetForegroundColour(wxColour(100, 100, 100));
    box->Add(m_restartNote, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

void AppearanceSettingsPanel::createIconSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Icons");

    // Store description for dynamic wrapping
    m_iconDescription = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Configure icon display size throughout the application");
    m_iconDescription->SetFont(m_iconDescription->GetFont().MakeItalic());
    box->Add(m_iconDescription, 0, wxALL | wxEXPAND, 5);

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

    // Store description for dynamic wrapping
    m_typographyDescription = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Adjust text size for better readability");
    m_typographyDescription->SetFont(m_typographyDescription->GetFont().MakeItalic());
    box->Add(m_typographyDescription, 0, wxALL | wxEXPAND, 5);

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

    // Example text (store for dynamic wrapping)
    m_exampleText = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Example: This is how text will appear at current scaling");
    wxFont exampleFont = m_exampleText->GetFont();

    // Store original font size for proper scaling in onFontScalingChanged()
    m_exampleTextBaseFontSize = exampleFont.GetPointSize();

    // Apply current scaling from state
    exampleFont.SetPointSize(static_cast<int>(m_exampleTextBaseFontSize * m_state.fontScaling));
    m_exampleText->SetFont(exampleFont);
    box->Add(m_exampleText, 0, wxALL | wxEXPAND, 5);

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

// ============================================================================
// Event Handlers
// ============================================================================

void AppearanceSettingsPanel::onSize(wxSizeEvent& event) {
    // Dynamic text wrapping mechanism (consistent with LogSettingsPanel)
    // IMPORTANT: Only process if panel is shown (avoid processing during construction)
    if (!IsShown()) {
        event.Skip();
        return;
    }

    int panelWidth = GetClientSize().GetWidth();
    int availableWidth = panelWidth - 40;  // Account for borders and margins

    if (availableWidth > 100) {  // Minimum reasonable width
        // Wrap all description and note texts
        if (m_themeDescription && m_themeDescription->IsShown()) {
            m_themeDescription->Wrap(availableWidth);
        }
        if (m_restartNote && m_restartNote->IsShown()) {
            m_restartNote->Wrap(availableWidth);
        }
        if (m_iconDescription && m_iconDescription->IsShown()) {
            m_iconDescription->Wrap(availableWidth);
        }
        if (m_typographyDescription && m_typographyDescription->IsShown()) {
            m_typographyDescription->Wrap(availableWidth);
        }
        if (m_exampleText && m_exampleText->IsShown()) {
            m_exampleText->Wrap(availableWidth);
        }

        // Trigger layout recalculation
        Layout();

        // Notify parent (ContentPanel) to update scrollbars
        if (GetParent()) {
            GetParent()->Layout();
            if (auto* scrolled = dynamic_cast<wxScrolledWindow*>(GetParent())) {
                scrolled->FitInside();
            }
        }
    }

    event.Skip();
}

void AppearanceSettingsPanel::onFontScalingChanged([[maybe_unused]] wxSpinDoubleEvent& event) {
    // Update example text font size dynamically when spinner value changes
    if (m_exampleText && m_fontScalingSpinner) {
        double newScaling = m_fontScalingSpinner->GetValue();

        // Use stored base font size for accurate scaling
        wxFont exampleFont = m_exampleText->GetFont();
        exampleFont.SetPointSize(static_cast<int>(m_exampleTextBaseFontSize * newScaling));

        m_exampleText->SetFont(exampleFont);
        m_exampleText->Refresh();

        core::Logger::getInstance().debug("Font scaling changed to {} - example text updated", newScaling);

        // Trigger layout recalculation
        Layout();
        if (GetParent()) {
            GetParent()->Layout();
            if (auto* scrolled = dynamic_cast<wxScrolledWindow*>(GetParent())) {
                scrolled->FitInside();
            }
        }
    }
}

} // namespace gui
} // namespace kalahari
