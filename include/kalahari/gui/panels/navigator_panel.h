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
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QToolButton;
class QTimer;
class QComboBox;

namespace kalahari {
namespace core {
    class Document;  // Forward declaration
}

namespace gui {

/// @brief Navigator panel showing project structure tree
///
/// Displays a QTreeWidget for project structure (chapters/scenes).
/// Supports icons, element selection, and automatic theme refresh.
/// OpenSpec #00034 Phase C: Added editor synchronization (highlight current chapter).
class NavigatorPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit NavigatorPanel(QWidget* parent = nullptr);

    /// @brief Highlight element in tree by ID (OpenSpec #00034 Phase C)
    /// @param elementId Element ID to highlight (from BookElement::getId())
    /// @note Scrolls to the item and expands parent nodes
    /// @note Uses theme-aware highlight color (QPalette::Highlight with alpha)
    void highlightElement(const QString& elementId);

    /// @brief Clear current highlight (OpenSpec #00034 Phase C)
    void clearHighlight();

    /// @brief Load document structure into tree
    /// @param document Document to display
    void loadDocument(const core::Document& document);

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
    /// @note Call after loadDocument() to restore tree expansion state
    void restoreExpansionState(const QString& projectId);

    /// @brief Destructor
    ~NavigatorPanel() override = default;

signals:
    /// @brief Emitted when user double-clicks a selectable element in tree
    /// @param elementId Unique ID of the element (from BookElement::getId())
    /// @param elementTitle Display title of the element
    /// @note Only emitted for leaf elements (chapters, frontmatter items, backmatter items)
    /// @note Section headers (Front Matter, Body, Back Matter) and Parts do not emit this signal
    void elementSelected(const QString& elementId, const QString& elementTitle);

    /// @brief Request to rename an element
    /// @param elementId Element ID
    /// @param currentTitle Current title for edit dialog
    void requestRename(const QString& elementId, const QString& currentTitle);

    /// @brief Request to delete an element
    /// @param elementId Element ID
    /// @param elementType Type of element (for confirmation message)
    void requestDelete(const QString& elementId, const QString& elementType);

    /// @brief Request to add a chapter to a part
    /// @param partId Part ID to add chapter to
    void requestAddChapter(const QString& partId);

    /// @brief Request to add a new part to the body
    void requestAddPart();

    /// @brief Request to add an item to front/back matter
    /// @param sectionType "front_matter" or "back_matter"
    void requestAddItem(const QString& sectionType);

    /// @brief Request to move an element up or down
    /// @param elementId Element ID
    /// @param direction -1 for up, +1 for down
    void requestMoveElement(const QString& elementId, int direction);

    /// @brief Emitted when chapter is reordered via drag & drop (OpenSpec #00034 Phase D)
    /// @param partId Part ID containing the chapter
    /// @param fromIndex Original index of the chapter
    /// @param toIndex New index of the chapter
    void chapterReordered(const QString& partId, int fromIndex, int toIndex);

    /// @brief Emitted when part is reordered via drag & drop (OpenSpec #00034 Phase D)
    /// @param fromIndex Original index of the part
    /// @param toIndex New index of the part
    void partReordered(int fromIndex, int toIndex);

    /// @brief Request to show properties dialog
    /// @param elementId Element ID (empty for document properties)
    void requestProperties(const QString& elementId);

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

private:
    /// @brief Recursively refresh icons on all tree items
    /// @param item Starting item (nullptr for root)
    void refreshItemIcons(QTreeWidgetItem* item);

    /// @brief Get icon ID for element type
    /// @param elementType Type string stored in Qt::UserRole + 1
    /// @return Icon ID for ArtProvider (e.g., "common.folder", "template.chapter")
    QString getIconIdForType(const QString& elementType) const;

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

    /// @brief Handle drop event for drag & drop reordering (OpenSpec #00034 Phase D)
    /// @param item The item being dropped
    /// @param dropTarget The target item/position
    /// @param dropIndicator Position indicator (above/below/on)
    void handleDropEvent(QTreeWidgetItem* item, QTreeWidgetItem* dropTarget, int dropIndicator);

    /// @brief Check if drag operation is valid (OpenSpec #00034 Phase D)
    /// @param sourceItem Item being dragged
    /// @param targetItem Target drop location
    /// @return true if drop is allowed
    bool isDragDropValid(QTreeWidgetItem* sourceItem, QTreeWidgetItem* targetItem) const;

    /// @brief Get part ID for a chapter item (OpenSpec #00034 Phase D)
    /// @param chapterItem Tree item of type "chapter"
    /// @return Part ID or empty string if not found
    QString getPartIdForChapter(QTreeWidgetItem* chapterItem) const;

    /// @brief Get index of item within its parent (OpenSpec #00034 Phase D)
    /// @param item Tree item
    /// @return Index within parent, or -1 if no parent
    int getItemIndex(QTreeWidgetItem* item) const;

    /// @brief Document type filter options
    enum class FilterType {
        All,          ///< Show all items
        TextFiles,    ///< Show chapters, frontmatter, backmatter items
        MindMaps,     ///< Show mind map files (.kmap)
        Timelines,    ///< Show timeline files (.ktl)
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
};

} // namespace gui
} // namespace kalahari
