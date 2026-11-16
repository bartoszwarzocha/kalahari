// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plugin_coordinator.cpp
/// @brief The commands and panels of the plugins in a book window, and what the program gives
///        the plugins

#include "kalahari/gui/plugin_coordinator.h"

#include "kalahari/core/chapter_document.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/plugin_manager.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/trusted_keys.h"
#include "kalahari/editor/book_export.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/main_window.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/panel_tabs.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/utils/access_keys.h"
#include "kalahari/gui/utils/panel_toggle.h"

#include <QAction>
#include <QDate>
#include <QDesktopServices>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QTextBrowser>
#include <QUrl>

#include <algorithm>
#include <set>
#include <sstream>

namespace kalahari::gui {

namespace {

/// The icon of the plugins' commands and panels without one of their own
constexpr const char* PLUGINS_ICON = "menu.plugins";
/// The object name of the line between Tools > Plugins and the plugins' menus
constexpr const char* SEPARATOR_NAME = "pluginMenusSeparator";
/// The start of the object names of the plugins' own submenus of Tools > Plugins
constexpr const char* MENU_NAME_PREFIX = "PluginMenu_";

QList<PluginCoordinator*>& coordinatorList() {
    static QList<PluginCoordinator*> list;
    return list;
}

QString qt(const std::string& text) {
    return QString::fromStdString(text);
}

/// The parts of a menu path: "TOOLS/Statistics" → ["TOOLS", "Statistics"]
std::vector<std::string> partsOf(const std::string& path) {
    std::vector<std::string> parts;
    std::istringstream stream(path);
    std::string part;
    while (std::getline(stream, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

/// The submenu of @p menu with the text @p text, nullptr when it has none
QMenu* submenuOf(const QMenu* menu, const QString& text) {
    for (QAction* action : menu->actions()) {
        if (action->menu() != nullptr && action->text() == text) {
            return action->menu();
        }
    }
    return nullptr;
}

/// The name of a plugin for the writer: its manifest's, else its id
QString pluginName(const std::string& pluginId) {
    const core::PluginMetadata* metadata =
        core::PluginManager::getInstance().getPluginMetadata(pluginId);
    return metadata != nullptr && !metadata->name.empty() ? qt(metadata->name) : qt(pluginId);
}

/// Register the icon of a plugin's command or panel under @p id: an SVG file of the plugin,
/// an icon of the program, or the icon of the plugins
/// The object name of the plugin's own submenu of Tools > Plugins
QString menuNameOf(const std::string& pluginId) {
    return QString::fromLatin1(MENU_NAME_PREFIX) + qt(pluginId);
}

void registerIcon(const std::string& id, const std::string& icon, const QString& label) {
    auto& icons = core::IconRegistry::getInstance();
    QString path;
    if (!icon.empty() && QString::fromStdString(icon).endsWith(QLatin1String(".svg"))) {
        path = qt(icon);  // Absolute already (core::PythonPlugin)
    } else if (const core::IconDescriptor* own =
                   icon.empty() ? nullptr : icons.getIconDescriptor(qt(icon))) {
        path = own->defaultSVGPath;
    } else if (const core::IconDescriptor* plugins =
                   icons.getIconDescriptor(QString::fromLatin1(PLUGINS_ICON))) {
        path = plugins->defaultSVGPath;
    }
    if (!path.isEmpty()) {
        icons.registerIcon(qt(id), path, label);
    }
}

}  // namespace

PluginCoordinator::PluginCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                                     QMainWindow* window, DockCoordinator* docks,
                                     QObject* parent)
    : QObject(parent), m_book(book), m_commands(commands), m_window(window), m_docks(docks) {
    coordinatorList().append(this);
    // PluginManager tells of the plugins loaded and unloaded on the main thread; the window
    // follows them a moment later, when the plugin is registered or gone
    m_listener = core::PluginManager::getInstance().addChangeListener(
        [guard = QPointer<PluginCoordinator>(this)](const std::string&) {
            if (guard != nullptr) {
                QMetaObject::invokeMethod(guard.data(), "sync", Qt::QueuedConnection);
            }
        });
    sync();
}

PluginCoordinator::~PluginCoordinator() {
    core::PluginManager::getInstance().removeChangeListener(m_listener);
    coordinatorList().removeAll(this);
}

QList<PluginCoordinator*> PluginCoordinator::coordinators() {
    return coordinatorList();
}

std::string PluginCoordinator::commandIdOf(const std::string& pluginId,
                                           const std::string& commandId) {
    return "plugin." + pluginId + "." + commandId;
}

std::string PluginCoordinator::panelCommandIdOf(const std::string& pluginId,
                                                const std::string& panelId) {
    return "plugin-panel." + pluginId + "." + panelId;
}

void PluginCoordinator::sync() {
    auto& registry = core::ExtensionPointRegistry::getInstance();
    std::set<std::string> present;
    for (const auto& provider : registry.getPluginsOfType<core::ICommandProvider>()) {
        present.insert(provider->getPluginId());
    }
    for (const auto& provider : registry.getPluginsOfType<core::IPanelProvider>()) {
        present.insert(provider->getPluginId());
    }

    std::vector<std::string> gone;
    for (const auto& [pluginId, parts] : m_plugins) {
        if (present.count(pluginId) == 0) {
            gone.push_back(pluginId);
        }
    }
    for (const std::string& pluginId : gone) {
        removePlugin(pluginId);
    }
    for (const std::string& pluginId : present) {
        if (m_plugins.count(pluginId) == 0) {
            addPlugin(pluginId);
        }
    }
}

void PluginCoordinator::updateCommands() {
    for (const auto& [pluginId, parts] : m_plugins) {
        for (const std::string& commandId : parts.commandIds) {
            m_commands.updateActionState(commandId);
        }
    }
}

void PluginCoordinator::addPlugin(const std::string& pluginId) {
    auto& registry = core::ExtensionPointRegistry::getInstance();
    Parts& parts = m_plugins[pluginId];
    const QString name = pluginName(pluginId);
    try {
        if (const auto provider = registry.getPluginAs<core::ICommandProvider>(pluginId)) {
            for (const core::PluginCommandInfo& info : provider->getCommands()) {
                addCommand(pluginId, name, info, parts);
            }
        }
        if (const auto provider = registry.getPluginAs<core::IPanelProvider>(pluginId)) {
            for (const core::PluginPanelInfo& info : provider->getPanels()) {
                addPanel(pluginId, info, parts);
            }
        }
    } catch (const std::exception& error) {
        core::Logger::getInstance().error("Plugins: '{}' did not give its commands and panels: {}",
                                          pluginId, error.what());
    }
    core::Logger::getInstance().info("Plugins: '{}' in the window: {} commands, {} panels",
                                     pluginId, parts.commandIds.size(), parts.panels.size());
}

void PluginCoordinator::removePlugin(const std::string& pluginId) {
    const auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return;
    }
    Parts& parts = it->second;
    for (const std::string& commandId : parts.commandIds) {
        m_commands.unregisterCommand(commandId);  // Its action leaves the menus
    }
    for (Panel& panel : parts.panels) {
        m_commands.unregisterCommand(panel.commandId);
        if (panel.dock != nullptr) {
            m_window->removeDockWidget(panel.dock);
            panel.dock->deleteLater();
        }
    }
    m_plugins.erase(it);

    const QMenu* plugins = existingMenu("TOOLS/Plugins");
    if (plugins == nullptr) {
        return;
    }
    // sync() runs after the events of the change (and the menus they closed), so the menu
    // can go at once
    delete plugins->findChild<QMenu*>(menuNameOf(pluginId), Qt::FindDirectChildrenOnly);
    // The line above the plugins' menus goes with the last of them
    const QList<QMenu*> menus = plugins->findChildren<QMenu*>(Qt::FindDirectChildrenOnly);
    const bool anyMenu = std::any_of(menus.cbegin(), menus.cend(), [](const QMenu* menu) {
        return menu->objectName().startsWith(QLatin1String(MENU_NAME_PREFIX));
    });
    if (!anyMenu) {
        delete plugins->findChild<QAction*>(QString::fromLatin1(SEPARATOR_NAME),
                                            Qt::FindDirectChildrenOnly);
    }
}

void PluginCoordinator::addCommand(const std::string& pluginId, const QString& pluginName,
                                   const core::PluginCommandInfo& info, Parts& parts) {
    const std::string id = commandIdOf(pluginId, info.id);
    QMenu* menu = info.menu.empty() ? nullptr : existingMenu(info.menu);
    std::string menuPath = info.menu;
    if (menu == nullptr) {
        if (!info.menu.empty()) {
            core::Logger::getInstance().warn(
                "Plugins: '{}' asks for the menu '{}', which the program does not have", id,
                info.menu);
        }
        menu = pluginMenu(pluginId, pluginName);
        menuPath = "TOOLS/Plugins/" + pluginName.toStdString();
    }
    if (menu == nullptr) {
        return;
    }

    const QString label = withoutAccessKey(qt(info.label));
    Command command;
    command.id = id;
    command.label = label.toStdString();
    command.menuText = info.label;
    command.tooltip = info.tooltip;
    command.category = partsOf(menuPath).front();
    command.menuPath = menuPath + "/" + command.label;
    command.isPluginCommand = true;
    command.pluginId = pluginId;
    command.shortcut = KeyboardShortcut::fromString(allowedKeys(id, info.shortcut));
    command.execute = [this, pluginId, commandId = info.id]() {
        const auto provider =
            core::ExtensionPointRegistry::getInstance().getPluginAs<core::ICommandProvider>(
                pluginId);
        if (provider == nullptr) {
            return;
        }
        try {
            provider->runCommand(commandId);
        } catch (const std::exception& error) {
            showError(pluginId, QString::fromUtf8(error.what()));
        }
    };
    if (info.needsBook) {
        command.isEnabled = [this]() { return m_book.isProjectOpen(); };
        //: The tooltip of a command of a plugin while no book is open. In Polish: Otwórz
        //: książkę, nad którą działa to polecenie
        command.disabledReason = []() { return tr("Open the book this command works on"); };
    }
    registerIcon(id, info.icon, label);
    m_commands.registerCommand(std::move(command));
    if (QAction* action = m_commands.getAction(id)) {
        menu->addAction(action);
    }
    parts.commandIds.push_back(id);
}

void PluginCoordinator::addPanel(const std::string& pluginId, const core::PluginPanelInfo& info,
                                 Parts& parts) {
    const std::string commandId = panelCommandIdOf(pluginId, info.id);
    const QString title = qt(info.title);

    auto* dock = new QDockWidget(title, m_window);
    dock->setObjectName(QStringLiteral("PluginDock_%1_%2").arg(qt(pluginId), qt(info.id)));
    auto* page = new QTextBrowser(dock);
    page->setOpenLinks(false);  // The plugin or the browser opens them
    page->setObjectName(QStringLiteral("pluginPanelPage"));
    dock->setWidget(page);

    registerIcon(commandId, info.icon, title);
    Command command;
    command.id = commandId;
    command.label = title.toStdString();
    command.menuText = literalMenuText(title).toStdString();
    //: The tooltip of the command of a plugin's panel in View > Panels. In Polish: Pokaż albo
    //: ukryj panel %1
    command.tooltip = tr("Show or hide the panel %1").arg(title).toStdString();
    command.category = "VIEW";
    command.menuPath = "VIEW/Panels/" + command.label;
    command.isPluginCommand = true;
    command.pluginId = pluginId;
    command.execute = [dock]() { utils::togglePanel(dock); };
    command.isChecked = [dock]() { return utils::isPanelOpen(dock); };
    m_commands.registerCommand(std::move(command));

    if (m_docks != nullptr) {
        m_docks->setupDockTitleBar(dock, qt(commandId), title);
    }
    m_window->addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->hide();  // Closed until the writer opens it
    if (m_docks != nullptr && m_docks->panelTabs() != nullptr) {
        m_docks->panelTabs()->addPanel(dock, commandId);
    }
    QAction* toggle = m_commands.getAction(commandId);
    if (toggle != nullptr) {
        utils::followPanel(toggle, dock);
        if (QMenu* panels = existingMenu("VIEW/Panels")) {
            panels->addAction(toggle);
        }
    }

    parts.panels.push_back(Panel{pluginId, info.id, commandId, dock, page, true});
    const std::string panelId = info.id;
    connect(page, &QTextBrowser::anchorClicked, this,
            [this, pluginId, panelId](const QUrl& link) { openLink(pluginId, panelId, link); });
    connect(dock, &QDockWidget::visibilityChanged, this, [this, pluginId, panelId](bool shown) {
        if (shown) {
            refreshPanel(pluginId, panelId);
        }
    });
}

QString PluginCoordinator::allowedKeys(const std::string& commandId,
                                       const std::string& keys) const {
    if (keys.empty()) {
        return {};
    }
    auto& logger = core::Logger::getInstance();
    const KeyboardShortcut shortcut = KeyboardShortcut::fromString(qt(keys));
    if (shortcut.isEmpty()) {
        logger.warn("Plugins: '{}' has keys the program does not know: '{}'", commandId, keys);
        return {};
    }
    const QKeyCombination combination(shortcut.modifiers, static_cast<Qt::Key>(shortcut.keyCode));
    const KeyCheck check = ShortcutRules().check(combination, commandId);
    if (check.result == KeyCheck::Result::Refused) {
        logger.warn("Plugins: '{}' cannot have the keys {}: {}", commandId, keys,
                    check.reason.toStdString());
        return {};
    }
    for (const Command& other : m_commands.getAllCommands()) {
        if (other.id != commandId && other.shortcut == shortcut) {
            logger.warn("Plugins: '{}' cannot have the keys {}: '{}' has them", commandId, keys,
                        other.id);
            return {};
        }
    }
    return shortcut.toString();
}

QMenu* PluginCoordinator::existingMenu(const std::string& menuPath) const {
    const std::vector<std::string> parts = partsOf(menuPath);
    if (parts.empty() || m_window == nullptr) {
        return nullptr;
    }
    const std::vector<std::string>& top = MenuBuilder::topLevelMenus();
    if (std::find(top.begin(), top.end(), parts.front()) == top.end()) {
        return nullptr;
    }
    auto* menu = m_window->menuBar()->findChild<QMenu*>(qt(parts.front() + "Menu"),
                                                        Qt::FindDirectChildrenOnly);
    for (size_t i = 1; menu != nullptr && i < parts.size(); ++i) {
        menu = submenuOf(menu, MenuBuilder::menuText(parts[i]));
    }
    return menu;
}

QMenu* PluginCoordinator::pluginMenu(const std::string& pluginId,
                                     const QString& pluginName) const {
    QMenu* plugins = existingMenu("TOOLS/Plugins");
    if (plugins == nullptr) {
        return nullptr;
    }
    const QString name = menuNameOf(pluginId);
    if (auto* own = plugins->findChild<QMenu*>(name, Qt::FindDirectChildrenOnly)) {
        return own;
    }
    if (plugins->findChild<QAction*>(QString::fromLatin1(SEPARATOR_NAME),
                                     Qt::FindDirectChildrenOnly) == nullptr) {
        plugins->addSeparator()->setObjectName(QString::fromLatin1(SEPARATOR_NAME));
    }
    QMenu* own = plugins->addMenu(literalMenuText(pluginName));
    own->setObjectName(name);
    own->setToolTipsVisible(true);
    MenuBuilder::showSubmenuIcon(own, QString::fromLatin1(PLUGINS_ICON));
    return own;
}

void PluginCoordinator::refreshPanel(const std::string& pluginId, const std::string& panelId) {
    const auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return;
    }
    for (Panel& panel : it->second.panels) {
        if (panel.panelId != panelId) {
            continue;
        }
        panel.stale = true;
        if (panel.dock != nullptr && panel.dock->isVisible()) {
            readPage(panel);
        }
    }
}

void PluginCoordinator::readPage(Panel& panel) {
    if (panel.page == nullptr) {
        return;
    }
    panel.stale = false;
    const auto provider =
        core::ExtensionPointRegistry::getInstance().getPluginAs<core::IPanelProvider>(
            panel.pluginId);
    if (provider == nullptr) {
        return;
    }
    try {
        panel.page->setHtml(qt(provider->panelContent(panel.panelId)));
    } catch (const std::exception& error) {
        core::Logger::getInstance().error("Plugins: the panel '{}' of '{}' failed: {}",
                                          panel.panelId, panel.pluginId, error.what());
        //: The page of a plugin's panel when the plugin failed to give it. %1: what the plugin
        //: said. In Polish: Wtyczka nie podała zawartości panelu: %1
        panel.page->setPlainText(
            tr("The plugin did not give the content of the panel: %1").arg(
                QString::fromUtf8(error.what())));
    }
}

void PluginCoordinator::openLink(const std::string& pluginId, const std::string& panelId,
                                 const QUrl& link) {
    const QString scheme = link.scheme().toLower();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https") ||
        scheme == QLatin1String("mailto")) {
        QDesktopServices::openUrl(link);
        return;
    }
    const auto provider =
        core::ExtensionPointRegistry::getInstance().getPluginAs<core::IPanelProvider>(pluginId);
    if (provider == nullptr) {
        return;
    }
    try {
        provider->onPanelLink(panelId, link.toString().toStdString());
    } catch (const std::exception& error) {
        showError(pluginId, QString::fromUtf8(error.what()));
    }
}

void PluginCoordinator::showError(const std::string& pluginId, const QString& what) const {
    core::Logger::getInstance().error("Plugins: '{}' failed: {}", pluginId, what.toStdString());
    //: The title of the window of an error of a plugin. In Polish: Błąd wtyczki
    dialogs::MessageDialog::error(m_window, tr("Plugin Error"),
                                  //: %1: the name of the plugin. In Polish: Wtyczka „%1” nie
                                  //: wykonała polecenia.
                                  tr("The plugin “%1” did not do what it was asked.")
                                      .arg(pluginName(pluginId)),
                                  what);
}

const PluginCoordinator::Panel* PluginCoordinator::findPanel(const std::string& pluginId,
                                                             const std::string& panelId) const {
    const auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return nullptr;
    }
    for (const Panel& panel : it->second.panels) {
        if (panel.panelId == panelId) {
            return &panel;
        }
    }
    return nullptr;
}

QDockWidget* PluginCoordinator::panelDock(const std::string& pluginId,
                                          const std::string& panelId) const {
    const Panel* panel = findPanel(pluginId, panelId);
    return panel != nullptr ? panel->dock.data() : nullptr;
}

QTextBrowser* PluginCoordinator::panelPage(const std::string& pluginId,
                                           const std::string& panelId) const {
    const Panel* panel = findPanel(pluginId, panelId);
    return panel != nullptr ? panel->page.data() : nullptr;
}

// ============================================================================
// What the program gives the plugins
// ============================================================================

core::PluginBook pluginBookOf(const editor::BookExport& book) {
    core::PluginBook result;
    result.title = book.title.toStdString();
    result.author = book.author.toStdString();
    result.language = book.language.toStdString();
    for (const editor::ExportItem& item : book.items) {
        core::PluginText text;
        text.id = item.elementId.toStdString();
        text.title = item.title.toStdString();
        switch (item.kind) {
            case editor::ExportItemKind::Text:
                text.kind = "text";
                text.kml = item.kml.toStdString();
                text.text = core::ChapterDocument::kmlToPlainText(item.kml).toStdString();
                break;
            case editor::ExportItemKind::Group:
                text.kind = "part";
                break;
            case editor::ExportItemKind::Contents:
                text.kind = "contents";
                break;
        }
        result.items.push_back(std::move(text));
    }
    return result;
}

namespace {

/// The export of the window in front, of the book or of the text in front
std::optional<editor::BookExport> frontExport(dialogs::ExportBookDialog::Content content) {
    MainWindow* window = MainWindow::frontWindow();
    if (window == nullptr || window->bookExportCoordinator() == nullptr) {
        return std::nullopt;
    }
    dialogs::ExportBookDialog::Choice choice;
    choice.content = content;
    return window->bookExportCoordinator()->bookExport(choice);
}

}  // namespace

void installPluginHost() {
    using Content = dialogs::ExportBookDialog::Content;
    auto& host = core::PluginHost::getInstance();
    host.setBookSource([]() -> std::optional<core::PluginBook> {
        MainWindow* window = MainWindow::frontWindow();
        if (window == nullptr || !window->book().isProjectOpen()) {
            return std::nullopt;
        }
        const std::optional<editor::BookExport> book = frontExport(Content::Book);
        return book ? std::optional(pluginBookOf(*book)) : std::nullopt;
    });
    host.setFrontTextSource([]() -> std::optional<core::PluginText> {
        const std::optional<editor::BookExport> front = frontExport(Content::Front);
        if (!front || front->items.empty()) {
            return std::nullopt;
        }
        return pluginBookOf(*front).items.front();
    });
    host.setMessageSink([](const std::string& title, const std::string& text) {
        dialogs::MessageDialog::information(MainWindow::frontWindow(), qt(title), qt(text));
    });
    host.setPanelRefresh([](const std::string& pluginId, const std::string& panelId) {
        for (PluginCoordinator* coordinator : PluginCoordinator::coordinators()) {
            coordinator->refreshPanel(pluginId, panelId);
        }
    });
    host.setProgramLanguage(core::SettingsManager::getInstance().getLanguage());
}

void loadPlugins() {
    core::TrustedKeys::getInstance().loadBuiltinKeys();
    core::PluginManager::getInstance().loadEnabledPlugins();
}

void unloadPlugins() {
    core::PluginManager::getInstance().unloadAll();
}

void tellPluginsGoalReached() {
    static QDate told;
    const QDate today = QDate::currentDate();
    if (told == today) {
        return;  // Another window saw it already
    }
    told = today;
    for (const auto& assistant :
         core::ExtensionPointRegistry::getInstance().getPluginsOfType<core::IAssistant>()) {
        try {
            assistant->onGoalReached();
        } catch (const std::exception& error) {
            core::Logger::getInstance().error("Plugins: '{}' failed on the goal reached: {}",
                                              assistant->getPluginId(), error.what());
        }
    }
}

}  // namespace kalahari::gui
