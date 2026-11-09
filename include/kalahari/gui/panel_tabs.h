/// @file panel_tabs.h
/// @brief The tabs of the panels docked one over another: the program's own in place of Qt's

#pragma once

#include "kalahari/gui/widgets/panel_tab_bar.h"

#include <QIcon>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <deque>
#include <string>

class QBoxLayout;
class QDockWidget;
class QMainWindow;
class QTabBar;

namespace kalahari {
namespace gui {

/// @brief The tabs of the panels docked one over another in a main window
///
/// Qt draws such tabs itself, with the panels' names alone in one row that shortens them to a
/// few letters. Here each panel has tabs of its own (PanelTabBar) with the tabs of its whole
/// group, above its title bar or under its content, showing icons, names or both, in one row
/// or in more, as the settings say (Settings > Appearance > General > Panels). Only the panel
/// in front can be seen, so one row of tabs shows, with its tab marked.
///
/// Qt's tabs stay as the order of the group (saved with the window's layout), but take no
/// room and no focus. The groups follow Qt's layout: panels docked onto others, moved,
/// floated, shown and closed. A group with a panel added without addPanel() keeps Qt's tabs.
class PanelTabs : public QObject {
    Q_OBJECT

public:
    /// @brief The setting of what the tabs show: LOOK_ICONS (the default), LOOK_ICONS_AND_NAMES
    ///        or LOOK_NAMES
    static constexpr const char* LOOK_KEY = "panels.tabLook";
    static constexpr const char* LOOK_ICONS = "icons";
    static constexpr const char* LOOK_ICONS_AND_NAMES = "iconsAndNames";
    static constexpr const char* LOOK_NAMES = "names";

    /// @brief The setting of tabs that do not fit: ROWS_WRAP (more rows, the default) or
    ///        ROWS_SINGLE (one row, names shortened)
    static constexpr const char* ROWS_KEY = "panels.tabRows";
    static constexpr const char* ROWS_WRAP = "wrap";
    static constexpr const char* ROWS_SINGLE = "single";

    /// @brief The setting of the side of the panels the tabs are on: SIDE_ABOVE (the default)
    ///        or SIDE_BELOW
    static constexpr const char* SIDE_KEY = "panels.tabSide";
    static constexpr const char* SIDE_ABOVE = "above";
    static constexpr const char* SIDE_BELOW = "below";

    /// @brief Tabs for the panels of @p window
    /// @param window The main window
    /// @param parent Parent object
    explicit PanelTabs(QMainWindow* window, QObject* parent = nullptr);

    /// @brief Give a panel its tabs
    ///
    /// The tabs go above the title bar when its layout is a QBoxLayout (they take its first
    /// place), else under the content; the content gets a widget of its own to hold them
    /// (QDockWidget::widget() is that widget afterwards).
    /// @param dock The panel, with its title bar and its content set
    /// @param commandId The command that shows the panel (View > Panels): its icon is the
    ///        tab's, its keys go to the tab's tooltip
    void addPanel(QDockWidget* dock, const std::string& commandId);

    /// @brief Show the tabs as the settings say
    void applySettings();

    /// @brief The tabs of a panel added with addPanel(), nullptr for another
    [[nodiscard]] PanelTabBar* tabBar(const QDockWidget* dock) const;

    /// @brief What the settings say the tabs show
    [[nodiscard]] static PanelTabBar::Look lookSetting();

    /// @brief What the settings say of tabs that do not fit
    [[nodiscard]] static PanelTabBar::Rows rowsSetting();

    /// @brief The side of the panels the settings put the tabs on
    [[nodiscard]] static PanelTabBar::Side sideSetting();

public slots:
    /// @brief Bring the tabs up to date with the window's layout now (they follow it by
    ///        themselves a moment after it changes)
    void refresh();

protected:
    /// @brief The window's layout changed, or Qt made new tabs: refresh() soon
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief A panel and its tabs
    struct Panel {
        QPointer<QDockWidget> dock;
        std::string commandId;
        QIcon icon;
        PanelTabBar* tabs = nullptr;
        QBoxLayout* body = nullptr;  ///< The layout of its content, the tabs' place below it
        QList<QDockWidget*> group;   ///< The panels of its tabs, as last shown
    };

    /// @brief refresh() once the events waiting now are handled
    void scheduleRefresh();

    /// @brief Put a panel's tabs on the side the settings say
    void placeTabs(Panel& panel, PanelTabBar::Side side);

    /// @brief Give a panel's tabs the panels of its group
    void fill(Panel& panel);

    /// @brief The panel of @p dock, nullptr for a dock without tabs of its own
    [[nodiscard]] Panel* panelOf(const QDockWidget* dock);

    /// @brief The panel of tab @p index of Qt's tabs, nullptr for a dock without tabs of its
    ///        own
    [[nodiscard]] QDockWidget* dockOfTab(const QTabBar* bar, int index) const;

    /// @brief Qt's tabs of the group of @p dock, nullptr without a group
    [[nodiscard]] QTabBar* qtTabsOf(const QDockWidget* dock) const;

    /// @brief Hide Qt's tabs (they take no room and no focus) or show them again
    static void hideQtTabs(QTabBar* bar, bool hide);

    /// @brief The tooltip of a panel's tab: its name and its keys
    [[nodiscard]] static QString toolTipOf(const Panel& panel);

    /// @brief Bring tab @p index of @p dock's tabs to the front; the keys follow the tabs
    void choose(const QDockWidget* dock, int index);

    /// @brief Move tab @p from of @p dock's tabs to @p to
    void move(const QDockWidget* dock, int from, int to);

    QMainWindow* m_window;
    std::deque<Panel> m_panels;  ///< A deque keeps the panels in place as more come
    bool m_refreshPending = false;
    bool m_refill = false;       ///< The next refresh() fills all the tabs again
};

} // namespace gui
} // namespace kalahari
