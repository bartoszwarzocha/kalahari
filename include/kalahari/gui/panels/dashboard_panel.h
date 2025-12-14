/// @file dashboard_panel.h
/// @brief Dashboard panel - welcome screen and quick actions
///
/// Task #00015 - Central Tabbed Workspace
/// OpenSpec #00036 - Enhanced Dashboard with recent books

#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QList>
#include <QCheckBox>

namespace kalahari {
namespace gui {

class RecentBookCard;

/// @brief Dashboard panel - welcome screen with recent books
///
/// Displays welcome message, recent books cards, and quick start actions.
/// Integrates with RecentBooksManager for dynamic recent files list.
///
/// Features:
/// - Welcome header with app description
/// - Recent books section (max 5 cards)
/// - Quick start section with keyboard shortcuts
/// - Auto-refresh when recent files change
///
/// Example usage:
/// @code
/// DashboardPanel* dashboard = new DashboardPanel(this);
/// m_centralTabs->addTab(dashboard, tr("Dashboard"));
/// connect(dashboard, &DashboardPanel::openRecentBookRequested,
///         this, &MainWindow::onOpenRecentFile);
/// @endcode
class DashboardPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget (typically MainWindow or QTabWidget)
    explicit DashboardPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~DashboardPanel() override = default;

signals:
    /// @brief Emitted when user wants to open a recent book
    /// @param filePath Full path to the .klh file to open
    void openRecentBookRequested(const QString& filePath);

private slots:
    /// @brief Handle click on a recent book card
    /// @param filePath Path to the clicked book
    void onRecentBookClicked(const QString& filePath);

    /// @brief Refresh the recent books section
    void refreshRecentBooks();

private:
    /// @brief Setup UI components
    void setupUI();

    /// @brief Create welcome header section
    /// @return Widget containing welcome header
    QWidget* createWelcomeHeader();

    /// @brief Create recent books section
    /// @return Widget containing recent books group
    QWidget* createRecentBooksSection();

    /// @brief Clear all recent book cards
    void clearRecentBookCards();

    QLabel* m_welcomeLabel;               ///< Welcome message label (HTML-styled)
    QVBoxLayout* m_recentBooksLayout;     ///< Layout for recent book cards
    QWidget* m_recentBooksContainer;      ///< Container for recent books section
    QLabel* m_noRecentBooksLabel;         ///< "No recent books" message
    QList<RecentBookCard*> m_bookCards;   ///< List of recent book card widgets
    QCheckBox* m_autoLoadCheckbox;        ///< Auto-load last project on startup checkbox

    static constexpr int MAX_RECENT_BOOKS = 5;  ///< Maximum recent books to display
};

} // namespace gui
} // namespace kalahari
