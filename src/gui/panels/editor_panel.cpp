/// @file editor_panel.cpp
/// @brief Implementation of EditorPanel using bwxTextEditor (Task #00019)

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/core/book_element.h"
#include <kalahari/core/logger.h>
#include <wx/fontdlg.h>
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
    core::Logger::getInstance().info("Creating Editor panel (bwxTextEditor)");
    setupLayout();
}

EditorPanel::~EditorPanel() {
    // CRITICAL: Unregister observer BEFORE destroying timer
    // Prevents observer callbacks to destroyed object
    if (m_textEditor) {
        m_textEditor->GetDocument().RemoveObserver(this);
        core::Logger::getInstance().debug("EditorPanel: Observer unregistered");
    }

    // Stop and destroy timer
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

    // Create bwxTextEditor control (Task #00019)
    // Full View renderer with Command-based undo/redo
    m_textEditor = new bwx_sdk::gui::bwxTextEditor(this, wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxBORDER_SUNKEN);

    // Add to sizer - fill entire panel (no margins)
    sizer->Add(m_textEditor, 1, wxEXPAND, 0);
    SetSizer(sizer);

    // CRITICAL: Register as document observer for word count updates
    // This enables true debouncing via OnTextChanged() callback
    m_textEditor->GetDocument().AddObserver(this);
    core::Logger::getInstance().debug("EditorPanel: Registered as document observer");

    // Create word count timer (ONE_SHOT for debouncing)
    // Timer will be started by OnTextChanged(), not here
    m_wordCountTimer = new wxTimer(this, ID_WORDCOUNT_TIMER);

    core::Logger::getInstance().info("Editor panel initialized (bwxTextEditor + Observer)");
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool EditorPanel::loadChapter(const core::BookElement& element) {
    // CRITICAL: Unregister from old document before loading new one
    // This ensures observer is attached to correct document
    if (m_textEditor && m_currentElement) {
        m_textEditor->GetDocument().RemoveObserver(this);
        core::Logger::getInstance().debug("loadChapter: Unregistered from old document");
    }

    std::filesystem::path filePath = element.getFile();

    if (!std::filesystem::exists(filePath)) {
        // New chapter - clear editor
        clearContent();
        m_currentElement = const_cast<core::BookElement*>(&element);

        // Register observer for new empty document
        if (m_textEditor) {
            m_textEditor->GetDocument().AddObserver(this);
            core::Logger::getInstance().debug("loadChapter: Registered with new empty document");
        }

        core::Logger::getInstance().info("New chapter '{}', editor cleared", element.getTitle());
        return true;
    }

    // Load .ktxt file using bwxTextEditor
    if (!m_textEditor) {
        core::Logger::getInstance().error("Text editor not initialized");
        return false;
    }

    wxString path = wxString::FromUTF8(filePath.string());
    if (!m_textEditor->LoadFromFile(path)) {
        core::Logger::getInstance().error("Failed to load file: {}", filePath.string());
        wxMessageBox(
            wxString::Format("Failed to load chapter '%s'",
                element.getTitle().c_str()),
            "Load Error", wxOK | wxICON_ERROR, this);
        return false;
    }

    m_currentElement = const_cast<core::BookElement*>(&element);
    m_isModified = false;

    // Register observer with loaded document
    m_textEditor->GetDocument().AddObserver(this);
    core::Logger::getInstance().debug("loadChapter: Registered with loaded document");

    core::Logger::getInstance().info("Loaded chapter '{}' from {}",
        element.getTitle(), filePath.string());
    return true;
}

bool EditorPanel::saveChapter(core::BookElement& element) {
    if (!m_currentElement) {
        core::Logger::getInstance().warn("saveChapter called but no current element");
        return false;
    }

    if (!m_textEditor) {
        core::Logger::getInstance().error("Text editor not initialized");
        return false;
    }

    std::filesystem::path filePath = element.getFile();

    // Create directory structure if missing
    std::filesystem::path dir = filePath.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        try {
            std::filesystem::create_directories(dir);
            core::Logger::getInstance().info("Created directory: {}", dir.string());
        } catch (const std::filesystem::filesystem_error& e) {
            core::Logger::getInstance().error("Failed to create directory: {}", e.what());
            return false;
        }
    }

    // Save .ktxt file using bwxTextEditor
    wxString path = wxString::FromUTF8(filePath.string());
    if (!m_textEditor->SaveToFile(path)) {
        core::Logger::getInstance().error("Failed to save file: {}", filePath.string());
        wxMessageBox(
            wxString::Format("Failed to save chapter '%s'",
                element.getTitle().c_str()),
            "Save Error", wxOK | wxICON_ERROR, this);
        return false;
    }

    // Update metadata
    int wordCount = getWordCount();
    element.setWordCount(wordCount);
    element.touch();  // Update modified timestamp

    core::Logger::getInstance().info("Saved chapter '{}' ({} words) to {}",
        element.getTitle(), wordCount, filePath.string());

    m_isModified = false;
    return true;
}

int EditorPanel::getWordCount() const {
    if (!m_textEditor) {
        return 0;
    }

    // Get word count from bwxTextEditor document
    // Word count is cached in document metadata
    return m_textEditor->GetDocument().GetWordCount();
}

void EditorPanel::clearContent() {
    if (m_textEditor) {
        m_textEditor->GetDocument().Clear();
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
    if (!m_textEditor) {
        return;
    }

    // CRITICAL: Update word count FIRST
    // This is expensive (O(n) on text length) but debounced - only runs
    // 500ms after user stops typing, so performance is acceptable
    m_textEditor->GetDocument().UpdateWordCount();
    core::Logger::getInstance().debug("onWordCountTimer: Word count updated");

    // Get updated word count from document metadata
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
                core::Logger::getInstance().debug("StatusBar updated: {} words", wordCount);
            }
        }
    }
}


// ============================================================================
// Format Event Handlers
// ============================================================================

void EditorPanel::onFormatBold([[maybe_unused]] wxCommandEvent& event) {
    if (!m_textEditor) {
        return;
    }

    bwx_sdk::gui::bwxTextDocument& doc = m_textEditor->GetDocument();
    bwx_sdk::gui::Selection sel = doc.GetSelection();

    if (sel.IsEmpty()) {
        core::Logger::getInstance().debug("Bold: no selection");
        return;
    }

    // Toggle bold at selection
    bwx_sdk::gui::TextFormat format = doc.GetFormatAt(sel.GetMin());
    format.bold = !format.bold;
    doc.ApplyFormat(sel.GetMin(), sel.GetMax(), format);
    m_textEditor->Refresh();

    core::Logger::getInstance().debug("Applied bold={} to selection [{}, {})",
        format.bold, sel.GetMin(), sel.GetMax());
}

void EditorPanel::onFormatItalic([[maybe_unused]] wxCommandEvent& event) {
    if (!m_textEditor) {
        return;
    }

    bwx_sdk::gui::bwxTextDocument& doc = m_textEditor->GetDocument();
    bwx_sdk::gui::Selection sel = doc.GetSelection();

    if (sel.IsEmpty()) {
        core::Logger::getInstance().debug("Italic: no selection");
        return;
    }

    // Toggle italic at selection
    bwx_sdk::gui::TextFormat format = doc.GetFormatAt(sel.GetMin());
    format.italic = !format.italic;
    doc.ApplyFormat(sel.GetMin(), sel.GetMax(), format);
    m_textEditor->Refresh();

    core::Logger::getInstance().debug("Applied italic={} to selection [{}, {})",
        format.italic, sel.GetMin(), sel.GetMax());
}

void EditorPanel::onFormatUnderline([[maybe_unused]] wxCommandEvent& event) {
    if (!m_textEditor) {
        return;
    }

    bwx_sdk::gui::bwxTextDocument& doc = m_textEditor->GetDocument();
    bwx_sdk::gui::Selection sel = doc.GetSelection();

    if (sel.IsEmpty()) {
        core::Logger::getInstance().debug("Underline: no selection");
        return;
    }

    // Toggle underline at selection
    bwx_sdk::gui::TextFormat format = doc.GetFormatAt(sel.GetMin());
    format.underline = !format.underline;
    doc.ApplyFormat(sel.GetMin(), sel.GetMax(), format);
    m_textEditor->Refresh();

    core::Logger::getInstance().debug("Applied underline={} to selection [{}, {})",
        format.underline, sel.GetMin(), sel.GetMax());
}

void EditorPanel::onFormatFont([[maybe_unused]] wxCommandEvent& event) {
    if (!m_textEditor) {
        return;
    }

    bwx_sdk::gui::bwxTextDocument& doc = m_textEditor->GetDocument();
    bwx_sdk::gui::Selection sel = doc.GetSelection();

    if (sel.IsEmpty()) {
        core::Logger::getInstance().debug("Font: no selection");
        return;
    }

    // Get current format
    bwx_sdk::gui::TextFormat format = doc.GetFormatAt(sel.GetMin());

    // Show font dialog
    wxFontData fontData;
    wxFont currentFont(format.fontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
        wxFONTWEIGHT_NORMAL, false, format.fontName);
    fontData.SetInitialFont(currentFont);
    fontData.SetColour(format.textColor);

    wxFontDialog dialog(this, fontData);
    if (dialog.ShowModal() == wxID_OK) {
        wxFontData selectedData = dialog.GetFontData();
        wxFont selectedFont = selectedData.GetChosenFont();
        wxColour selectedColor = selectedData.GetColour();

        // Apply new font and color
        format.fontName = selectedFont.GetFaceName();
        format.fontSize = selectedFont.GetPointSize();
        format.textColor = selectedColor;

        doc.ApplyFormat(sel.GetMin(), sel.GetMax(), format);
        m_textEditor->Refresh();

        core::Logger::getInstance().debug("Applied font={} size={} color=#{:02X}{:02X}{:02X} to selection [{}, {})",
            format.fontName.ToStdString(), format.fontSize,
            format.textColor.Red(), format.textColor.Green(), format.textColor.Blue(),
            sel.GetMin(), sel.GetMax());
    }
}

void EditorPanel::onFormatClear([[maybe_unused]] wxCommandEvent& event) {
    if (!m_textEditor) {
        return;
    }

    bwx_sdk::gui::bwxTextDocument& doc = m_textEditor->GetDocument();
    bwx_sdk::gui::Selection sel = doc.GetSelection();

    if (sel.IsEmpty()) {
        core::Logger::getInstance().debug("Clear formatting: no selection");
        return;
    }

    // Clear formatting (apply default format)
    doc.ClearFormatting(sel.GetMin(), sel.GetMax());
    m_textEditor->Refresh();

    core::Logger::getInstance().debug("Cleared formatting for selection [{}, {})",
        sel.GetMin(), sel.GetMax());
}


// ============================================================================
// Edit Menu Event Handlers (Task #00019)
// ============================================================================

void EditorPanel::onEditCut([[maybe_unused]] wxCommandEvent& event) {
    if (!m_textEditor) {
        core::Logger::getInstance().warn("Cut: text editor not initialized");
        return;
    }

    m_textEditor->Cut();
    core::Logger::getInstance().debug("Cut operation executed");
}

void EditorPanel::onEditCopy([[maybe_unused]] wxCommandEvent& event) {
    if (!m_textEditor) {
        core::Logger::getInstance().warn("Copy: text editor not initialized");
        return;
    }

    m_textEditor->Copy();
    core::Logger::getInstance().debug("Copy operation executed");
}

void EditorPanel::onEditPaste([[maybe_unused]] wxCommandEvent& event) {
    if (!m_textEditor) {
        core::Logger::getInstance().warn("Paste: text editor not initialized");
        return;
    }

    m_textEditor->Paste();
    core::Logger::getInstance().debug("Paste operation executed");
}

void EditorPanel::onEditSelectAll([[maybe_unused]] wxCommandEvent& event) {
    if (!m_textEditor) {
        core::Logger::getInstance().warn("Select All: text editor not initialized");
        return;
    }

    m_textEditor->SelectAll();
    core::Logger::getInstance().debug("Select All operation executed");
}


// ============================================================================
// View Mode Control (Task #00019)
// ============================================================================

void EditorPanel::setViewMode(bwx_sdk::gui::bwxTextEditor::ViewMode mode) {
    if (!m_textEditor) {
        core::Logger::getInstance().warn("setViewMode: text editor not initialized");
        return;
    }

    m_textEditor->SetViewMode(mode);
    core::Logger::getInstance().info("Editor view mode changed to: {}", static_cast<int>(mode));
}


// ============================================================================
// IDocumentObserver Implementation (Task #00019 Days 12-13)
// ============================================================================

void EditorPanel::OnTextChanged() {
    // Implement TRUE DEBOUNCING:
    // Restart timer on each text change - timer triggers 500ms AFTER last change
    // Using wxTIMER_ONE_SHOT = timer triggers once, then stops automatically
    // Each text change restarts the timer = perfect debouncing pattern!
    if (m_wordCountTimer) {
        m_wordCountTimer->Stop();
        m_wordCountTimer->Start(500, wxTIMER_ONE_SHOT);
    }

    // CRITICAL: Mark content as modified
    // Fixes hasUnsavedChanges() bug - was never set to true!
    m_isModified = true;

    core::Logger::getInstance().debug("OnTextChanged: Timer restarted (debouncing), modified flag set");
}

void EditorPanel::OnCursorMoved() {
    // Cursor movement doesn't affect word count - ignore
    // No timer restart needed (optimization)
}

void EditorPanel::OnSelectionChanged() {
    // Selection change doesn't affect word count - ignore
    // No timer restart needed (optimization)
}

void EditorPanel::OnFormatChanged() {
    // Formatting changes MIGHT affect word count in edge cases
    // (e.g., Clear Formatting could theoretically delete characters - unlikely but possible)
    // Restart timer to be safe
    if (m_wordCountTimer) {
        m_wordCountTimer->Stop();
        m_wordCountTimer->Start(500, wxTIMER_ONE_SHOT);
    }

    // Mark as modified
    m_isModified = true;

    core::Logger::getInstance().debug("OnFormatChanged: Timer restarted, modified flag set");
}


} // namespace gui
} // namespace kalahari
