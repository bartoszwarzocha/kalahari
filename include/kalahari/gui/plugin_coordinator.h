// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plugin_coordinator.h
/// @brief The commands and panels of the plugins in a book window, and what the program gives
///        the plugins
///
/// PluginManager loads the plugins (Kalahari's own add-ons) before the first window and when
/// the writer turns one on; each book window's PluginCoordinator puts their commands into its
/// menus and their panels into its docks, and takes them away when a plugin goes. The window
/// keeps only the ids: every call goes to the plugin through its extension points
/// (core::ICommandProvider, core::IPanelProvider).

#pragma once

#include <kalahari/core/extension_points.h>
#include <kalahari/core/plugin_host.h>

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <map>
#include <string>
#include <vector>

class QAction;
class QDockWidget;
class QMainWindow;
class QMenu;
class QTextBrowser;

namespace kalahari {

namespace core {
class ProjectManager;
}

namespace editor {
struct BookExport;
}

namespace gui {

class CommandRegistry;
class DockCoordinator;

/// @brief The commands and panels of the plugins in one book window
class PluginCoordinator : public QObject {
    Q_OBJECT

public:
    /// @param book The book of the window (commands that need a book are greyed out without
    ///        one)
    /// @param commands The commands of the window
    /// @param window The book window: its menus and its docks
    /// @param docks The docks of the window (title bars, tabs of the panels); may be nullptr
    /// @param parent Parent object
    PluginCoordinator(core::ProjectManager& book, CommandRegistry& commands, QMainWindow* window,
                      DockCoordinator* docks, QObject* parent = nullptr);
    ~PluginCoordinator() override;

    PluginCoordinator(const PluginCoordinator&) = delete;
    PluginCoordinator& operator=(const PluginCoordinator&) = delete;

    /// @brief The coordinators of the windows there are now
    [[nodiscard]] static QList<PluginCoordinator*> coordinators();

    /// @brief Bring the window up to date with the plugins loaded now: the commands and panels
    ///        of the new ones in, those of the plugins gone out
    void sync();

    /// @brief Show the states of the commands again (a book opened or closed)
    void updateCommands();

    /// @brief Read the page of a panel again, when it is open (later otherwise)
    void refreshPanel(const std::string& pluginId, const std::string& panelId);

    /// @brief The id of a plugin's command in the window: "plugin.<plugin id>.<command id>"
    [[nodiscard]] static std::string commandIdOf(const std::string& pluginId,
                                                 const std::string& commandId);

    /// @brief The id of the command that opens a plugin's panel (View > Panels):
    ///        "plugin-panel.<plugin id>.<panel id>"
    [[nodiscard]] static std::string panelCommandIdOf(const std::string& pluginId,
                                                      const std::string& panelId);

    /// @brief The dock of a plugin's panel; nullptr when the window has none
    [[nodiscard]] QDockWidget* panelDock(const std::string& pluginId,
                                         const std::string& panelId) const;

    /// @brief The page of a plugin's panel; nullptr when the window has none
    [[nodiscard]] QTextBrowser* panelPage(const std::string& pluginId,
                                          const std::string& panelId) const;

private:
    /// A panel of a plugin in the window
    struct Panel {
        std::string pluginId;
        std::string panelId;
        std::string commandId;
        QPointer<QDockWidget> dock;
        QPointer<QTextBrowser> page;
        bool stale = true;  ///< Its page is to be read again when it is shown
    };

    /// What a plugin added to the window
    struct Parts {
        std::vector<std::string> commandIds;
        std::vector<Panel> panels;
        QPointer<QMenu> menu;  ///< Its own submenu of Tools > Plugins, when it has one
    };

    void addPlugin(const std::string& pluginId);
    void removePlugin(const std::string& pluginId);
    void addCommand(const std::string& pluginId, const QString& pluginName,
                    const core::PluginCommandInfo& info, Parts& parts);
    void addPanel(const std::string& pluginId, const core::PluginPanelInfo& info, Parts& parts);

    /// The keys a plugin's command may have: none when the program or another command has them
    [[nodiscard]] QString allowedKeys(const std::string& commandId, const std::string& keys) const;

    /// The menu of the window at @p menuPath ("TOOLS/Statistics"); nullptr when it has none
    [[nodiscard]] QMenu* existingMenu(const std::string& menuPath) const;

    /// Tools > Plugins > the plugin's name, made when it is not there yet
    QMenu* pluginMenu(const QString& pluginName, Parts& parts);

    /// Read a panel's page from its plugin
    void readPage(Panel& panel);

    /// A link clicked on a panel's page: web links to the browser, the others to the plugin
    void openLink(const std::string& pluginId, const std::string& panelId, const QUrl& link);

    /// Tell the writer what went wrong in a plugin
    void showError(const std::string& pluginId, const QString& what) const;

    /// The panel of @p pluginId and @p panelId, nullptr when the window has none
    [[nodiscard]] const Panel* findPanel(const std::string& pluginId,
                                         const std::string& panelId) const;

    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;   ///< The commands of its window
    QMainWindow* m_window;
    DockCoordinator* m_docks;
    std::map<std::string, Parts> m_plugins;  ///< By plugin id
    QPointer<QAction> m_separator;           ///< Between Tools > Plugins and the plugins' menus
    int m_listener = 0;                      ///< Of PluginManager
};

/// @brief The book of an export as the plugins see it
[[nodiscard]] core::PluginBook pluginBookOf(const editor::BookExport& book);

/// @brief Give the plugins the book and the text of the window in front, the messages in
///        windows of the program and the refreshing of their panels in every window
/// @note Call once, before the plugins are loaded
void installPluginHost();

/// @brief Tell the plugins the daily goal was reached; once a day, whichever window saw it
void tellPluginsGoalReached();

}  // namespace gui
}  // namespace kalahari
