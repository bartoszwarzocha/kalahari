/// @file editor_settings_panel.cpp
/// @brief Implementation of EditorSettingsPanel

#include "editor_settings_panel.h"
#include "settings_dialog.h"
#include <kalahari/core/logger.h>

namespace kalahari {
namespace gui {

// ============================================================================
// Event Table
// ============================================================================

wxBEGIN_EVENT_TABLE(EditorSettingsPanel, wxPanel)
    // Event handlers will be added as needed
wxEND_EVENT_TABLE()

// ============================================================================
// Constructor
// ============================================================================

EditorSettingsPanel::EditorSettingsPanel(wxWindow* parent, SettingsState& state)
    : wxPanel(parent), m_state(state)
{
    core::Logger::getInstance().debug("EditorSettingsPanel: Creating panel");

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Create 4 sections
    createCursorSection(mainSizer);
    createMarginsSection(mainSizer);
    createRenderingSection(mainSizer);
    createBehaviorSection(mainSizer);

    SetSizer(mainSizer);
    core::Logger::getInstance().info("EditorSettingsPanel: Panel created with 4 sections");
}

// ============================================================================
// Section Creators
// ============================================================================

void EditorSettingsPanel::createCursorSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Cursor & Caret");

    // Caret blink checkbox
    m_caretBlinkCheckbox = new wxCheckBox(box->GetStaticBox(), wxID_ANY,
        "Enable caret blinking");
    m_caretBlinkCheckbox->SetValue(m_state.caretBlinkEnabled);
    m_caretBlinkCheckbox->SetToolTip("Toggle caret blinking animation (fixes bug #5)");
    box->Add(m_caretBlinkCheckbox, 0, wxALL | wxEXPAND, 5);

    // Blink rate
    wxBoxSizer* blinkRateSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* blinkRateLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Blink rate (ms):");
    blinkRateSizer->Add(blinkRateLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_caretBlinkRateSpinner = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY);
    m_caretBlinkRateSpinner->SetRange(100, 2000);
    m_caretBlinkRateSpinner->SetValue(m_state.caretBlinkRate);
    m_caretBlinkRateSpinner->SetToolTip("Blink interval in milliseconds (100-2000)");
    blinkRateSizer->Add(m_caretBlinkRateSpinner, 1, wxEXPAND, 0);

    box->Add(blinkRateSizer, 0, wxALL | wxEXPAND, 5);

    // Caret width
    wxBoxSizer* caretWidthSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* caretWidthLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Caret width (px):");
    caretWidthSizer->Add(caretWidthLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_caretWidthSpinner = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY);
    m_caretWidthSpinner->SetRange(1, 5);
    m_caretWidthSpinner->SetValue(m_state.caretWidth);
    m_caretWidthSpinner->SetToolTip("Caret line thickness in pixels (1-5)");
    caretWidthSizer->Add(m_caretWidthSpinner, 1, wxEXPAND, 0);

    box->Add(caretWidthSizer, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

void EditorSettingsPanel::createMarginsSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Margins & Padding");

    wxStaticText* description = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Configure text margins around editor content (fixes bug #8)");
    description->SetFont(description->GetFont().MakeItalic());
    box->Add(description, 0, wxALL | wxEXPAND, 5);

    // Grid for 4 margin spinners (4 rows × 2 cols = 8 slots)
    wxFlexGridSizer* grid = new wxFlexGridSizer(4, 2, 5, 10);
    grid->AddGrowableCol(1, 1);

    // Left margin
    grid->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, "Left:"),
        0, wxALIGN_CENTER_VERTICAL);
    m_marginLeftSpinner = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY);
    m_marginLeftSpinner->SetRange(0, 100);
    m_marginLeftSpinner->SetValue(m_state.marginLeft);
    grid->Add(m_marginLeftSpinner, 1, wxEXPAND);

    // Right margin
    grid->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, "Right:"),
        0, wxALIGN_CENTER_VERTICAL);
    m_marginRightSpinner = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY);
    m_marginRightSpinner->SetRange(0, 100);
    m_marginRightSpinner->SetValue(m_state.marginRight);
    grid->Add(m_marginRightSpinner, 1, wxEXPAND);

    // Top margin
    grid->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, "Top:"),
        0, wxALIGN_CENTER_VERTICAL);
    m_marginTopSpinner = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY);
    m_marginTopSpinner->SetRange(0, 100);
    m_marginTopSpinner->SetValue(m_state.marginTop);
    grid->Add(m_marginTopSpinner, 1, wxEXPAND);

    // Bottom margin
    grid->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, "Bottom:"),
        0, wxALIGN_CENTER_VERTICAL);
    m_marginBottomSpinner = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY);
    m_marginBottomSpinner->SetRange(0, 100);
    m_marginBottomSpinner->SetValue(m_state.marginBottom);
    grid->Add(m_marginBottomSpinner, 1, wxEXPAND);

    box->Add(grid, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

void EditorSettingsPanel::createRenderingSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Rendering");

    // Line spacing
    wxBoxSizer* lineSpacingSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* lineSpacingLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Line spacing:");
    lineSpacingSizer->Add(lineSpacingLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_lineSpacingSpinner = new wxSpinCtrlDouble(box->GetStaticBox(), wxID_ANY);
    m_lineSpacingSpinner->SetRange(1.0, 3.0);
    m_lineSpacingSpinner->SetIncrement(0.1);
    m_lineSpacingSpinner->SetDigits(1);
    m_lineSpacingSpinner->SetValue(m_state.lineSpacing);
    m_lineSpacingSpinner->SetToolTip("Line spacing multiplier (1.0 = single, 1.5 = 1.5x, 2.0 = double)");
    lineSpacingSizer->Add(m_lineSpacingSpinner, 1, wxEXPAND, 0);

    box->Add(lineSpacingSizer, 0, wxALL | wxEXPAND, 5);

    // Selection opacity slider
    wxBoxSizer* opacitySizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* opacityLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Selection opacity:");
    opacitySizer->Add(opacityLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_selectionOpacitySlider = new wxSlider(box->GetStaticBox(), wxID_ANY,
        m_state.selectionOpacity, 0, 255,
        wxDefaultPosition, wxDefaultSize,
        wxSL_HORIZONTAL | wxSL_LABELS);
    m_selectionOpacitySlider->SetToolTip("Selection transparency (0=transparent, 255=opaque) - fixes bug #6");
    opacitySizer->Add(m_selectionOpacitySlider, 1, wxEXPAND, 0);

    box->Add(opacitySizer, 0, wxALL | wxEXPAND, 5);

    // Selection color picker
    wxBoxSizer* colorSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* colorLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Selection color:");
    colorSizer->Add(colorLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_selectionColorPicker = new wxColourPickerCtrl(box->GetStaticBox(), wxID_ANY,
        m_state.selectionColor);
    m_selectionColorPicker->SetToolTip("Background color for selected text");
    colorSizer->Add(m_selectionColorPicker, 0, wxEXPAND, 0);

    box->Add(colorSizer, 0, wxALL | wxEXPAND, 5);

    // Antialiasing checkbox
    m_antialiasingCheckbox = new wxCheckBox(box->GetStaticBox(), wxID_ANY,
        "Enable text antialiasing");
    m_antialiasingCheckbox->SetValue(m_state.antialiasing);
    m_antialiasingCheckbox->SetToolTip("Smooth text rendering (may affect performance)");
    box->Add(m_antialiasingCheckbox, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

void EditorSettingsPanel::createBehaviorSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Behavior");

    // Auto-focus checkbox
    m_autoFocusCheckbox = new wxCheckBox(box->GetStaticBox(), wxID_ANY,
        "Auto-focus editor on load");
    m_autoFocusCheckbox->SetValue(m_state.autoFocus);
    m_autoFocusCheckbox->SetToolTip("Automatically focus editor when loading chapter (fixes bug #1)");
    box->Add(m_autoFocusCheckbox, 0, wxALL | wxEXPAND, 5);

    // Word wrap checkbox
    m_wordWrapCheckbox = new wxCheckBox(box->GetStaticBox(), wxID_ANY,
        "Enable word wrap");
    m_wordWrapCheckbox->SetValue(m_state.wordWrap);
    m_wordWrapCheckbox->SetToolTip("Wrap long lines at window edge");
    box->Add(m_wordWrapCheckbox, 0, wxALL | wxEXPAND, 5);

    // Undo limit
    wxBoxSizer* undoSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* undoLabel = new wxStaticText(box->GetStaticBox(), wxID_ANY,
        "Undo limit:");
    undoSizer->Add(undoLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_undoLimitSpinner = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY);
    m_undoLimitSpinner->SetRange(10, 1000);
    m_undoLimitSpinner->SetValue(m_state.undoLimit);
    m_undoLimitSpinner->SetToolTip("Maximum number of undo operations (10-1000)");
    undoSizer->Add(m_undoLimitSpinner, 1, wxEXPAND, 0);

    box->Add(undoSizer, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}

// ============================================================================
// Public Methods
// ============================================================================

void EditorSettingsPanel::saveToState() {
    core::Logger::getInstance().debug("EditorSettingsPanel: Saving values to state");

    // Cursor & Caret
    m_state.caretBlinkEnabled = m_caretBlinkCheckbox->GetValue();
    m_state.caretBlinkRate = m_caretBlinkRateSpinner->GetValue();
    m_state.caretWidth = m_caretWidthSpinner->GetValue();

    // Margins & Padding
    m_state.marginLeft = m_marginLeftSpinner->GetValue();
    m_state.marginRight = m_marginRightSpinner->GetValue();
    m_state.marginTop = m_marginTopSpinner->GetValue();
    m_state.marginBottom = m_marginBottomSpinner->GetValue();

    // Rendering
    m_state.lineSpacing = m_lineSpacingSpinner->GetValue();
    m_state.selectionOpacity = m_selectionOpacitySlider->GetValue();
    m_state.selectionColor = m_selectionColorPicker->GetColour();
    m_state.antialiasing = m_antialiasingCheckbox->GetValue();

    // Behavior
    m_state.autoFocus = m_autoFocusCheckbox->GetValue();
    m_state.wordWrap = m_wordWrapCheckbox->GetValue();
    m_state.undoLimit = m_undoLimitSpinner->GetValue();

    core::Logger::getInstance().info("EditorSettingsPanel: Saved {} settings values", 14);
}

// ============================================================================
// Event Handlers
// ============================================================================

void EditorSettingsPanel::onCaretBlinkChanged([[maybe_unused]] wxCommandEvent& event) {
    // Enable/disable blink rate spinner based on checkbox
    bool enabled = m_caretBlinkCheckbox->GetValue();
    m_caretBlinkRateSpinner->Enable(enabled);
}

} // namespace gui
} // namespace kalahari
