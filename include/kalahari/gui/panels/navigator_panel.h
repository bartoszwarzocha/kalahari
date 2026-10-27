/// @file navigator_panel.h
/// @brief Navigator panel for project structure navigation (Qt6)
///
/// This file defines the NavigatorPanel class, displaying the project
/// structure tree with icons and element selection support.
///
/// OpenSpec #00033 Phase D: Enhanced with icons, element IDs, and theme refresh.
/// OpenSpec #00033 Phase F: Added "Other Files" section for standalone files.
/// OpenSpec #00034 Phase F: Added expansion state persistence between sessions.

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/gui/add_target.h"

#include <QWidget>
#include <QHash>
#include <QIcon>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

class QAction;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QToolButton;
class QTimer;
class QComboBox;

namespace kalahari {
namespace core {
    struct BookProject;
    struct PackageCard;
    struct ProjectElement;
    struct UnassignedFile;
    class BookTypeRegistry;
    enum class IconContext;
}

namespace gui {

/// @brief Navigator panel showing project structure tree
///
/// Displays a QTreeWidget for the project, named after the title of its book. Under it are the
/// book ("Book") and the Workshop, which the writer can name (core::ProjectBook::name,
/// core::ProjectWorkshop::name). The book has its front, main and back sections with the
/// book's names and their elements, and the elements inside groups (parts). A book without
/// sections shows the elements of the three sections one after another under the book's item.
/// Supports icons, element selection, and automatic theme refresh.
/// OpenSpec #00034 Phase C: Added editor synchronization (highlight current chapter).
///
/// The Workshop, which the finished book does not have, has a group for each of its kinds in
/// the order of the book's type, named after the kind ("Working notes"), with the elements in
/// alphabetical order of their titles. A kind whose elements the program can make has its
/// group always, also empty, so that an element can be added to it; the other kinds when they
/// have elements. A grouped Workshop (core::ProjectWorkshop::grouped) has the groups of cards
/// in "Libraries" and those of materials in "Resources". The elements of the Workshop have no
/// status in the tree, and are not dragged: their place is that of their title.
///
/// The chapter files in the folders of the book and of the Workshop that no element has (the
/// files of elements deleted without their files) are listed, grey, under "Unassigned Files"
/// at the end of the project, from where they go back to the book or are deleted.
///
/// "Add..." on a node (its menu, the "+" button of the bar, Insert or Ctrl+Shift+Enter) asks
/// for the Add Element window with the kinds of that node (requestAdd()). The menu of a node
/// also names the kinds most added there ("Add Chapter...", "Add Part..."), unless the setting
/// "navigator.addCommands" is "single". F2 renames the node and Delete deletes the element.
///
/// An element is dragged to any place of the book its kind can have (dropPlaceOf()), also in
/// another group or section; a collapsed group or section under it opens after a moment, so
/// that it can go inside. The drop asks for the move (elementMovedTo()), and the tree shows
/// the book as it is once it is loaded again.
///
/// The files of the book outside its text, such as notes, which the finished book does not
/// have, are in a group of their own after the sections, "Other Files" (core::ProjectBook::files).
/// Every file open outside the book is listed under "Files Outside the Book", a node of its own
/// after the project, also a new file that has no file yet (addNewFile()).
///
/// Item data: Qt::UserRole holds the element ID (the path of a standalone file or the key of a
/// new one, the file of an unassigned file relative to the project folder, "workshop:" and the
/// kind or the shelf for a group of the Workshop), Qt::UserRole + 1 the item type: "document"
/// (the project), "book", "section_frontmatter", "section_body", "section_backmatter",
/// "text_element", "group_element", "window_element", "book_files", "book_file", "workshop",
/// "workshop_shelf", "workshop_group", "unassigned_files", "unassigned_file", "other_files" or
/// "standalone_file". The elements of the Workshop are "text_element" and "window_element"
/// items, which isWorkshopItem() tells apart.
class NavigatorPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit NavigatorPanel(QWidget* parent = nullptr);

    /// @brief Highlight element in tree by ID (OpenSpec #00034 Phase C)
    /// @param elementId Element ID to highlight
    /// @note Scrolls to the item and expands parent nodes
    /// @note Uses theme-aware highlight color (QPalette::Highlight with alpha)
    void highlightElement(const QString& elementId);

    /// @brief Clear current highlight (OpenSpec #00034 Phase C)
    void clearHighlight();

    /// @brief Load the first book of a project into the tree
    /// @param project Project to display
    /// @param registry Packages whose kinds give the elements their form and icon
    /// @note The files that no element has come with setUnassignedFiles()
    void loadProject(const core::BookProject& project, const core::BookTypeRegistry& registry);

    /// @brief List the chapter files that no element has under "Unassigned Files", after the
    /// sections of the book; without files the tree has no such item
    ///
    /// The item opens when it gets more files than it had, so that the writer sees the files of
    /// the elements just deleted from the book; else it stays as the writer left it.
    /// @param files The files (core::ProjectManager::unassignedFiles())
    /// @param projectFolder Folder of the project: the files are relative to it
    void setUnassignedFiles(const QList<core::UnassignedFile>& files,
                            const QString& projectFolder);

    /// @brief ArtProvider id of the icon of @p element: of its kind or, when @p registry does
    /// not have its kind, of the kind on @p card, the book's card of its package, else of its
    /// form; that of a file for a file of the book outside its text
    static QString iconIdOf(const core::BookTypeRegistry& registry,
                            const core::ProjectElement& element,
                            const core::PackageCard* card = nullptr);

    /// @brief The icon of @p element of the open book: of its kind, as the program has it or,
    /// for a kind the program does not have, as the book's card of its package shows it
    /// (core::ProjectManager::packageCard()); else of its form
    static QIcon iconOf(const core::BookTypeRegistry& registry,
                        const core::ProjectElement& element, core::IconContext context);

    /// @brief Show the book as one open read-only (core::ProjectManager::isReadOnly()), or as
    /// one that can be changed
    ///
    /// Read-only, the icons of the book are grey, nothing is dragged or dropped, and the
    /// commands of the context menu that would change the book are greyed out, their tooltips
    /// and the status bar saying why. The files open outside the book stay as they are.
    void setReadOnly(bool readOnly);

    /// @brief Whether it shows the book as one open read-only (setReadOnly())
    bool isReadOnly() const { return m_readOnly; }

    /// @brief ArtProvider id of the icon of the elements of @p kind: its own, or that of its
    /// form; that of a text without a kind
    static QString iconIdOf(const core::KindRef& kind);

    /// @brief The setting of the commands that add on a node: "named" (variant A: "Add..." and
    /// the kinds most added there, "Add Chapter...") or "single" (variant B: only "Add...")
    static constexpr const char* ADD_COMMANDS_SETTING = "navigator.addCommands";

    /// @brief The name of the node of @p book: the writer's own, or "Book"
    static QString bookName(const core::ProjectBook& book);

    /// @brief The name of the node of the Workshop of @p project: the writer's own, or
    /// "Workshop"
    static QString workshopName(const core::BookProject& project);

    /// @brief Whether @p item is an element of the Workshop
    static bool isWorkshopItem(const QTreeWidgetItem* item);

    /// @brief Where Add Element adds an element when it is opened on @p item, with the kinds
    /// it offers there; none for the project, the files and the nodes of a book that is not
    /// open
    std::optional<AddTarget> addTargetOf(const QTreeWidgetItem* item) const;

    /// @brief The tree of the project
    QTreeWidget* tree() const { return m_treeWidget; }

    /// @brief The "+" button of the bar: Add... on the current node
    QToolButton* addButton() const { return m_addButton; }

    /// @brief Where an element is dropped, as the line or the frame of the tree shows it
    enum class DropSpot {
        Above,  ///< Above an item
        Below,  ///< Below an item
        On,     ///< On an item that takes elements: a section of the book or a group
    };

    /// @brief The place of the element of item @p dragged when it is dropped at @p spot of item
    /// @p target, in @p project without the element (core::ProjectManager::moveElementTo())
    ///
    /// Above or below an element, the element goes to the line between the items, also in
    /// another group or section; below a group that shows its elements, first in the group,
    /// else after it. On a section or a group, it goes where an element of its kind is added
    /// there: a chapter before the epilogue. In a book without sections, the line between the
    /// last element of a section and the first one of the next is in both of them: the
    /// element goes to the first one that can have it. The element goes only where its kind can
    /// be (core::BookProject::movePlacesOf()).
    /// @param registry Kinds of the elements
    /// @return The place; none when the element cannot go there or stays where it is
    static std::optional<core::ElementPlace> dropPlaceOf(const core::BookProject& project,
                                                         const core::BookTypeRegistry& registry,
                                                         const QTreeWidgetItem* dragged,
                                                         const QTreeWidgetItem* target,
                                                         DropSpot spot);

    /// @brief Clear tree (when no document is loaded)
    void clearDocument();

    /// @brief Add a standalone file to the "Other Files" section
    /// @param path Absolute file path
    /// @note Creates "Other Files" section if not exists
    /// @note The name has "*" before it while the file has unsaved changes (setFileModified())
    void addStandaloneFile(const QString& path);

    /// @brief Add a new file that has no file yet to the "Other Files" section, named as its
    /// tab ("Untitled 1"), so that the section lists every file open outside the book
    /// @param key Key of the file, which no path has ("untitled:1"): the id of its item, which
    /// standaloneFileSelected(), setFileModified() and removeStandaloneFile() take
    /// @param name Name of the file: the title of its tab
    void addNewFile(const QString& key, const QString& name);

    /// @brief Remove a standalone file from the "Other Files" section
    /// @param path Absolute file path, or the key of a new file (addNewFile())
    /// @note Hides "Other Files" section if empty after removal
    void removeStandaloneFile(const QString& path);

    /// @brief Clear all standalone files from the "Other Files" section
    void clearStandaloneFiles();

    /// @brief Check if there are any standalone files
    /// @return True if there are standalone files
    bool hasStandaloneFiles() const;

    /// @brief Save expansion state for a project (OpenSpec #00034 Phase F)
    /// @param projectId Unique identifier for the project (e.g., manifest path hash)
    /// @note Stores expanded item IDs in SettingsManager under navigator.expansion.<projectId>
    /// @note For sections without IDs, uses format "type:<elementType>:<text>"
    void saveExpansionState(const QString& projectId);

    /// @brief Restore expansion state for a project (OpenSpec #00034 Phase F)
    /// @param projectId Unique identifier for the project
    /// @note Call after loadProject() to restore tree expansion state
    void restoreExpansionState(const QString& projectId);

    /// @brief Get IDs of all expanded items
    /// @return Item IDs, "type:<elementType>:<text>" for sections without IDs
    QStringList expandedItemIds() const;

    /// @brief Expand exactly the given items and collapse all others
    /// @param ids Item IDs as returned by expandedItemIds()
    void setExpandedItemIds(const QStringList& ids);

    /// @brief Show an element: expand the items above it, make it the current item and
    /// scroll to it
    /// @param elementId Element ID; nothing happens when the tree has no such element
    void revealElement(const QString& elementId);

    /// @brief Refresh a single item's display text by element ID
    /// @param elementId Element ID of the item to refresh
    /// @note Updates the display title (including status suffix) from ProjectManager
    /// @note Does nothing if element not found in tree
    void refreshItem(const QString& elementId);

    /// @brief Set modified indicator for a chapter element (OpenSpec #00042 Phase 7.5)
    /// @param elementId Element ID of the chapter
    /// @param isModified True to show modified indicator, false to hide
    /// @note Shows asterisk (*) prefix when modified
    void setElementModified(const QString& elementId, bool isModified);

    /// @brief Clear all modified indicators (OpenSpec #00042 Phase 7.5)
    /// @note Call after project save or close
    /// @note The files outside the book keep theirs: they are not saved or closed with the book
    void clearAllModifiedIndicators();

    /// @brief Show whether file @p path outside the book has unsaved changes: "*" before its
    /// name. The mark stays when the tree is built again (a book opened or closed), and goes
    /// when the file leaves the list.
    void setFileModified(const QString& path, bool isModified);

    /// @brief Destructor
    ~NavigatorPanel() override = default;

signals:
    /// @brief Emitted when user double-clicks a selectable element in tree
    /// @param elementId Unique ID of the element
    /// @param elementTitle Title of the element, without the status and modified marks
    /// @note Only emitted for text elements (chapters, front and back matter items)
    /// @note Sections, groups (parts) and window elements do not emit this signal
    void elementSelected(const QString& elementId, const QString& elementTitle);

    /// @brief Request to rename an element
    /// @param elementId Element ID
    /// @param currentTitle Current title for edit dialog
    void requestRename(const QString& elementId, const QString& currentTitle);

    /// @brief Request to delete an element
    /// @param elementId Element ID
    void requestDelete(const QString& elementId);

    /// @brief Request to add an element in the Add Element window, with the kinds of
    /// @p target and the kind it names chosen at the start
    void requestAdd(const kalahari::gui::AddTarget& target);

    /// @brief Request to name the node of the book or of the Workshop
    /// @param itemType "book" or "workshop"
    void requestRenameNode(const QString& itemType);

    /// @brief Request to group the Workshop (Libraries and Resources) or not
    void requestWorkshopGrouped(bool grouped);

    /// @brief Request to open material @p elementId of the Workshop in the program the system
    /// gives its file
    void requestOpenMaterial(const QString& elementId);

    /// @brief Request to show the sections of the book, or to hide them
    /// @param shown Whether the Navigator shows the sections
    void requestShowSections(bool shown);

    /// @brief Request to rename a section of the book
    /// @param sectionType "section_frontmatter", "section_body" or "section_backmatter"
    void requestRenameSection(const QString& sectionType);

    /// @brief Request to move an element up or down
    /// @param elementId Element ID
    /// @param direction -1 for up, +1 for down
    void requestMoveElement(const QString& elementId, int direction);

    /// @brief Emitted when an element is moved to the start or the end of its list (Move to
    /// Start, Move to End)
    /// @param elementId Element ID
    /// @param index Its new index in its list (a part of the book or a group)
    void elementMoved(const QString& elementId, int index);

    /// @brief Emitted when an element is dragged to another place of the book
    /// @param elementId Element ID
    /// @param place Its new place, in the book without it (dropPlaceOf())
    /// @note The tree does not change; it shows the new order once it is loaded again
    void elementMovedTo(const QString& elementId, const kalahari::core::ElementPlace& place);

    /// @brief Request to choose a new place of an element in the book (Move...)
    /// @param elementId Element ID
    void requestMoveTo(const QString& elementId);

    /// @brief Request to show properties dialog
    /// @param elementId Element ID (empty for document properties)
    void requestProperties(const QString& elementId);

    /// @brief Request to show the versions of the text of an element (Version History...)
    /// @param elementId Element ID
    void requestVersionHistory(const QString& elementId);

    /// @brief Request to show section properties (aggregate statistics)
    /// @param sectionType Section type ("section_frontmatter", "section_body", "section_backmatter")
    void requestSectionProperties(const QString& sectionType);

    /// @brief Request to show part properties (aggregate statistics)
    /// @param partId Group (part) ID
    void requestPartProperties(const QString& partId);

    /// @brief Request to bring the Properties panel to the front
    /// @note Only the Properties command of the context menu asks for it. Choosing an
    ///       element only fills the panel, so the tab on top of its group stays there.
    void requestPropertiesPanel();

    /// @brief Project Properties... on the project's node: the window of the book's type and
    /// kinds
    void requestProjectProperties();

    /// @brief Emitted when a file of the "Other Files" section is opened
    /// @param filePath Absolute file path
    void standaloneFileSelected(const QString& filePath);

    /// @brief Request to add a standalone file to the project
    /// @param filePath Absolute file path
    void requestAddToProject(const QString& filePath);

    /// @brief Request to remove a standalone file from the list
    /// @param filePath Absolute file path
    void requestRemoveStandaloneFile(const QString& filePath);

    /// @brief Request to show the text of a file of "Unassigned Files"
    /// @param file The file, relative to the project folder
    void requestPreviewUnassigned(const QString& file);

    /// @brief Request to add a file of "Unassigned Files" to the book
    /// @param file The file, relative to the project folder
    void requestAddUnassigned(const QString& file);

    /// @brief Request to delete a file of "Unassigned Files"
    /// @param file The file, relative to the project folder
    void requestDeleteUnassigned(const QString& file);

    /// @brief Request to delete all the files of "Unassigned Files"
    void requestDeleteAllUnassigned();

private slots:
    /// @brief Refresh icons when theme/colors change
    void refreshIcons();

    /// @brief Update highlight color when theme changes (OpenSpec #00034 Phase C)
    void updateHighlightColor();

    /// @brief Filter tree based on search text
    /// @param text Filter text (case-insensitive match)
    void filterTree(const QString& text);

    /// @brief Clear filter and show all items
    void clearFilter();

    /// @brief Handle type filter change
    /// @param index New combo box index
    void onTypeFilterChanged(int index);

    /// @brief Show context menu at position
    /// @param pos Position in widget coordinates
    void showContextMenu(const QPoint& pos);

    // Context menu action handlers
    void onContextMenuOpen();
    void onContextMenuRename();
    void onContextMenuDelete();
    void onContextMenuMoveUp();
    void onContextMenuMoveDown();
    void onContextMenuExpandAll();
    void onContextMenuCollapseAll();
    void onContextMenuProperties();
    void onContextMenuAddToProject();
    void onContextMenuRemoveFromList();
    void onContextMenuSetStatus(QAction* action);

    /// @brief Handle keyboard navigation - update Properties panel
    /// @param current Current item (newly selected via arrow keys)
    /// @param previous Previous item (before navigation)
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

    /// @brief Handle Enter key or double-click on item - open element
    /// @param item Activated item
    /// @param column Column index
    void onItemActivated(QTreeWidgetItem* item, int column);

protected:
    /// @brief The keys of the tree: Insert and Ctrl+Shift+Enter add, F2 renames, Delete
    /// deletes, Alt+Shift+H shows the versions of a text
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Add... on @p item: the Add Element window with its kinds, the kind @p kind
    /// chosen at the start; nothing when nothing can be added there
    void addOn(const QTreeWidgetItem* item, const core::KindRef& kind = {});

    /// @brief Rename @p item: an element, a section, the book or the Workshop; nothing for the
    /// other items
    void renameItem(const QTreeWidgetItem* item);

    /// @brief Delete @p item: an element, a file of the book or one that no element has;
    /// nothing for the other items
    void deleteItem(const QTreeWidgetItem* item);

    /// @brief The element of @p item when its text has versions (a text of the book, or a
    /// file of the book with a text); empty for the other items
    QString versionsElementOf(const QTreeWidgetItem* item) const;

    /// @brief Why the command of @p item cannot change the book; empty when it can
    QString changeRefusal() const;

    /// @brief The "+" button can add on the current node
    void updateAddButton();

    /// @brief The kinds the menu of @p item names in its own commands ("Add Chapter..."),
    /// those most added there; none in variant "single" of "navigator.addCommands"
    QList<core::KindRef> namedKindsOf(const QTreeWidgetItem* item) const;

    /// @brief Add the groups of the kinds of the Workshop of @p project with their elements
    /// under @p workshopItem
    void addWorkshopItems(QTreeWidgetItem* workshopItem, const core::BookProject& project,
                          const core::BookTypeRegistry& registry);

    /// @brief Recursively refresh icons on all tree items
    /// @param item Starting item (nullptr for root)
    void refreshItemIcons(QTreeWidgetItem* item);

    /// @brief Get icon ID for item type
    /// @param elementType Type string stored in Qt::UserRole + 1
    /// @return Icon ID for ArtProvider (e.g., "common.folder", "template.chapter")
    QString getIconIdForType(const QString& elementType) const;

    /// @brief The icon @p iconId of @p item: from the book's card of the package of its kind
    /// when the program does not have the kind; grey for the files that are not in the book,
    /// and for the items of a book open read-only (setReadOnly())
    QIcon itemIcon(const QTreeWidgetItem* item, const QString& iconId) const;

    /// @brief Add the items of @p elements, and of the elements inside them, under @p parent
    /// @param workshop The elements are of the Workshop: no status, not dragged
    void addElementItems(QTreeWidgetItem* parent, const QList<core::ProjectElement>& elements,
                         const core::BookTypeRegistry& registry, bool workshop = false);

    /// @brief Add the group "Other Files" with the files of the book outside its text, @p files,
    /// under @p parent; nothing when the book has none
    void addBookFileItems(QTreeWidgetItem* parent, const QList<core::ProjectElement>& files);

    /// @brief Title of an element as the tree shows it: "*" when it has unsaved changes, and
    /// for a text element its status when it is not final ("Chapter 1 [Draft]")
    static QString getDisplayTitle(const core::ProjectElement& element, bool isText,
                                   bool isModified);

    /// @brief Add a section of the book (front, body, back) under @p parent
    QTreeWidgetItem* addSectionItem(QTreeWidgetItem* parent, const QString& title,
                                    const QString& sectionType);

    /// @brief Get icon ID for file based on extension
    /// @param path File path
    /// @return Icon ID for ArtProvider
    QString getIconIdForFile(const QString& path) const;

    /// @brief Ensure "Files Outside the Book" exists, after the project
    void ensureOtherFilesSection();

    /// @brief Give the items of "Unassigned Files" their grey look in the colors of the
    /// current theme
    void updateUnassignedLook();

    /// @brief Show @p path (absolute) in the file manager of the system, or say that it could
    /// not be shown
    void showInFileManager(const QString& path);

    /// @brief Process filter for a single item and its children
    /// @param item Item to process
    /// @param filterText Filter text (lowercase)
    /// @return True if item or any children match the filter
    bool processFilterItem(QTreeWidgetItem* item, const QString& filterText);

    /// @brief Check if item matches the current type filter
    /// @param item Item to check
    /// @return True if item matches type filter or type filter is "All"
    bool matchesTypeFilter(QTreeWidgetItem* item) const;

    /// @brief Set item and all children visible/hidden recursively
    /// @param item Item to modify
    /// @param visible Visibility state
    void setItemVisibleRecursive(QTreeWidgetItem* item, bool visible);

    /// @brief Find tree item by element ID (OpenSpec #00034 Phase C)
    /// @param elementId Element ID to find
    /// @return Tree item or nullptr if not found
    QTreeWidgetItem* findItemByElementId(const QString& elementId) const;

    /// @brief Recursive helper for findItemByElementId (OpenSpec #00034 Phase C)
    /// @param parent Parent item to search
    /// @param elementId Element ID to find
    /// @return Tree item or nullptr if not found
    QTreeWidgetItem* findItemByElementIdRecursive(QTreeWidgetItem* parent,
                                                   const QString& elementId) const;

    /// @brief Collect expanded item IDs recursively (OpenSpec #00034 Phase F)
    /// @param item Starting item
    /// @param expandedIds Output list of IDs for expanded items
    /// @note For sections without IDs, stores "type:<elementType>:<text>"
    void collectExpandedIds(QTreeWidgetItem* item, QStringList& expandedIds) const;

    /// @brief Expand items by their IDs (OpenSpec #00034 Phase F)
    /// @param ids List of item IDs to expand
    /// @note Handles both regular IDs and "type:<elementType>:<text>" format
    void expandItemsById(const QStringList& ids);

    /// @brief Find item by type-text identifier (OpenSpec #00034 Phase F)
    /// @param elementType Element type stored in Qt::UserRole + 1
    /// @param text Item text; not compared for the items the tree has one of: the document,
    ///        its sections and "Other Files", whose names change
    /// @return Tree item or nullptr if not found
    QTreeWidgetItem* findItemByTypeAndText(const QString& elementType, const QString& text) const;

    /// @brief Recursive helper for findItemByTypeAndText (OpenSpec #00034 Phase F)
    QTreeWidgetItem* findItemByTypeAndTextRecursive(QTreeWidgetItem* parent,
                                                     const QString& elementType,
                                                     const QString& text) const;

    /// @brief Document type filter options
    enum class FilterType {
        All,          ///< Show all items
        TextFiles,    ///< Show text elements: chapters, front and back matter items
        MindMaps,     ///< Show mind maps (elements and .kmap files)
        Timelines,    ///< Show timelines (elements and .ktl files)
        OtherFiles    ///< Show items in "Other Files" section
    };

    QTreeWidget* m_treeWidget;
    QTreeWidgetItem* m_otherFilesItem;  ///< "Other Files" section (always at bottom)
    QMap<QString, QTreeWidgetItem*> m_standaloneFiles;  ///< path -> tree item
    /// Key -> name of the new files that have no file yet (addNewFile())
    QHash<QString, QString> m_newFileNames;

    // Unassigned files
    QTreeWidgetItem* m_unassignedItem = nullptr;  ///< "Unassigned Files"; nullptr: no files
    int m_unassignedCount = 0;                    ///< Files it listed last
    bool m_unassignedExpanded = true;             ///< Whether it was open before a reload
    QString m_projectFolder;                      ///< The files are relative to it

    // Search/filter components
    QComboBox* m_typeFilter;             ///< Type filter combo box
    FilterType m_currentFilterType;      ///< Current type filter
    QLineEdit* m_searchEdit;             ///< Filter input field
    QToolButton* m_clearButton;          ///< Clear filter button
    QToolButton* m_addButton = nullptr;  ///< Add... on the current node
    QToolButton* m_expandAllButton;      ///< Expand all tree items button
    QToolButton* m_collapseAllButton;    ///< Collapse all tree items button
    QTimer* m_filterDebounceTimer;       ///< Debounce timer for filter (300ms)

    // Context menu
    QTreeWidgetItem* m_contextMenuItem;  ///< Item for current context menu (temporary)

    // Editor synchronization (OpenSpec #00034 Phase C)
    QTreeWidgetItem* m_highlightedItem;  ///< Currently highlighted item (nullptr if none)
    QColor m_highlightColor;             ///< Theme-aware highlight color (with alpha)

    // Icon size tracking for dynamic updates
    int m_currentIconSize;               ///< Current icon size (to detect changes)

    bool m_readOnly = false;             ///< The book is open read-only (setReadOnly())

    // Modified state tracking (OpenSpec #00042 Phase 7.5)
    QSet<QString> m_modifiedElements;    ///< Set of element IDs with unsaved changes
    QSet<QString> m_modifiedFiles;       ///< Paths of the files outside the book with changes
};

} // namespace gui
} // namespace kalahari
