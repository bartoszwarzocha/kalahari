// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_plugin_coordinator.cpp
/// @brief The commands and panels of the plugins in a book window

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/extension_points.h"
#include "kalahari/core/plugin_host.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_export.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/plugin_coordinator.h"
#include "kalahari/gui/utils/access_keys.h"
#include "test_support/window_parts.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QTextBrowser>
#include <QUrl>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

constexpr const char* FAKE_ID = "org.kalahari.test.fake";

/// A plugin of the tests, written in C++
class FakePlugin final : public core::ICommandProvider, public core::IPanelProvider {
public:
    [[nodiscard]] std::string getPluginId() const override { return FAKE_ID; }
    [[nodiscard]] std::string getVersion() const override { return "1.0.0"; }
    void onInit() override {}
    void onActivate() override {}

    std::vector<core::PluginCommandInfo> getCommands() override {
        return {
            {"hello", "&Hello", "Says hello", "", "Ctrl+Shift+F8", "", false},
            {"tools", "In Tools", "", "TOOLS", "", "", false},
            {"lost", "Lost", "", "NOWHERE/At All", "", "", false},
            {"book", "With a Book", "", "", "", "", true},
            {"taken", "Taken Keys", "", "", "Ctrl+S", "", false},
            {"fail", "Fail", "", "", "", "", false},
        };
    }

    void runCommand(const std::string& commandId) override {
        if (commandId == "fail") {
            throw std::runtime_error("broken");
        }
        ran.push_back(commandId);
    }

    std::vector<core::PluginPanelInfo> getPanels() override {
        return {{"page", "Fake Page", ""}};
    }

    std::string panelContent(const std::string& /*panelId*/) override {
        ++reads;
        return "<p>" + word + "</p>";
    }

    void onPanelLink(const std::string& /*panelId*/, const std::string& link) override {
        links.push_back(link);
    }

    std::vector<std::string> ran;
    std::vector<std::string> links;
    std::string word = "first";
    int reads = 0;
};

/// The submenu of @p menu with the text @p text, nullptr when it has none
QMenu* submenu(const QMenu* menu, const QString& text) {
    if (menu == nullptr) {
        return nullptr;
    }
    for (QAction* action : menu->actions()) {
        if (action->menu() != nullptr && action->text() == text) {
            return action->menu();
        }
    }
    return nullptr;
}

/// Whether @p menu has the action of the command @p commandId
bool hasCommand(const QMenu* menu, const std::string& commandId) {
    if (menu == nullptr) {
        return false;
    }
    QAction* action = kalahari::test::commands().getAction(commandId);
    return action != nullptr && menu->actions().contains(action);
}

/// A book window's menus, as MainWindow makes them, with the fake plugin registered
class PluginWindow {
public:
    PluginWindow() : m_plugin(std::make_shared<FakePlugin>()) {
        registerAllCommands(kalahari::test::commands(), CommandCallbacks{});
        m_builder.buildMenuBar(kalahari::test::commands(), &m_window);
        core::ExtensionPointRegistry::getInstance().registerPlugin(m_plugin);
    }

    ~PluginWindow() {
        core::ExtensionPointRegistry::getInstance().unregisterPlugin(FAKE_ID);
    }

    PluginWindow(const PluginWindow&) = delete;
    PluginWindow& operator=(const PluginWindow&) = delete;

    [[nodiscard]] QMenu* menu(const std::string& name) const { return m_builder.getMenu(name); }
    [[nodiscard]] QMainWindow& window() { return m_window; }
    [[nodiscard]] FakePlugin& plugin() const { return *m_plugin; }

private:
    QMainWindow m_window;
    MenuBuilder m_builder;
    std::shared_ptr<FakePlugin> m_plugin;
};

}  // namespace

TEST_CASE("Plugins in a window: the commands go to their menus with their keys",
          "[gui][plugin]") {
    PluginWindow window;
    PluginCoordinator coordinator(kalahari::test::book(), kalahari::test::commands(),
                                  &window.window(), nullptr);
    CommandRegistry& commands = kalahari::test::commands();

    // By default under Tools > Plugins > the name of the plugin (its id without a manifest)
    QMenu* plugins = submenu(window.menu("TOOLS"), MenuBuilder::menuText("Plugins"));
    REQUIRE(plugins != nullptr);
    QMenu* own = submenu(plugins, literalMenuText(QString::fromLatin1(FAKE_ID)));
    REQUIRE(own != nullptr);
    const std::string hello = PluginCoordinator::commandIdOf(FAKE_ID, "hello");
    CHECK(hello == "plugin.org.kalahari.test.fake.hello");
    CHECK(hasCommand(own, hello));
    // A menu of the program it names, and the default for a menu the program does not have
    CHECK(hasCommand(window.menu("TOOLS"), PluginCoordinator::commandIdOf(FAKE_ID, "tools")));
    CHECK(hasCommand(own, PluginCoordinator::commandIdOf(FAKE_ID, "lost")));

    const Command* command = commands.getCommand(hello);
    REQUIRE(command != nullptr);
    CHECK(command->isPluginCommand);
    CHECK(command->pluginId == FAKE_ID);
    CHECK(command->label == "Hello");
    CHECK(command->tooltip == "Says hello");
    CHECK(command->shortcut.toString() == QStringLiteral("Ctrl+Shift+F8"));
    // Keys another command has are not given to it
    const Command* taken = commands.getCommand(PluginCoordinator::commandIdOf(FAKE_ID, "taken"));
    REQUIRE(taken != nullptr);
    CHECK(taken->shortcut.isEmpty());

    // Choosing it runs the plugin's command
    commands.getAction(hello)->trigger();
    CHECK(window.plugin().ran == std::vector<std::string>{"hello"});
}

TEST_CASE("Plugins in a window: a command that needs a book waits for one", "[gui][plugin]") {
    PluginWindow window;
    PluginCoordinator coordinator(kalahari::test::book(), kalahari::test::commands(),
                                  &window.window(), nullptr);
    REQUIRE_FALSE(kalahari::test::book().isProjectOpen());
    QAction* action =
        kalahari::test::commands().getAction(PluginCoordinator::commandIdOf(FAKE_ID, "book"));
    REQUIRE(action != nullptr);
    coordinator.updateCommands();
    CHECK_FALSE(action->isEnabled());
    CHECK_FALSE(action->toolTip().isEmpty());
}

TEST_CASE("Plugins in a window: a panel is closed until opened and reads its page then",
          "[gui][plugin]") {
    PluginWindow window;
    PluginCoordinator coordinator(kalahari::test::book(), kalahari::test::commands(),
                                  &window.window(), nullptr);
    QDockWidget* dock = coordinator.panelDock(FAKE_ID, "page");
    QTextBrowser* page = coordinator.panelPage(FAKE_ID, "page");
    REQUIRE(dock != nullptr);
    REQUIRE(page != nullptr);
    CHECK(dock->isHidden());
    CHECK(window.window().dockWidgetArea(dock) == Qt::RightDockWidgetArea);

    // Its command in View > Panels opens it
    const std::string toggle = PluginCoordinator::panelCommandIdOf(FAKE_ID, "page");
    QMenu* panels = submenu(window.menu("VIEW"), MenuBuilder::menuText("Panels"));
    CHECK(hasCommand(panels, toggle));
    window.window().show();
    kalahari::test::commands().getAction(toggle)->trigger();
    QApplication::processEvents();
    CHECK(dock->isVisible());
    CHECK(page->toPlainText() == QStringLiteral("first"));

    // The plugin asks for its page to be read again
    window.plugin().word = "second";
    core::PluginHost::getInstance().setPanelRefresh(
        [](const std::string& pluginId, const std::string& panelId) {
            for (PluginCoordinator* each : PluginCoordinator::coordinators()) {
                each->refreshPanel(pluginId, panelId);
            }
        });
    core::PluginHost::getInstance().refreshPanel(FAKE_ID, "page");
    CHECK(page->toPlainText() == QStringLiteral("second"));
    core::PluginHost::getInstance().reset();

    // Its links go to the plugin
    emit page->anchorClicked(QUrl(QStringLiteral("chapter:7")));
    CHECK(window.plugin().links == std::vector<std::string>{"chapter:7"});
    window.window().hide();
}

TEST_CASE("Plugins in a window: the commands and panels of a plugin gone leave the window",
          "[gui][plugin]") {
    PluginWindow window;
    PluginCoordinator coordinator(kalahari::test::book(), kalahari::test::commands(),
                                  &window.window(), nullptr);
    CommandRegistry& commands = kalahari::test::commands();
    const std::string hello = PluginCoordinator::commandIdOf(FAKE_ID, "hello");
    QMenu* plugins = submenu(window.menu("TOOLS"), MenuBuilder::menuText("Plugins"));
    REQUIRE(plugins != nullptr);
    const qsizetype before = plugins->actions().size();
    REQUIRE(commands.isCommandRegistered(hello));

    core::ExtensionPointRegistry::getInstance().unregisterPlugin(FAKE_ID);
    coordinator.sync();
    QApplication::processEvents();
    CHECK_FALSE(commands.isCommandRegistered(hello));
    CHECK_FALSE(commands.isCommandRegistered(PluginCoordinator::panelCommandIdOf(FAKE_ID, "page")));
    CHECK(coordinator.panelDock(FAKE_ID, "page") == nullptr);
    CHECK(submenu(plugins, literalMenuText(QString::fromLatin1(FAKE_ID))) == nullptr);
    // Its submenu and the line above the plugins' menus are gone
    CHECK(plugins->actions().size() == before - 2);
    CHECK_FALSE(hasCommand(window.menu("TOOLS"), PluginCoordinator::commandIdOf(FAKE_ID, "tools")));

    // Back again, in its place
    core::ExtensionPointRegistry::getInstance().registerPlugin(std::make_shared<FakePlugin>());
    coordinator.sync();
    CHECK(commands.isCommandRegistered(hello));
    CHECK(submenu(plugins, literalMenuText(QString::fromLatin1(FAKE_ID))) != nullptr);
}

TEST_CASE("Plugins in a window: the book of an export as the plugins see it", "[gui][plugin]") {
    editor::BookExport book;
    book.title = QStringLiteral("Book");
    book.author = QStringLiteral("Author");
    book.language = QStringLiteral("pl");
    editor::ExportItem part;
    part.kind = editor::ExportItemKind::Group;
    part.elementId = QStringLiteral("p1");
    part.title = QStringLiteral("Part One");
    book.items.push_back(part);
    editor::ExportItem text;
    text.kind = editor::ExportItemKind::Text;
    text.elementId = QStringLiteral("t1");
    text.title = QStringLiteral("One");
    text.kml = QStringLiteral("<kml><p>Ala ma kota.</p></kml>");
    book.items.push_back(text);

    const core::PluginBook seen = pluginBookOf(book);
    CHECK(seen.title == "Book");
    CHECK(seen.author == "Author");
    CHECK(seen.language == "pl");
    REQUIRE(seen.items.size() == 2);
    CHECK(seen.items[0].kind == "part");
    CHECK(seen.items[0].title == "Part One");
    CHECK(seen.items[0].text.empty());
    CHECK(seen.items[1].kind == "text");
    CHECK(seen.items[1].id == "t1");
    CHECK(seen.items[1].kml == "<kml><p>Ala ma kota.</p></kml>");
    CHECK(seen.items[1].text == "Ala ma kota.");
}
