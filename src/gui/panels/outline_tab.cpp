/// @file outline_tab.cpp
/// @brief Implementation of OutlineTab (Task #00020 Phase 2)

#include "kalahari/gui/panels/outline_tab.h"
#include "kalahari/gui/icon_registry.h"
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

    // Create horizontal toolbar (top)
    m_toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER);

    // Set toolbar button size from IconRegistry (24px)
    auto& iconReg = IconRegistry::getInstance();
    int toolbarSize = iconReg.getSizeForClient(wxART_TOOLBAR);
    m_toolBar->SetToolBitmapSize(wxSize(toolbarSize, toolbarSize));

    // Add toolbar buttons with Material Design SVG icons
    // Custom tool IDs for buttons without standard wxID
    enum {
        ID_OUTLINE_RENAME = wxID_HIGHEST + 100,
        ID_OUTLINE_EXPAND_ALL,
        ID_OUTLINE_COLLAPSE_ALL
    };

    // Button 1: Add Chapter
    wxBitmap addIcon = wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR, wxDefaultSize);
    m_toolBar->AddTool(wxID_ADD, _("Add Chapter"), addIcon, _("Add new chapter"));
    m_toolBar->Bind(wxEVT_TOOL, &OutlineTab::onAddChapter, this, wxID_ADD);

    // Button 2: Rename (using EDIT icon from IconRegistry)
    wxBitmap renameIcon = wxArtProvider::GetBitmap(wxART_HELP_SETTINGS, wxART_TOOLBAR, wxDefaultSize);
    m_toolBar->AddTool(ID_OUTLINE_RENAME, _("Rename"), renameIcon, _("Rename selected item"));
    m_toolBar->Bind(wxEVT_TOOL, &OutlineTab::onRenameChapter, this, ID_OUTLINE_RENAME);

    // Button 3: Delete
    wxBitmap deleteIcon = wxArtProvider::GetBitmap(wxART_DELETE, wxART_TOOLBAR, wxDefaultSize);
    m_toolBar->AddTool(wxID_DELETE, _("Delete"), deleteIcon, _("Delete selected item"));
    m_toolBar->Bind(wxEVT_TOOL, &OutlineTab::onDeleteChapter, this, wxID_DELETE);

    // Button 4: Move Up (using UP_ARROW from IconRegistry)
    wxBitmap upIcon = wxArtProvider::GetBitmap(wxART_GO_UP, wxART_TOOLBAR, wxDefaultSize);
    m_toolBar->AddTool(wxID_UP, _("Move Up"), upIcon, _("Move chapter up"));
    m_toolBar->Bind(wxEVT_TOOL, &OutlineTab::onMoveChapterUp, this, wxID_UP);

    // Button 5: Move Down (using DOWN_ARROW from IconRegistry)
    wxBitmap downIcon = wxArtProvider::GetBitmap(wxART_GO_DOWN, wxART_TOOLBAR, wxDefaultSize);
    m_toolBar->AddTool(wxID_DOWN, _("Move Down"), downIcon, _("Move chapter down"));
    m_toolBar->Bind(wxEVT_TOOL, &OutlineTab::onMoveChapterDown, this, wxID_DOWN);

    // Separator
    m_toolBar->AddSeparator();

    // Button 6: Expand All (using UNFOLD_MORE from IconRegistry when available)
    wxBitmap expandIcon = wxArtProvider::GetBitmap(wxART_PLUS, wxART_TOOLBAR, wxDefaultSize);
    m_toolBar->AddTool(ID_OUTLINE_EXPAND_ALL, _("Expand All"), expandIcon, _("Expand all nodes"));
    m_toolBar->Bind(wxEVT_TOOL, &OutlineTab::onExpandAll, this, ID_OUTLINE_EXPAND_ALL);

    // Button 7: Collapse All (using UNFOLD_LESS from IconRegistry when available)
    wxBitmap collapseIcon = wxArtProvider::GetBitmap(wxART_MINUS, wxART_TOOLBAR, wxDefaultSize);
    m_toolBar->AddTool(ID_OUTLINE_COLLAPSE_ALL, _("Collapse All"), collapseIcon, _("Collapse all nodes"));
    m_toolBar->Bind(wxEVT_TOOL, &OutlineTab::onCollapseAll, this, ID_OUTLINE_COLLAPSE_ALL);

    m_toolBar->Realize();

    // Add toolbar to sizer (fixed height at top)
    mainSizer->Add(m_toolBar, 0, wxEXPAND);

    // Create wxTreeCtrl
    m_tree = new wxTreeCtrl(this, wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT | wxTR_SINGLE);

    // Create image list (use IconRegistry size for trees/panels)
    int iconSize = iconReg.getSizeForClient(wxART_OTHER);
    m_imageList = new wxImageList(iconSize, iconSize);

    // Load icons from wxArtProvider (Material Design SVG via KalahariArtProvider)
    // Icon 0: Book (FILE_NEW placeholder, will be 'menu_book' later)
    wxBitmap bookIcon = wxArtProvider::GetBitmap(wxART_NEW, wxART_OTHER, wxDefaultSize);
    m_imageList->Add(bookIcon);

    // Icon 1: Part (FOLDER - already correct)
    wxBitmap partIcon = wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, wxDefaultSize);
    m_imageList->Add(partIcon);

    // Icon 2: Chapter (FILE_OPEN placeholder, will be 'description' later)
    wxBitmap chapterIcon = wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_OTHER, wxDefaultSize);
    m_imageList->Add(chapterIcon);

    // Assign image list to tree (tree takes ownership)
    m_tree->AssignImageList(m_imageList);

    // Add tree to sizer (flexible height, fills remaining space)
    mainSizer->Add(m_tree, 1, wxEXPAND);
    SetSizer(mainSizer);

    // Populate tree if document available
    if (m_document) {
        populateTree();
    }

    // Bind keyboard shortcuts for tree operations
    m_tree->Bind(wxEVT_CHAR_HOOK, &OutlineTab::onKeyDown, this);

    logger.info("OutlineTab: wxTreeCtrl created with 3 icon types and keyboard shortcuts");
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
        logger.info("OutlineTab::onTreeItemActivated() - Chapter {} activated (double-click)", data->getId());

        // Note: For now we just log activation
        // Full editor integration will be implemented when EditorPanel supports loading chapters
        // The infrastructure is ready (MainWindow can receive events via NavigatorPanel)
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

        // Note: Editor integration ready for future implementation
        // When EditorPanel supports loadChapter(chapterId):
        // 1. Define custom event: wxDECLARE_EVENT(EVT_LOAD_CHAPTER, wxCommandEvent)
        // 2. Post event to NavigatorPanel parent
        // 3. NavigatorPanel forwards to MainWindow
        // 4. MainWindow calls m_editorPanel->loadChapter(chapterId)
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

    // Bind events for all menu items
    menu.Bind(wxEVT_MENU, &OutlineTab::onAddChapter, this, ID_ADD_CHAPTER);
    menu.Bind(wxEVT_MENU, &OutlineTab::onAddPart, this, ID_ADD_PART);
    menu.Bind(wxEVT_MENU, &OutlineTab::onRenameBook, this, ID_RENAME_BOOK);
    menu.Bind(wxEVT_MENU, &OutlineTab::onRenamePart, this, ID_RENAME_PART);
    menu.Bind(wxEVT_MENU, &OutlineTab::onRenameChapter, this, ID_RENAME_CHAPTER);
    menu.Bind(wxEVT_MENU, &OutlineTab::onDeletePart, this, ID_DELETE_PART);
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
    logger.debug("OutlineTab::onMoveChapterUp()");

    if (!m_document || !m_contextMenuItem.IsOk()) {
        logger.error("OutlineTab::onMoveChapterUp() - No document or context item");
        return;
    }

    // Get chapter data
    ChapterItemData* chapterData = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(m_contextMenuItem));
    if (!chapterData || chapterData->getType() != ChapterItemData::NodeType::Chapter) {
        logger.error("OutlineTab::onMoveChapterUp() - Context item is not a Chapter");
        return;
    }

    // Get parent part
    wxTreeItemId parentItem = m_tree->GetItemParent(m_contextMenuItem);
    ChapterItemData* partData = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(parentItem));
    if (!partData || partData->getType() != ChapterItemData::NodeType::Part) {
        logger.error("OutlineTab::onMoveChapterUp() - Parent is not a Part");
        return;
    }

    // Get previous sibling
    wxTreeItemId prevSibling = m_tree->GetPrevSibling(m_contextMenuItem);
    if (!prevSibling.IsOk()) {
        logger.debug("OutlineTab::onMoveChapterUp() - Already at top");
        return; // Already at top
    }

    // Find chapter index in Part
    std::string partId = partData->getId();
    std::string chapterId = chapterData->getId();

    std::vector<std::shared_ptr<core::BookElement>>* chapters = nullptr;

    if (partId == "frontMatter") {
        chapters = &m_document->getBook().getFrontMatter();
    } else if (partId == "backMatter") {
        chapters = &m_document->getBook().getBackMatter();
    } else {
        // Find part in body
        for (auto& part : m_document->getBook().getBody()) {
            if (part->getId() == partId) {
                chapters = const_cast<std::vector<std::shared_ptr<core::BookElement>>*>(&part->getChapters());
                break;
            }
        }
    }

    if (!chapters) {
        logger.error("OutlineTab::onMoveChapterUp() - Could not find part");
        return;
    }

    // Find chapter index
    size_t currentIndex = 0;
    bool found = false;
    for (size_t i = 0; i < chapters->size(); i++) {
        if ((*chapters)[i]->getId() == chapterId) {
            currentIndex = i;
            found = true;
            break;
        }
    }

    if (!found || currentIndex == 0) {
        logger.debug("OutlineTab::onMoveChapterUp() - Chapter not found or already at top");
        return;
    }

    // Swap with previous chapter
    std::swap((*chapters)[currentIndex], (*chapters)[currentIndex - 1]);

    // Refresh tree to reflect new order
    populateTree();

    // Re-select the moved item
    selectChapter(chapterId);

    logger.info("OutlineTab::onMoveChapterUp() - Chapter {} moved up", chapterId);
}

void OutlineTab::onMoveChapterDown([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onMoveChapterDown()");

    if (!m_document || !m_contextMenuItem.IsOk()) {
        logger.error("OutlineTab::onMoveChapterDown() - No document or context item");
        return;
    }

    // Get chapter data
    ChapterItemData* chapterData = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(m_contextMenuItem));
    if (!chapterData || chapterData->getType() != ChapterItemData::NodeType::Chapter) {
        logger.error("OutlineTab::onMoveChapterDown() - Context item is not a Chapter");
        return;
    }

    // Get parent part
    wxTreeItemId parentItem = m_tree->GetItemParent(m_contextMenuItem);
    ChapterItemData* partData = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(parentItem));
    if (!partData || partData->getType() != ChapterItemData::NodeType::Part) {
        logger.error("OutlineTab::onMoveChapterDown() - Parent is not a Part");
        return;
    }

    // Get next sibling
    wxTreeItemId nextSibling = m_tree->GetNextSibling(m_contextMenuItem);
    if (!nextSibling.IsOk()) {
        logger.debug("OutlineTab::onMoveChapterDown() - Already at bottom");
        return; // Already at bottom
    }

    // Find chapter index in Part
    std::string partId = partData->getId();
    std::string chapterId = chapterData->getId();

    std::vector<std::shared_ptr<core::BookElement>>* chapters = nullptr;

    if (partId == "frontMatter") {
        chapters = &m_document->getBook().getFrontMatter();
    } else if (partId == "backMatter") {
        chapters = &m_document->getBook().getBackMatter();
    } else {
        // Find part in body
        for (auto& part : m_document->getBook().getBody()) {
            if (part->getId() == partId) {
                chapters = const_cast<std::vector<std::shared_ptr<core::BookElement>>*>(&part->getChapters());
                break;
            }
        }
    }

    if (!chapters) {
        logger.error("OutlineTab::onMoveChapterDown() - Could not find part");
        return;
    }

    // Find chapter index
    size_t currentIndex = 0;
    bool found = false;
    for (size_t i = 0; i < chapters->size(); i++) {
        if ((*chapters)[i]->getId() == chapterId) {
            currentIndex = i;
            found = true;
            break;
        }
    }

    if (!found || currentIndex >= chapters->size() - 1) {
        logger.debug("OutlineTab::onMoveChapterDown() - Chapter not found or already at bottom");
        return;
    }

    // Swap with next chapter
    std::swap((*chapters)[currentIndex], (*chapters)[currentIndex + 1]);

    // Refresh tree to reflect new order
    populateTree();

    // Re-select the moved item
    selectChapter(chapterId);

    logger.info("OutlineTab::onMoveChapterDown() - Chapter {} moved down", chapterId);
}

void OutlineTab::onRenameBook([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onRenameBook()");

    if (!m_document || !m_contextMenuItem.IsOk()) {
        logger.error("OutlineTab::onRenameBook() - No document or context item");
        return;
    }

    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(m_contextMenuItem));
    if (!data || data->getType() != ChapterItemData::NodeType::Book) {
        logger.error("OutlineTab::onRenameBook() - Context item is not a Book");
        return;
    }

    // Show dialog to get new book name
    wxString currentName = m_tree->GetItemText(m_contextMenuItem);
    wxTextEntryDialog dialog(this, "Enter new book name:", "Rename Book", currentName);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    wxString newName = dialog.GetValue().Trim();
    if (newName.IsEmpty()) {
        wxMessageBox("Book name cannot be empty", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    // Update document (title is in Document, not Book)
    m_document->setTitle(newName.ToStdString());
    m_document->touch();

    // Update tree
    m_tree->SetItemText(m_contextMenuItem, newName);

    logger.info("OutlineTab::onRenameBook() - Book renamed to: {}", newName.ToStdString());
}

void OutlineTab::onRenamePart([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onRenamePart()");

    if (!m_document || !m_contextMenuItem.IsOk()) {
        logger.error("OutlineTab::onRenamePart() - No document or context item");
        return;
    }

    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(m_contextMenuItem));
    if (!data || data->getType() != ChapterItemData::NodeType::Part) {
        logger.error("OutlineTab::onRenamePart() - Context item is not a Part");
        return;
    }

    std::string partId = data->getId();

    // Don't allow renaming frontMatter and backMatter
    if (partId == "frontMatter" || partId == "backMatter") {
        wxMessageBox("Cannot rename Front Matter or Back Matter", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    // Show dialog to get new part name
    wxString currentName = m_tree->GetItemText(m_contextMenuItem);
    wxTextEntryDialog dialog(this, "Enter new part name:", "Rename Part", currentName);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    wxString newName = dialog.GetValue().Trim();
    if (newName.IsEmpty()) {
        wxMessageBox("Part name cannot be empty", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    // Find and update part in document
    for (auto& part : m_document->getBook().getBody()) {
        if (part->getId() == partId) {
            part->setTitle(newName.ToStdString());
            m_document->touch();

            // Update tree
            m_tree->SetItemText(m_contextMenuItem, newName);

            logger.info("OutlineTab::onRenamePart() - Part {} renamed to: {}", partId, newName.ToStdString());
            return;
        }
    }

    logger.error("OutlineTab::onRenamePart() - Part {} not found", partId);
}

void OutlineTab::onDeletePart([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onDeletePart()");

    if (!m_document || !m_contextMenuItem.IsOk()) {
        logger.error("OutlineTab::onDeletePart() - No document or context item");
        return;
    }

    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(m_contextMenuItem));
    if (!data || data->getType() != ChapterItemData::NodeType::Part) {
        logger.error("OutlineTab::onDeletePart() - Context item is not a Part");
        return;
    }

    std::string partId = data->getId();

    // Don't allow deleting frontMatter and backMatter
    if (partId == "frontMatter" || partId == "backMatter") {
        logger.error("OutlineTab::onDeletePart() - Cannot delete Front Matter or Back Matter");
        return;
    }

    // Confirmation dialog
    wxString partName = m_tree->GetItemText(m_contextMenuItem);
    int result = wxMessageBox(
        wxString::Format("Delete part \"%s\" and all its chapters?\n\nThis action cannot be undone.", partName),
        "Confirm Delete",
        wxYES_NO | wxICON_WARNING,
        this
    );

    if (result != wxYES) {
        return;
    }

    // Find and remove part from document
    auto& body = m_document->getBook().getBody();
    auto it = std::remove_if(body.begin(), body.end(),
        [&partId](const std::shared_ptr<core::Part>& part) {
            return part->getId() == partId;
        });

    if (it != body.end()) {
        body.erase(it, body.end());
        m_document->touch();

        // Update tree
        m_tree->Delete(m_contextMenuItem);

        logger.info("OutlineTab::onDeletePart() - Deleted part: {}", partName.ToStdString());
    } else {
        logger.error("OutlineTab::onDeletePart() - Part {} not found", partId);
    }
}

void OutlineTab::onAddPart([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onAddPart()");

    if (!m_document || !m_contextMenuItem.IsOk()) {
        logger.error("OutlineTab::onAddPart() - No document or context item");
        return;
    }

    ChapterItemData* data = dynamic_cast<ChapterItemData*>(m_tree->GetItemData(m_contextMenuItem));
    if (!data || data->getType() != ChapterItemData::NodeType::Book) {
        logger.error("OutlineTab::onAddPart() - Context item is not a Book");
        return;
    }

    // Show dialog to get part name
    wxTextEntryDialog dialog(this, "Enter part name:", "Add Part", "New Part");
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    wxString partName = dialog.GetValue().Trim();
    if (partName.IsEmpty()) {
        wxMessageBox("Part name cannot be empty", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    // Create new part
    std::string partId = core::Document::generateId();
    auto part = std::make_shared<core::Part>(
        partId,
        partName.ToStdString()
    );

    // Add to document body
    m_document->getBook().addPart(part);
    m_document->touch();

    // Refresh tree to show new part
    populateTree();

    logger.info("OutlineTab::onAddPart() - Added new part: {}", partName.ToStdString());
}

// ============================================================================
// Toolbar Handlers
// ============================================================================

void OutlineTab::onExpandAll([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onExpandAll()");

    if (!m_tree) {
        logger.error("OutlineTab::onExpandAll() - Tree control not initialized");
        return;
    }

    // Get root item (hidden, but we need it to iterate children)
    wxTreeItemId root = m_tree->GetRootItem();
    if (!root.IsOk()) {
        logger.debug("OutlineTab::onExpandAll() - No root item");
        return;
    }

    // Expand all nodes recursively (but not root, as it's hidden)
    expandAllRecursive(root, true);

    logger.info("OutlineTab: Expanded all nodes");
}

void OutlineTab::onCollapseAll([[maybe_unused]] wxCommandEvent& event)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("OutlineTab::onCollapseAll()");

    if (!m_tree) {
        logger.error("OutlineTab::onCollapseAll() - Tree control not initialized");
        return;
    }

    // Get root item (hidden, but we need it to iterate children)
    wxTreeItemId root = m_tree->GetRootItem();
    if (!root.IsOk()) {
        logger.debug("OutlineTab::onCollapseAll() - No root item");
        return;
    }

    // Collapse all nodes recursively (but not root, as it's hidden)
    collapseAllRecursive(root, true);

    logger.info("OutlineTab: Collapsed all nodes");
}

// ============================================================================
// Helper Methods
// ============================================================================

void OutlineTab::expandAllRecursive(wxTreeItemId item, bool isRoot)
{
    if (!item.IsOk()) return;

    // Expand this item (but not root, as it's hidden)
    if (!isRoot) {
        m_tree->Expand(item);
    }

    // Recursively expand all children
    wxTreeItemIdValue cookie;
    wxTreeItemId child = m_tree->GetFirstChild(item, cookie);
    while (child.IsOk()) {
        expandAllRecursive(child, false);
        child = m_tree->GetNextChild(item, cookie);
    }
}

void OutlineTab::collapseAllRecursive(wxTreeItemId item, bool isRoot)
{
    if (!item.IsOk()) return;

    // Recursively collapse all children first
    wxTreeItemIdValue cookie;
    wxTreeItemId child = m_tree->GetFirstChild(item, cookie);
    while (child.IsOk()) {
        collapseAllRecursive(child, false);
        child = m_tree->GetNextChild(item, cookie);
    }

    // Collapse this item (but not root, as it's hidden)
    if (!isRoot) {
        m_tree->Collapse(item);
    }
}

// ============================================================================
// Keyboard Shortcuts
// ============================================================================

void OutlineTab::onKeyDown(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();
    bool ctrlDown = event.ControlDown();

    // Get currently selected item
    wxTreeItemId selectedItem = m_tree->GetSelection();
    if (!selectedItem.IsOk()) {
        event.Skip(); // No selection, let default processing happen
        return;
    }

    // F2 - Rename
    if (keyCode == WXK_F2) {
        auto& logger = core::Logger::getInstance();
        logger.debug("OutlineTab: F2 pressed - Rename");

        m_contextMenuItem = selectedItem;
        wxCommandEvent cmdEvent(wxEVT_COMMAND_MENU_SELECTED, wxID_ANY);
        onRenameChapter(cmdEvent);
        return; // Don't skip - we handled it
    }

    // Delete key - Delete item
    if (keyCode == WXK_DELETE) {
        auto& logger = core::Logger::getInstance();
        logger.debug("OutlineTab: Delete pressed - Delete item");

        m_contextMenuItem = selectedItem;
        wxCommandEvent cmdEvent(wxEVT_COMMAND_MENU_SELECTED, wxID_ANY);
        onDeleteChapter(cmdEvent);
        return; // Don't skip - we handled it
    }

    // Ctrl+Up - Move chapter up
    if (ctrlDown && keyCode == WXK_UP) {
        auto& logger = core::Logger::getInstance();
        logger.debug("OutlineTab: Ctrl+Up pressed - Move chapter up");

        m_contextMenuItem = selectedItem;
        wxCommandEvent cmdEvent(wxEVT_COMMAND_MENU_SELECTED, wxID_ANY);
        onMoveChapterUp(cmdEvent);
        return; // Don't skip - we handled it
    }

    // Ctrl+Down - Move chapter down
    if (ctrlDown && keyCode == WXK_DOWN) {
        auto& logger = core::Logger::getInstance();
        logger.debug("OutlineTab: Ctrl+Down pressed - Move chapter down");

        m_contextMenuItem = selectedItem;
        wxCommandEvent cmdEvent(wxEVT_COMMAND_MENU_SELECTED, wxID_ANY);
        onMoveChapterDown(cmdEvent);
        return; // Don't skip - we handled it
    }

    // Let default processing happen for other keys
    event.Skip();
}

} // namespace gui
} // namespace kalahari
