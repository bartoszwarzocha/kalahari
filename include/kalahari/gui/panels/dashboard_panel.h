/// @file dashboard_panel.h
/// @brief Dashboard panel - welcome screen with native Qt widgets
///
/// Task #00015 - Central Tabbed Workspace
/// OpenSpec #00036 - Enhanced Dashboard with recent books
/// Redesign: Native Qt widgets (QGridLayout, QLabel, QFrame)

#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <vector>

class QFrame;

namespace kalahari {
namespace gui {

/// @brief Dashboard panel - welcome screen with native Qt widgets
///
/// Displays welcome message, keyboard shortcuts, and recent books
/// using native Qt widgets for proper theming and scaling.
///
/// Layout (75% width, centered):
/// - Header: "Welcome to Kalahari" + tagline
/// - Shortcuts: 3 shortcuts in horizontal row
/// - Main content: Two 50/50 columns (News | Recent Files)
/// - Checkbox: Auto-load last project
///
/// Features:
/// - Welcome header with app description
/// - Keyboard shortcuts section
/// - Recent books as clickable cards
/// - Auto-refresh when recent files change
/// - Theme-aware colors via ThemeManager
/// - Responsive layout with scroll area
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

public slots:
    /// @brief Handle settings changes (refreshes dashboard content)
    /// Called by MainWindow after settings are applied
    void onSettingsChanged();

protected:
    /// @brief Handle resize events for responsive layout
    /// @param event Resize event
    void resizeEvent(QResizeEvent* event) override;

private slots:
    /// @brief Refresh the content (called when recent files change)
    void refreshContent();

    /// @brief Handle theme changes
    void onThemeChanged();

    /// @brief Handle click on a recent file card
    void onRecentFileClicked();

private:
    /// @brief Setup UI components
    void setupUI();

    /// @brief Apply theme colors to all widgets
    void applyThemeColors();

    /// @brief Create the header section
    /// @param parent Parent widget
    /// @return Header widget
    QWidget* createHeaderSection(QWidget* parent);

    /// @brief Create the shortcuts section
    /// @param parent Parent widget
    /// @return Shortcuts widget
    QWidget* createShortcutsSection(QWidget* parent);

    /// @brief Create the main content section (News + Recent Files)
    /// @param parent Parent widget
    /// @return Main content widget
    QWidget* createMainContentSection(QWidget* parent);

    /// @brief Create a single recent file card
    /// @param filePath Path to the .klh file
    /// @param parent Parent widget
    /// @return Card widget
    QWidget* createRecentFileCard(const QString& filePath, QWidget* parent);

    /// @brief Update the recent files list
    void updateRecentFilesList();

    /// @brief Populate news column
    void populateNewsColumn();

    /// @brief Reorganize layout for single/dual column mode
    /// @param singleColumn True for single column (narrow), false for dual column (wide)
    void reorganizeLayout(bool singleColumn);

    /// @brief Make path string breakable by inserting zero-width spaces after separators
    /// @param path Original path string
    /// @return Path with zero-width spaces for wrapping
    QString makeBreakablePath(const QString& path) const;

    /// @brief Load icon with theme colors at consistent DASHBOARD_ICON_SIZE
    /// @param actionId Action ID registered in ArtProvider (e.g., "file.open")
    /// @return QPixmap at DASHBOARD_ICON_SIZE, or null if not found
    QPixmap loadThemedIcon(const QString& actionId) const;

    // Main layout components
    QScrollArea* m_scrollArea;         ///< Scroll area for content
    QWidget* m_contentWidget;          ///< Main content container
    QVBoxLayout* m_mainLayout;         ///< Main vertical layout

    // Header components
    QLabel* m_titleLabel;              ///< "Welcome to Kalahari"
    QLabel* m_taglineLabel;            ///< Tagline text

    // Shortcuts section
    QFrame* m_shortcutsFrame;          ///< Shortcuts container frame
    QLabel* m_shortcutsTitleLabel;     ///< "KEYBOARD SHORTCUTS"
    std::vector<QLabel*> m_shortcutLabels;  ///< Individual shortcut labels

    // Main content columns
    QWidget* m_columnsWidget;          ///< Container for columns
    QFrame* m_newsColumn;              ///< News column
    QFrame* m_recentFilesColumn;       ///< Recent files column
    QFrame* m_columnDivider;           ///< Divider between columns
    QLabel* m_newsIcon;                ///< News column icon
    QLabel* m_newsTitle;               ///< News column title
    QLabel* m_filesIcon;               ///< Files column icon
    QLabel* m_filesTitle;              ///< Files column title
    QVBoxLayout* m_newsListLayout;     ///< Layout for news items
    QVBoxLayout* m_filesListLayout;    ///< Layout for file cards
    QWidget* m_newsListWidget;         ///< Container for news items
    QWidget* m_filesListWidget;        ///< Container for file cards

    // Footer
    QCheckBox* m_autoLoadCheckbox;     ///< Auto-load last project checkbox

    // Cached recent file cards for click handling
    std::vector<std::pair<QWidget*, QString>> m_fileCards;  ///< Card widget -> file path

    // Responsive layout state
    bool m_singleColumnMode;               ///< True if in single column layout
    QGridLayout* m_columnsGridLayout;      ///< Grid layout for columns (stored for reorganization)

    static constexpr int MAX_RECENT_BOOKS = 5;    ///< Maximum recent books to display
    static constexpr int SINGLE_COLUMN_THRESHOLD = 750;  ///< Width threshold for single column mode
};

} // namespace gui
} // namespace kalahari
