/// @file dock_coordinator.h
/// @brief Dock widget and panel coordination for MainWindow
///
/// OpenSpec #00038 - Phase 4: Extract Dock/Panel Management from MainWindow
/// This class manages dock widget creation, layout, title bar customization,
/// and panel instantiation.

#pragma once

#include <QObject>
#include <QList>
#include <QPointer>

class QMainWindow;
class QDockWidget;
class QTabWidget;
class QLabel;
class QToolButton;
class QWidget;
class QAction;
class QRect;

namespace kalahari {
namespace gui {

// Forward declarations for panels
class NavigatorPanel;
class PropertiesPanel;
class LogPanel;
class DashboardPanel;
class SearchPanel;
class AssistantPanel;
class AnnotationsPanel;
class InfoBar;
class StandaloneInfoBar;

/// @brief Coordinates dock widgets and panel management
///
/// Manages:
/// - Dock widget creation and layout
/// - Panel instantiation
/// - Dock title bar customization (icons, float/close buttons)
/// - Icon refresh on theme change
/// - Central tabbed workspace (Dashboard, editor tabs)
///
/// Example usage:
/// @code
/// auto coordinator = new DockCoordinator(this, this);
/// coordinator->createDocks();
/// // Access panels
/// NavigatorPanel* nav = coordinator->navigatorPanel();
/// @endcode
class DockCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param mainWindow Parent QMainWindow
    /// @param parent Parent QObject
    explicit DockCoordinator(QMainWindow* mainWindow, QObject* parent = nullptr);

    /// @brief Destructor
    ~DockCoordinator() override = default;

    // =========================================================================
    // Dock/panel creation
    // =========================================================================

    /// @brief Create all dock widgets and panels
    /// @note Must be called after the commands are registered (the panel toggles use them)
    void createDocks();

    /// @brief Reset dock layout to default
    ///
    /// On a small screen the default is the layout of fitToScreen(), and the bar above the
    /// text says why the panels on the right are gone.
    /// @param diagnosticMode true if diagnostic mode is enabled
    /// @param devMode true if dev mode is enabled
    void resetLayout(bool diagnosticMode, bool devMode);

    /// @brief The first layout of a window without a saved one: on a small screen the layout
    /// of fitToScreen(), and the bar of showLayoutNotice() the first time only, so that later
    /// starts keep the writer's own layout without a word
    /// @param screenArea The part of the window's screen that windows can take (the screen
    ///        without the taskbar)
    void fitFirstLayout(const QRect& screenArea);

    /// @brief On a small screen (narrower than SMALL_SCREEN_WIDTH), a layout that leaves the
    /// text room: a narrower Navigator and no panels on the right
    ///
    /// The panels on the right are hidden, not removed: View > Panels shows each of them in
    /// its place again, as does the button of the bar of showLayoutNotice().
    /// @param screenArea The part of the window's screen that windows can take
    /// @return Whether the screen is small and the layout was fitted to it
    bool fitToScreen(const QRect& screenArea);

    /// @brief A screen whose part for windows is narrower than this, in the pixels Qt counts
    /// after the system's scaling, is small: the Navigator, the text and the panels on the
    /// right do not fit side by side. 1366×768 at 125% is 1093 pixels wide, at 150% 911.
    static constexpr int SMALL_SCREEN_WIDTH = 1200;

    /// @brief Show the bar above the text that says the panels on the right were hidden for a
    /// small screen and how to show them again
    void showLayoutNotice();

    /// @brief Show the panels on the right again, with the Annotations panel on top, and hide
    /// the bar of showLayoutNotice()
    void showRightPanels();

    // =========================================================================
    // Dock customization
    // =========================================================================

    /// @brief Setup custom title bar for dock widget
    /// @param dock The dock widget to customize
    /// @param iconId Icon command ID (e.g., "view.navigator")
    /// @param title Translated title text
    /// @note Creates horizontal layout with icon label + title label + float/close buttons
    void setupDockTitleBar(QDockWidget* dock, const QString& iconId, const QString& title);

    /// @brief Refresh all dock title bar icons
    /// @note Called when theme changes to update icon colors
    void refreshDockIcons();

    /// @brief Re-apply theme colors to central tab icons on resourcesChanged.
    /// Tab icons are plain QIcons (not managed QActions), so they do not
    /// auto-refresh. Each tab page stores its icon id in the "tabIconId"
    /// property; this re-fetches every tab's icon from ArtProvider.
    void refreshTabIcons();

    // =========================================================================
    // Panel accessors
    // =========================================================================

    /// @brief Get Navigator panel
    [[nodiscard]] NavigatorPanel* navigatorPanel() const { return m_navigatorPanel; }

    /// @brief Get Properties panel
    [[nodiscard]] PropertiesPanel* propertiesPanel() const { return m_propertiesPanel; }

    /// @brief Get Log panel
    [[nodiscard]] LogPanel* logPanel() const { return m_logPanel; }

    /// @brief Get Dashboard panel
    /// @note Defined in the .cpp: QPointer needs the complete DashboardPanel type
    [[nodiscard]] DashboardPanel* dashboardPanel() const;

    /// @brief Set Dashboard panel (when recreated from View > Dashboard)
    /// @note Uses QPointer for safe tracking of dynamically-deleted panel
    void setDashboardPanel(DashboardPanel* panel);

    /// @brief Get Search panel
    [[nodiscard]] SearchPanel* searchPanel() const { return m_searchPanel; }

    /// @brief Get Assistant panel
    [[nodiscard]] AssistantPanel* assistantPanel() const { return m_assistantPanel; }

    /// @brief Get Annotations panel
    [[nodiscard]] AnnotationsPanel* annotationsPanel() const { return m_annotationsPanel; }

    /// @brief Get Standalone info bar
    [[nodiscard]] StandaloneInfoBar* standaloneInfoBar() const { return m_standaloneInfoBar; }

    /// @brief The bar of showLayoutNotice()
    [[nodiscard]] InfoBar* layoutNoticeBar() const { return m_layoutNoticeBar; }

    // =========================================================================
    // Dock accessors
    // =========================================================================

    /// @brief Get Navigator dock widget
    [[nodiscard]] QDockWidget* navigatorDock() const { return m_navigatorDock; }

    /// @brief Get Properties dock widget
    [[nodiscard]] QDockWidget* propertiesDock() const { return m_propertiesDock; }

    /// @brief Get Log dock widget
    [[nodiscard]] QDockWidget* logDock() const { return m_logDock; }

    /// @brief Get Search dock widget
    [[nodiscard]] QDockWidget* searchDock() const { return m_searchDock; }

    /// @brief Get Assistant dock widget
    [[nodiscard]] QDockWidget* assistantDock() const { return m_assistantDock; }

    /// @brief Get Annotations dock widget
    [[nodiscard]] QDockWidget* annotationsDock() const { return m_annotationsDock; }

    // =========================================================================
    // Central widget accessors
    // =========================================================================

    /// @brief Get central tab widget
    [[nodiscard]] QTabWidget* centralTabs() const { return m_centralTabs; }

    /// @brief Get central wrapper widget (contains info bar + tabs)
    [[nodiscard]] QWidget* centralWrapper() const { return m_centralWrapper; }

    // =========================================================================
    // View action accessors
    // =========================================================================

    /// @brief Get Navigator toggle action
    [[nodiscard]] QAction* viewNavigatorAction() const { return m_viewNavigatorAction; }

    /// @brief Get Properties toggle action
    [[nodiscard]] QAction* viewPropertiesAction() const { return m_viewPropertiesAction; }

    /// @brief Get Log toggle action
    [[nodiscard]] QAction* viewLogAction() const { return m_viewLogAction; }

    /// @brief Get Search toggle action
    [[nodiscard]] QAction* viewSearchAction() const { return m_viewSearchAction; }

    /// @brief Get Assistant toggle action
    [[nodiscard]] QAction* viewAssistantAction() const { return m_viewAssistantAction; }

    /// @brief Get Annotations toggle action
    [[nodiscard]] QAction* viewAnnotationsAction() const { return m_viewAnnotationsAction; }

signals:
    /// @brief Emitted when tab close is requested
    /// @param index Tab index to close
    void tabCloseRequested(int index);

    /// @brief Emitted when Dashboard tab is closed
    void dashboardClosed();

    /// @brief Emitted when navigator element is selected
    /// @param elementId Unique ID of the selected element
    /// @param elementTitle Display title of the element
    void navigatorElementSelected(const QString& elementId, const QString& elementTitle);

    /// @brief Emitted when recent book is requested to open
    /// @param filePath Path to the book file
    void openRecentBookRequested(const QString& filePath);

    /// @brief Emitted when add to project is requested from info bar
    void addToProjectRequested();

    /// @brief Emitted when info bar is dismissed
    void infoBarDismissed();

    /// @brief Emitted when an element is dragged to another place of its list in the navigator
    /// @param elementId Element ID
    /// @param index Its new index in its list
    void elementMoved(const QString& elementId, int index);

    /// @brief Emitted when rename is requested from navigator
    void navigatorRequestRename(const QString& elementId, const QString& currentTitle);

    /// @brief Emitted when delete is requested from navigator
    void navigatorRequestDelete(const QString& elementId);

    /// @brief Emitted when move is requested from navigator
    void navigatorRequestMove(const QString& elementId, int direction);

    /// @brief Emitted when properties are requested from navigator
    void navigatorRequestProperties(const QString& elementId);

    /// @brief Emitted when section properties are requested from navigator
    void navigatorRequestSectionProperties(const QString& sectionType);

    /// @brief Emitted when part properties are requested from navigator
    void navigatorRequestPartProperties(const QString& partId);

    /// @brief Emitted when current tab changes
    /// @param index New current tab index
    void currentTabChanged(int index);

    /// @brief Emitted when the title or status of a chapter changes in the properties panel
    /// @param elementId Chapter element ID
    void chapterStatusChanged(const QString& elementId);

    /// @brief Emitted when Log panel requests settings dialog
    void openSettingsRequested();

    /// @brief Emitted when add chapter is requested from navigator context menu
    /// @param groupId Group (part) to add the chapter to; empty: the body of the book
    void requestAddChapter(const QString& groupId);

    /// @brief Emitted when add part is requested from navigator context menu
    void requestAddPart();

    /// @brief Emitted when add item is requested from navigator context menu
    /// @param sectionType Section type ("front_matter" or "back_matter")
    void requestAddItem(const QString& sectionType);

private:
    /// @brief Create Navigator dock widget
    void createNavigatorDock();

    /// @brief Create Properties dock widget
    void createPropertiesDock();

    /// @brief Create Log dock widget
    void createLogDock();

    /// @brief Create Search dock widget
    void createSearchDock();

    /// @brief Create Assistant dock widget
    void createAssistantDock();

    /// @brief Create Annotations dock widget
    void createAnnotationsDock();

    /// @brief Create central tabbed workspace
    void createCentralWidget();

    /// @brief Setup VIEW menu panel actions
    void setupViewMenuActions();

    /// @brief Connect panel toggle command to dock widget
    /// @param cmdId Command ID (e.g., "view.navigator")
    /// @param dock Dock widget to control
    void connectPanelCommand(const std::string& cmdId, QDockWidget* dock);

    /// @brief Make a panel command's action a check box that follows its dock
    /// @param cmdId Command ID
    /// @param dock Dock widget
    /// @return The command's action, or nullptr when there is none
    QAction* createPanelAction(const std::string& cmdId, QDockWidget* dock);

    /// @brief The docks on the right of the window, in the order of their tabs
    [[nodiscard]] QList<QDockWidget*> rightDocks() const;

    QMainWindow* m_mainWindow;

    // Dock widgets
    QDockWidget* m_navigatorDock{nullptr};
    QDockWidget* m_propertiesDock{nullptr};
    QDockWidget* m_logDock{nullptr};
    QDockWidget* m_searchDock{nullptr};
    QDockWidget* m_assistantDock{nullptr};
    QDockWidget* m_annotationsDock{nullptr};

    // Panels
    NavigatorPanel* m_navigatorPanel{nullptr};
    PropertiesPanel* m_propertiesPanel{nullptr};
    LogPanel* m_logPanel{nullptr};
    QPointer<DashboardPanel> m_dashboardPanel;  ///< QPointer: auto-nulls when panel is deleted by user
    SearchPanel* m_searchPanel{nullptr};
    AssistantPanel* m_assistantPanel{nullptr};
    AnnotationsPanel* m_annotationsPanel{nullptr};

    // Central widget
    QTabWidget* m_centralTabs{nullptr};
    QWidget* m_centralWrapper{nullptr};
    StandaloneInfoBar* m_standaloneInfoBar{nullptr};
    InfoBar* m_layoutNoticeBar{nullptr};  ///< The panels on the right hidden for a small screen

    // View actions (panel toggles)
    QAction* m_viewNavigatorAction{nullptr};
    QAction* m_viewPropertiesAction{nullptr};
    QAction* m_viewLogAction{nullptr};
    QAction* m_viewSearchAction{nullptr};
    QAction* m_viewAssistantAction{nullptr};
    QAction* m_viewAnnotationsAction{nullptr};

    // Icon tracking for refresh
    QList<QLabel*> m_dockIconLabels;
    QList<QToolButton*> m_dockToolButtons;
};

} // namespace gui
} // namespace kalahari
