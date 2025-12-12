/// @file navigator_panel.h
/// @brief Navigator panel for project structure navigation (Qt6)
///
/// This file defines the NavigatorPanel class, displaying the project
/// structure tree with icons and element selection support.
///
/// OpenSpec #00033 Phase D: Enhanced with icons, element IDs, and theme refresh.

#pragma once

#include <QWidget>

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

    QTreeWidget* m_treeWidget;
};

} // namespace gui
} // namespace kalahari
