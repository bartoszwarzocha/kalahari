/// @file test_shortcut_customization.cpp
/// @brief The user's keys of the commands: in the registry, in the settings and in the files

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/shortcut_settings.h"

#include <QAction>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QStringList>

#include <string>

using namespace kalahari::gui;
using ShortcutMap = CommandRegistry::ShortcutMap;

namespace {

KeyboardShortcut ctrl(Qt::Key key) {
    return KeyboardShortcut(key, Qt::ControlModifier);
}

/// A command of the menus with keys, which the program runs
void registerCommand(const std::string& id, const KeyboardShortcut& keys) {
    Command command;
    command.id = id;
    command.label = id;
    command.menuPath = "FILE/" + id;
    command.shortcut = keys;
    command.execute = []() {};
    CommandRegistry::getInstance().registerCommand(command);
}

/// Counts the changes of the keys while it lives (the registry outlives the test)
class ChangeCounter {
public:
    ChangeCounter()
        : m_connection(QObject::connect(&CommandRegistry::getInstance(),
                                        &CommandRegistry::shortcutsChanged,
                                        [this]() { ++m_count; }))
    {
    }
    ~ChangeCounter() { QObject::disconnect(m_connection); }
    ChangeCounter(const ChangeCounter&) = delete;
    ChangeCounter& operator=(const ChangeCounter&) = delete;

    [[nodiscard]] int count() const { return m_count; }

private:
    int m_count = 0;
    QMetaObject::Connection m_connection;
};

KeyboardShortcut keysOf(const std::string& id) {
    const Command* command = CommandRegistry::getInstance().getCommand(id);
    REQUIRE(command != nullptr);
    return command->shortcut;
}

}  // namespace

TEST_CASE("Custom shortcuts: the user's keys take the place of the program's",
          "[gui][shortcuts][custom]") {
    const ShortcutMap defaults = {
        {"a", ctrl(Qt::Key_1)}, {"b", ctrl(Qt::Key_2)}, {"c", KeyboardShortcut()}};

    SECTION("Without the user's keys the program's") {
        CHECK(CommandRegistry::resolveShortcuts(defaults, {}) == defaults);
    }
    SECTION("Other keys, or none") {
        const ShortcutMap keys =
            CommandRegistry::resolveShortcuts(defaults, {{"a", ctrl(Qt::Key_3)}, {"b", {}}});
        CHECK(keys.at("a") == ctrl(Qt::Key_3));
        CHECK(keys.at("b").isEmpty());
        CHECK(keys.at("c").isEmpty());
    }
    SECTION("Keys taken from another command leave it without keys") {
        const ShortcutMap keys =
            CommandRegistry::resolveShortcuts(defaults, {{"c", ctrl(Qt::Key_1)}});
        CHECK(keys.at("c") == ctrl(Qt::Key_1));
        CHECK(keys.at("a").isEmpty());
        CHECK(keys.at("b") == ctrl(Qt::Key_2));
    }
    SECTION("Of two commands given the same keys, the first by id keeps them") {
        const ShortcutMap keys = CommandRegistry::resolveShortcuts(
            defaults, {{"a", ctrl(Qt::Key_9)}, {"b", ctrl(Qt::Key_9)}});
        CHECK(keys.at("a") == ctrl(Qt::Key_9));
        CHECK(keys.at("b").isEmpty());
    }
    SECTION("The keys of a command not registered take nothing") {
        const ShortcutMap keys =
            CommandRegistry::resolveShortcuts(defaults, {{"plugin.x", ctrl(Qt::Key_2)}});
        CHECK(keys.at("b") == ctrl(Qt::Key_2));
        CHECK(keys.count("plugin.x") == 0);
    }
}

TEST_CASE("Custom shortcuts: the commands and their actions get the user's keys",
          "[gui][shortcuts][custom]") {
    auto& registry = CommandRegistry::getInstance();
    registerCommand("test.a", ctrl(Qt::Key_1));
    registerCommand("test.b", ctrl(Qt::Key_2));
    QAction* actionA = registry.getAction(std::string("test.a"));
    QAction* actionB = registry.getAction(std::string("test.b"));
    REQUIRE(actionA != nullptr);
    REQUIRE(actionB != nullptr);
    const ChangeCounter changed;

    registry.setCustomShortcuts({{"test.b", ctrl(Qt::Key_1)}});
    CHECK(changed.count() == 1);
    CHECK(keysOf("test.b") == ctrl(Qt::Key_1));
    CHECK(keysOf("test.a").isEmpty());
    CHECK(actionB->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_1));
    CHECK(actionA->shortcut().isEmpty());
    CHECK(registry.defaultShortcut("test.a") == ctrl(Qt::Key_1));
    CHECK(registry.defaultShortcuts().size() == 2);
    CHECK(registry.customShortcuts() == ShortcutMap{{"test.b", ctrl(Qt::Key_1)}});

    registry.setCustomShortcuts({});
    CHECK(changed.count() == 2);
    CHECK(keysOf("test.a") == ctrl(Qt::Key_1));
    CHECK(keysOf("test.b") == ctrl(Qt::Key_2));
    CHECK(actionA->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_1));
    CHECK(actionB->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_2));
}

TEST_CASE("Custom shortcuts: a command registered later gets the user's keys",
          "[gui][shortcuts][custom]") {
    auto& registry = CommandRegistry::getInstance();
    registerCommand("test.a", ctrl(Qt::Key_1));
    QAction* actionA = registry.getAction(std::string("test.a"));
    REQUIRE(actionA != nullptr);
    registry.setCustomShortcuts({{"plugin.c", ctrl(Qt::Key_1)}, {"plugin.d", ctrl(Qt::Key_5)}});
    // A plugin not loaded takes nothing yet
    CHECK(keysOf("test.a") == ctrl(Qt::Key_1));

    const ChangeCounter changed;
    registerCommand("plugin.d", ctrl(Qt::Key_6));
    CHECK(keysOf("plugin.d") == ctrl(Qt::Key_5));
    CHECK(changed.count() == 0);  // only its own keys

    // The plugin's command takes keys the user gave it from the command that has them
    registerCommand("plugin.c", KeyboardShortcut());
    CHECK(keysOf("plugin.c") == ctrl(Qt::Key_1));
    CHECK(keysOf("test.a").isEmpty());
    CHECK(actionA->shortcut().isEmpty());
    CHECK(changed.count() == 1);

    // ...and gives them back when it goes
    registry.unregisterCommand("plugin.c");
    CHECK(keysOf("test.a") == ctrl(Qt::Key_1));
    CHECK(actionA->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_1));
    CHECK(changed.count() == 2);
}

TEST_CASE("Custom shortcuts: the settings keep the keys in the portable form",
          "[gui][shortcuts][custom]") {
    const ShortcutMap custom = {
        {"file.close", KeyboardShortcut(Qt::Key_F, Qt::ControlModifier | Qt::ShiftModifier)},
        {"view.focus", KeyboardShortcut()}};
    const nlohmann::json json = shortcutsToJson(custom);
    CHECK(json == nlohmann::json({{"file.close", "Ctrl+Shift+F"}, {"view.focus", ""}}));
    CHECK(shortcutsFromJson(json) == custom);

    // Keys of several presses, other values and words that are no keys cannot be read
    QStringList unreadable;
    const ShortcutMap read = shortcutsFromJson(
        {{"a", 5}, {"b", "Ctrl+K, Ctrl+C"}, {"c", "Ctrl+Nokey"}, {"d", "F12"}}, &unreadable);
    CHECK(read == ShortcutMap{{"d", KeyboardShortcut(Qt::Key_F12)}});
    CHECK(unreadable == QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
    CHECK(shortcutsFromJson(nlohmann::json::array()).empty());
}

TEST_CASE("Custom shortcuts: keys a command cannot have are left out", "[gui][shortcuts][custom]") {
    const ShortcutRules rules(ShortcutPlatform::Windows);
    QStringList dropped;
    const ShortcutMap valid = validShortcuts(
        {{"a.typing", KeyboardShortcut(Qt::Key_K)},
         {"b.altgr", KeyboardShortcut(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier)},
         {"c.first", ctrl(Qt::Key_K)},
         {"d.second", ctrl(Qt::Key_K)},
         {"e.none", KeyboardShortcut()}},
        rules, &dropped);
    CHECK(valid == ShortcutMap{{"c.first", ctrl(Qt::Key_K)}, {"e.none", KeyboardShortcut()}});
    CHECK(dropped.size() == 3);
}

TEST_CASE("Custom shortcuts: the program reads the user's keys from the settings",
          "[gui][shortcuts][custom]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    CHECK(settings.get<nlohmann::json>(SHORTCUTS_SETTING) == nlohmann::json::object());
    CHECK(loadCustomShortcuts().empty());

    settings.set<nlohmann::json>(SHORTCUTS_SETTING,
                                 {{"file.save", "Ctrl+Shift+F12"}, {"file.close", "K"}});
    CHECK(loadCustomShortcuts() ==
          ShortcutMap{{"file.save", KeyboardShortcut(Qt::Key_F12,
                                                     Qt::ControlModifier | Qt::ShiftModifier)}});
}

TEST_CASE("Custom shortcuts: the file of Export", "[gui][shortcuts][custom]") {
    const nlohmann::json file = shortcutsFile({{"file.close", ctrl(Qt::Key_W)}});
    CHECK(file.at("format") == "kalahari-keyboard-shortcuts");
    CHECK(file.at("version") == 1);
    CHECK(shortcutsOfFile(file) == nlohmann::json({{"file.close", "Ctrl+W"}}));

    // An object of command ids and keys, as the settings keep it, is read as well
    const nlohmann::json bare = {{"file.close", "Ctrl+W"}};
    CHECK(shortcutsOfFile(bare) == bare);
    CHECK_FALSE(shortcutsOfFile(nlohmann::json::array()).has_value());
    CHECK_FALSE(shortcutsOfFile({{"shortcuts", 5}}).has_value());
    CHECK_FALSE(shortcutsOfFile({{"theme", {{"name", "Dark"}}}}).has_value());
}

TEST_CASE("Custom shortcuts: Help > Keyboard Shortcuts opens them", "[gui][shortcuts][custom]") {
    bool opened = false;
    CommandCallbacks callbacks;
    callbacks.onKeyboardShortcuts = [&opened]() { opened = true; };
    registerAllCommands(callbacks);
    CHECK(CommandRegistry::getInstance().executeCommand("help.shortcuts") ==
          CommandExecutionResult::Success);
    CHECK(opened);
}

TEST_CASE("Custom shortcuts: the texts that name the keys follow them", "[gui][shortcuts][custom]") {
    registerAllCommands(CommandCallbacks{});
    auto& registry = CommandRegistry::getInstance();
    const auto textsOf = [](const QWidget& widget) {
        QStringList texts;
        for (const QLabel* label : widget.findChildren<QLabel*>()) {
            texts.append(label->text());
        }
        return texts.join(QLatin1Char('\n'));
    };
    const auto native = [](const KeyboardShortcut& keys) {
        return keys.toQKeySequence().toString(QKeySequence::NativeText);
    };

    DashboardPanel dashboard;
    EditorGeneralPage editorPage;
    const KeyboardShortcut newBook = keysOf("file.new.project");
    const KeyboardShortcut typewriter = keysOf("view.typewriter");
    REQUIRE_FALSE(newBook.isEmpty());
    REQUIRE_FALSE(typewriter.isEmpty());
    // The Dashboard writes the keys in bold before the command
    CHECK(textsOf(dashboard).contains(QStringLiteral(">%1<").arg(native(newBook))));
    CHECK(textsOf(editorPage).contains(QStringLiteral("(%1)").arg(native(typewriter))));

    const KeyboardShortcut userKeys(Qt::Key_F12, Qt::ControlModifier | Qt::ShiftModifier);
    registry.setCustomShortcuts({{"file.new.project", userKeys}, {"view.typewriter", {}}});
    CHECK(textsOf(dashboard).contains(QStringLiteral(">%1<").arg(native(userKeys))));
    CHECK_FALSE(textsOf(dashboard).contains(QStringLiteral(">%1<").arg(native(newBook))));
    CHECK(textsOf(editorPage).contains(QStringLiteral("View > Typewriter Scrolling keeps")));

    // Without keys for any of its commands the Dashboard leaves the section out
    registry.setCustomShortcuts(
        {{"file.new.project", {}}, {"file.open", {}}, {"file.new", {}}});
    auto* section = dashboard.findChild<QFrame*>(QStringLiteral("shortcutsFrame"));
    REQUIRE(section != nullptr);
    CHECK(section->isHidden());
    registry.setCustomShortcuts({});
    CHECK_FALSE(section->isHidden());
}
