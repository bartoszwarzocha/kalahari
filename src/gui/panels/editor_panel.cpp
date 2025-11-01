/// @file editor_panel.cpp
/// @brief Implementation of EditorPanel (placeholder for Phase 1)

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/core/book_element.h"
#include <kalahari/core/logger.h>
#include <wx/textctrl.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace kalahari {
namespace gui {

// Event IDs
enum {
    ID_WORDCOUNT_TIMER = wxID_HIGHEST + 1
};

wxBEGIN_EVENT_TABLE(EditorPanel, wxPanel)
    EVT_TIMER(ID_WORDCOUNT_TIMER, EditorPanel::onWordCountTimer)
wxEND_EVENT_TABLE()

EditorPanel::EditorPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    core::Logger::getInstance().info("Creating Editor panel (placeholder)");
    setupLayout();
}

EditorPanel::~EditorPanel() {
    if (m_wordCountTimer) {
        m_wordCountTimer->Stop();
        delete m_wordCountTimer;
    }
}

void EditorPanel::setupLayout() {
    core::Logger::getInstance().debug("EditorPanel::setupLayout() - START");

    // Set panel background to light gray (paper-like effect)
    SetBackgroundColour(wxColour(240, 240, 240));

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // Create simple multiline text control as placeholder
    // Phase 1 will replace this with wxRichTextCtrl or custom editor
    m_textCtrl = new wxTextCtrl(this, wxID_ANY,
        "Editor Panel - Phase 1\n\n"
        "This is a placeholder for the rich text editor.\n"
        "Phase 1 will implement:\n"
        "- wxRichTextCtrl for WYSIWYG editing\n"
        "- or custom wxGraphicsContext-based editor (via bwx_sdk)\n"
        "- Chapter content loading/saving\n"
        "- Basic formatting (bold, italic, underline)\n"
        "- Word count tracking\n",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_WORDWRAP | wxTE_RICH2);

    // Add to sizer with padding (creates "page on desk" effect)
    sizer->Add(m_textCtrl, 1, wxALL | wxEXPAND, 20);
    SetSizer(sizer);

    // Create word count timer (500ms debounce)
    m_wordCountTimer = new wxTimer(this, ID_WORDCOUNT_TIMER);

    core::Logger::getInstance().info("Editor panel initialized (placeholder)");
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool EditorPanel::loadChapter(const core::BookElement& element) {
    std::filesystem::path htmlPath = element.getFile();

    if (!std::filesystem::exists(htmlPath)) {
        // New chapter - clear editor
        clearContent();
        m_currentElement = const_cast<core::BookElement*>(&element);
        core::Logger::getInstance().info("New chapter '{}', editor cleared", element.getTitle());
        return true;
    }

    // Load HTML file
    try {
        std::ifstream file(htmlPath);
        if (!file.is_open()) {
            core::Logger::getInstance().error("Failed to open HTML file: {}", htmlPath.string());
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string htmlContent = buffer.str();

        // Load HTML content into text control
        if (m_textCtrl) {
            m_textCtrl->SetValue(wxString::FromUTF8(htmlContent));
        }
        m_currentElement = const_cast<core::BookElement*>(&element);
        m_isModified = false;

        core::Logger::getInstance().info("Loaded chapter '{}' from {}",
            element.getTitle(), htmlPath.string());
        return true;

    } catch (const std::exception& e) {
        core::Logger::getInstance().error("Exception loading chapter: {}", e.what());
        wxMessageBox(
            wxString::Format("Failed to load chapter '%s': %s",
                element.getTitle().c_str(), e.what()),
            "Load Error", wxOK | wxICON_ERROR, this);
        return false;
    }
}

bool EditorPanel::saveChapter(core::BookElement& element) {
    if (!m_currentElement) {
        core::Logger::getInstance().warn("saveChapter called but no current element");
        return false;
    }

    std::filesystem::path htmlPath = element.getFile();

    // Create directory structure if missing
    std::filesystem::path dir = htmlPath.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        try {
            std::filesystem::create_directories(dir);
            core::Logger::getInstance().info("Created directory: {}", dir.string());
        } catch (const std::filesystem::filesystem_error& e) {
            core::Logger::getInstance().error("Failed to create directory: {}", e.what());
            return false;
        }
    }

    // Get content from text control
    wxString htmlContent;
    if (m_textCtrl) {
        htmlContent = m_textCtrl->GetValue();
    } else {
        htmlContent = "<p>Placeholder content</p>";
    }

    try {
        std::ofstream file(htmlPath);
        if (!file.is_open()) {
            core::Logger::getInstance().error("Failed to open file for writing: {}",
                htmlPath.string());
            return false;
        }

        file << htmlContent.ToStdString();
        file.close();

        // Update metadata
        int wordCount = getWordCount();
        element.setWordCount(wordCount);
        element.touch();  // Update modified timestamp

        core::Logger::getInstance().info("Saved chapter '{}' ({} words) to {}",
            element.getTitle(), wordCount, htmlPath.string());

        m_isModified = false;
        return true;

    } catch (const std::exception& e) {
        core::Logger::getInstance().error("Exception saving chapter: {}", e.what());
        wxMessageBox(
            wxString::Format("Failed to save chapter '%s': %s",
                element.getTitle().c_str(), e.what()),
            "Save Error", wxOK | wxICON_ERROR, this);
        return false;
    }
}

int EditorPanel::getWordCount() const {
    // Return cached word count (updated by JavaScript messages)
    return m_cachedWordCount;
}

void EditorPanel::clearContent() {
    if (m_textCtrl) {
        m_textCtrl->Clear();
    }
    m_currentElement = nullptr;
    m_isModified = false;
    m_cachedWordCount = 0;
    core::Logger::getInstance().info("Editor content cleared");
}

bool EditorPanel::hasUnsavedChanges() const {
    return m_isModified;
}

// ============================================================================
// Event Handlers
// ============================================================================

void EditorPanel::onWordCountTimer([[maybe_unused]] wxTimerEvent& event) {
    // Update StatusBar with word count
    int wordCount = getWordCount();

    // Find parent wxFrame (MainWindow) via wxWindow hierarchy
    wxWindow* parent = GetParent();
    while (parent && !parent->IsTopLevel()) {
        parent = parent->GetParent();
    }

    if (parent) {
        wxFrame* frame = wxDynamicCast(parent, wxFrame);
        if (frame) {
            wxStatusBar* statusBar = frame->GetStatusBar();
            if (statusBar) {
                statusBar->SetStatusText(wxString::Format("Words: %d", wordCount), 0);
            }
        }
    }
}


// ============================================================================
// Format Event Handlers
// ============================================================================

void EditorPanel::onFormatBold([[maybe_unused]] wxCommandEvent& event) {
    // TODO: Implement formatting in Phase 1
    core::Logger::getInstance().warn("Bold formatting not yet implemented (Phase 1)");
}

void EditorPanel::onFormatItalic([[maybe_unused]] wxCommandEvent& event) {
    // TODO: Implement formatting in Phase 1
    core::Logger::getInstance().warn("Italic formatting not yet implemented (Phase 1)");
}

void EditorPanel::onFormatUnderline([[maybe_unused]] wxCommandEvent& event) {
    // TODO: Implement formatting in Phase 1
    core::Logger::getInstance().warn("Underline formatting not yet implemented (Phase 1)");
}

void EditorPanel::onFormatFont([[maybe_unused]] wxCommandEvent& event) {
    // TODO: Implement custom font picker in Phase 1/2
    core::Logger::getInstance().warn("Font formatting not yet implemented (Phase 1/2)");
    wxMessageBox("Font formatting will be implemented in Phase 1 or 2",
        "Not Implemented", wxOK | wxICON_INFORMATION, this);
}

void EditorPanel::onFormatClear([[maybe_unused]] wxCommandEvent& event) {
    // TODO: Implement formatting clearing in Phase 1
    core::Logger::getInstance().warn("Clear formatting not yet implemented (Phase 1)");
}


} // namespace gui
} // namespace kalahari
