/// @file outline_tab.cpp
/// @brief Implementation of OutlineTab (Task #00020 Phase 2)

#include "kalahari/gui/panels/outline_tab.h"
#include <kalahari/core/logger.h>
#include <kalahari/core/document.h>
#include <kalahari/core/book.h>
#include <kalahari/core/part.h>
#include <kalahari/core/book_element.h>
#include <wx/artprov.h>

namespace kalahari {
namespace gui {

// ============================================================================
// ChapterItemData - Store chapter ID in tree nodes
// ============================================================================

/// @brief Custom tree item data to store chapter/part/book IDs
class ChapterItemData : public wxTreeItemData {
public:
    enum class NodeType {
        Book,
        Part,
        Chapter
    };

    ChapterItemData(NodeType type, const std::string& id)
        : m_type(type), m_id(id) {}

    NodeType getType() const { return m_type; }
    const std::string& getId() const { return m_id; }

private:
    NodeType m_type;
    std::string m_id;
};

// ============================================================================
// Event Table (Phase 2)
// ============================================================================

wxBEGIN_EVENT_TABLE(OutlineTab, wxPanel)
    EVT_TREE_ITEM_ACTIVATED(wxID_ANY, OutlineTab::onTreeItemActivated)
    EVT_TREE_ITEM_RIGHT_CLICK(wxID_ANY, OutlineTab::onTreeItemRightClick)
    EVT_TREE_SEL_CHANGED(wxID_ANY, OutlineTab::onTreeSelectionChanged)
wxEND_EVENT_TABLE()

// ============================================================================
// Constructor
// ============================================================================

OutlineTab::OutlineTab(wxWindow* parent, core::Document* document)
    : wxPanel(parent), m_document(document)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab: Creating (Phase 2 - Full wxTreeCtrl)");

    // Main sizer
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Create wxTreeCtrl
    m_tree = new wxTreeCtrl(this, wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT | wxTR_SINGLE);

    // Create image list (16x16 icons)
    m_imageList = new wxImageList(16, 16);

    // Load icons from wxArtProvider (placeholders)
    // Icon 0: Book (FILE_NEW placeholder, will be 'menu_book' later)
    wxBitmap bookIcon = wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16, 16));
    m_imageList->Add(bookIcon);

    // Icon 1: Part (FOLDER - already correct)
    wxBitmap partIcon = wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, wxSize(16, 16));
    m_imageList->Add(partIcon);

    // Icon 2: Chapter (FILE_OPEN placeholder, will be 'description' later)
    wxBitmap chapterIcon = wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_OTHER, wxSize(16, 16));
    m_imageList->Add(chapterIcon);

    // Assign image list to tree (tree takes ownership)
    m_tree->AssignImageList(m_imageList);

    // Add tree to sizer
    mainSizer->Add(m_tree, 1, wxEXPAND);
    SetSizer(mainSizer);

    // Populate tree if document available
    if (m_document) {
        populateTree();
    }

    logger.info("OutlineTab: wxTreeCtrl created with 3 icon types");
}

// ============================================================================
// Destructor
// ============================================================================

OutlineTab::~OutlineTab()
{
    core::Logger::getInstance().debug("OutlineTab: Destroying");
    // m_imageList is managed by wxTreeCtrl (AssignImageList), no manual cleanup needed
}

// ============================================================================
// Public API (Phase 2)
// ============================================================================

void OutlineTab::populateTree()
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::populateTree() - Building tree from Document");

    if (!m_tree) {
        logger.error("OutlineTab::populateTree() - m_tree is nullptr!");
        return;
    }

    // Clear existing tree
    m_tree->DeleteAllItems();

    if (!m_document) {
        logger.debug("OutlineTab::populateTree() - No document, tree cleared");
        return;
    }

    const core::Book& book = m_document->getBook();

    // Add hidden root
    wxTreeItemId hiddenRoot = m_tree->AddRoot("Root");

    // Add Book node (visible root)
    wxTreeItemId bookItem = m_tree->AppendItem(
        hiddenRoot,
        m_document->getTitle(),
        0, // Icon 0 = Book
        -1,
        new ChapterItemData(ChapterItemData::NodeType::Book, m_document->getId())
    );

    // Add Front Matter
    auto& frontMatter = book.getFrontMatter();
    if (!frontMatter.empty()) {
        wxTreeItemId frontItem = m_tree->AppendItem(
            bookItem,
            "Front Matter",
            1, // Icon 1 = Part
            -1,
            new ChapterItemData(ChapterItemData::NodeType::Part, "frontMatter")
        );

        for (const auto& element : frontMatter) {
            m_tree->AppendItem(
                frontItem,
                element->getTitle(),
                2, // Icon 2 = Chapter
                -1,
                new ChapterItemData(ChapterItemData::NodeType::Chapter, element->getId())
            );
        }
    }

    // Add Body (Parts → Chapters)
    auto& body = book.getBody();
    for (const auto& part : body) {
        wxTreeItemId partItem = m_tree->AppendItem(
            bookItem,
            part->getTitle(),
            1, // Icon 1 = Part
            -1,
            new ChapterItemData(ChapterItemData::NodeType::Part, part->getId())
        );

        for (const auto& chapter : part->getChapters()) {
            m_tree->AppendItem(
                partItem,
                chapter->getTitle(),
                2, // Icon 2 = Chapter
                -1,
                new ChapterItemData(ChapterItemData::NodeType::Chapter, chapter->getId())
            );
        }

        // Expand part by default
        m_tree->Expand(partItem);
    }

    // Add Back Matter
    auto& backMatter = book.getBackMatter();
    if (!backMatter.empty()) {
        wxTreeItemId backItem = m_tree->AppendItem(
            bookItem,
            "Back Matter",
            1, // Icon 1 = Part
            -1,
            new ChapterItemData(ChapterItemData::NodeType::Part, "backMatter")
        );

        for (const auto& element : backMatter) {
            m_tree->AppendItem(
                backItem,
                element->getTitle(),
                2, // Icon 2 = Chapter
                -1,
                new ChapterItemData(ChapterItemData::NodeType::Chapter, element->getId())
            );
        }
    }

    // Expand book node
    m_tree->Expand(bookItem);

    logger.info("OutlineTab::populateTree() - Tree populated with {} parts",
        body.size() + (frontMatter.empty() ? 0 : 1) + (backMatter.empty() ? 0 : 1));
}

void OutlineTab::selectChapter(const std::string& chapterId)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::selectChapter({})", chapterId);

    if (!m_tree) {
        logger.error("OutlineTab::selectChapter() - m_tree is nullptr!");
        return;
    }

    // Find tree item by chapter ID (recursive search)
    wxTreeItemId found = findItemById(m_tree->GetRootItem(), chapterId);

    if (found.IsOk()) {
        m_tree->SelectItem(found);
        m_tree->EnsureVisible(found);
        logger.debug("OutlineTab::selectChapter() - Chapter {} selected", chapterId);
    } else {
        logger.warn("OutlineTab::selectChapter() - Chapter {} not found", chapterId);
    }
}

void OutlineTab::setDocument(core::Document* document)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::setDocument() - New document: {}",
        document ? document->getTitle() : "nullptr");

    m_document = document;
    populateTree();
}

// ============================================================================
// Helper Methods
// ============================================================================

wxTreeItemId OutlineTab::findItemById(wxTreeItemId parent, const std::string& id)
{
    if (!parent.IsOk()) {
        return wxTreeItemId();
    }

    // Check current item
    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(parent));
    if (data && data->getId() == id) {
        return parent;
    }

    // Recursively search children
    wxTreeItemIdValue cookie;
    wxTreeItemId child = m_tree->GetFirstChild(parent, cookie);
    while (child.IsOk()) {
        wxTreeItemId found = findItemById(child, id);
        if (found.IsOk()) {
            return found;
        }
        child = m_tree->GetNextChild(parent, cookie);
    }

    return wxTreeItemId(); // Not found
}

// ============================================================================
// Event Handlers (Phase 3)
// ============================================================================

void OutlineTab::onTreeItemActivated(wxTreeEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onTreeItemActivated() - Double-click");

    wxTreeItemId item = event.GetItem();
    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(item));

    if (!data) {
        logger.warn("OutlineTab::onTreeItemActivated() - No data attached to item");
        return;
    }

    // Only handle Chapter nodes
    if (data->getType() == ChapterItemData::NodeType::Chapter) {
        logger.info("OutlineTab::onTreeItemActivated() - Chapter {} activated", data->getId());

        // TODO Phase 3.2: Notify MainWindow to load chapter in EditorPanel
        // wxCommandEvent evt(wxEVT_KALAHARI_LOAD_CHAPTER);
        // evt.SetString(data->getId());
        // wxPostEvent(GetParent(), evt);
    }
}

void OutlineTab::onTreeItemRightClick(wxTreeEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onTreeItemRightClick() - Right-click");

    wxTreeItemId item = event.GetItem();
    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(item));

    if (!data) {
        logger.warn("OutlineTab::onTreeItemRightClick() - No data attached to item");
        return;
    }

    logger.debug("OutlineTab::onTreeItemRightClick() - Node type: {}, ID: {}",
        static_cast<int>(data->getType()), data->getId());

    // Phase 4: Show context menu
    showContextMenu(item, event.GetPoint());
}

void OutlineTab::onTreeSelectionChanged(wxTreeEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onTreeSelectionChanged() - Selection changed");

    wxTreeItemId item = event.GetItem();
    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(item));

    if (!data) {
        logger.warn("OutlineTab::onTreeSelectionChanged() - No data attached to item");
        return;
    }

    // Only handle Chapter nodes
    if (data->getType() == ChapterItemData::NodeType::Chapter) {
        logger.info("OutlineTab::onTreeSelectionChanged() - Chapter {} selected", data->getId());

        // TODO Phase 3.2: Notify MainWindow to load chapter in EditorPanel
        // wxCommandEvent evt(wxEVT_KALAHARI_LOAD_CHAPTER);
        // evt.SetString(data->getId());
        // wxPostEvent(GetParent()->GetParent(), evt); // NavigatorPanel -> MainWindow
    }
}

// ============================================================================
// Context Menu Handlers (Phase 4)
// ============================================================================

enum {
    ID_ADD_CHAPTER = wxID_HIGHEST + 1000,
    ID_RENAME_BOOK,
    ID_RENAME_PART,
    ID_RENAME_CHAPTER,
    ID_DELETE_PART,
    ID_DELETE_CHAPTER,
    ID_MOVE_CHAPTER_UP,
    ID_MOVE_CHAPTER_DOWN,
    ID_ADD_PART
};

void OutlineTab::showContextMenu(wxTreeItemId item, const wxPoint& pos)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::showContextMenu()");

    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(item));
    if (!data) {
        logger.warn("OutlineTab::showContextMenu() - No data attached to item");
        return;
    }

    wxMenu menu;

    switch (data->getType()) {
        case ChapterItemData::NodeType::Book:
            // Book node context menu
            menu.Append(ID_RENAME_BOOK, "Rename Book");
            menu.AppendSeparator();
            menu.Append(ID_ADD_PART, "Add Part");
            break;

        case ChapterItemData::NodeType::Part:
            // Part node context menu
            menu.Append(ID_ADD_CHAPTER, "Add Chapter");
            menu.AppendSeparator();
            menu.Append(ID_RENAME_PART, "Rename Part");
            if (data->getId() != "frontMatter" && data->getId() != "backMatter") {
                // Only allow deleting custom parts, not front/back matter
                menu.Append(ID_DELETE_PART, "Delete Part");
            }
            break;

        case ChapterItemData::NodeType::Chapter:
            // Chapter node context menu
            menu.Append(ID_RENAME_CHAPTER, "Rename Chapter");
            menu.Append(ID_DELETE_CHAPTER, "Delete Chapter");
            menu.AppendSeparator();
            menu.Append(ID_MOVE_CHAPTER_UP, "Move Up");
            menu.Append(ID_MOVE_CHAPTER_DOWN, "Move Down");
            break;
    }

    // Store current item for event handlers
    m_contextMenuItem = item;

    // Bind events
    menu.Bind(wxEVT_MENU, &OutlineTab::onAddChapter, this, ID_ADD_CHAPTER);
    menu.Bind(wxEVT_MENU, &OutlineTab::onRenameChapter, this, ID_RENAME_CHAPTER);
    menu.Bind(wxEVT_MENU, &OutlineTab::onDeleteChapter, this, ID_DELETE_CHAPTER);
    menu.Bind(wxEVT_MENU, &OutlineTab::onMoveChapterUp, this, ID_MOVE_CHAPTER_UP);
    menu.Bind(wxEVT_MENU, &OutlineTab::onMoveChapterDown, this, ID_MOVE_CHAPTER_DOWN);

    // Show menu
    PopupMenu(&menu, pos);
}

void OutlineTab::onAddChapter([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onAddChapter()");

    if (!m_document || !m_contextMenuItem.IsOk()) {
        logger.error("OutlineTab::onAddChapter() - No document or context item");
        return;
    }

    // Get part data
    ChapterItemData* partData = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(m_contextMenuItem));
    if (!partData || partData->getType() != ChapterItemData::NodeType::Part) {
        logger.error("OutlineTab::onAddChapter() - Context item is not a Part");
        return;
    }

    // Show dialog to get chapter name
    wxTextEntryDialog dialog(this, "Enter chapter name:", "Add Chapter", "New Chapter");
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    wxString chapterName = dialog.GetValue().Trim();
    if (chapterName.IsEmpty()) {
        wxMessageBox("Chapter name cannot be empty", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    // Create new chapter
    std::string chapterId = core::Document::generateId();
    auto chapter = std::make_shared<core::BookElement>(
        "chapter",
        chapterId,
        chapterName.ToStdString(),
        ""
    );

    // Add to document
    std::string partId = partData->getId();
    if (partId == "frontMatter") {
        m_document->getBook().addFrontMatter(chapter);
    } else if (partId == "backMatter") {
        m_document->getBook().addBackMatter(chapter);
    } else {
        // Find part in body
        for (auto& part : m_document->getBook().getBody()) {
            if (part->getId() == partId) {
                part->addChapter(chapter);
                break;
            }
        }
    }

    // Update tree
    m_tree->AppendItem(m_contextMenuItem, chapterName, 2, -1,
        new ChapterItemData(ChapterItemData::NodeType::Chapter, chapterId));

    m_document->touch();
    logger.info("OutlineTab::onAddChapter() - Added chapter: {}", chapterName.ToStdString());
}

void OutlineTab::onRenameChapter([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onRenameChapter()");

    if (!m_document || !m_contextMenuItem.IsOk()) {
        logger.error("OutlineTab::onRenameChapter() - No document or context item");
        return;
    }

    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(m_contextMenuItem));
    if (!data || data->getType() != ChapterItemData::NodeType::Chapter) {
        logger.error("OutlineTab::onRenameChapter() - Context item is not a Chapter");
        return;
    }

    // Get current name
    wxString currentName = m_tree->GetItemText(m_contextMenuItem);

    // Show dialog
    wxTextEntryDialog dialog(this, "Enter new chapter name:", "Rename Chapter", currentName);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    wxString newName = dialog.GetValue().Trim();
    if (newName.IsEmpty()) {
        wxMessageBox("Chapter name cannot be empty", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    if (newName == currentName) {
        return; // No change
    }

    // Find chapter in document and rename
    std::string chapterId = data->getId();
    bool found = false;

    // Search in frontMatter
    for (auto& element : m_document->getBook().getFrontMatter()) {
        if (element->getId() == chapterId) {
            element->setTitle(newName.ToStdString());
            found = true;
            break;
        }
    }

    // Search in body
    if (!found) {
        for (auto& part : m_document->getBook().getBody()) {
            auto chapter = part->getChapter(chapterId);
            if (chapter) {
                chapter->setTitle(newName.ToStdString());
                found = true;
                break;
            }
        }
    }

    // Search in backMatter
    if (!found) {
        for (auto& element : m_document->getBook().getBackMatter()) {
            if (element->getId() == chapterId) {
                element->setTitle(newName.ToStdString());
                found = true;
                break;
            }
        }
    }

    if (found) {
        m_tree->SetItemText(m_contextMenuItem, newName);
        m_document->touch();
        logger.info("OutlineTab::onRenameChapter() - Renamed to: {}", newName.ToStdString());
    } else {
        logger.error("OutlineTab::onRenameChapter() - Chapter not found: {}", chapterId);
    }
}

void OutlineTab::onDeleteChapter([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onDeleteChapter()");

    if (!m_document || !m_contextMenuItem.IsOk()) {
        logger.error("OutlineTab::onDeleteChapter() - No document or context item");
        return;
    }

    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(m_contextMenuItem));
    if (!data || data->getType() != ChapterItemData::NodeType::Chapter) {
        logger.error("OutlineTab::onDeleteChapter() - Context item is not a Chapter");
        return;
    }

    // Confirmation dialog
    wxString chapterName = m_tree->GetItemText(m_contextMenuItem);
    int result = wxMessageBox(
        wxString::Format("Delete chapter \"%s\"?\n\nThis action cannot be undone.", chapterName),
        "Confirm Delete",
        wxYES_NO | wxICON_WARNING,
        this
    );

    if (result != wxYES) {
        return;
    }

    // Remove from document
    std::string chapterId = data->getId();
    bool removed = false;

    // Try frontMatter
    if (m_document->getBook().removeFrontMatter(chapterId)) {
        removed = true;
    }

    // Try body
    if (!removed) {
        for (auto& part : m_document->getBook().getBody()) {
            if (part->removeChapter(chapterId)) {
                removed = true;
                break;
            }
        }
    }

    // Try backMatter
    if (!removed) {
        removed = m_document->getBook().removeBackMatter(chapterId);
    }

    if (removed) {
        m_tree->Delete(m_contextMenuItem);
        m_document->touch();
        logger.info("OutlineTab::onDeleteChapter() - Deleted chapter: {}", chapterName.ToStdString());
    } else {
        logger.error("OutlineTab::onDeleteChapter() - Chapter not found: {}", chapterId);
    }
}

void OutlineTab::onMoveChapterUp([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onMoveChapterUp() - Phase 4 stub (TODO: implement)");

    // TODO: Find chapter index, call Part::moveChapter(index, index-1), refresh tree
    wxMessageBox("Move Up not yet implemented (Phase 4 TODO)", "Info", wxOK | wxICON_INFORMATION, this);
}

void OutlineTab::onMoveChapterDown([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onMoveChapterDown() - Phase 4 stub (TODO: implement)");

    // TODO: Find chapter index, call Part::moveChapter(index, index+1), refresh tree
    wxMessageBox("Move Down not yet implemented (Phase 4 TODO)", "Info", wxOK | wxICON_INFORMATION, this);
}

} // namespace gui
} // namespace kalahari
