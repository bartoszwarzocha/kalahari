/// @file test_formatting_characters.cpp
/// @brief Insert > Formatting Character: the characters of the typesetting a keyboard does not
///        have, with the keys of Word

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/utils/access_keys.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QStringList>

#include <string>
#include <utility>
#include <vector>

using namespace kalahari::gui;

namespace {

/// The items of a menu as it shows them, without the access keys ("-" for a line)
QStringList itemsOf(const QMenu& menu) {
    QStringList items;
    for (const QAction* action : menu.actions()) {
        items.append(action->isSeparator() ? QStringLiteral("-")
                                           : withoutAccessKey(action->text()));
    }
    return items;
}

/// The submenu of a menu by its name, without the access key
QMenu* submenuOf(const QMenu& menu, const QString& name) {
    for (const QAction* action : menu.actions()) {
        if (action->menu() != nullptr && withoutAccessKey(action->text()) == name) {
            return action->menu();
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("Formatting characters: the submenu of the Insert menu", "[gui][characters]") {
    registerAllCommands(CommandCallbacks{});
    CommandRegistry& registry = CommandRegistry::getInstance();
    QMainWindow window;
    MenuBuilder builder;
    builder.buildMenuBar(registry, &window);
    QMenu* insert = builder.getMenu("INSERT");
    REQUIRE(insert != nullptr);

    // Right after Special Character...
    const QStringList items = itemsOf(*insert);
    const qsizetype special = items.indexOf(QStringLiteral("Special Character..."));
    REQUIRE(special >= 0);
    CHECK(items.value(special + 1) == QStringLiteral("Formatting Character"));

    QMenu* characters = submenuOf(*insert, QStringLiteral("Formatting Character"));
    REQUIRE(characters != nullptr);
    CHECK(itemsOf(*characters) == QStringList{QStringLiteral("Non-breaking Space"),
                                              QStringLiteral("Non-breaking Hyphen"),
                                              QStringLiteral("Optional Hyphen"),
                                              QStringLiteral("-"),
                                              QStringLiteral("En Dash"),
                                              QStringLiteral("Em Dash")});
}

TEST_CASE("Formatting characters: each command inserts its character", "[gui][characters]") {
    QStringList inserted;
    CommandCallbacks callbacks;
    callbacks.onInsertCharacters = [&inserted](const QString& characters) {
        inserted.append(characters);
    };
    registerAllCommands(callbacks);
    CommandRegistry& registry = CommandRegistry::getInstance();

    const std::vector<std::pair<std::string, QString>> commands = {
        {"insert.noBreakSpace", QStringLiteral("\u00A0")},
        {"insert.nonBreakingHyphen", QStringLiteral("\u2011")},
        {"insert.optionalHyphen", QStringLiteral("\u00AD")},
        {"insert.enDash", QStringLiteral("–")},
        {"insert.emDash", QStringLiteral("—")},
    };
    for (const auto& [id, character] : commands) {
        INFO(id);
        QAction* action = registry.getAction(id);
        REQUIRE(action != nullptr);
        REQUIRE(action->isEnabled());
        inserted.clear();
        action->trigger();
        CHECK(inserted == QStringList{character});
    }
}

TEST_CASE("Formatting characters: without a text to insert into they are greyed out",
          "[gui][characters]") {
    registerAllCommands(CommandCallbacks{});
    CommandRegistry& registry = CommandRegistry::getInstance();
    for (const char* id : {"insert.noBreakSpace", "insert.nonBreakingHyphen",
                           "insert.optionalHyphen", "insert.enDash", "insert.emDash"}) {
        INFO(id);
        QAction* action = registry.getAction(std::string(id));
        REQUIRE(action != nullptr);
        CHECK_FALSE(action->isEnabled());
    }
}
