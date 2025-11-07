/// @file navigator_panel.cpp
/// @brief Implementation of NavigatorPanel (Task #00020)

#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/outline_tab.h"
#include "kalahari/gui/panels/files_tab.h"
#include "kalahari/gui/panels/libraries_tab.h"
#include "kalahari/gui/icon_registry.h"
#include "kalahari/resources/icons_material.h"
#include <kalahari/core/logger.h>
#include <wx/artprov.h>

namespace kalahari {
namespace gui {

// ============================================================================
// Constructor
// ============================================================================

NavigatorPanel::NavigatorPanel(wxWindow* parent, core::Document* document)
    : wxPanel(parent, wxID_ANY), m_document(document)
{
    auto& logger = core::Logger::getInstance();
    logger.info("Navigator Panel: Creating with 3 tabs (Outline, Files, Libraries)");

    // Main sizer
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Create wxAuiNotebook
    m_notebook = new wxAuiNotebook(this, wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS);

    // Create tabs
    createTabs();

    // Setup icons
    setupIcons();

    // Add notebook to sizer
    mainSizer->Add(m_notebook, 1, wxALL | wxEXPAND, 0);

    SetSizer(mainSizer);

    logger.info("Navigator Panel: Created successfully with {} tabs",
        m_notebook->GetPageCount());
}

// ============================================================================
// Destructor
// ============================================================================

NavigatorPanel::~NavigatorPanel()
{
    core::Logger::getInstance().debug("Navigator Panel: Destroying");
    // wxWidgets handles child widget cleanup automatically
}

// ============================================================================
// Public API
// ============================================================================

void NavigatorPanel::refresh()
{
    core::Logger::getInstance().debug("Navigator Panel: refresh()");

    if (!m_document) {
        core::Logger::getInstance().warn("Navigator Panel: No document loaded");
        return;
    }

    // Refresh OutlineTab (Phase 2)
    if (m_outlineTab) {
        m_outlineTab->populateTree();
    }

    // Refresh other tabs when implemented (Phase 2+)
}

void NavigatorPanel::selectChapter(const std::string& chapterId)
{
    core::Logger::getInstance().debug("Navigator Panel: selectChapter({})", chapterId);

    // Forward to OutlineTab (Phase 2)
    if (m_outlineTab) {
        m_outlineTab->selectChapter(chapterId);
    }

    // Switch to Outline tab if not already active
    for (size_t i = 0; i < m_notebook->GetPageCount(); ++i) {
        if (m_notebook->GetPage(i) == m_outlineTab) {
            m_notebook->SetSelection(i);
            break;
        }
    }
}

void NavigatorPanel::setDocument(core::Document* document)
{
    core::Logger::getInstance().debug("Navigator Panel: setDocument()");

    m_document = document;

    // Update all tabs
    if (m_outlineTab) {
        m_outlineTab->setDocument(document);
    }

    // Refresh display
    refresh();
}

// ============================================================================
// Setup Methods
// ============================================================================

void NavigatorPanel::createTabs()
{
    auto& logger = core::Logger::getInstance();

    // Tab 1: Outline (Book structure) - Phase 1 placeholder, Phase 2 functional
    m_outlineTab = new OutlineTab(m_notebook, m_document);
    m_notebook->AddPage(m_outlineTab, "Outline", true); // true = select by default
    logger.debug("Navigator Panel: Added Outline tab");

    // Tab 2: Files (Project files) - Phase 2 implementation
    m_filesTab = new FilesTab(m_notebook);
    m_notebook->AddPage(m_filesTab, "Files", false);
    logger.debug("Navigator Panel: Added Files tab (placeholder)");

    // Tab 3: Libraries (Characters/Places/Items) - Phase 2+ implementation
    m_librariesTab = new LibrariesTab(m_notebook);
    m_notebook->AddPage(m_librariesTab, "Libraries", false);
    logger.debug("Navigator Panel: Added Libraries tab (placeholder)");
}

void NavigatorPanel::setupIcons()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("Navigator Panel: Setting up tab icons");

    // Get IconRegistry instance
    auto& iconRegistry = IconRegistry::getInstance();

    // Tab icons (16x16 for tabs)
    wxSize iconSize(16, 16);

    // Icon 0: Outline tab - FOLDER (placeholder for account_tree)
    wxBitmap outlineIcon = iconRegistry.getBitmap("FOLDER", wxART_OTHER, iconSize);
    if (outlineIcon.IsOk()) {
        m_notebook->SetPageBitmap(0, outlineIcon);
        logger.debug("Navigator Panel: Outline tab icon set (FOLDER placeholder)");
    }

    // Icon 1: Files tab - FILE_OPEN (placeholder for folder_open)
    wxBitmap filesIcon = iconRegistry.getBitmap("FILE_OPEN", wxART_OTHER, iconSize);
    if (filesIcon.IsOk()) {
        m_notebook->SetPageBitmap(1, filesIcon);
        logger.debug("Navigator Panel: Files tab icon set (FILE_OPEN placeholder)");
    }

    // Icon 2: Libraries tab - FOLDER (placeholder for library_books)
    wxBitmap librariesIcon = iconRegistry.getBitmap("FOLDER", wxART_OTHER, iconSize);
    if (librariesIcon.IsOk()) {
        m_notebook->SetPageBitmap(2, librariesIcon);
        logger.debug("Navigator Panel: Libraries tab icon set (FOLDER placeholder)");
    }

    logger.info("Navigator Panel: Tab icons configured (using placeholders)");
}

} // namespace gui
} // namespace kalahari
