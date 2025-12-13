/// @file navigator_panel.h
/// @brief Navigator panel for project structure navigation (Qt6)
///
/// This file defines the NavigatorPanel class, displaying the project
/// structure tree with icons and element selection support.
///
/// OpenSpec #00033 Phase D: Enhanced with icons, element IDs, and theme refresh.
/// OpenSpec #00033 Phase F: Added "Other Files" section for standalone files.

#pragma once

#include <QWidget>
#include <QMap>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace core {
    class Document;  // Forward declaration
}

namespace gui {

/// @brief Navigator panel showing project structure tree
///
/// Displays a QTreeWidget for project structure (chapters/scenes).
/// Supports icons, element selection, and automatic theme refresh.
class NavigatorPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit NavigatorPanel(QWidget* parent = nullptr);

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

    /// @brief Destructor
    ~NavigatorPanel() override = default;

signals:
    /// @brief Emitted when user double-clicks a selectable element in tree
    /// @param elementId Unique ID of the element (from BookElement::getId())
    /// @param elementTitle Display title of the element
    /// @note Only emitted for leaf elements (chapters, frontmatter items, backmatter items)
    /// @note Section headers (Front Matter, Body, Back Matter) and Parts do not emit this signal
    void elementSelected(const QString& elementId, const QString& elementTitle);

private slots:
    /// @brief Refresh icons when theme/colors change
    void refreshIcons();

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

    QTreeWidget* m_treeWidget;
    QTreeWidgetItem* m_otherFilesItem;  ///< "Other Files" section (always at bottom)
    QMap<QString, QTreeWidgetItem*> m_standaloneFiles;  ///< path -> tree item
};

} // namespace gui
} // namespace kalahari
