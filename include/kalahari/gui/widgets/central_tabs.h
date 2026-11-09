/// @file central_tabs.h
/// @brief The tabs in the middle of the window: Qt's one row, or as many rows as the width needs

#pragma once

#include "kalahari/gui/widgets/panel_tab_bar.h"

#include <QTabWidget>

class QAction;
class QToolButton;

namespace kalahari {
namespace gui {

class CentralTabBar;

/// @brief The tabs in the middle of the window: the Dashboard, the chapters, the lists and the
///        materials
///
/// In one row (the default) they are Qt's tabs, with arrows when they do not fit. In more
/// rows (Settings > Appearance > General > Tabs in the middle of the window) they are a
/// PanelTabBar drawn by the style as Qt's tabs, each with its close button, over Qt's tab bar.
/// Qt's tab bar stays the tabs of the rest of the program, with their order, texts, icons and
/// signals: it only asks for the height of the rows and paints nothing, its close buttons and
/// arrows hidden. A click, the keys, a drag and a close button of the rows act on it as on its
/// own tabs (tabBarClicked(), tabMoved(), tabCloseRequested()).
///
/// A button in the right corner, after the tabs, switches between one row and more rows at
/// once and keeps the choice in the setting (rowsButton()).
class CentralTabs : public QTabWidget {
    Q_OBJECT

public:
    /// @brief The setting of tabs that do not fit: ROWS_SINGLE (one row with arrows, the
    ///        default) or ROWS_WRAP (more rows)
    static constexpr const char* ROWS_KEY = "window.tabRows";
    static constexpr const char* ROWS_SINGLE = "single";
    static constexpr const char* ROWS_WRAP = "wrap";

    /// @brief Tabs in one row
    /// @param parent Parent widget
    explicit CentralTabs(QWidget* parent = nullptr);

    /// @brief Qt's tabs in one row, or the tabs in as many rows as the width needs
    void setRows(PanelTabBar::Rows rows);
    [[nodiscard]] PanelTabBar::Rows rows() const;

    /// @brief The tabs in more rows, nullptr in one row
    [[nodiscard]] PanelTabBar* tabRows() const;

    /// @brief The button in the right corner: down in more rows, up in one row; clicked, it
    ///        switches the rows and keeps the choice in the setting (ROWS_KEY)
    [[nodiscard]] QToolButton* rowsButton() const;

    /// @brief Show the tabs in the rows the settings say
    void applySettings();

    /// @brief What the settings say of tabs that do not fit
    [[nodiscard]] static PanelTabBar::Rows rowsSetting();

private:
    CentralTabBar* m_bar;
    QAction* m_rowsAction;  ///< Of rowsButton(), checked in more rows
    QToolButton* m_rowsButton;
};

} // namespace gui
} // namespace kalahari
