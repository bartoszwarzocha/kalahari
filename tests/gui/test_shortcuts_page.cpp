/// @file test_shortcuts_page.cpp
/// @brief Settings > Keyboard Shortcuts: the list, the checks, the search, the files

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/settings/shortcuts_page.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/widgets/shortcut_recorder.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <algorithm>
#include <string>

using namespace kalahari::gui;

namespace {

constexpr Qt::KeyboardModifiers CTRL = Qt::ControlModifier;
constexpr Qt::KeyboardModifiers SHIFT = Qt::ShiftModifier;

KeyboardShortcut shortcut(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    return KeyboardShortcut(key, modifiers);
}

QString native(const KeyboardShortcut& keys) {
    return keys.toQKeySequence().toString(QKeySequence::NativeText);
}

/// The keys that close the program on this system
KeyboardShortcut exitKeys() {
    return currentShortcutPlatform() == ShortcutPlatform::MacOS
               ? shortcut(Qt::Key_Q, CTRL)
               : shortcut(Qt::Key_F4, Qt::AltModifier);
}

void addCommand(const std::string& id, const std::string& label, const std::string& path,
                int order, const KeyboardShortcut& keys, bool runs = true) {
    Command command;
    command.id = id;
    command.label = label;
    command.menuPath = path;
    command.menuOrder = order;
    command.shortcut = keys;
    if (runs) {
        command.execute = []() {};
    }
    CommandRegistry::getInstance().registerCommand(command);
}

/// A few commands of every kind: in submenus, without keys, not available yet
void registerCommands() {
    addCommand("file.open.file", "Open File...", "FILE/Open/Open File...", 5,
               shortcut(Qt::Key_O, CTRL | SHIFT));
    addCommand("file.save", "Save", "FILE/Save", 10, shortcut(Qt::Key_S, CTRL));
    addCommand("file.close", "Close Book", "FILE/Close Book", 20, shortcut(Qt::Key_F4, CTRL));
    addCommand("file.exit", "Exit", "FILE/Exit", 100, exitKeys());
    addCommand("edit.find", "Find...", "EDIT/Find...", 10, shortcut(Qt::Key_F, CTRL));
    addCommand("view.focus", "Focus", "VIEW/Focus", 10, shortcut(Qt::Key_4, CTRL));
    addCommand("view.zoomIn", "Zoom In", "VIEW/Zoom/Zoom In", 15, shortcut(Qt::Key_Plus, CTRL));
    addCommand("view.darkPaper", "Dark Paper", "VIEW/Dark Paper", 20, KeyboardShortcut());
    addCommand("help.manual", "Kalahari Help", "HELP/Kalahari Help", 10, shortcut(Qt::Key_F1),
               false);
    addCommand("help.future", "Future", "HELP/Future", 20, KeyboardShortcut(), false);
}

QTreeWidget* listOf(QWidget& page) {
    auto* list = page.findChild<QTreeWidget*>(QStringLiteral("shortcutsList"));
    REQUIRE(list != nullptr);
    return list;
}

template <typename T>
T* child(QWidget& page, const QString& name) {
    auto* widget = page.findChild<T*>(name);
    REQUIRE(widget != nullptr);
    return widget;
}

QPushButton* button(QWidget& page, const QString& text) {
    for (QPushButton* candidate : page.findChildren<QPushButton*>()) {
        if (candidate->text() == text) {
            return candidate;
        }
    }
    FAIL("No button " << text.toStdString());
    return nullptr;
}

/// The texts of the items the list shows under a group (or of its groups)
QStringList shownTexts(const QTreeWidgetItem* parent, int column = 0) {
    QStringList texts;
    for (int index = 0; index < parent->childCount(); ++index) {
        if (!parent->child(index)->isHidden()) {
            texts.append(parent->child(index)->text(column));
        }
    }
    return texts;
}

QTreeWidgetItem* groupNamed(QTreeWidget* list, const QString& text) {
    for (int index = 0; index < list->topLevelItemCount(); ++index) {
        if (list->topLevelItem(index)->text(0) == text) {
            return list->topLevelItem(index);
        }
    }
    FAIL("No group " << text.toStdString());
    return nullptr;
}

QTreeWidgetItem* itemNamed(QTreeWidget* list, const QString& text) {
    for (int top = 0; top < list->topLevelItemCount(); ++top) {
        QTreeWidgetItem* group = list->topLevelItem(top);
        for (int index = 0; index < group->childCount(); ++index) {
            if (group->child(index)->text(0) == text) {
                return group->child(index);
            }
        }
    }
    FAIL("No item " << text.toStdString());
    return nullptr;
}

bool isShown(const QTreeWidgetItem* item) {
    return !item->isHidden() && !item->parent()->isHidden();
}

QString messageOf(QWidget& page) {
    auto* message = child<QFrame>(page, QStringLiteral("shortcutMessage"));
    return message->isVisibleTo(&page)
               ? child<QLabel>(page, QStringLiteral("shortcutMessageText"))->text()
               : QString();
}

void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &event);
}

}  // namespace

TEST_CASE("Shortcuts page: the commands as the menus show them, then the fixed keys",
          "[gui][shortcuts][page]") {
    registerCommands();
    ShortcutsPage page;
    page.load();
    QTreeWidget* list = listOf(page);

    QStringList groups;
    for (int index = 0; index < list->topLevelItemCount(); ++index) {
        if (!list->topLevelItem(index)->isHidden()) {
            groups.append(list->topLevelItem(index)->text(0));
        }
    }
    REQUIRE(groups.size() > 6);
    CHECK(groups.mid(0, 4) == QStringList{QStringLiteral("File"), QStringLiteral("Edit"),
                                          QStringLiteral("View"), QStringLiteral("Help")});
    CHECK(groups.at(4) == QStringLiteral("In the text – fixed keys"));
    CHECK(groups.last().endsWith(QStringLiteral(" – keys of the system")));

    // In the menus' order, a submenu where its first command is
    CHECK(shownTexts(groupNamed(list, QStringLiteral("File"))) ==
          QStringList{QStringLiteral("Open › Open File..."), QStringLiteral("Save"),
                      QStringLiteral("Close Book"), QStringLiteral("Exit")});
    CHECK(shownTexts(groupNamed(list, QStringLiteral("File")), 1) ==
          QStringList{native(shortcut(Qt::Key_O, CTRL | SHIFT)), native(shortcut(Qt::Key_S, CTRL)),
                      native(shortcut(Qt::Key_F4, CTRL)), native(exitKeys())});
    CHECK(shownTexts(groupNamed(list, QStringLiteral("View"))) ==
          QStringList{QStringLiteral("Focus"), QStringLiteral("Zoom › Zoom In"),
                      QStringLiteral("Dark Paper")});
    // A command not available yet is there for the keys kept for it, the others are not
    CHECK(shownTexts(groupNamed(list, QStringLiteral("Help"))) ==
          QStringList{QStringLiteral("Kalahari Help (not available yet)")});

    // The fixed keys are listed with what they do
    CHECK(shownTexts(groupNamed(list, QStringLiteral("Find bar – fixed keys")))
              .contains(QStringLiteral("Close the bar")));
}

TEST_CASE("Shortcuts page: new keys stay on the page until Apply", "[gui][shortcuts][page]") {
    registerCommands();
    auto& settings = kalahari::core::SettingsManager::getInstance();
    ShortcutsPage page;
    page.load();
    CHECK_FALSE(page.isChanged());

    REQUIRE(page.selectCommand("file.close"));
    page.assignKeys(QKeyCombination(CTRL | SHIFT, Qt::Key_K));
    const KeyboardShortcut newKeys = shortcut(Qt::Key_K, CTRL | SHIFT);
    CHECK(page.shortcutOf("file.close") == newKeys);
    CHECK(messageOf(page).isEmpty());
    CHECK(page.isChanged());

    // The list marks the change and names the default
    QTreeWidgetItem* item = itemNamed(listOf(page), QStringLiteral("Close Book"));
    CHECK(item->text(1) == QStringLiteral("● ") + native(newKeys));
    CHECK(item->font(1).bold());
    CHECK(item->toolTip(1) == QStringLiteral("Default: ") + native(shortcut(Qt::Key_F4, CTRL)));

    // The command keeps its keys until Apply
    CHECK(CommandRegistry::getInstance().getCommand("file.close")->shortcut ==
          shortcut(Qt::Key_F4, CTRL));
    CHECK(page.apply() == std::vector<std::string>{SHORTCUTS_SETTING});
    CHECK(settings.get<nlohmann::json>(SHORTCUTS_SETTING) ==
          nlohmann::json({{"file.close", "Ctrl+Shift+K"}}));
    // What the program does after Apply (SettingsCoordinator)
    CommandRegistry::getInstance().setCustomShortcuts(loadCustomShortcuts());
    CHECK(CommandRegistry::getInstance().getCommand("file.close")->shortcut == newKeys);

    // The default keys again are no change of the command
    page.assignKeys(QKeyCombination(CTRL, Qt::Key_F4));
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));
    CHECK(item->text(1) == native(shortcut(Qt::Key_F4, CTRL)));
    CHECK_FALSE(item->font(1).bold());
    page.apply();
    CHECK(settings.get<nlohmann::json>(SHORTCUTS_SETTING) == nlohmann::json::object());
}

TEST_CASE("Shortcuts page: keys of another command are taken only when the user says so",
          "[gui][shortcuts][page]") {
    registerCommands();
    ShortcutsPage page;
    page.load();
    auto* accept = child<QPushButton>(page, QStringLiteral("shortcutMessageAccept"));
    auto* cancel = child<QPushButton>(page, QStringLiteral("shortcutMessageCancel"));

    REQUIRE(page.selectCommand("file.close"));
    page.assignKeys(QKeyCombination(CTRL, Qt::Key_S));
    CHECK(messageOf(page).contains(QStringLiteral("<b>File › Save</b>")));
    CHECK(accept->isVisibleTo(&page));
    CHECK(accept->text() == QStringLiteral("Assi&gn Anyway"));
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));

    accept->click();
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_S, CTRL));
    CHECK(page.shortcutOf("file.save").isEmpty());
    CHECK(messageOf(page).contains(QStringLiteral("is now without a shortcut")));
    CHECK_FALSE(accept->isVisibleTo(&page));
    CHECK(itemNamed(listOf(page), QStringLiteral("Save"))->text(1) == QStringLiteral("● none"));

    // Cancel leaves both commands as they were
    REQUIRE(page.selectCommand("view.focus"));
    page.assignKeys(QKeyCombination(CTRL, Qt::Key_S));
    REQUIRE(cancel->isVisibleTo(&page));
    cancel->click();
    CHECK(messageOf(page).isEmpty());
    CHECK(page.shortcutOf("view.focus") == shortcut(Qt::Key_4, CTRL));
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_S, CTRL));
}

TEST_CASE("Shortcuts page: keys no command can have are refused with the reason",
          "[gui][shortcuts][page]") {
    registerCommands();
    ShortcutsPage page;
    page.load();
    REQUIRE(page.selectCommand("file.close"));

    page.assignKeys(QKeyCombination(CTRL, Qt::Key_Backspace));
    CHECK(messageOf(page).contains(QStringLiteral("fixed key")));
    CHECK_FALSE(
        child<QPushButton>(page, QStringLiteral("shortcutMessageAccept"))->isVisibleTo(&page));
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));

    page.assignKeys(QKeyCombination(Qt::NoModifier, Qt::Key_K));
    CHECK(messageOf(page).contains(QStringLiteral("types text")));

    // The keys that close the window are the system's: only Exit may have them
    page.assignKeys(exitKeys().toQKeySequence()[0]);
    CHECK(messageOf(page).contains(QStringLiteral("Exit")));
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));
    REQUIRE(page.selectCommand("file.exit"));
    page.assignKeys(QKeyCombination(CTRL | SHIFT, Qt::Key_K));
    page.assignKeys(exitKeys().toQKeySequence()[0]);
    CHECK(page.shortcutOf("file.exit") == exitKeys());
    CHECK_FALSE(page.isChanged());

    if (currentShortcutPlatform() == ShortcutPlatform::Linux) {
        // A key of the desktops: the user decides
        REQUIRE(page.selectCommand("file.close"));
        page.assignKeys(QKeyCombination(CTRL, Qt::Key_F2));
        CHECK(messageOf(page).contains(QStringLiteral("KDE and Xfce")));
        child<QPushButton>(page, QStringLiteral("shortcutMessageAccept"))->click();
        CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F2, CTRL));
    }
}

TEST_CASE("Shortcuts page: remove, restore the default, restore all", "[gui][shortcuts][page]") {
    registerCommands();
    ShortcutsPage page;
    page.load();
    QPushButton* remove = button(page, QStringLiteral("&Remove"));
    QPushButton* restore = button(page, QStringLiteral("Restore &Default"));
    QPushButton* restoreAll = button(page, QStringLiteral("Restore &All Defaults"));
    QPushButton* exportButton = button(page, QStringLiteral("&Export..."));
    CHECK_FALSE(restoreAll->isEnabled());
    CHECK_FALSE(exportButton->isEnabled());

    REQUIRE(page.selectCommand("file.save"));
    CHECK_FALSE(restore->isEnabled());
    remove->click();
    CHECK(page.shortcutOf("file.save").isEmpty());
    CHECK_FALSE(remove->isEnabled());
    CHECK(restore->isEnabled());
    CHECK(restoreAll->isEnabled());
    CHECK(exportButton->isEnabled());
    restore->click();
    CHECK(page.shortcutOf("file.save") == shortcut(Qt::Key_S, CTRL));
    CHECK_FALSE(page.isChanged());

    // The default keys another command has now: the user decides
    REQUIRE(page.selectCommand("file.close"));
    page.assignKeys(QKeyCombination(CTRL, Qt::Key_S));
    child<QPushButton>(page, QStringLiteral("shortcutMessageAccept"))->click();
    REQUIRE(page.selectCommand("file.save"));
    restore->click();
    CHECK(messageOf(page).contains(QStringLiteral("<b>File › Close Book</b>")));
    auto* accept = child<QPushButton>(page, QStringLiteral("shortcutMessageAccept"));
    CHECK(accept->text() == QStringLiteral("Restore A&nyway"));
    accept->click();
    CHECK(page.shortcutOf("file.save") == shortcut(Qt::Key_S, CTRL));
    CHECK(page.shortcutOf("file.close").isEmpty());

    restoreAll->click();
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));
    CHECK(page.shortcutOf("file.save") == shortcut(Qt::Key_S, CTRL));
    CHECK(messageOf(page).contains(QStringLiteral("default shortcuts again")));
    CHECK_FALSE(restoreAll->isEnabled());
    CHECK_FALSE(page.isChanged());
}

TEST_CASE("Shortcuts page: the search finds commands by name and by keys", "[gui][shortcuts][page]") {
    registerCommands();
    ShortcutsPage page;
    page.load();
    QTreeWidget* list = listOf(page);
    auto* search = child<ShortcutRecorder>(page, QStringLiteral("shortcutsSearch"));
    QTreeWidgetItem* close = itemNamed(list, QStringLiteral("Close Book"));
    QTreeWidgetItem* save = itemNamed(list, QStringLiteral("Save"));
    QTreeWidgetItem* noMatch = list->topLevelItem(list->topLevelItemCount() - 1);
    CHECK(noMatch->isHidden());

    search->setText(QStringLiteral("close"));
    CHECK(isShown(close));
    CHECK_FALSE(isShown(save));
    CHECK(list->currentItem() == close);

    // Keys written either way, with or without spaces
    search->setText(QStringLiteral("CTRL + F4"));
    CHECK(isShown(close));
    CHECK_FALSE(isShown(save));

    search->setText(QStringLiteral("zzzz"));
    CHECK_FALSE(noMatch->isHidden());
    CHECK(noMatch->text(0) == QStringLiteral("Nothing matches the search."));
    CHECK(list->currentItem() == nullptr);

    search->clear();
    CHECK(isShown(close));
    CHECK(isShown(save));
    CHECK(noMatch->isHidden());

    // Only the keys the user changed
    auto* onlyChanged = child<QCheckBox>(page, QStringLiteral("shortcutsOnlyChanged"));
    onlyChanged->setChecked(true);
    CHECK_FALSE(isShown(close));
    CHECK(noMatch->text(0) == QStringLiteral("You have not changed any shortcut."));
    onlyChanged->setChecked(false);
    REQUIRE(page.selectCommand("file.close"));
    page.assignKeys(QKeyCombination(CTRL | SHIFT, Qt::Key_K));
    onlyChanged->setChecked(true);
    CHECK(isShown(close));
    CHECK_FALSE(isShown(save));
    CHECK(groupNamed(list, QStringLiteral("In the text – fixed keys"))->isHidden());
    onlyChanged->setChecked(false);
}

TEST_CASE("Shortcuts page: the search by keys", "[gui][shortcuts][page]") {
    registerCommands();
    ShortcutsPage page;
    page.load();
    QTreeWidget* list = listOf(page);
    auto* search = child<ShortcutRecorder>(page, QStringLiteral("shortcutsSearch"));
    auto* byKeys = child<QPushButton>(page, QStringLiteral("shortcutsByKeys"));
    QTreeWidgetItem* close = itemNamed(list, QStringLiteral("Close Book"));
    QTreeWidgetItem* save = itemNamed(list, QStringLiteral("Save"));
    QTreeWidgetItem* noMatch = list->topLevelItem(list->topLevelItemCount() - 1);

    search->setText(QStringLiteral("save"));
    byKeys->click();
    CHECK(byKeys->isChecked());
    CHECK(search->isRecording());
    CHECK(isShown(close));  // nothing pressed yet: everything

    press(search, Qt::Key_F4, CTRL);
    CHECK(isShown(close));
    CHECK_FALSE(isShown(save));
    CHECK(search->text() == native(shortcut(Qt::Key_F4, CTRL)));
    CHECK(search->isRecording());

    // Keys of the text show where they work
    press(search, Qt::Key_Backspace, CTRL);
    CHECK_FALSE(isShown(close));
    CHECK(isShown(itemNamed(list, QStringLiteral("Delete the word before / after the cursor"))));

    // Keys no command has are free
    press(search, Qt::Key_F12, CTRL | SHIFT);
    CHECK_FALSE(noMatch->isHidden());
    CHECK(noMatch->text(0).endsWith(QStringLiteral("the shortcut is free.")));

    // Esc: back to the text searched for before
    press(search, Qt::Key_Escape);
    CHECK_FALSE(search->isRecording());
    CHECK_FALSE(byKeys->isChecked());
    CHECK(search->text() == QStringLiteral("save"));
    CHECK(isShown(save));
    CHECK_FALSE(isShown(close));
}

TEST_CASE("Shortcuts page: the keys of the list do not close the window", "[gui][shortcuts][page]") {
    registerCommands();
    SettingsDialog dialog(nullptr);
    dialog.showShortcutsPage();
    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    auto* scrollArea = qobject_cast<QScrollArea*>(stack->currentWidget());
    REQUIRE(scrollArea != nullptr);
    auto* page = qobject_cast<ShortcutsPage*>(scrollArea->widget());
    REQUIRE(page != nullptr);
    dialog.show();
    QApplication::processEvents();

    QTreeWidget* list = listOf(*page);
    auto* field = child<ShortcutRecorder>(*page, QStringLiteral("shortcutKeys"));
    REQUIRE(page->selectCommand("file.close"));
    list->setFocus();

    // Enter changes the keys, Esc stops it: the dialog stays open
    press(list, Qt::Key_Return);
    CHECK(dialog.isVisible());
    CHECK(field->isRecording());
    press(field, Qt::Key_Escape);
    CHECK_FALSE(field->isRecording());
    CHECK(dialog.isVisible());
    CHECK(page->shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));

    // F2, then the keys
    press(list, Qt::Key_F2);
    REQUIRE(field->isRecording());
    press(field, Qt::Key_K, CTRL | SHIFT);
    CHECK_FALSE(field->isRecording());
    CHECK(page->shortcutOf("file.close") == shortcut(Qt::Key_K, CTRL | SHIFT));
    CHECK(field->text() == native(shortcut(Qt::Key_K, CTRL | SHIFT)));

    // Delete leaves the command without keys
    press(list, Qt::Key_Delete);
    CHECK(page->shortcutOf("file.close").isEmpty());

    // Esc on the buttons of a message closes the message, not the window
    press(list, Qt::Key_Return);
    press(field, Qt::Key_S, CTRL);
    auto* accept = child<QPushButton>(*page, QStringLiteral("shortcutMessageAccept"));
    REQUIRE(accept->isVisible());
    press(accept, Qt::Key_Escape);
    CHECK_FALSE(accept->isVisible());
    CHECK(dialog.isVisible());

    // The changes are saved with the others
    CHECK(dialog.hasChanges());
    QStringList applied;
    QObject::connect(&dialog, &SettingsDialog::settingsApplied,
                     [&applied](const QStringList& keys) { applied = keys; });
    dialog.applyChanges();
    CHECK(applied == QStringList{QString::fromLatin1(SHORTCUTS_SETTING)});
    dialog.reject();
}

TEST_CASE("Shortcuts page: export and import", "[gui][shortcuts][page]") {
    registerCommands();
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    ShortcutsPage page;
    page.load();
    REQUIRE(page.selectCommand("file.close"));
    page.assignKeys(QKeyCombination(CTRL | SHIFT, Qt::Key_K));
    REQUIRE(page.selectCommand("view.focus"));
    QPushButton* remove = button(page, QStringLiteral("&Remove"));
    remove->click();

    const QString exported = directory.filePath(QStringLiteral("shortcuts.json"));
    REQUIRE(page.exportShortcuts(exported));
    CHECK(messageOf(page).contains(QStringLiteral("saved to")));
    QFile file(exported);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const nlohmann::json written = nlohmann::json::parse(file.readAll().toStdString());
    file.close();
    CHECK(written.at("format") == "kalahari-keyboard-shortcuts");
    CHECK(written.at("shortcuts") ==
          nlohmann::json({{"file.close", "Ctrl+Shift+K"}, {"view.focus", ""}}));

    // Another computer: the file in place of the changes there
    ShortcutsPage other;
    other.load();
    REQUIRE(other.selectCommand("file.save"));
    other.assignKeys(QKeyCombination(CTRL | SHIFT, Qt::Key_J));
    REQUIRE(other.importShortcuts(exported));
    CHECK(other.shortcutOf("file.close") == shortcut(Qt::Key_K, CTRL | SHIFT));
    CHECK(other.shortcutOf("view.focus").isEmpty());
    CHECK(other.shortcutOf("file.save") == shortcut(Qt::Key_S, CTRL));
    CHECK(messageOf(other).contains(QStringLiteral("read from")));

    // What cannot be is skipped and named
    const QString mixed = directory.filePath(QStringLiteral("mixed.json"));
    QFile mixedFile(mixed);
    REQUIRE(mixedFile.open(QIODevice::WriteOnly));
    mixedFile.write(R"({"shortcuts": {"file.save": "Ctrl+Shift+J", "file.close": "K",
                       "plugin.unknown": "Ctrl+Shift+L", "view.focus": "Ctrl+Shift+J",
                       "edit.find": 7}})");
    mixedFile.close();
    REQUIRE(other.importShortcuts(mixed));
    CHECK(other.shortcutOf("file.save") == shortcut(Qt::Key_J, CTRL | SHIFT));
    CHECK(other.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));
    CHECK(other.shortcutOf("view.focus") == shortcut(Qt::Key_4, CTRL));
    CHECK(other.shortcutOf("edit.find") == shortcut(Qt::Key_F, CTRL));
    const QString summary = messageOf(other);
    CHECK(summary.contains(QStringLiteral("Skipped")));
    CHECK(summary.contains(QStringLiteral("types text")));
    CHECK(summary.contains(QStringLiteral("plugin.unknown")));
    CHECK(summary.contains(QStringLiteral("the file gives it to File › Save too")));
    CHECK(summary.contains(QStringLiteral("Edit › Find...: the keys cannot be read")));

    // A file of something else changes nothing
    const QString notShortcuts = directory.filePath(QStringLiteral("other.json"));
    QFile otherFile(notShortcuts);
    REQUIRE(otherFile.open(QIODevice::WriteOnly));
    otherFile.write("not json");
    otherFile.close();
    CHECK_FALSE(other.importShortcuts(notShortcuts));
    CHECK(messageOf(other).contains(
        QStringLiteral("is not a file of Kalahari keyboard shortcuts")));
    CHECK(other.shortcutOf("file.save") == shortcut(Qt::Key_J, CTRL | SHIFT));
}

TEST_CASE("Shortcuts page: fits a small screen", "[gui][shortcuts][page]") {
    // 1366 × 768 at 150%: the dialog gets 90% of 911 × 464 above the taskbar
    registerCommands();
    SettingsDialog dialog(nullptr);
    dialog.resize(800, 417);
    dialog.showShortcutsPage();
    dialog.show();
    const auto settle = []() {
        for (int pass = 0; pass < 3; ++pass) {
            QApplication::processEvents();
        }
    };
    settle();

    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    auto* scrollArea = qobject_cast<QScrollArea*>(stack->currentWidget());
    REQUIRE(scrollArea != nullptr);
    QWidget* page = scrollArea->widget();
    REQUIRE(page != nullptr);
    QWidget* viewport = scrollArea->viewport();

    // No side scrolling, nothing squeezed: the list gives up its rows, so the page does not
    // scroll at all
    CHECK(page->minimumSizeHint().width() <= viewport->width());
    CHECK(page->height() >= page->minimumSizeHint().height());
    CHECK(page->height() <= viewport->height());
    for (const QPushButton* pageButton : page->findChildren<QPushButton*>()) {
        if (pageButton->isVisible()) {
            INFO(pageButton->text().toStdString());
            CHECK(pageButton->width() >= pageButton->minimumSizeHint().width());
        }
    }

    // Where even the smallest list does not fit, the page scrolls to a message under the
    // keys, with its buttons
    dialog.resize(800, dialog.minimumHeight());
    settle();
    REQUIRE(page->height() > viewport->height());
    auto* shortcuts = qobject_cast<ShortcutsPage*>(page);
    REQUIRE(shortcuts != nullptr);
    REQUIRE(shortcuts->selectCommand("file.close"));
    shortcuts->assignKeys(QKeyCombination(CTRL, Qt::Key_S));
    settle();
    auto* message = child<QFrame>(*shortcuts, QStringLiteral("shortcutMessage"));
    REQUIRE(message->isVisible());
    const QRect shown(message->mapTo(viewport, QPoint(0, 0)), message->size());
    CHECK(viewport->rect().contains(shown));
    dialog.reject();
}

TEST_CASE("Shortcuts page: the field shows the longest keys of the list whole",
          "[gui][shortcuts][page]") {
    registerCommands();
    SettingsDialog dialog(nullptr);
    dialog.resize(800, 700);
    dialog.showShortcutsPage();
    dialog.show();
    const auto settle = []() {
        for (int pass = 0; pass < 3; ++pass) {
            QApplication::processEvents();
        }
    };
    settle();
    auto* page = dialog.findChild<ShortcutsPage*>();
    REQUIRE(page != nullptr);
    auto* field = child<QWidget>(*page, QStringLiteral("shortcutKeys"));
    QPushButton* change = button(*page, QStringLiteral("&Change..."));
    QPushButton* restore = button(*page, QStringLiteral("Restore &Default"));
    const auto top = [page](const QWidget* widget) { return widget->mapTo(page, QPoint()).y(); };

    int widest = 0;
    const QFontMetrics metrics(field->font());
    for (const FixedKeyGroup& group : ShortcutRules().fixedGroups()) {
        for (const FixedKeys& row : group.rows) {
            widest = std::max(widest, metrics.horizontalAdvance(row.keysText()));
        }
    }

    // Beside the field the buttons would leave it too little room: they go under it
    CHECK(top(change) >= top(field) + field->height());
    CHECK(field->width() > widest);

    // A wide window has room for both in one row
    dialog.resize(1400, 700);
    settle();
    CHECK(top(change) < top(field) + field->height());
    CHECK(field->width() > widest);
    CHECK(restore->mapTo(page, QPoint(restore->width(), 0)).x() <= page->width());
    dialog.reject();
}

TEST_CASE("Shortcuts page: the selected command stays in sight when the list gets lower",
          "[gui][shortcuts][page]") {
    registerCommands();
    SettingsDialog dialog(nullptr);
    dialog.resize(800, 460);
    dialog.showShortcutsPage();
    dialog.show();
    const auto settle = []() {
        for (int pass = 0; pass < 3; ++pass) {
            QApplication::processEvents();
        }
    };
    settle();
    auto* page = dialog.findChild<ShortcutsPage*>();
    REQUIRE(page != nullptr);
    QTreeWidget* list = listOf(*page);
    REQUIRE(page->selectCommand("view.focus"));
    QTreeWidgetItem* item = list->currentItem();
    list->scrollToItem(item, QAbstractItemView::PositionAtBottom);
    settle();
    const QRect before = list->visualItemRect(item);
    INFO("row " << before.top() << "-" << before.bottom() << ", list " << list->height());
    // The command is the last row the list shows
    REQUIRE(list->viewport()->rect().contains(before));
    REQUIRE(before.bottom() + before.height() > list->viewport()->height());
    const int listHeight = list->height();

    // A message under the keys takes rows from the list
    page->assignKeys(QKeyCombination(CTRL, Qt::Key_S));
    settle();
    REQUIRE_FALSE(messageOf(*page).isEmpty());
    REQUIRE(list->height() < listHeight);
    CHECK(list->viewport()->rect().contains(list->visualItemRect(item)));
    dialog.reject();
}
