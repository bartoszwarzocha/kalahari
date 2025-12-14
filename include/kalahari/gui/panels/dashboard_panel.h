/// @file dashboard_panel.h
/// @brief Dashboard panel - welcome screen with HTML content
///
/// Task #00015 - Central Tabbed Workspace
/// OpenSpec #00036 - Enhanced Dashboard with recent books
/// Redesign: Pure HTML rendering via QTextBrowser

#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QUrl>

class QTextBrowser;

namespace kalahari {
namespace gui {

/// @brief Dashboard panel - welcome screen rendered as HTML
///
/// Displays welcome message, keyboard shortcuts, and recent books
/// as a nicely formatted HTML document in a QTextBrowser.
/// Clicking on recent book links emits openRecentBookRequested signal.
///
/// Features:
/// - Welcome header with app description
/// - Keyboard shortcuts section
/// - Recent books as clickable HTML cards
/// - Auto-refresh when recent files change
/// - Theme-aware colors in HTML CSS
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
    /// @brief Handle click on a link in the HTML content
    /// @param url URL of the clicked link (file:// for recent books)
    void onAnchorClicked(const QUrl& url);

    /// @brief Refresh the HTML content (called when recent files change)
    void refreshContent();

    /// @brief Handle theme changes
    void onThemeChanged();

private:
    /// @brief Setup UI components
    void setupUI();

    /// @brief Generate complete HTML content for the dashboard
    /// @return HTML string with inline CSS
    QString generateHtml();

    /// @brief Generate CSS styles for HTML content
    /// @return CSS string with theme-aware colors
    QString generateCss();

    /// @brief Generate HTML for a single recent book card
    /// @param filePath Path to the .klh file
    /// @return HTML string for the book card
    QString generateBookCardHtml(const QString& filePath);

    QTextBrowser* m_browser;          ///< HTML content browser
    QCheckBox* m_autoLoadCheckbox;    ///< Auto-load last project on startup checkbox

    static constexpr int MAX_RECENT_BOOKS = 5;  ///< Maximum recent books to display
};

} // namespace gui
} // namespace kalahari
