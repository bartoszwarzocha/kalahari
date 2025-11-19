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
    // No event handlers (reserved for future)
wxEND_EVENT_TABLE()

// ============================================================================
// Constructor
// ============================================================================

AppearanceSettingsPanel::AppearanceSettingsPanel(wxWindow* parent, SettingsState& state)
    : wxPanel(parent), m_state(state)
{
    core::Logger::getInstance().debug("AppearanceSettingsPanel: Creating panel");

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Create 3 sections (Task #00046: Font size added)
    createThemeSection(mainSizer);
    createFontSizeSection(mainSizer);
    createIconSection(mainSizer);

    SetSizer(mainSizer);
    core::Logger::getInstance().info("AppearanceSettingsPanel: Panel created with 3 sections (Theme, Font Size, Icons)");
}

// ============================================================================
// Section Creators
// ============================================================================

void AppearanceSettingsPanel::createThemeSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Theme");

    // Store description for dynamic wrapping (Task #00043: BWX Reactive)
    m_themeDescription = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Choose the color scheme for the application interface");
    m_themeDescription->SetFont(m_themeDescription->GetFont().MakeItalic());
    box->Add(m_themeDescription, 0, wxALL | wxEXPAND, 5);

    // Theme choice
    wxBoxSizer* themeSizer = new wxBoxSizer(wxHORIZONTAL);
    bwx::gui::StaticText* themeLabel = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Application theme:");
    themeSizer->Add(themeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_themeChoice = new bwx::gui::Choice(box->GetStaticBox(), wxID_ANY);
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

    // Note about restart (store for dynamic wrapping, Task #00043: BWX Reactive)
    m_restartNote = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Note: Theme changes require application restart to fully apply.");
    wxFont noteFont = m_restartNote->GetFont();
    noteFont.MakeItalic();
    noteFont.SetPointSize(noteFont.GetPointSize() - 1);
    m_restartNote->SetFont(noteFont);
    m_restartNote->SetForegroundColour(wxColour(100, 100, 100));
    box->Add(m_restartNote, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

void AppearanceSettingsPanel::createFontSizeSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Font Size");

    // Description (Task #00046)
    m_fontSizeDescription = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Adjust text size throughout the application");
    m_fontSizeDescription->SetFont(m_fontSizeDescription->GetFont().MakeItalic());
    box->Add(m_fontSizeDescription, 0, wxALL | wxEXPAND, 5);

    // Font size choice
    wxBoxSizer* fontSizeSizer = new wxBoxSizer(wxHORIZONTAL);
    bwx::gui::StaticText* fontSizeLabel = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Application font size:");
    fontSizeLabel->Wrap(200);  // Task #00046: Prevent text overflow with large fonts
    fontSizeSizer->Add(fontSizeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_fontSizeChoice = new bwx::gui::Choice(box->GetStaticBox(), wxID_ANY);
    m_fontSizeChoice->Append("Extra Small");  // Index 0 (70%)
    m_fontSizeChoice->Append("Small");        // Index 1 (85%)
    m_fontSizeChoice->Append("Normal");       // Index 2 (100%) - DEFAULT
    m_fontSizeChoice->Append("Medium");       // Index 3 (115%)
    m_fontSizeChoice->Append("Large");        // Index 4 (130%)
    m_fontSizeChoice->Append("Extra Large");  // Index 5 (150%)

    // Select current preset from state (default: Normal = 2)
    m_fontSizeChoice->SetSelection(m_state.fontSizePreset);
    m_fontSizeChoice->SetToolTip("Choose font size for all text in the application");
    fontSizeSizer->Add(m_fontSizeChoice, 1, wxEXPAND, 0);

    box->Add(fontSizeSizer, 0, wxALL | wxEXPAND, 5);

    // Note about immediate apply
    bwx::gui::StaticText* note = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Note: Font size changes apply immediately when you click Apply.");
    wxFont noteFont = note->GetFont();
    noteFont.MakeItalic();
    noteFont.SetPointSize(noteFont.GetPointSize() - 1);
    note->SetFont(noteFont);
    note->SetForegroundColour(wxColour(100, 100, 100));
    box->Add(note, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

void AppearanceSettingsPanel::createIconSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Icons");

    // Store description for dynamic wrapping (Task #00043: BWX Reactive)
    m_iconDescription = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Configure icon display size throughout the application");
    m_iconDescription->SetFont(m_iconDescription->GetFont().MakeItalic());
    box->Add(m_iconDescription, 0, wxALL | wxEXPAND, 5);

    // Icon size choice
    wxBoxSizer* iconSizer = new wxBoxSizer(wxHORIZONTAL);
    bwx::gui::StaticText* iconLabel = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Icon size:");
    iconSizer->Add(iconLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_iconSizeChoice = new bwx::gui::Choice(box->GetStaticBox(), wxID_ANY);
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

    // Font size preset (Task #00046)
    int fontSizeSelection = m_fontSizeChoice->GetSelection();
    // Map directly: selection index = preset value (0-5)
    if (fontSizeSelection >= 0 && fontSizeSelection <= 5) {
        m_state.fontSizePreset = fontSizeSelection;
    } else {
        m_state.fontSizePreset = 2;  // Default: Normal
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

    core::Logger::getInstance().info("AppearanceSettingsPanel: Saved 3 settings values (theme={}, fontSizePreset={}, iconSize={})",
        m_state.themeName.ToStdString(), m_state.fontSizePreset, m_state.iconSize);
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
        if (m_fontSizeDescription && m_fontSizeDescription->IsShown()) {  // Task #00046
            m_fontSizeDescription->Wrap(availableWidth);
        }
        if (m_iconDescription && m_iconDescription->IsShown()) {
            m_iconDescription->Wrap(availableWidth);
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

} // namespace gui
} // namespace kalahari
