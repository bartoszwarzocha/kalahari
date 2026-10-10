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

#include <QWidget>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

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
    struct ProjectElement;
    class BookTypeRegistry;
}

namespace gui {

/// @brief Navigator panel showing project structure tree
///
/// Displays a QTreeWidget for the book of the project: its front, main and back parts with
/// their elements, and the elements inside groups (parts). Supports icons, element selection,
/// and automatic theme refresh.
/// OpenSpec #00034 Phase C: Added editor synchronization (highlight current chapter).
///
/// Item data: Qt::UserRole holds the element ID (the path of a standalone file), Qt::UserRole + 1
/// the item type: "document", "section_frontmatter", "section_body", "section_backmatter",
/// "text_element", "group_element", "window_element", "other_files" or "standalone_file".
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
    void loadProject(const core::BookProject& project, const core::BookTypeRegistry& registry);

    /// @brief ArtProvider id of the icon of @p element: of its kind or, when @p registry does
    /// not have its kind, of its form
    static QString iconIdOf(const core::BookTypeRegistry& registry,
                            const core::ProjectElement& element);

    /// @brief New index of the element at @p from of a list when it is dropped above the
    /// element at @p target of the list, or below it (@p below)
    /// @return The index; -1 when the element stays where it is
    static int dropIndex(int from, int target, bool below);

    /// @brief Clear tree (when no document is loaded)
    void clearDocument();

    /// @brief Add a standalone file to the "Other Files" section
    /// @param path Absolute file path
    /// @note Creates "Other Files" section if not exists
    void addStandaloneFile(const QString& path);

    /// @brief Remove a standalone file from the "Other Files" section
    /// @param path Absolute file path
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
    void clearAllModifiedIndicators();

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

    /// @brief Request to add a chapter to a group (part) or to the body of the book
    /// @param groupId Group ID to add chapter to; empty: the body of the book
    void requestAddChapter(const QString& groupId);

    /// @brief Request to add a new part to the body
    void requestAddPart();

    /// @brief Request to add an item to front/back matter
    /// @param sectionType "front_matter" or "back_matter"
    void requestAddItem(const QString& sectionType);

    /// @brief Request to move an element up or down
    /// @param elementId Element ID
    /// @param direction -1 for up, +1 for down
    void requestMoveElement(const QString& elementId, int direction);

    /// @brief Emitted when an element is dragged to another place of its list
    /// @param elementId Element ID
    /// @param index Its new index in its list (a part of the book or a group)
    /// @note The tree does not change; it shows the new order once it is loaded again
    void elementMoved(const QString& elementId, int index);

    /// @brief Request to show properties dialog
    /// @param elementId Element ID (empty for document properties)
    void requestProperties(const QString& elementId);

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

    /// @brief Emitted when a file of the "Other Files" section is opened
    /// @param filePath Absolute file path
    void standaloneFileSelected(const QString& filePath);

    /// @brief Request to add a standalone file to the project
    /// @param filePath Absolute file path
    void requestAddToProject(const QString& filePath);

    /// @brief Request to remove a standalone file from the list
    /// @param filePath Absolute file path
    void requestRemoveStandaloneFile(const QString& filePath);

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
    void onContextMenuAddChapter();
    void onContextMenuAddPart();
    void onContextMenuAddItem();
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

private:
    /// @brief Recursively refresh icons on all tree items
    /// @param item Starting item (nullptr for root)
    void refreshItemIcons(QTreeWidgetItem* item);

    /// @brief Get icon ID for item type
    /// @param elementType Type string stored in Qt::UserRole + 1
    /// @return Icon ID for ArtProvider (e.g., "common.folder", "template.chapter")
    QString getIconIdForType(const QString& elementType) const;

    /// @brief Add the items of @p elements, and of the elements inside them, under @p parent
    void addElementItems(QTreeWidgetItem* parent, const QList<core::ProjectElement>& elements,
                         const core::BookTypeRegistry& registry);

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

    /// @brief Ensure "Other Files" section exists and is visible
    void ensureOtherFilesSection();

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
    /// @param text Item text
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

    // Search/filter components
    QComboBox* m_typeFilter;             ///< Type filter combo box
    FilterType m_currentFilterType;      ///< Current type filter
    QLineEdit* m_searchEdit;             ///< Filter input field
    QToolButton* m_clearButton;          ///< Clear filter button
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

    // Modified state tracking (OpenSpec #00042 Phase 7.5)
    QSet<QString> m_modifiedElements;    ///< Set of element IDs with unsaved changes
};

} // namespace gui
} // namespace kalahari
