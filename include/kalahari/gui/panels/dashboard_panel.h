// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file dashboard_panel.h
/// @brief Dashboard panel - welcome screen with native Qt widgets
///
/// Task #00015 - Central Tabbed Workspace
/// OpenSpec #00036 - Enhanced Dashboard with recent books
/// Redesign: Native Qt widgets (QGridLayout, QLabel, QFrame)

#ifndef KALAHARI_GUI_PANELS_DASHBOARD_PANEL_H
#define KALAHARI_GUI_PANELS_DASHBOARD_PANEL_H

#include <QWidget>
#include <QCheckBox>
#include <QPointer>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <vector>

class QEvent;
class QFrame;
class QObject;

namespace kalahari::gui {
class CommandRegistry;
}  // namespace kalahari::gui

namespace kalahari::gui {

class DashboardHeaderLayout;
class DashboardHintsLayout;

/// @brief Dashboard panel - welcome screen with native Qt widgets
///
/// Displays welcome message, keyboard shortcuts, and recent books
/// using native Qt widgets for proper theming and scaling.
///
/// Layout (75% width, centered; on a narrow panel all of it but a margin):
/// - Header: the logo beside "Welcome to Kalahari" and the tagline, or above them
/// - Shortcuts: 3 shortcuts in a row, or one under another where the row is too narrow
/// - Main content: Two 50/50 columns (News | Recent Files), or one under the other
/// - Checkbox: Auto-load last project
///
/// Nothing is cut off on a small screen: the texts wrap, the logo gets smaller, and a panel
/// narrower than the longest word scrolls sideways.
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
    /// @param commands The commands of its window
    /// @param parent Parent widget (typically MainWindow or QTabWidget)
    explicit DashboardPanel(CommandRegistry& commands, QWidget* parent = nullptr);

    /// @brief Destructor: the auto-load checkbox no longer follows its setting
    ~DashboardPanel() override;

    /// @brief Open the recent book @p filePath, as a click on its card does: the card darkened
    /// with a spinner on it until the book is open (openRecentBookRequested() returned)
    void openRecentBook(const QString& filePath);

    /// @brief The darkened card with its spinner over the card of the book being opened;
    /// nullptr: none is
    [[nodiscard]] QWidget* loadingOverlay() const { return m_loading; }

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

    /// @brief A click on the text of the auto-load checkbox changes it
    bool eventFilter(QObject* watched, QEvent* event) override;

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

    /// @brief Show the commands to start with and their keys, as the user set them
    void updateShortcutLabels();

    /// @brief Colors of the shortcut labels, of the current theme
    void styleShortcutLabels();

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

    /// @brief Update column visibility based on settings
    /// Reads dashboard.showKalahariNews and dashboard.showRecentFiles from SettingsManager
    /// and shows/hides columns accordingly. Also handles divider visibility.
    void updateColumnVisibility();

    /// @brief Load icon with theme colors at consistent DASHBOARD_ICON_SIZE
    /// @param actionId Action ID registered in ArtProvider (e.g., "file.open")
    /// @return QPixmap at DASHBOARD_ICON_SIZE, or null if not found
    [[nodiscard]] QPixmap loadThemedIcon(const QString& actionId) const;

    /// @brief The icon @p actionId of a book card in the icon colors of the toolbars
    /// (ArtProvider), drawn sharp at any size
    static QIcon bookIcon(const QString& actionId);

    /// @brief The icon of a book card's type at @p size, after the properties of @p label (the
    /// card's "cardBookIcon"): its icon id; where the program does not have the type, its icon
    /// from the program or from the book's card of the type, greyed out
    static QPixmap bookIconPixmap(const QLabel* label, int size);

    // Main layout components
    QScrollArea* m_scrollArea = nullptr;  ///< Scroll area for content
    QWidget* m_contentWidget = nullptr;   ///< Main content container
    QVBoxLayout* m_mainLayout = nullptr;  ///< Main vertical layout

    // Header components
    DashboardHeaderLayout* m_headerLayout = nullptr;  ///< The logo beside the texts, or above them
    QLabel* m_logoLabel = nullptr;     ///< Application logo (at most 256x256)
    QLabel* m_titleLabel = nullptr;    ///< "Welcome to Kalahari"
    QLabel* m_taglineLabel = nullptr;  ///< Tagline text

    // Shortcuts section
    QFrame* m_shortcutsFrame = nullptr;       ///< Shortcuts container frame
    QLabel* m_shortcutsTitleLabel = nullptr;  ///< "KEYBOARD SHORTCUTS"
    DashboardHintsLayout* m_shortcutsLayout = nullptr;  ///< In a row, or one under another
    std::vector<QLabel*> m_shortcutLabels;  ///< The keys and the name of each command

    // Main content columns
    QWidget* m_columnsWidget = nullptr;        ///< Container for columns
    QFrame* m_newsColumn = nullptr;            ///< News column
    QFrame* m_recentFilesColumn = nullptr;     ///< Recent files column
    QWidget* m_columnDivider = nullptr;        ///< Divider between columns (QWidget for reliable
                                               ///< styling)
    QLabel* m_newsIcon = nullptr;              ///< News column icon
    QLabel* m_newsTitle = nullptr;             ///< News column title
    QLabel* m_filesIcon = nullptr;             ///< Files column icon
    QLabel* m_filesTitle = nullptr;            ///< Files column title
    QVBoxLayout* m_newsListLayout = nullptr;   ///< Layout for news items
    QVBoxLayout* m_filesListLayout = nullptr;  ///< Layout for file cards
    QWidget* m_newsListWidget = nullptr;       ///< Container for news items
    QWidget* m_filesListWidget = nullptr;      ///< Container for file cards

    // Footer
    QCheckBox* m_autoLoadCheckbox = nullptr;  ///< Auto-load last project checkbox
    QLabel* m_autoLoadLabel = nullptr;        ///< Its text, which wraps (a QCheckBox cannot)
    int m_autoLoadSubscription = -1;          ///< The checkbox following its setting
                                              ///< (SettingsManager)

    // Cached recent file cards for click handling
    std::vector<std::pair<QWidget*, QString>> m_fileCards;  ///< Card widget -> file path
    QString m_openingPath;           ///< The book being opened from its card; empty: none
    QPointer<QWidget> m_loading;     ///< Over its card: darkened, with a spinner
    bool m_refreshWaits = false;     ///< The list changed while the book opened

    // Responsive layout state
    bool m_singleColumnMode = false;             ///< True if in single column layout
    QGridLayout* m_columnsGridLayout = nullptr;  ///< Grid layout for columns (stored for
                                                 ///< reorganization)

    // Section visibility state (from settings)
    bool m_showNews = true;                ///< Show news column (from settings)
    bool m_showRecentFiles = true;         ///< Show recent files column (from settings)

    /// @brief Get max items to display from settings
    /// @return Number of items (3-9, default 5)
    [[nodiscard]] int getMaxItems() const;

    /// @brief Get icon size from settings
    /// @return Icon size in pixels (24-64, default 48)
    [[nodiscard]] int getIconSize() const;

    static constexpr int SINGLE_COLUMN_THRESHOLD = 750;  ///< Width threshold for single column mode
    CommandRegistry& m_commands;  ///< The commands of its window
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_DASHBOARD_PANEL_H
