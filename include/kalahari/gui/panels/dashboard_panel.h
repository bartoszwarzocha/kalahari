/// @file dashboard_panel.h
/// @brief Dashboard panel - welcome screen and quick actions
///
/// @author Claude (AI Assistant)
/// @date 2025-11-21
/// @task #00015 - Central Tabbed Workspace

#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

/// @brief Dashboard panel - welcome screen (Phase 0 placeholder)
///
/// Simple welcome screen displayed at application startup.
/// Shows basic instructions for new users.
///
/// **Phase 0:** Simple centered label with HTML content
/// **Phase 1+:** Add Recent Files, Quick Actions, Tips & Tricks
///
/// Example usage:
/// @code
/// DashboardPanel* dashboard = new DashboardPanel(this);
/// m_centralTabs->addTab(dashboard, tr("Dashboard"));
/// @endcode
class DashboardPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget (typically MainWindow or QTabWidget)
    explicit DashboardPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~DashboardPanel() override = default;

private:
    QLabel* m_welcomeLabel;  ///< Welcome message label

    // Phase 1+ additions:
    // - Recent files list (QListWidget)
    // - Quick action buttons (New Document, Open Document)
    // - Tips & Tricks carousel
    // - Statistics overview (total words, documents count)
};

} // namespace gui
} // namespace kalahari
