/// @file test_shortcuts_page.cpp
/// @brief Settings > Keyboard Shortcuts: the list, the checks, the search, the files

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/settings/shortcuts_page.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/widgets/shortcut_recorder.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOption>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <algorithm>
#include <sstream>
#include <string>

using namespace kalahari::gui;

namespace {

constexpr Qt::KeyboardModifiers CTRL = Qt::ControlModifier;
constexpr Qt::KeyboardModifiers SHIFT = Qt::ShiftModifier;

/// Room QLineEdit keeps on each side of its text, inside its frame
constexpr int LINE_EDIT_MARGIN = 2;

KeyboardShortcut shortcut(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    return KeyboardShortcut(key, modifiers);
}

QString native(const KeyboardShortcut& keys) {
    return keys.toQKeySequence().toString(QKeySequence::NativeText);
}

/// Whether the tests run on macOS, where the keys of the text and of the system differ
bool onMac() {
    return ShortcutRules().platform() == ShortcutPlatform::MacOS;
}

/// The keys that close the program on this system
KeyboardShortcut exitKeys() {
    return onMac() ? shortcut(Qt::Key_Q, CTRL) : shortcut(Qt::Key_F4, Qt::AltModifier);
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
    // As the program says when the help comes; the others are "in preparation"
    CommandRegistry::getInstance().getCommand("help.manual")->unavailableNote =
        "not available yet";
    addCommand("help.tips", "Tips", "HELP/Tips", 15, shortcut(Qt::Key_F12, SHIFT), false);
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
    QPushButton* found = nullptr;
    for (QPushButton* candidate : page.findChildren<QPushButton*>()) {
        if (candidate->text() == text) {
            found = candidate;
            break;
        }
    }
    INFO("No button " << text.toStdString());
    REQUIRE(found != nullptr);
    return found;
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
    QTreeWidgetItem* found = nullptr;
    for (int index = 0; index < list->topLevelItemCount() && found == nullptr; ++index) {
        if (list->topLevelItem(index)->text(0) == text) {
            found = list->topLevelItem(index);
        }
    }
    INFO("No group " << text.toStdString());
    REQUIRE(found != nullptr);
    return found;
}

QTreeWidgetItem* itemNamed(QTreeWidget* list, const QString& text) {
    QTreeWidgetItem* found = nullptr;
    for (int top = 0; top < list->topLevelItemCount() && found == nullptr; ++top) {
        QTreeWidgetItem* group = list->topLevelItem(top);
        for (int index = 0; index < group->childCount() && found == nullptr; ++index) {
            if (group->child(index)->text(0) == text) {
                found = group->child(index);
            }
        }
    }
    INFO("No item " << text.toStdString());
    REQUIRE(found != nullptr);
    return found;
}

bool isShown(const QTreeWidgetItem* item) {
    return !item->isHidden() && !item->parent()->isHidden();
}

/// Whether the list shows the whole row of the item (bottom() is the row's last pixel)
bool inSight(const QTreeWidget* list, const QTreeWidgetItem* item) {
    const QRect row = list->visualItemRect(item);
    return row.isValid() && row.top() >= 0 && row.bottom() < list->viewport()->height();
}

/// Where the row of the item is and how much room the rows have, for a failed check
std::string placeOf(const QTreeWidget* list, const QTreeWidgetItem* item) {
    const QRect row = list->visualItemRect(item);
    std::ostringstream text;
    text << "row " << row.top() << "-" << row.bottom() << ", rows' room "
         << list->viewport()->height() << ", list " << list->height() << ", header "
         << list->header()->height() << ", horizontal scroll bar "
         << list->horizontalScrollBar()->isVisible();
    return text.str();
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

/// The width of the text a line edit shows whole: its frame and margins off
int textRoomOf(const QLineEdit* field) {
    QStyleOptionFrame option;
    option.initFrom(field);
    option.lineWidth = field->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, field);
    const QRect contents =
        field->style()->subElementRect(QStyle::SE_LineEditContents, &option, field);
    const QMargins margins = field->textMargins();
    return contents.width() - margins.left() - margins.right() - 2 * LINE_EDIT_MARGIN;
}

/// The layouts of a shown window settle
void settle() {
    for (int pass = 0; pass < 3; ++pass) {
        QApplication::processEvents();
    }
}

/// The page of the settings dialog
ShortcutsPage* pageOf(SettingsDialog& dialog) {
    auto* page = dialog.findChild<ShortcutsPage*>();
    REQUIRE(page != nullptr);
    return page;
}

/// Puts the theme back when the test ends, also when it fails
class ThemeRestorer {
public:
    ThemeRestorer()
        : m_name(QString::fromStdString(
              kalahari::core::ThemeManager::getInstance().getCurrentTheme().name))
    {
    }
    ~ThemeRestorer() { kalahari::core::ThemeManager::getInstance().reloadTheme(m_name); }
    ThemeRestorer(const ThemeRestorer&) = delete;
    ThemeRestorer& operator=(const ThemeRestorer&) = delete;

private:
    QString m_name;
};

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
    const std::vector<FixedKeyGroup> fixedGroups = ShortcutRules().fixedGroups();
    QStringList fixedTitles;
    for (const FixedKeyGroup& group : fixedGroups) {
        fixedTitles.append(group.title);
    }
    REQUIRE(groups.size() == 4 + fixedTitles.size());
    CHECK(groups.mid(0, 4) == QStringList{QStringLiteral("File"), QStringLiteral("Edit"),
                                          QStringLiteral("View"), QStringLiteral("Help")});
    // The fixed keys after the menus, the system's last
    CHECK(groups.mid(4) == fixedTitles);
    CHECK(groups.at(4) == QStringLiteral("Fixed: in the text"));
    CHECK(fixedGroups.back().system);

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
    // The commands not available yet are there for the keys kept for them, each with its
    // note; those without keys are not
    CHECK(shownTexts(groupNamed(list, QStringLiteral("Help"))) ==
          QStringList{QStringLiteral("Kalahari Help (not available yet)"),
                      QStringLiteral("Tips (in preparation)")});

    // The fixed keys are listed with what they do
    CHECK(shownTexts(groupNamed(list, QStringLiteral("Fixed: the find bar")))
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

    // The list marks the change, in its own color, and names the default
    QTreeWidgetItem* item = itemNamed(listOf(page), QStringLiteral("Close Book"));
    CHECK(item->text(1) == QStringLiteral("● ") + native(newKeys));
    CHECK(item->font(1).bold());
    CHECK(item->data(1, Qt::ForegroundRole).isValid());
    const QString programKeys =
        QStringLiteral("Default: ") + native(shortcut(Qt::Key_F4, CTRL));
    CHECK(item->toolTip(0) == programKeys);
    CHECK(item->toolTip(1) == programKeys);
    // A screen reader says "changed" in place of the mark
    CHECK(item->data(1, Qt::AccessibleTextRole).toString() ==
          native(newKeys) + QStringLiteral(", changed"));

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
    CHECK_FALSE(item->data(1, Qt::ForegroundRole).isValid());
    CHECK(item->toolTip(1).isEmpty());
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
    CHECK(messageOf(page) ==
          native(shortcut(Qt::Key_S, CTRL)) +
              QStringLiteral(" already belongs to <b>File › Save</b>: if you assign it here, "
                             "that command will be left without a shortcut."));
    CHECK(accept->isVisibleTo(&page));
    CHECK(accept->text() == QStringLiteral("Assi&gn Anyway"));
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));

    accept->click();
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_S, CTRL));
    CHECK(page.shortcutOf("file.save").isEmpty());
    CHECK(messageOf(page) == QStringLiteral("File › Save now has no shortcut."));
    CHECK_FALSE(accept->isVisibleTo(&page));
    // The mark alone where the keys were removed
    CHECK(itemNamed(listOf(page), QStringLiteral("Save"))->text(1) == QStringLiteral("●"));

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

    // A fixed key says what it does
    const QKeyCombination wordKeys(CTRL, Qt::Key_Backspace);
    const QString deletes = onMac() ? QStringLiteral("deletes the text to the start of the line")
                                    : QStringLiteral("deletes the word before the cursor");
    page.assignKeys(wordKeys);
    CHECK(messageOf(page) == QStringLiteral("In the text %1 %2. It is a fixed key: choose "
                                            "another shortcut.")
                                 .arg(ShortcutRules::keysText(wordKeys), deletes)
                                 .toHtmlEscaped());
    CHECK_FALSE(
        child<QPushButton>(page, QStringLiteral("shortcutMessageAccept"))->isVisibleTo(&page));
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));

    page.assignKeys(QKeyCombination(Qt::NoModifier, Qt::Key_K));
    CHECK(messageOf(page).contains(QStringLiteral("types text")));

    // The keys that close the window are the system's: only Exit may have them
    page.assignKeys(exitKeys().toQKeySequence()[0]);
    CHECK(messageOf(page).contains(QStringLiteral("only the Exit command can have it")));
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));
    REQUIRE(page.selectCommand("file.exit"));
    page.assignKeys(QKeyCombination(CTRL | SHIFT, Qt::Key_K));
    page.assignKeys(exitKeys().toQKeySequence()[0]);
    CHECK(page.shortcutOf("file.exit") == exitKeys());
    CHECK_FALSE(page.isChanged());

    if (ShortcutRules().platform() == ShortcutPlatform::Linux) {
        // A key of the desktops: the user decides
        REQUIRE(page.selectCommand("file.close"));
        page.assignKeys(QKeyCombination(CTRL, Qt::Key_F2));
        CHECK(messageOf(page).contains(QStringLiteral("In KDE and Xfce")));
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
    auto* accept = child<QPushButton>(page, QStringLiteral("shortcutMessageAccept"));

    // Restore All says when there is nothing to restore
    CHECK(restoreAll->isEnabled());
    restoreAll->click();
    CHECK(messageOf(page) ==
          QStringLiteral("All the commands already have their default shortcuts."));

    REQUIRE(page.selectCommand("file.save"));
    CHECK(messageOf(page).isEmpty());
    CHECK_FALSE(restore->isEnabled());
    remove->click();
    CHECK(page.shortcutOf("file.save").isEmpty());
    CHECK_FALSE(remove->isEnabled());
    CHECK(restore->isEnabled());
    restore->click();
    CHECK(page.shortcutOf("file.save") == shortcut(Qt::Key_S, CTRL));
    CHECK_FALSE(page.isChanged());

    // The default keys another command has now: the user decides
    REQUIRE(page.selectCommand("file.close"));
    page.assignKeys(QKeyCombination(CTRL, Qt::Key_S));
    accept->click();
    REQUIRE(page.selectCommand("file.save"));
    restore->click();
    CHECK(messageOf(page) ==
          QStringLiteral("The default shortcut %1 now belongs to <b>File › Close Book</b>: if "
                         "you restore it, that command will be left without a shortcut.")
              .arg(native(shortcut(Qt::Key_S, CTRL))));
    CHECK(accept->text() == QStringLiteral("Restore A&nyway"));
    accept->click();
    CHECK(page.shortcutOf("file.save") == shortcut(Qt::Key_S, CTRL));
    CHECK(page.shortcutOf("file.close").isEmpty());
    CHECK(messageOf(page) == QStringLiteral("File › Close Book now has no shortcut."));

    restoreAll->click();
    CHECK(page.shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));
    CHECK(page.shortcutOf("file.save") == shortcut(Qt::Key_S, CTRL));
    CHECK(messageOf(page) ==
          QStringLiteral("All the commands have their default shortcuts again. Apply or OK will "
                         "save this change, Cancel will discard it."));
    CHECK(restoreAll->isEnabled());
    CHECK_FALSE(page.isChanged());
}

TEST_CASE("Shortcuts page: what the selected row is and does", "[gui][shortcuts][page]") {
    registerCommands();
    ShortcutsPage page;
    page.load();
    QTreeWidget* list = listOf(page);
    auto* title = child<QLabel>(page, QStringLiteral("shortcutTitle"));
    auto* field = child<ShortcutRecorder>(page, QStringLiteral("shortcutKeys"));
    auto* info = child<QLabel>(page, QStringLiteral("shortcutInfo"));
    QPushButton* change = button(page, QStringLiteral("&Change..."));

    // A command: its keys and the default ones
    REQUIRE(page.selectCommand("file.close"));
    CHECK(title->text() == QStringLiteral("File › Close Book"));
    CHECK(field->text() == native(shortcut(Qt::Key_F4, CTRL)));
    CHECK(info->text() ==
          QStringLiteral("Default: %1.").arg(native(shortcut(Qt::Key_F4, CTRL))));
    CHECK(change->isEnabled());

    REQUIRE(page.selectCommand("view.darkPaper"));
    CHECK(field->text() == QStringLiteral("No shortcut"));
    CHECK(info->text() == QStringLiteral("Default: no shortcut."));

    // A command not available yet keeps its keys for later
    REQUIRE(page.selectCommand("help.manual"));
    CHECK(info->text() ==
          QStringLiteral("The command is not available yet: its shortcut is reserved. "
                         "Default: %1.")
              .arg(native(shortcut(Qt::Key_F1))));

    // The window closes with Alt+F4 also without the keys of Exit
    REQUIRE(page.selectCommand("file.exit"));
    const ShortcutPlatform platform = ShortcutRules().platform();
    if (platform == ShortcutPlatform::Linux) {
        CHECK(info->text().endsWith(QStringLiteral(
            "Alt+F4 closes the window also without this shortcut: the desktop does it.")));
    } else if (platform == ShortcutPlatform::Windows) {
        CHECK(info->text().endsWith(
            QStringLiteral("Alt+F4 closes the window also without this shortcut: Windows does "
                           "it.")));
    }

    // Fixed keys: where they work and what they are, nothing to change
    const std::vector<FixedKeyGroup> fixedGroups = ShortcutRules().fixedGroups();
    const FixedKeys& textKeys = fixedGroups.front().rows.front();
    list->setCurrentItem(itemNamed(list, textKeys.label));
    CHECK(title->text() == QStringLiteral("Fixed: in the text › ") + textKeys.label);
    CHECK(field->text() == textKeys.keysText());
    CHECK_FALSE(field->isEnabled());
    CHECK_FALSE(change->isEnabled());
    CHECK(info->text() ==
          QStringLiteral("A fixed key: it works as in other programs and cannot be changed."));

    list->setCurrentItem(groupNamed(list, QStringLiteral("Fixed: in the text")));
    CHECK(title->text() == QStringLiteral("Fixed: in the text"));
    CHECK(info->text() ==
          QStringLiteral("Fixed keys: they work as in other programs and cannot be changed."));

    const FixedKeyGroup& system = fixedGroups.back();
    list->setCurrentItem(itemNamed(list, system.rows.front().label));
    CHECK(title->text() == system.title + QStringLiteral(" › ") + system.rows.front().label);
    CHECK(info->text() == QStringLiteral("A key of the system: the program does not get it or "
                                         "cannot change it."));

    // The keys of the list of a panel: a command may have them too
    list->setCurrentItem(groupNamed(list, QStringLiteral("Fixed: the Annotations panel")));
    CHECK(info->text() == QStringLiteral("These keys work only while the list of the panel is "
                                         "active, so a command may have them as well."));

    // A menu
    list->setCurrentItem(groupNamed(list, QStringLiteral("File")));
    CHECK(title->text() == QStringLiteral("File"));
    CHECK(info->text() ==
          QStringLiteral("Choose a command of the menu to see or change its shortcut."));
    CHECK_FALSE(change->isEnabled());
}

TEST_CASE("Shortcuts page: the search finds commands by name and by keys",
          "[gui][shortcuts][page]") {
    registerCommands();
    ShortcutsPage page;
    page.load();
    QTreeWidget* list = listOf(page);
    auto* search = child<ShortcutRecorder>(page, QStringLiteral("shortcutsSearch"));
    QTreeWidgetItem* close = itemNamed(list, QStringLiteral("Close Book"));
    QTreeWidgetItem* save = itemNamed(list, QStringLiteral("Save"));
    QTreeWidgetItem* noMatch = list->topLevelItem(list->topLevelItemCount() - 1);
    CHECK(noMatch->isHidden());
    // An example of the keys the list has: those of Find here
    CHECK(search->placeholderText() ==
          QStringLiteral("Command name or keys, e.g. ") + native(shortcut(Qt::Key_F, CTRL)));

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
    CHECK(groupNamed(list, QStringLiteral("Fixed: in the text"))->isHidden());
    search->setText(QStringLiteral("zzzz"));
    CHECK(noMatch->text(0) == QStringLiteral("No changed shortcut matches the search."));
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
    const QString textPrompt = search->placeholderText();

    // The field starts empty and says what to do
    search->setText(QStringLiteral("save"));
    byKeys->click();
    CHECK(byKeys->isChecked());
    CHECK(search->isRecording());
    CHECK(search->text().isEmpty());
    CHECK(search->placeholderText() == QStringLiteral("Press a shortcut... (Esc – normal search)"));
    CHECK(isShown(close));  // nothing pressed yet: everything

    press(search, Qt::Key_F4, CTRL);
    CHECK(isShown(close));
    CHECK_FALSE(isShown(save));
    CHECK(search->text() == native(shortcut(Qt::Key_F4, CTRL)));
    CHECK(search->isRecording());

    // Keys of the text show where they work
    press(search, Qt::Key_Backspace, CTRL);
    CHECK_FALSE(isShown(close));
    CHECK(isShown(itemNamed(list, onMac() ? QStringLiteral("Delete to the start of the line")
                                          : QStringLiteral("Delete the word before / after "
                                                           "the cursor"))));

    // Keys no command has are free
    const KeyboardShortcut freeKeys = shortcut(Qt::Key_F12, CTRL | SHIFT);
    press(search, Qt::Key_F12, CTRL | SHIFT);
    CHECK_FALSE(noMatch->isHidden());
    CHECK(noMatch->text(0) ==
          QStringLiteral("No command has %1: the shortcut is free.").arg(native(freeKeys)));

    // Keys no command can have say why
    press(search, Qt::Key_K);
    CHECK_FALSE(noMatch->isHidden());
    CHECK(noMatch->text(0).contains(QStringLiteral("types text")));

    // Esc: back to the search by text, from an empty field
    press(search, Qt::Key_Escape);
    CHECK_FALSE(search->isRecording());
    CHECK_FALSE(byKeys->isChecked());
    CHECK(search->text().isEmpty());
    CHECK(search->placeholderText() == textPrompt);
    CHECK(isShown(save));
    CHECK(isShown(close));
}

TEST_CASE("Shortcuts page: the keys of the list do not close the window",
          "[gui][shortcuts][page]") {
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
    QPushButton* change = button(*page, QStringLiteral("&Change..."));
    QPushButton* remove = button(*page, QStringLiteral("&Remove"));
    REQUIRE(page->selectCommand("file.close"));
    list->setFocus();

    // Enter changes the keys, Esc stops it: the dialog stays open, the keys go back to the list
    press(list, Qt::Key_Return);
    CHECK(dialog.isVisible());
    CHECK(field->isRecording());
    CHECK(dialog.focusWidget() == field);
    press(field, Qt::Key_Escape);
    CHECK_FALSE(field->isRecording());
    CHECK(dialog.isVisible());
    CHECK(dialog.focusWidget() == list);
    CHECK(page->shortcutOf("file.close") == shortcut(Qt::Key_F4, CTRL));

    // F2, then the keys
    press(list, Qt::Key_F2);
    REQUIRE(field->isRecording());
    press(field, Qt::Key_K, CTRL | SHIFT);
    CHECK_FALSE(field->isRecording());
    CHECK(page->shortcutOf("file.close") == shortcut(Qt::Key_K, CTRL | SHIFT));
    CHECK(field->text() == native(shortcut(Qt::Key_K, CTRL | SHIFT)));
    CHECK(dialog.focusWidget() == list);

    // Change... records the keys; Esc gives the keys back to the button
    change->click();
    REQUIRE(field->isRecording());
    press(field, Qt::Key_Escape);
    CHECK(dialog.focusWidget() == change);

    // Remove is off without keys: the keys go to Change...
    remove->click();
    CHECK(page->shortcutOf("file.close").isEmpty());
    CHECK_FALSE(remove->isEnabled());
    CHECK(dialog.focusWidget() == change);

    // Delete on the list leaves the command without keys; the keys stay on the list
    REQUIRE(page->selectCommand("file.save"));
    list->setFocus();
    press(list, Qt::Key_Delete);
    CHECK(page->shortcutOf("file.save").isEmpty());
    CHECK(dialog.focusWidget() == list);

    // Esc on the buttons of a message closes the message, not the window
    press(list, Qt::Key_Return);
    press(field, Qt::Key_O, CTRL | SHIFT);
    auto* accept = child<QPushButton>(*page, QStringLiteral("shortcutMessageAccept"));
    REQUIRE(accept->isVisible());
    CHECK(dialog.focusWidget() == accept);
    press(accept, Qt::Key_Escape);
    CHECK_FALSE(accept->isVisible());
    CHECK(dialog.isVisible());
    CHECK(dialog.focusWidget() == list);

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

    // Nothing changed: no file to export
    ShortcutsPage page;
    page.load();
    button(page, QStringLiteral("&Export..."))->click();
    CHECK(messageOf(page) == QStringLiteral("There are no changed shortcuts: the file would be "
                                            "empty. Change a shortcut, then export it."));

    REQUIRE(page.selectCommand("file.close"));
    page.assignKeys(QKeyCombination(CTRL | SHIFT, Qt::Key_K));
    REQUIRE(page.selectCommand("view.focus"));
    button(page, QStringLiteral("&Remove"))->click();

    const QString exported = directory.filePath(QStringLiteral("shortcuts.json"));
    REQUIRE(page.exportShortcuts(exported));
    CHECK(messageOf(page) == QStringLiteral("Saved 2 changed shortcut(s) to the file %1.")
                                 .arg(QDir::toNativeSeparators(exported).toHtmlEscaped()));
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
    CHECK(messageOf(other) ==
          QStringLiteral("Read 2 shortcut(s) from the file %1. Apply or OK will save the "
                         "changes.")
              .arg(QDir::toNativeSeparators(exported).toHtmlEscaped()));

    // What cannot be is skipped and named, each with why in a few words
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
    CHECK(messageOf(other) ==
          QStringLiteral("Read 1 shortcut(s) from the file %1. Skipped 4: “Find...” (the keys "
                         "cannot be read); K for “Close Book” (types text); “plugin.unknown” "
                         "(Kalahari has no such command); %2 for “Focus” (the file gives it to "
                         "“Save” too). Apply or OK will save the changes.")
              .arg(QDir::toNativeSeparators(mixed).toHtmlEscaped(),
                   native(shortcut(Qt::Key_J, CTRL | SHIFT))));

    // A file of something else changes nothing
    const QString notShortcuts = directory.filePath(QStringLiteral("other.json"));
    QFile otherFile(notShortcuts);
    REQUIRE(otherFile.open(QIODevice::WriteOnly));
    otherFile.write("not json");
    otherFile.close();
    CHECK_FALSE(other.importShortcuts(notShortcuts));
    CHECK(messageOf(other) ==
          QStringLiteral("%1 is not a file of Kalahari keyboard shortcuts.")
              .arg(QDir::toNativeSeparators(notShortcuts).toHtmlEscaped()));
    CHECK(other.shortcutOf("file.save") == shortcut(Qt::Key_J, CTRL | SHIFT));
}

TEST_CASE("Shortcuts page: fits a small screen", "[gui][shortcuts][page]") {
    registerCommands();
    SettingsDialog dialog(nullptr);
    dialog.showShortcutsPage();
    dialog.show();

    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    auto* scrollArea = qobject_cast<QScrollArea*>(stack->currentWidget());
    REQUIRE(scrollArea != nullptr);
    QWidget* page = scrollArea->widget();
    REQUIRE(page != nullptr);
    QWidget* viewport = scrollArea->viewport();
    QTreeWidget* list = listOf(*page);
    auto* search = child<ShortcutRecorder>(*page, QStringLiteral("shortcutsSearch"));
    auto* byKeys = child<QPushButton>(*page, QStringLiteral("shortcutsByKeys"));
    auto* field = child<ShortcutRecorder>(*page, QStringLiteral("shortcutKeys"));
    QPushButton* change = button(*page, QStringLiteral("&Change..."));
    const auto top = [page](const QWidget* widget) { return widget->mapTo(page, QPoint()).y(); };

    // 1366 × 768 at 125%: the dialog gets 90% of 1093 × 566 above the taskbar, and the page
    // has room for everything
    dialog.resize(840, 509);
    settle();
    CHECK(page->height() <= viewport->height());
    // The columns fit: the list does not scroll sideways
    CHECK(list->header()->length() <= list->viewport()->width());

    // At 150%, 90% of 911 × 464: the list shrinks, the fields and the buttons stay whole, and
    // where even the smallest list leaves too little room the page scrolls, never sideways
    dialog.resize(820, 417);
    settle();
    CHECK(page->minimumSizeHint().width() <= viewport->width());
    CHECK(page->height() >= page->minimumSizeHint().height());
    CHECK(list->header()->length() <= list->viewport()->width());
    if (page->height() > viewport->height()) {
        CHECK(list->height() == list->minimumHeight());
    }
    for (const QPushButton* pageButton : page->findChildren<QPushButton*>()) {
        if (pageButton->isVisible()) {
            INFO(pageButton->text().toStdString());
            CHECK(pageButton->width() >= pageButton->minimumSizeHint().width());
            CHECK(pageButton->mapTo(page, QPoint(pageButton->width(), 0)).x() <= page->width());
        }
    }
    // With the search and the keys in one row each, as the design has them (the fonts of
    // Windows leave room for it), nothing scrolls
    if (top(byKeys) < top(search) + search->height() &&
        top(change) < top(field) + field->height()) {
        CHECK(page->height() <= viewport->height());
    }

    dialog.reject();
}

TEST_CASE("Shortcuts page: a message under the keys is in sight on a small screen",
          "[gui][shortcuts][page]") {
    registerCommands();
    // The page can scroll for the message: it scrolls to it, with its buttons, also when the
    // scroll bar takes the room of the options beside the search
    for (const int height : {417, 436, 400}) {
        INFO("height " << height);
        SettingsDialog dialog(nullptr);
        dialog.resize(820, height);
        dialog.showShortcutsPage();
        dialog.show();
        settle();
        auto* scrollArea = qobject_cast<QScrollArea*>(
            dialog.findChild<QStackedWidget*>()->currentWidget());
        REQUIRE(scrollArea != nullptr);
        QWidget* viewport = scrollArea->viewport();
        ShortcutsPage* page = pageOf(dialog);
        REQUIRE(page->selectCommand("file.close"));
        settle();
        page->assignKeys(QKeyCombination(CTRL, Qt::Key_S));
        settle();
        auto* message = child<QFrame>(*page, QStringLiteral("shortcutMessage"));
        REQUIRE(message->isVisible());
        const QRect shown(message->mapTo(viewport, QPoint(0, 0)), message->size());
        CHECK(viewport->rect().contains(shown));
        dialog.reject();
    }
}

TEST_CASE("Shortcuts page: a message that makes the page scroll is in sight and moves nothing",
          "[gui][shortcuts][page]") {
    registerCommands();
    SettingsDialog dialog(nullptr);
    dialog.resize(600, 700);
    dialog.showShortcutsPage();
    dialog.show();
    settle();
    auto* scrollArea =
        qobject_cast<QScrollArea*>(dialog.findChild<QStackedWidget*>()->currentWidget());
    REQUIRE(scrollArea != nullptr);
    QWidget* viewport = scrollArea->viewport();
    ShortcutsPage* page = pageOf(dialog);
    auto* search = child<ShortcutRecorder>(*page, QStringLiteral("shortcutsSearch"));
    auto* byKeys = child<QPushButton>(*page, QStringLiteral("shortcutsByKeys"));
    const auto beside = [&]() {
        return byKeys->mapTo(page, QPoint()).y() < search->mapTo(page, QPoint()).y() +
                                                       search->height();
    };
    const auto scrolls = [&]() { return page->height() > viewport->height(); };

    // The narrowest window with the options beside the search: the page leaves room there
    // for the scroll bar a message may bring
    int width = dialog.width();
    while (!beside() && width < 1400) {
        width += 2;
        dialog.resize(width, 700);
        settle();
    }
    REQUIRE(beside());
    // The lowest window the page fits in without the message
    int height = 700;
    while (!scrolls() && height > dialog.minimumHeight()) {
        height -= 2;
        dialog.resize(width, height);
        settle();
    }
    dialog.resize(width, height + 2);
    settle();
    REQUIRE_FALSE(scrolls());
    REQUIRE(beside());

    // The message makes the page scroll: the options stay beside the search, and the page
    // scrolls to the message when it has settled
    REQUIRE(page->selectCommand("file.close"));
    page->assignKeys(QKeyCombination(CTRL, Qt::Key_S));
    settle();
    auto* message = child<QFrame>(*page, QStringLiteral("shortcutMessage"));
    REQUIRE(message->isVisible());
    REQUIRE(scrolls());
    INFO("window " << width << " x " << height + 2);
    CHECK(beside());
    const QRect shown(message->mapTo(viewport, QPoint(0, 0)), message->size());
    CHECK(viewport->rect().contains(shown));

    // Without the message the page fits again, as it was
    REQUIRE(page->selectCommand("file.save"));
    settle();
    CHECK_FALSE(scrolls());
    CHECK(beside());
    dialog.reject();
}

TEST_CASE("Shortcuts page: the fields show their longest texts whole", "[gui][shortcuts][page]") {
    registerCommands();
    SettingsDialog dialog(nullptr);
    dialog.resize(840, 700);
    dialog.showShortcutsPage();
    dialog.show();
    settle();
    ShortcutsPage* page = pageOf(dialog);
    auto* field = child<ShortcutRecorder>(*page, QStringLiteral("shortcutKeys"));
    auto* search = child<ShortcutRecorder>(*page, QStringLiteral("shortcutsSearch"));
    auto* byKeys = child<QPushButton>(*page, QStringLiteral("shortcutsByKeys"));
    QPushButton* change = button(*page, QStringLiteral("&Change..."));
    QPushButton* restore = button(*page, QStringLiteral("Restore &Default"));
    const auto top = [page](const QWidget* widget) { return widget->mapTo(page, QPoint()).y(); };

    // The keys of every command, and what the field asks for while it records new ones
    const QFontMetrics metrics(field->font());
    int widest = std::max(
        metrics.horizontalAdvance(QStringLiteral("Press the new shortcut... (Esc – cancel)")),
        metrics.horizontalAdvance(QStringLiteral("No shortcut")));
    for (const auto& [id, keys] : CommandRegistry::getInstance().defaultShortcuts()) {
        widest = std::max(widest, metrics.horizontalAdvance(native(keys)));
    }
    // Both prompts of the search. They show while the field is empty, and then QLineEdit
    // keeps no room for its clear button.
    const QFontMetrics searchMetrics(search->font());
    const int prompts =
        std::max(searchMetrics.horizontalAdvance(search->placeholderText()),
                 searchMetrics.horizontalAdvance(
                     QStringLiteral("Press a shortcut... (Esc – normal search)")));

    CHECK(textRoomOf(field) >= widest);
    CHECK(textRoomOf(search) >= prompts);

    // A wide window has room for the buttons and the options beside the fields
    dialog.resize(1400, 700);
    settle();
    CHECK(top(change) < top(field) + field->height());
    CHECK(top(byKeys) < top(search) + search->height());
    CHECK(textRoomOf(field) >= widest);
    CHECK(textRoomOf(search) >= prompts);
    CHECK(restore->mapTo(page, QPoint(restore->width(), 0)).x() <= page->width());
    dialog.reject();
}

TEST_CASE("Shortcuts page: the selected command stays in sight when the list gets lower",
          "[gui][shortcuts][page]") {
    registerCommands();
    SettingsDialog dialog(nullptr);
    dialog.resize(840, 460);
    dialog.showShortcutsPage();
    dialog.show();
    settle();
    ShortcutsPage* page = pageOf(dialog);
    QTreeWidget* list = listOf(*page);
    // A window as low as leaves the list a few rows over its smallest height, for a message
    // to take (the larger controls of macOS need more than 460 pixels for it)
    const int rowHeight = list->visualItemRect(list->topLevelItem(0)).height();
    const int roomy = list->minimumHeight() + 3 * rowHeight;
    for (int height = 470; height <= 800 && list->height() < roomy; height += 10) {
        dialog.resize(840, height);
        settle();
    }
    REQUIRE(list->height() >= roomy);
    REQUIRE(page->selectCommand("view.focus"));
    QTreeWidgetItem* item = list->currentItem();
    // As the page scrolls (scrollToItem() would go past the list's own scrollTo())
    list->scrollTo(list->indexFromItem(item), QAbstractItemView::PositionAtBottom);
    settle();
    const QRect before = list->visualItemRect(item);
    INFO("before: " << placeOf(list, item));
    // The command is the last row the list shows whole
    REQUIRE(inSight(list, item));
    REQUIRE(before.bottom() + before.height() >= list->viewport()->height());
    const int listHeight = list->height();

    // A message under the keys takes rows from the list
    page->assignKeys(QKeyCombination(CTRL, Qt::Key_S));
    settle();
    INFO("after: " << placeOf(list, item));
    REQUIRE_FALSE(messageOf(*page).isEmpty());
    REQUIRE(list->height() < listHeight);
    CHECK(inSight(list, item));
    dialog.reject();
}

TEST_CASE("Shortcuts page: the selected command stays in sight when its rows get less room",
          "[gui][shortcuts][page]") {
    // Regression: only a lower list kept it in sight. A higher header, or a scroll bar that
    // does not lie over the rows (it does on macOS), take the rows' room too, and the list
    // keeps its height.
    registerCommands();
    SettingsDialog dialog(nullptr);
    dialog.resize(840, 460);
    dialog.showShortcutsPage();
    dialog.show();
    settle();
    ShortcutsPage* page = pageOf(dialog);
    QTreeWidget* list = listOf(*page);
    REQUIRE(page->selectCommand("view.focus"));
    QTreeWidgetItem* item = list->currentItem();
    // As the page scrolls (scrollToItem() would go past the list's own scrollTo())
    list->scrollTo(list->indexFromItem(item), QAbstractItemView::PositionAtBottom);
    settle();
    const QRect before = list->visualItemRect(item);
    INFO("before: " << placeOf(list, item));
    // The command is the last row the list shows whole
    REQUIRE(inSight(list, item));
    REQUIRE(before.bottom() + before.height() >= list->viewport()->height());
    const int listHeight = list->height();
    const int room = list->viewport()->height();

    // A larger font makes the header higher. A style can keep the header's height whatever
    // the font (that of macOS does): the minimum height makes it higher there too.
    QHeaderView* header = list->header();
    header->setMinimumHeight(header->height() * 2);
    QFont font = header->font();
    font.setPixelSize(QFontInfo(font).pixelSize() * 2);
    header->setFont(font);
    settle();
    INFO("after: " << placeOf(list, item));
    REQUIRE(list->height() == listHeight);
    REQUIRE(list->viewport()->height() < room);
    CHECK(inSight(list, item));
    dialog.reject();
}

TEST_CASE("Shortcuts page: the muted and the changed texts can be read in every theme",
          "[gui][shortcuts][page]") {
    registerCommands();
    auto& themes = kalahari::core::ThemeManager::getInstance();
    const ThemeRestorer restorer;
    ShortcutsPage page;
    page.load();
    QTreeWidget* list = listOf(page);
    auto* info = child<QLabel>(page, QStringLiteral("shortcutInfo"));
    REQUIRE(page.selectCommand("file.close"));
    page.assignKeys(QKeyCombination(CTRL | SHIFT, Qt::Key_K));
    const QString fixedRow = ShortcutRules().fixedGroups().front().rows.front().label;

    for (const QString& theme : {QStringLiteral("Light"), QStringLiteral("Dark")}) {
        INFO(theme.toStdString());
        REQUIRE(themes.reloadTheme(theme));
        const QColor base = themes.getCurrentTheme().palette.base;
        const QColor changed =
            itemNamed(list, QStringLiteral("Close Book"))->foreground(1).color();
        const QColor muted =
            groupNamed(list, QStringLiteral("Fixed: in the text"))->foreground(0).color();
        const QColor fixedKeys = itemNamed(list, fixedRow)->foreground(1).color();
        CHECK(contrastRatio(changed, base) >= MIN_TEXT_CONTRAST);
        CHECK(contrastRatio(muted, base) >= MIN_TEXT_CONTRAST);
        CHECK(contrastRatio(fixedKeys, base) >= MIN_TEXT_CONTRAST);

        // The note under the keys, on the window's background
        info->ensurePolished();
        const QColor note = info->palette().color(info->foregroundRole());
        const QColor window = themes.getCurrentTheme().palette.window;
        CHECK(contrastRatio(note, window) >= MIN_TEXT_CONTRAST);
    }
}
