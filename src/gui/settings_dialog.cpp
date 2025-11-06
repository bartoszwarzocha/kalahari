/// @file settings_dialog.cpp
/// @brief Implementation of Settings dialog

#include "settings_dialog.h"
#include "editor_settings_panel.h"  // Phase 1: Editor settings (Task #00019)
#include <kalahari/core/logger.h>
#include <wx/artprov.h>

namespace kalahari {
namespace gui {

// ============================================================================
// DiagnosticsPanel Implementation
// ============================================================================

wxBEGIN_EVENT_TABLE(DiagnosticsPanel, wxPanel)
wxEND_EVENT_TABLE()

DiagnosticsPanel::DiagnosticsPanel(wxWindow* parent, SettingsState& state)
    : wxPanel(parent), m_state(state)
{
    // Main vertical sizer for panel
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Static box sizer for diagnostics section
    wxStaticBoxSizer* diagBox = new wxStaticBoxSizer(
        wxVERTICAL,
        this,
        "Diagnostic Options"
    );

    // Warning header with native icon (bold, larger font)
    wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);

    // Native warning icon from system resources (like wxMessageBox wxICON_EXCLAMATION)
    wxBitmap warningBmp = wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX);
    wxStaticBitmap* iconBitmap = new wxStaticBitmap(
        diagBox->GetStaticBox(),
        wxID_ANY,
        warningBmp
    );
    headerSizer->Add(iconBitmap, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    // Header text (without emoji)
    wxStaticText* warning = new wxStaticText(
        diagBox->GetStaticBox(),
        wxID_ANY,
        "Advanced Settings"
    );
    wxFont headerFont = warning->GetFont();
    headerFont.MakeBold();
    headerFont.MakeLarger();
    warning->SetFont(headerFont);
    headerSizer->Add(warning, 0, wxALIGN_CENTER_VERTICAL);

    diagBox->Add(headerSizer, 0, wxALL | wxEXPAND, 5);

    // Checkbox
    m_diagnosticCheckbox = new wxCheckBox(
        diagBox->GetStaticBox(),
        wxID_ANY,
        "Enable Diagnostic Options"
    );
    m_diagnosticCheckbox->SetValue(m_state.diagnosticModeEnabled);
    m_diagnosticCheckbox->Bind(wxEVT_CHECKBOX,
        &DiagnosticsPanel::onCheckboxChanged, this);
    diagBox->Add(m_diagnosticCheckbox, 0, wxALL | wxEXPAND, 5);

    // Description text (uses wxEXPAND for responsive wrapping)
    wxStaticText* desc = new wxStaticText(
        diagBox->GetStaticBox(),
        wxID_ANY,
        "Shows the Diagnostics menu with developer tools for debugging.\n"
        "This setting is not saved and resets on application restart.\n\n"
        "Use this only when troubleshooting issues or requested by support."
    );
    diagBox->Add(desc, 1, wxALL | wxEXPAND, 5);

    // Gray out checkbox if launched with --diag CLI flag
    if (m_state.launchedWithDiagFlag) {
        m_diagnosticCheckbox->Enable(false);
        m_diagnosticCheckbox->SetToolTip(
            "Diagnostic mode was enabled via --diag command-line flag.\n"
            "It cannot be changed during this session."
        );
    }

    // Add the static box to main sizer
    mainSizer->Add(diagBox, 1, wxALL | wxEXPAND, 10);

    SetSizer(mainSizer);
}

void DiagnosticsPanel::saveToState() {
    // Only save if not launched with CLI flag
    if (!m_state.launchedWithDiagFlag) {
        m_state.diagnosticModeEnabled = m_diagnosticCheckbox->GetValue();
    }
}

void DiagnosticsPanel::onCheckboxChanged([[maybe_unused]] wxCommandEvent& event) {
    // State will be updated in saveToState() when OK/Apply clicked
    // This handler exists for future validation logic if needed
}

// ============================================================================
// SettingsDialog Implementation
// ============================================================================

wxBEGIN_EVENT_TABLE(SettingsDialog, wxDialog)
    EVT_TREE_SEL_CHANGED(wxID_ANY, SettingsDialog::onTreeSelectionChanged)
    EVT_BUTTON(wxID_OK, SettingsDialog::onOK)
    EVT_BUTTON(wxID_CANCEL, SettingsDialog::onCancel)
    EVT_BUTTON(wxID_APPLY, SettingsDialog::onApply)
wxEND_EVENT_TABLE()

SettingsDialog::SettingsDialog(wxWindow* parent, const SettingsState& currentState)
    : wxDialog(parent, wxID_ANY, "Settings", wxDefaultPosition, wxSize(800, 600),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_originalState(currentState),
      m_workingState(currentState)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("Settings dialog initializing...");

    // Main vertical sizer
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Splitter window (tree on left, content on right)
    m_splitter = new wxSplitterWindow(this, wxID_ANY,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxSP_3D | wxSP_LIVE_UPDATE);

    // Left: Tree control
    m_tree = new wxTreeCtrl(m_splitter, wxID_ANY,
                           wxDefaultPosition, wxDefaultSize,
                           wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT | wxTR_SINGLE);

    // Icon list for tree nodes (16x16 icons)
    m_iconList = new wxImageList(16, 16);
    // Material Design icons from IconRegistry via KalahariArtProvider
    m_iconList->Add(wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, wxSize(16, 16)));        // Folder icon
    m_iconList->Add(wxArtProvider::GetBitmap(wxART_HELP_SETTINGS, wxART_OTHER, wxSize(16, 16))); // Settings icon (for Diagnostics)
    m_tree->AssignImageList(m_iconList);

    // Right: Content panel (scrollable)
    m_contentPanel = new wxScrolledWindow(m_splitter, wxID_ANY,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxVSCROLL);
    m_contentPanel->SetScrollRate(0, 10);

    // Create sizer for content panel
    wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);
    m_contentPanel->SetSizer(contentSizer);

    // Split with tree on left (280px default)
    m_splitter->SplitVertically(m_tree, m_contentPanel, 280);
    m_splitter->SetMinimumPaneSize(200); // Min tree width

    mainSizer->Add(m_splitter, 1, wxEXPAND | wxALL, 5);

    // Bottom: Button panel
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add(new wxButton(this, wxID_OK, "OK"), 0, wxALL, 5);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxALL, 5);
    buttonSizer->Add(new wxButton(this, wxID_APPLY, "Apply"), 0, wxALL, 5);
    mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);

    // Build tree structure and panels
    buildTree();

    logger.debug("Settings dialog initialized successfully");
}

void SettingsDialog::buildTree() {
    // Icon indices
    const int ICON_FOLDER = 0;
    const int ICON_SETTING = 1;

    // Root (hidden)
    wxTreeItemId root = m_tree->AddRoot("Settings", ICON_FOLDER);

    // ========================================================================
    // Phase 1: Editor Settings (Task #00019)
    // ========================================================================

    // Editor leaf (top-level, not in folder)
    wxTreeItemId editor = m_tree->AppendItem(
        root,
        "Editor",
        ICON_SETTING
    );

    // Create panel for Editor
    EditorSettingsPanel* editorPanel = new EditorSettingsPanel(
        m_contentPanel,
        m_workingState
    );
    m_panels[editor] = editorPanel;

    // ========================================================================
    // Phase 0: Advanced → Diagnostics
    // ========================================================================

    // Advanced branch
    wxTreeItemId advanced = m_tree->AppendItem(
        root,
        "Advanced",
        ICON_FOLDER
    );

    // Diagnostics leaf
    wxTreeItemId diagnostics = m_tree->AppendItem(
        advanced,
        "Diagnostics",
        ICON_SETTING
    );

    // Create panel for Diagnostics
    DiagnosticsPanel* diagPanel = new DiagnosticsPanel(
        m_contentPanel,
        m_workingState
    );
    m_panels[diagnostics] = diagPanel;

    // ========================================================================
    // Default selection
    // ========================================================================

    // Expand Advanced by default
    m_tree->Expand(advanced);

    // Select Editor by default (Phase 1: most important settings)
    m_tree->SelectItem(editor);
    showPanel(editor);
}

void SettingsDialog::showPanel(wxTreeItemId item) {
    // Get sizer
    wxSizer* sizer = m_contentPanel->GetSizer();
    if (!sizer) {
        core::Logger::getInstance().error("Content panel has no sizer!");
        return;
    }

    // Hide and detach current panel
    if (m_currentPanel) {
        m_currentPanel->Hide();
        sizer->Detach(m_currentPanel);
    }

    // Show selected panel
    auto it = m_panels.find(item);
    if (it != m_panels.end()) {
        m_currentPanel = it->second;

        // Add panel to sizer with wxEXPAND
        sizer->Add(m_currentPanel, 1, wxALL | wxEXPAND, 10);

        m_currentPanel->Show();

        // Trigger layout recalculation
        m_contentPanel->Layout();
        m_contentPanel->FitInside();  // Update scrollbars
        m_contentPanel->Scroll(0, 0); // Reset to top
    } else {
        // No panel for this tree item (probably a folder node)
        m_currentPanel = nullptr;
    }
}

void SettingsDialog::onTreeSelectionChanged(wxTreeEvent& event) {
    wxTreeItemId item = event.GetItem();
    showPanel(item);
}

void SettingsDialog::onOK([[maybe_unused]] wxCommandEvent& event) {
    applyChanges();

    // Confirmation if enabling diagnostics (and wasn't enabled before)
    if (m_workingState.diagnosticModeEnabled &&
        !m_originalState.diagnosticModeEnabled &&
        !m_workingState.launchedWithDiagFlag)
    {
        int result = wxMessageBox(
            "Are you sure you want to enable advanced diagnostic options?\n\n"
            "These options are intended for debugging and may expose\n"
            "internal application state. Enable only when troubleshooting.",
            "Enable Diagnostic Options?",
            wxYES_NO | wxICON_WARNING,
            this
        );

        if (result != wxYES) {
            m_workingState.diagnosticModeEnabled = false;
            core::Logger::getInstance().debug("User cancelled diagnostic mode enable");
            return; // Don't close dialog
        }
    }

    // Validate all settings
    if (!validateSettings()) {
        wxMessageBox(
            "Some settings have invalid values. Please correct them.",
            "Invalid Settings",
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    core::Logger::getInstance().info("Settings saved by user");
    EndModal(wxID_OK);
}

void SettingsDialog::onCancel([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().debug("Settings dialog cancelled");
    EndModal(wxID_CANCEL);
}

void SettingsDialog::onApply([[maybe_unused]] wxCommandEvent& event) {
    applyChanges();

    // Validate all settings
    if (!validateSettings()) {
        wxMessageBox(
            "Some settings have invalid values. Please correct them.",
            "Invalid Settings",
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    // Update original state to reflect applied changes
    m_originalState = m_workingState;

    core::Logger::getInstance().info("Settings applied (dialog remains open)");
}

void SettingsDialog::applyChanges() {
    // Save all panel states to m_workingState
    for (auto& [id, panel] : m_panels) {
        // Phase 1: Editor settings (Task #00019)
        if (auto* editorPanel = dynamic_cast<EditorSettingsPanel*>(panel)) {
            editorPanel->saveToState();
        }
        // Phase 0: Diagnostics
        else if (auto* diagPanel = dynamic_cast<DiagnosticsPanel*>(panel)) {
            diagPanel->saveToState();
        }
        // Future phases: Add other panel types here
    }

    core::Logger::getInstance().debug("Settings changes applied to working state");
}

bool SettingsDialog::validateSettings() {
    // Phase 0: No validation needed yet
    // Phase 1+: Validate language codes, file paths, ranges, etc.
    return true;
}

} // namespace gui
} // namespace kalahari
