/// @file navigator_panel.h
/// @brief Navigator panel placeholder (Qt6)
///
/// This file defines the NavigatorPanel class, a placeholder for the
/// project structure tree. Will be enhanced in Phase 1 with real tree.

#pragma once

#include <QWidget>

class QTreeWidget;

namespace kalahari {
namespace gui {

/// @brief Navigator panel (placeholder)
///
/// Displays a QTreeWidget for project structure (chapters/scenes).
/// This is a placeholder - full implementation comes in Phase 1.
class NavigatorPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit NavigatorPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~NavigatorPanel() override = default;

private:
    QTreeWidget* m_treeWidget;
};

} // namespace gui
} // namespace kalahari
