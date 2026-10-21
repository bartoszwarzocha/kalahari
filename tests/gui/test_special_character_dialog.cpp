/// @file test_special_character_dialog.cpp
/// @brief Insert > Special Character...: the window in its two views, the favorites and the
///        characters used last, the keys of a character and a replacement of auto-correct

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/auto_correct_replacements.h"
#include "kalahari/gui/character_commands.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/character_dialogs.h"
#include "kalahari/gui/dialogs/special_character_dialog.h"
#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/special_character_choices.h"
#include "kalahari/gui/special_character_coordinator.h"

#include <QAbstractButton>
#include <QAction>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QTreeWidget>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;
using kalahari::gui::dialogs::CharacterTarget;
using kalahari::gui::dialogs::SpecialCharacterDialog;

namespace {

/// A window that can insert, with no keys and no auto-correct of its own
std::unique_ptr<SpecialCharacterDialog> newDialog(const QString& whyNot = QString())
{
    return std::make_unique<SpecialCharacterDialog>(
        [whyNot]() { return CharacterTarget{whyNot, QString()}; },
        [](char32_t code) { return code == U'—' ? QStringLiteral("Alt+_") : QString(); },
        [](char32_t code) {
            return code == U'©' ? QStringList{QStringLiteral("(c)")} : QStringList();
        });
}

/// A widget of the window by its type and accessible name, shown in the view in front
template <typename Widget>
Widget* shownChild(const QWidget& dialog, const QString& name)
{
    for (Widget* widget : dialog.findChildren<Widget*>()) {
        if (widget->accessibleName() == name && widget->isVisibleTo(&dialog)) {
            return widget;
        }
    }
    return nullptr;
}

/// The characters of a grid, as they are shown
QList<char32_t> charactersOfGrid(const QListWidget& grid)
{
    QList<char32_t> characters;
    for (int row = 0; row < grid.count(); ++row) {
        characters.append(static_cast<char32_t>(grid.item(row)->data(Qt::UserRole).toUInt()));
    }
    return characters;
}

/// Press a key in a widget
void press(QWidget* widget, Qt::Key key)
{
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

/// A button of the window by its text
QAbstractButton* buttonOf(const QWidget& dialog, const QString& text)
{
    for (QAbstractButton* button : dialog.findChildren<QAbstractButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

/// Click a cell of a grid with the left button
void clickCell(QListWidget& grid, int row)
{
    const QPoint at = grid.visualItemRect(grid.item(row)).center();
    const QPointF global = grid.viewport()->mapToGlobal(QPointF(at));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), global, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(grid.viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(at), global, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(grid.viewport(), &release);
}

}  // namespace

TEST_CASE("Special character window: the grid, the search, Insert",
          "[gui][special-characters][dialog]") {
    auto dialog = newDialog();
    dialog->show();
    QList<char32_t> inserted;
    QObject::connect(dialog.get(), &SpecialCharacterDialog::insertRequested,
                     [&inserted](char32_t code) { inserted.append(code); });

    // At first the favorites, the first one selected
    CHECK(dialog->view() == SpecialCharacterDialog::View::Grid);
    const QList<char32_t> favorites = favoriteCharacters();
    REQUIRE(favorites == QList<char32_t>{U'—', U'–', U'…', U'„', U'”', U'⁂', U'❦', U'→',
                                          U'°', U'§'});
    auto* grid = shownChild<QListWidget>(*dialog, QStringLiteral("Characters"));
    REQUIRE(grid != nullptr);
    CHECK(charactersOfGrid(*grid) == favorites);
    CHECK(dialog->currentCharacter() == U'—');
    auto* groups = shownChild<QListWidget>(*dialog, QStringLiteral("Groups"));
    REQUIRE(groups != nullptr);
    REQUIRE(groups->currentItem() != nullptr);
    CHECK(groups->currentItem()->text() == QStringLiteral("Favorites"));

    SECTION("A group") {
        groups->setCurrentRow(3);  // Favorites, Recently Used, then the groups
        CHECK(groups->currentItem()->text() == QStringLiteral("Spaces and Hyphens"));
        CHECK(charactersOfGrid(*grid).contains(U' '));
        CHECK(dialog->currentCharacter() == U' ');
    }

    SECTION("Found by a name, and Insert") {
        dialog->setSearchText(QStringLiteral("arrow right"));
        CHECK(groups->currentItem() == nullptr);
        CHECK(charactersOfGrid(*grid).first() == U'→');
        CHECK(dialog->currentCharacter() == U'→');
        dialog->accept();
        CHECK(inserted == QList<char32_t>{U'→'});
        CHECK(dialog->isVisible());  // stays open, as the setting says at first
    }

    SECTION("Up and Down in the search move the selection") {
        auto* search = shownChild<QLineEdit>(*dialog, QStringLiteral("Find a character"));
        REQUIRE(search != nullptr);
        press(search, Qt::Key_Down);
        CHECK(dialog->currentCharacter() == U'–');
        press(search, Qt::Key_Down);
        press(search, Qt::Key_Up);
        CHECK(dialog->currentCharacter() == U'–');
    }

    SECTION("A code finds a character of no group") {
        dialog->setSearchText(QStringLiteral("U+2603"));
        CHECK(charactersOfGrid(*grid) == QList<char32_t>{U'☃'});
        dialog->accept();
        CHECK(inserted == QList<char32_t>{U'☃'});
    }

    SECTION("Nothing found: nothing to insert") {
        dialog->setSearchText(QStringLiteral("nothing like this"));
        CHECK(grid->count() == 0);
        CHECK(dialog->currentCharacter() == 0);
        CHECK_FALSE(dialog->acceptButton()->isEnabled());
        dialog->accept();
        CHECK(inserted.isEmpty());
    }

    SECTION("Closed after Insert, when the window is not to stay open") {
        auto* keepOpen = dialog->findChild<QCheckBox*>();
        REQUIRE(keepOpen != nullptr);
        keepOpen->setChecked(false);
        CHECK_FALSE(core::SettingsManager::getInstance().get<bool>(
            KEEP_CHARACTER_WINDOW_OPEN_SETTING));
        dialog->accept();
        CHECK(inserted == QList<char32_t>{U'—'});
        CHECK_FALSE(dialog->isVisible());
        CHECK(dialog->result() == QDialog::Accepted);
    }
}

TEST_CASE("Special character window: the details of the character",
          "[gui][special-characters][dialog]") {
    auto dialog = newDialog();
    dialog->show();
    const auto shownTexts = [&dialog]() {
        QStringList texts;
        for (const QLabel* label : dialog->findChildren<QLabel*>()) {
            if (label->isVisibleTo(dialog.get()) && !label->text().isEmpty()) {
                texts.append(label->text());
            }
        }
        return texts;
    };

    dialog->selectCharacter(U'—');
    QStringList texts = shownTexts();
    CHECK(texts.contains(QStringLiteral("Em dash")));
    CHECK(texts.contains(QStringLiteral("Code: U+2014")));
    CHECK(texts.contains(QStringLiteral("Shortcut: Alt+_")));

    dialog->selectCharacter(U'©');
    texts = shownTexts();
    CHECK(texts.contains(QStringLiteral("Copyright sign")));
    CHECK(texts.contains(QStringLiteral("Auto-correct: <code>(c)</code>")));
    CHECK_FALSE(texts.contains(QStringLiteral("Shortcut: Alt+_")));

    // A code found selects its character
    dialog->setSearchText(QStringLiteral("2192"));
    CHECK(dialog->currentCharacter() == U'→');
    CHECK(shownTexts().contains(QStringLiteral("Rightwards arrow")));
}

TEST_CASE("Special character window: the list, the same search and character",
          "[gui][special-characters][dialog]") {
    auto dialog = newDialog();
    dialog->show();
    dialog->setSearchText(QStringLiteral("dash"));
    dialog->selectCharacter(U'–');

    dialog->setView(SpecialCharacterDialog::View::List);
    auto* search = shownChild<QLineEdit>(*dialog, QStringLiteral("Find a character"));
    REQUIRE(search != nullptr);
    CHECK(search->text() == QStringLiteral("dash"));
    CHECK(dialog->currentCharacter() == U'–');
    auto* list = shownChild<QTreeWidget>(*dialog, QStringLiteral("Characters"));
    REQUIRE(list != nullptr);
    REQUIRE(list->currentItem() != nullptr);
    CHECK(list->currentItem()->text(1) == QStringLiteral("En dash"));
    CHECK(list->currentItem()->text(2) == QStringLiteral("U+2013"));

    // The em dash with its keys, and a star of a favorite
    QTreeWidgetItem* emDash = nullptr;
    for (int row = 0; row < list->topLevelItemCount(); ++row) {
        if (list->topLevelItem(row)->text(2) == QStringLiteral("U+2014")) {
            emDash = list->topLevelItem(row);
        }
    }
    REQUIRE(emDash != nullptr);
    CHECK(emDash->text(3) == QStringLiteral("Alt+_"));
    CHECK(emDash->text(4) == QStringLiteral("★"));

    // All the characters, without the search; a group by its button
    dialog->setSearchText(QString());
    const int all = list->topLevelItemCount();
    CHECK(all > 300);
    QAbstractButton* arrows = nullptr;
    for (QAbstractButton* button : dialog->findChildren<QAbstractButton*>()) {
        if (button->text() == QStringLiteral("Arrows")) {
            arrows = button;
        }
    }
    REQUIRE(arrows != nullptr);
    arrows->click();
    CHECK(list->topLevelItemCount() < all);
    CHECK(list->topLevelItem(0)->text(0) == QStringLiteral("←"));
    CHECK(dialog->currentCharacter() == U'←');

    // Back to the grid: the same character
    dialog->setView(SpecialCharacterDialog::View::Grid);
    CHECK(dialog->currentCharacter() == U'←');
}

TEST_CASE("Special character window: the favorites", "[gui][special-characters][dialog]") {
    auto dialog = newDialog();
    dialog->show();
    dialog->setSearchText(QStringLiteral("U+00A9"));
    REQUIRE(dialog->currentCharacter() == U'©');
    CHECK_FALSE(isFavoriteCharacter(U'©'));

    QPushButton* favorite = nullptr;
    for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
        if (button->isVisibleTo(dialog.get()) &&
            button->text() == QStringLiteral("Add to &Favorites")) {
            favorite = button;
        }
    }
    REQUIRE(favorite != nullptr);
    favorite->click();
    CHECK(isFavoriteCharacter(U'©'));
    CHECK(favoriteCharacters().last() == U'©');
    CHECK(favorite->text() == QStringLiteral("Remove from &Favorites"));
    favorite->click();
    CHECK_FALSE(isFavoriteCharacter(U'©'));
}

TEST_CASE("Special character window: Grid and List, the view shown last opens next time",
          "[gui][special-characters][dialog]") {
    auto dialog = newDialog();
    dialog->show();
    QAbstractButton* grid = buttonOf(*dialog, QStringLiteral("&Grid"));
    QAbstractButton* list = buttonOf(*dialog, QStringLiteral("&List"));
    REQUIRE(grid != nullptr);
    REQUIRE(list != nullptr);
    CHECK(grid->isChecked());
    CHECK_FALSE(list->isChecked());
#ifndef Q_OS_MACOS
    // Alt and the letter, as the tooltips say
    CHECK(grid->shortcut() == QKeySequence(Qt::AltModifier | Qt::Key_G));
    CHECK(list->shortcut() == QKeySequence(Qt::AltModifier | Qt::Key_L));
    CHECK(list->toolTip().endsWith(QStringLiteral("(Alt+L)")));
#endif

    // One search over both views
    dialog->setSearchText(QStringLiteral("arrow"));
    list->click();
    CHECK(dialog->view() == SpecialCharacterDialog::View::List);
    CHECK(list->isChecked());
    CHECK_FALSE(grid->isChecked());
    auto* search = shownChild<QLineEdit>(*dialog, QStringLiteral("Find a character"));
    REQUIRE(search != nullptr);
    CHECK(search->text() == QStringLiteral("arrow"));
    CHECK(core::SettingsManager::getInstance().get<std::string>(
              CHARACTER_WINDOW_VIEW_SETTING) == "list");

    // The next window opens in the list, then in the grid again
    CHECK(newDialog()->view() == SpecialCharacterDialog::View::List);
    grid->click();
    CHECK(dialog->view() == SpecialCharacterDialog::View::Grid);
    CHECK(core::SettingsManager::getInstance().get<std::string>(
              CHARACTER_WINDOW_VIEW_SETTING) == "grid");
    CHECK(newDialog()->view() == SpecialCharacterDialog::View::Grid);
}

TEST_CASE("Special character window: the menu of a character",
          "[gui][special-characters][dialog]") {
    auto dialog = newDialog();
    dialog->show();
    QList<char32_t> inserted;
    QList<char32_t> keys;
    QList<char32_t> replacements;
    QObject::connect(dialog.get(), &SpecialCharacterDialog::insertRequested,
                     [&inserted](char32_t code) { inserted.append(code); });
    QObject::connect(dialog.get(), &SpecialCharacterDialog::shortcutRequested,
                     [&keys](char32_t code) { keys.append(code); });
    QObject::connect(dialog.get(), &SpecialCharacterDialog::replacementRequested,
                     [&replacements](char32_t code) { replacements.append(code); });

    // The menu of a character not among the favorites
    dialog->selectCharacter(U'©');
    auto* menu = dialog->findChild<QMenu*>();
    REQUIRE(menu != nullptr);
    QList<QAction*> actions;
    QStringList texts;
    for (QAction* action : menu->actions()) {
        if (!action->isSeparator()) {
            actions.append(action);
            texts.append(action->text());
        }
    }
    REQUIRE(texts == QStringList{QStringLiteral("&Insert"), QStringLiteral("Add to &Favorites"),
                                 QStringLiteral("Assign &Shortcut..."),
                                 QStringLiteral("Add &Replacement...")});
    CHECK(menu->defaultAction() == actions.at(0));

    // The right button on a cell: that character, and its menu
    dialog->setSearchText(QStringLiteral("arrow"));
    auto* grid = shownChild<QListWidget>(*dialog, QStringLiteral("Characters"));
    REQUIRE(grid != nullptr);
    REQUIRE(grid->count() > 1);
    const char32_t second = static_cast<char32_t>(grid->item(1)->data(Qt::UserRole).toUInt());
    const QPoint at = grid->visualItemRect(grid->item(1)).center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, at, grid->viewport()->mapToGlobal(at));
    QCoreApplication::sendEvent(grid->viewport(), &event);
    CHECK(dialog->currentCharacter() == second);
    CHECK(menu->isVisible());
    menu->hide();

    dialog->setSearchText(QString());
    dialog->selectCharacter(U'©');
    actions.at(1)->trigger();
    CHECK(isFavoriteCharacter(U'©'));
    CHECK(actions.at(1)->text() == QStringLiteral("Remove from &Favorites"));
    actions.at(2)->trigger();
    actions.at(3)->trigger();
    actions.at(0)->trigger();
    CHECK(keys == QList<char32_t>{U'©'});
    CHECK(replacements == QList<char32_t>{U'©'});
    CHECK(inserted == QList<char32_t>{U'©'});
}

TEST_CASE("Special character window: a click on a character used last inserts it",
          "[gui][special-characters][dialog]") {
    addRecentCharacter(U'§');
    addRecentCharacter(U'→');
    auto dialog = newDialog();
    dialog->show();
    QList<char32_t> inserted;
    QObject::connect(dialog.get(), &SpecialCharacterDialog::insertRequested,
                     [&inserted](char32_t code) { inserted.append(code); });
    auto* recent = shownChild<QListWidget>(*dialog, QStringLiteral("Recently used"));
    REQUIRE(recent != nullptr);
    REQUIRE(charactersOfGrid(*recent) == QList<char32_t>{U'→', U'§'});

    clickCell(*recent, 1);
    CHECK(inserted == QList<char32_t>{U'§'});
    CHECK(dialog->currentCharacter() == U'§');
    // The second click of a double click inserts nothing more
    clickCell(*recent, 1);
    CHECK(inserted == QList<char32_t>{U'§'});
}

TEST_CASE("Special character window: Insert greyed out, saying why",
          "[gui][special-characters][dialog]") {
    auto dialog = newDialog(QStringLiteral("The book is open read-only"));
    QList<char32_t> inserted;
    QObject::connect(dialog.get(), &SpecialCharacterDialog::insertRequested,
                     [&inserted](char32_t code) { inserted.append(code); });
    dialog->show();
    CHECK_FALSE(dialog->acceptButton()->isEnabled());
    CHECK(dialog->acceptButton()->toolTip() == QStringLiteral("The book is open read-only"));
    bool said = false;
    for (const QLabel* label : dialog->findChildren<QLabel*>()) {
        said = said ||
               (label->isVisibleTo(dialog.get()) &&
                label->text() == QStringLiteral("Insert is greyed out: The book is open read-only"));
    }
    CHECK(said);
    dialog->accept();
    CHECK(inserted.isEmpty());
    CHECK(dialog->isVisible());
}

TEST_CASE("Special characters: the favorites and those used last in the settings",
          "[gui][special-characters]") {
    setFavoriteCharacter(U'—', false);
    CHECK_FALSE(favoriteCharacters().contains(U'—'));
    setFavoriteCharacter(U'—', true);
    CHECK(favoriteCharacters().last() == U'—');

    CHECK(recentCharacters().isEmpty());
    addRecentCharacter(U'a');
    addRecentCharacter(U'b');
    addRecentCharacter(U'a');
    CHECK(recentCharacters() == QList<char32_t>{U'a', U'b'});
    for (char32_t code = U'c'; code < U'c' + 20; ++code) {
        addRecentCharacter(code);
    }
    CHECK(recentCharacters().size() == RECENT_CHARACTERS);
    CHECK(recentCharacters().first() == U'c' + 19);

    // A setting edited by hand: only characters, each once
    core::SettingsManager::getInstance().set<nlohmann::json>(
        FAVORITE_CHARACTERS_SETTING, nlohmann::json::array({"→", "word", 5, "→", "€"}));
    CHECK(favoriteCharacters() == QList<char32_t>{U'→', U'€'});
}

TEST_CASE("Special characters: the commands of the characters", "[gui][special-characters]") {
    CHECK(characterCommandId(U'—') == "insert.emDash");
    CHECK(characterCommandId(U' ') == "insert.noBreakSpace");
    CHECK(characterCommandId(U'→') == "insert.character.2192");
    CHECK(characterCommandId(U'é') == "insert.character.00E9");
    CHECK(characterCommandId(U'😀') == "insert.character.1F600");

    CHECK(characterOfCommand("insert.character.2192") == U'→');
    CHECK(characterOfCommand("insert.emDash") == U'—');
    CHECK_FALSE(characterOfCommand("insert.character.2014"));  // the em dash has its own
    CHECK_FALSE(characterOfCommand("insert.character.e9"));
    CHECK_FALSE(characterOfCommand("insert.character.xyz"));
    CHECK_FALSE(characterOfCommand("edit.copy"));
    CHECK(isCharacterCommand("insert.character.2192"));
    CHECK_FALSE(isCharacterCommand("insert.emDash"));

    QStringList inserted;
    const InsertCharacters insert = [&inserted](const QString& text) { inserted.append(text); };
    CHECK(registerCharacterCommand(U'→', insert));
    CHECK_FALSE(registerCharacterCommand(U'→', insert));
    const Command* command = CommandRegistry::getInstance().getCommand("insert.character.2192");
    REQUIRE(command != nullptr);
    CHECK(command->label == "Rightwards arrow (→)");
    CHECK_FALSE(command->showInMenu);
    command->execute();
    CHECK(inserted == QStringList{QStringLiteral("→")});
}

TEST_CASE("Special characters: keys given to a character", "[gui][special-characters][shortcuts]") {
    registerAllCommands(CommandCallbacks{});
    QMainWindow window;
    QStringList inserted;
    std::vector<std::string> textCommands;
    SpecialCharacterCoordinator coordinator(
        &window, window.statusBar(), []() -> editor::BookEditor* { return nullptr; },
        [&inserted](const QString& text) { inserted.append(text); },
        [&textCommands](const std::vector<std::string>& ids) {
            textCommands.insert(textCommands.end(), ids.begin(), ids.end());
        });
    coordinator.connectCommands();
    CHECK(textCommands.empty());

    // A character of no command: a command of its own, with the keys, in the settings
    const KeyboardShortcut keys(Qt::Key_F7, Qt::ControlModifier);
    coordinator.setCharacterKeys(U'→', keys);
    CHECK(textCommands == std::vector<std::string>{"insert.character.2192"});
    QAction* action =
        CommandRegistry::getInstance().getAction(std::string("insert.character.2192"));
    REQUIRE(action != nullptr);
    CHECK(action->shortcut() == keys.toQKeySequence());
    CHECK(window.actions().contains(action));
    CHECK(SpecialCharacterCoordinator::keysOf(U'→') == QStringLiteral("Ctrl+F7"));
    const auto stored =
        core::SettingsManager::getInstance().get<nlohmann::json>(SHORTCUTS_SETTING);
    CHECK(stored.value("insert.character.2192", std::string()) == "Ctrl+F7");
    action->trigger();
    CHECK(inserted == QStringList{QStringLiteral("→")});

    // Keys of another command (Insert > Annotation..., the same keys on every system): it is
    // left without them
    const KeyboardShortcut annotationKeys =
        CommandRegistry::getInstance().defaultShortcut("insert.annotation");
    REQUIRE_FALSE(annotationKeys.isEmpty());
    coordinator.setCharacterKeys(U'←', annotationKeys);
    CHECK(CommandRegistry::getInstance()
              .getAction(std::string("insert.annotation"))
              ->shortcut()
              .isEmpty());
    CHECK(CommandRegistry::getInstance()
              .getAction(std::string("insert.character.2190"))
              ->shortcut() == annotationKeys.toQKeySequence());

    // A character of Insert > Formatting Character: the keys of its command
    coordinator.setCharacterKeys(U'­', KeyboardShortcut(Qt::Key_F8, Qt::ControlModifier));
    CHECK(CommandRegistry::getInstance()
              .getAction(std::string("insert.optionalHyphen"))
              ->shortcut() == QKeySequence(Qt::ControlModifier | Qt::Key_F8));
    CHECK_FALSE(CommandRegistry::getInstance().isCommandRegistered("insert.character.00AD"));

    // At the next start the command comes back with its keys
    CommandRegistry::getInstance().unregisterCommand("insert.character.2192");
    CommandRegistry::getInstance().setCustomShortcuts(loadCustomShortcuts());
    const std::vector<std::string> registered = registerCharacterCommands(
        [](const QString&) {});
    CHECK(std::find(registered.begin(), registered.end(), "insert.character.2192") !=
          registered.end());
    CHECK(CommandRegistry::getInstance()
              .getAction(std::string("insert.character.2192"))
              ->shortcut() == keys.toQKeySequence());

    // Taken away
    coordinator.setCharacterKeys(U'→', KeyboardShortcut());
    CHECK(CommandRegistry::getInstance()
              .getAction(std::string("insert.character.2192"))
              ->shortcut()
              .isEmpty());
}

TEST_CASE("Special characters: the window inserts in the text in front",
          "[gui][special-characters][editor]") {
    QMainWindow window;
    auto* text = new editor::BookEditor();
    window.setCentralWidget(text);
    text->fromKml(test::kmlOf({QStringLiteral("Ala")}));
    text->setCursorPosition({0, 3});
    SpecialCharacterCoordinator coordinator(
        &window, window.statusBar(), [text]() { return text; }, {}, {});

    CHECK(coordinator.whyNotInsertable().isEmpty());
    CHECK(coordinator.insert(U'—'));
    CHECK(text->plainText() == QStringLiteral("Ala—"));
    CHECK(recentCharacters() == QList<char32_t>{U'—'});
    CHECK(window.statusBar()->currentMessage() == QStringLiteral("Inserted: Em dash (—)"));

    text->setReadOnly(true);
    CHECK(coordinator.whyNotInsertable() == QStringLiteral("The book is open read-only"));
    CHECK_FALSE(coordinator.insert(U'–'));
    CHECK(text->plainText() == QStringLiteral("Ala—"));

    SpecialCharacterCoordinator noText(
        &window, nullptr, []() -> editor::BookEditor* { return nullptr; }, {}, {});
    CHECK(noText.whyNotInsertable() == QStringLiteral("No chapter or file is open"));
    CHECK_FALSE(noText.insert(U'—'));
}

TEST_CASE("Special characters: a replacement of auto-correct",
          "[gui][special-characters][autocorrect]") {
    dialogs::ReplacementDialog dialog(U'→');
    const auto note = [&dialog]() {
        QString shown;
        for (const QLabel* label : dialog.findChildren<QLabel*>()) {
            if (label->text().startsWith(QStringLiteral("The ")) ||
                label->text().startsWith(QStringLiteral("A ")) ||
                label->text().startsWith(QStringLiteral("Auto-correct replaces “"))) {
                shown = label->text();
            }
        }
        return shown;
    };
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    dialog.wordField()->setText(QStringLiteral("->"));
    CHECK(note() == QStringLiteral("The replacement table has this pair already."));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    dialog.wordField()->setText(QStringLiteral("a b"));
    CHECK(note() == QStringLiteral("A replacement is one word: without spaces."));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    dialog.wordField()->setText(QStringLiteral("=>"));
    CHECK(note() == QStringLiteral("Auto-correct replaces “<code>=&gt;</code>” with “⇒” now: "
                                   "Add puts “→” in its place."));
    CHECK(dialog.acceptButton()->isEnabled());

    dialog.wordField()->setText(QStringLiteral(" >> "));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.word() == QStringLiteral(">>"));
    addReplacement(dialog.word(), dialog.replacement());
    const std::vector<ReplacementEntry> entries = replacementEntries();
    REQUIRE_FALSE(entries.empty());
    CHECK(entries.back() == ReplacementEntry{QStringLiteral(">>"), QStringLiteral("→"), true});
    CHECK(autoCorrectSources(QStringLiteral("→")) ==
          QStringList{QStringLiteral("->"), QStringLiteral(">>")});

    // A pair turned off, given the character again: on
    std::vector<ReplacementEntry> changed = entries;
    changed.back().enabled = false;
    setReplacementEntries(changed);
    CHECK(autoCorrectSources(QStringLiteral("→")) == QStringList{QStringLiteral("->")});
    addReplacement(QStringLiteral(">>"), QStringLiteral("→"));
    CHECK(replacementEntries().back().enabled);
    CHECK(replacementEntries().size() == entries.size());

    // The en dash and the ellipsis of auto-correct itself
    CHECK(autoCorrectSources(QStringLiteral("–")) == QStringList{QStringLiteral("--")});
    CHECK(autoCorrectSources(QStringLiteral("…")) == QStringList{QStringLiteral("...")});
    core::SettingsManager::getInstance().set<bool>("editor.autoCorrect.enabled", false);
    CHECK(autoCorrectSources(QStringLiteral("…")).isEmpty());
}
