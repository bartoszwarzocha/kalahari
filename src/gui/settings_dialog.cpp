/// @file settings_dialog.cpp
/// @brief Implementation of Settings dialog

#include "settings_dialog.h"
#include "editor_settings_panel.h"      // Phase 1: Editor settings (Task #00019)
#include "log_settings_panel.h"         // Phase 1: Diagnostic Log settings (Task #00020)
#include "appearance_settings_panel.h"  // Phase 1: Appearance settings (Task #00020 - Option C)
#include "kalahari/gui/icon_registry.h"
#include <kalahari/core/logger.h>
#include <bwx_sdk/bwx_core/bwx_exception.h>  // For test exception
#include <wx/artprov.h>

namespace kalahari {
namespace gui {

// ============================================================================
// Custom Event Definition
// ============================================================================

wxDEFINE_EVENT(EVT_SETTINGS_APPLIED, SettingsAppliedEvent);

// ============================================================================
// DiagnosticsPanel Implementation
// ============================================================================

wxBEGIN_EVENT_TABLE(DiagnosticsPanel, wxPanel)
wxEND_EVENT_TABLE()

DiagnosticsPanel::DiagnosticsPanel(wxWindow* parent, SettingsState& state)
    : wxPanel(parent), m_state(state)
{
    auto& logger = core::Logger::getInstance();

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

    // DEFENSIVE: Only add icon if bitmap is valid (prevents crash on Windows)
    if (warningBmp.IsOk()) {
        wxStaticBitmap* iconBitmap = new wxStaticBitmap(
            diagBox->GetStaticBox(),
            wxID_ANY,
            warningBmp
        );
        headerSizer->Add(iconBitmap, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    } else {
        logger.warn("Failed to load wxART_WARNING icon, skipping icon display");
    }

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
    logger.info("SettingsDialog: Constructor START");

    // DEBUG PHASE 1: Constructor START
    wxMessageBox("Constructor START", "DEBUG", wxOK | wxICON_INFORMATION);

    // Main vertical sizer
    logger.debug("SettingsDialog: Creating main sizer");
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Splitter window (tree on left, content on right)
    logger.debug("SettingsDialog: Creating splitter window");
    m_splitter = new wxSplitterWindow(this, wxID_ANY,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxSP_3D | wxSP_LIVE_UPDATE);

    // Left: Tree control
    logger.debug("SettingsDialog: Creating tree control");
    m_tree = new wxTreeCtrl(m_splitter, wxID_ANY,
                           wxDefaultPosition, wxDefaultSize,
                           wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT | wxTR_SINGLE);

    // Icon list for tree nodes
    // DEFENSIVE: Use fixed 16x16 size for tree icons (standard wxWidgets size)
    // Avoids dependency on IconRegistry which may not be fully initialized
    logger.debug("SettingsDialog: Creating icon list (16x16)");
    const int treeIconSize = 16;
    m_iconList = new wxImageList(treeIconSize, treeIconSize);

    // Get icons from wxArtProvider with defensive checks
    logger.debug("SettingsDialog: Loading folder icon");
    wxBitmap folderIcon = wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, wxSize(treeIconSize, treeIconSize));
    logger.debug("SettingsDialog: Loading settings icon");
    wxBitmap settingsIcon = wxArtProvider::GetBitmap(wxART_HELP_SETTINGS, wxART_OTHER, wxSize(treeIconSize, treeIconSize));

    // DEFENSIVE: Check if bitmaps are valid before adding
    logger.debug("SettingsDialog: Adding icons to image list");
    if (folderIcon.IsOk()) {
        m_iconList->Add(folderIcon);
        logger.debug("SettingsDialog: Folder icon OK");
    } else {
        // Fallback: add empty bitmap to maintain icon indices
        m_iconList->Add(wxBitmap(treeIconSize, treeIconSize));
        logger.warn("SettingsDialog: Failed to load folder icon, using empty bitmap");
    }

    if (settingsIcon.IsOk()) {
        m_iconList->Add(settingsIcon);
        logger.debug("SettingsDialog: Settings icon OK");
    } else {
        // Fallback: add empty bitmap
        m_iconList->Add(wxBitmap(treeIconSize, treeIconSize));
        logger.warn("SettingsDialog: Failed to load settings icon, using empty bitmap");
    }

    logger.debug("SettingsDialog: Assigning image list to tree");
    m_tree->AssignImageList(m_iconList);

    // Right: Content panel (scrollable)
    logger.debug("SettingsDialog: Creating content panel");
    m_contentPanel = new wxScrolledWindow(m_splitter, wxID_ANY,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxVSCROLL);
    m_contentPanel->SetScrollRate(0, 10);

    // Create sizer for content panel
    logger.debug("SettingsDialog: Creating content sizer");
    wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);
    m_contentPanel->SetSizer(contentSizer);

    // Split with tree on left (280px default)
    logger.debug("SettingsDialog: Splitting windows");
    m_splitter->SplitVertically(m_tree, m_contentPanel, 280);
    m_splitter->SetMinimumPaneSize(200); // Min tree width

    mainSizer->Add(m_splitter, 1, wxEXPAND | wxALL, 5);

    // Bottom: Button panel
    logger.debug("SettingsDialog: Creating button panel");
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add(new wxButton(this, wxID_OK, "OK"), 0, wxALL, 5);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxALL, 5);
    buttonSizer->Add(new wxButton(this, wxID_APPLY, "Apply"), 0, wxALL, 5);
    mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 5);

    logger.debug("SettingsDialog: Setting main sizer");
    SetSizer(mainSizer);

    // DEBUG PHASE 1: Before buildTree
    wxMessageBox("Before buildTree()", "DEBUG", wxOK | wxICON_INFORMATION);

    // Build tree structure and panels
    logger.info("SettingsDialog: Calling buildTree()");
    buildTree();

    // DEBUG PHASE 1: After buildTree
    wxMessageBox("After buildTree()", "DEBUG", wxOK | wxICON_INFORMATION);

    logger.info("SettingsDialog: Constructor COMPLETE");

    // DEBUG PHASE 1: Constructor END
    wxMessageBox("Constructor COMPLETE", "DEBUG", wxOK | wxICON_INFORMATION);
}

void SettingsDialog::buildTree() {
    auto& logger = core::Logger::getInstance();
    logger.info("buildTree: START");

    // DEBUG PHASE 1: Test EMPTY dialog (no panels, only tree structure)
    wxMessageBox("buildTree START - PHASE 1: NO PANELS CREATED", "DEBUG", wxOK | wxICON_INFORMATION);

    // Icon indices
    const int ICON_FOLDER = 0;
    const int ICON_SETTING = 1;

    // Root (hidden)
    logger.debug("buildTree: Adding root node");
    wxTreeItemId root = m_tree->AddRoot("Settings", ICON_FOLDER);

    // ========================================================================
    // Phase 1: Appearance Settings (Task #00020 - Option C)
    // ========================================================================

    // Appearance leaf (top-level, first item for easy access)
    logger.debug("buildTree: Adding Appearance node");
    wxTreeItemId appearance = m_tree->AppendItem(
        root,
        "Appearance",
        ICON_SETTING
    );

    // COMMENTED OUT FOR PHASE 1 DEBUG: Create panel for Appearance
    /*
    logger.info("buildTree: Creating AppearanceSettingsPanel");
    AppearanceSettingsPanel* appearancePanel = new AppearanceSettingsPanel(
        m_contentPanel,
        m_workingState
    );
    logger.debug("buildTree: Hiding Appearance panel");
    appearancePanel->Hide(); // IMPORTANT: Hide initially, showPanel() will show the selected one
    m_panels[appearance] = appearancePanel;
    logger.debug("buildTree: Appearance panel complete");
    */

    // ========================================================================
    // Phase 1: Editor Settings (Task #00019)
    // ========================================================================

    // Editor leaf (top-level, not in folder)
    logger.debug("buildTree: Adding Editor node");
    wxTreeItemId editor = m_tree->AppendItem(
        root,
        "Editor",
        ICON_SETTING
    );

    // COMMENTED OUT FOR PHASE 1 DEBUG: Create panel for Editor
    /*
    logger.info("buildTree: Creating EditorSettingsPanel");
    EditorSettingsPanel* editorPanel = new EditorSettingsPanel(
        m_contentPanel,
        m_workingState
    );
    editorPanel->Hide(); // IMPORTANT: Hide initially, showPanel() will show the selected one
    m_panels[editor] = editorPanel;
    logger.debug("buildTree: Editor panel complete");
    */

    // ========================================================================
    // Phase 0: Advanced → Diagnostics
    // ========================================================================

    // Advanced branch
    logger.debug("buildTree: Adding Advanced branch");
    wxTreeItemId advanced = m_tree->AppendItem(
        root,
        "Advanced",
        ICON_FOLDER
    );

    // Diagnostics leaf
    logger.debug("buildTree: Adding Diagnostics node");
    wxTreeItemId diagnostics = m_tree->AppendItem(
        advanced,
        "Diagnostics",
        ICON_SETTING
    );

    // COMMENTED OUT FOR PHASE 1 DEBUG: Create panel for Diagnostics
    /*
    logger.info("buildTree: Creating DiagnosticsPanel");
    DiagnosticsPanel* diagPanel = new DiagnosticsPanel(
        m_contentPanel,
        m_workingState
    );
    diagPanel->Hide(); // IMPORTANT: Hide initially, showPanel() will show the selected one
    m_panels[diagnostics] = diagPanel;
    logger.debug("buildTree: Diagnostics panel complete");
    */

    // COMMENTED OUT FOR PHASE 1 DEBUG: Diagnostic Log leaf (Task #00020 - Phase 1)
    /*
    // Only show when diagnostic mode is enabled
    if (m_workingState.diagnosticModeEnabled) {
        logger.debug("buildTree: Diagnostic mode enabled, adding Log node");
        wxTreeItemId diagnosticLog = m_tree->AppendItem(
            advanced,
            "Diagnostic Log",
            ICON_SETTING
        );

        // Create panel for Diagnostic Log
        logger.info("buildTree: Creating LogSettingsPanel");
        LogSettingsPanel* logPanel = new LogSettingsPanel(
            m_contentPanel,
            m_workingState
        );
        logPanel->Hide(); // IMPORTANT: Hide initially, showPanel() will show the selected one
        m_panels[diagnosticLog] = logPanel;
        logger.debug("buildTree: Log panel complete");
    }
    */

    // ========================================================================
    // Default selection
    // ========================================================================

    // Expand Advanced by default
    logger.debug("buildTree: Expanding Advanced branch");
    m_tree->Expand(advanced);

    // Select Appearance by default (Phase 1: most visible settings)
    logger.debug("buildTree: Selecting Appearance node");
    m_tree->SelectItem(appearance);

    // COMMENTED OUT FOR PHASE 1 DEBUG: showPanel
    /*
    logger.info("buildTree: Calling showPanel(appearance)");
    showPanel(appearance);
    */

    // DEBUG PHASE 1: buildTree END
    wxMessageBox("buildTree END - Tree created, NO PANELS", "DEBUG", wxOK | wxICON_INFORMATION);

    logger.info("buildTree: COMPLETE");
}

void SettingsDialog::showPanel(wxTreeItemId item) {
    auto& logger = core::Logger::getInstance();
    logger.info("showPanel: START");

    // Get sizer
    logger.debug("showPanel: Getting content panel sizer");
    wxSizer* sizer = m_contentPanel->GetSizer();
    if (!sizer) {
        logger.error("showPanel: Content panel has no sizer!");
        return;
    }
    logger.debug("showPanel: Sizer OK");

    // Hide and detach current panel
    if (m_currentPanel) {
        logger.debug("showPanel: Hiding current panel");
        m_currentPanel->Hide();
        logger.debug("showPanel: Detaching current panel from sizer");
        sizer->Detach(m_currentPanel);
    }

    // Show selected panel
    logger.debug("showPanel: Looking up panel in map");
    auto it = m_panels.find(item);
    if (it != m_panels.end()) {
        logger.debug("showPanel: Panel found in map");
        m_currentPanel = it->second;

        // Add panel to sizer with wxEXPAND
        logger.debug("showPanel: Adding panel to sizer");
        sizer->Add(m_currentPanel, 1, wxALL | wxEXPAND, 10);

        logger.debug("showPanel: Showing panel");
        m_currentPanel->Show();

        // Trigger layout recalculation
        logger.debug("showPanel: Calling Layout()");
        m_contentPanel->Layout();

        // DEFENSIVE: Verify panel state before FitInside()
        logger.debug("showPanel: Verifying panel state before FitInside()");
        if (!m_contentPanel->IsShown()) {
            logger.warn("showPanel: m_contentPanel is NOT shown - skipping FitInside()");
            return;
        }
        if (!m_currentPanel->IsShown()) {
            logger.warn("showPanel: m_currentPanel is NOT shown - skipping FitInside()");
            return;
        }

        wxSize contentSize = m_contentPanel->GetSize();
        wxSize panelSize = m_currentPanel->GetSize();
        logger.debug("showPanel: m_contentPanel size: {}x{}", contentSize.GetWidth(), contentSize.GetHeight());
        logger.debug("showPanel: m_currentPanel size: {}x{}", panelSize.GetWidth(), panelSize.GetHeight());

        // Check if panel has valid sizer
        wxSizer* panelSizer = m_currentPanel->GetSizer();
        if (!panelSizer) {
            logger.warn("showPanel: m_currentPanel has NO sizer - skipping FitInside()");
            return;
        }

        // CRITICAL FIX: Skip FitInside() if panel has zero size
        // This prevents crash when FitInside() tries to calculate scrollbars for 0x0 panel
        if (contentSize.GetWidth() <= 0 || contentSize.GetHeight() <= 0) {
            logger.warn("showPanel: m_contentPanel has ZERO size ({}x{}) - skipping FitInside(), will retry on EVT_SIZE",
                       contentSize.GetWidth(), contentSize.GetHeight());
            return;
        }

        logger.debug("showPanel: All checks passed, calling FitInside()");
        m_contentPanel->FitInside();  // Update scrollbars
        logger.debug("showPanel: FitInside() SUCCESS");

        logger.debug("showPanel: Calling Scroll(0,0)");
        m_contentPanel->Scroll(0, 0); // Reset to top
        logger.debug("showPanel: Scroll() SUCCESS");

        logger.info("showPanel: COMPLETE");
    } else {
        // No panel for this tree item (probably a folder node)
        logger.debug("showPanel: No panel found for this tree item (folder?)");
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

    // Notification if theme was changed (Task #00020 - Option C)
    // Theme changes require application restart to take effect
    if (m_workingState.themeName != m_originalState.themeName) {
        wxMessageBox(
            "Theme changes will take effect after restarting the application.\n\n"
            "Please close and reopen Kalahari to see the new theme.",
            "Restart Required",
            wxOK | wxICON_INFORMATION,
            this
        );
        core::Logger::getInstance().info("Theme changed from '{}' to '{}' (restart required)",
            m_originalState.themeName.ToStdString(),
            m_workingState.themeName.ToStdString());
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

    // Send custom event to parent (MainWindow) to save and apply settings
    SettingsAppliedEvent applyEvent(EVT_SETTINGS_APPLIED, GetId());
    applyEvent.setNewState(m_workingState);
    applyEvent.SetEventObject(this);
    ProcessWindowEvent(applyEvent);  // Send to parent window

    core::Logger::getInstance().info("Settings applied (dialog remains open) - event sent to MainWindow");
}

void SettingsDialog::applyChanges() {
    // Save all panel states to m_workingState
    for (auto& [id, panel] : m_panels) {
        // Phase 1: Appearance settings (Task #00020 - Option C)
        if (auto* appearancePanel = dynamic_cast<AppearanceSettingsPanel*>(panel)) {
            appearancePanel->saveToState();
        }
        // Phase 1: Editor settings (Task #00019)
        else if (auto* editorPanel = dynamic_cast<EditorSettingsPanel*>(panel)) {
            editorPanel->saveToState();
        }
        // Phase 1: Diagnostic Log settings (Task #00020)
        else if (auto* logPanel = dynamic_cast<LogSettingsPanel*>(panel)) {
            logPanel->saveToState();
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
