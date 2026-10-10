/// @file shortcuts_page.h
/// @brief Settings > Keyboard Shortcuts: the keys of the commands, changed and checked
///
/// The list shows the commands the program runs, in the groups and the order of the menus,
/// and at its end the fixed keys and the keys of the system, which no command can have.
/// A command gets new keys from the field under the list: every key is checked
/// (ShortcutRules), and keys another command has are taken from it only when the user
/// says so. Apply and OK save the keys the user changed (keyboard.shortcuts); Import and
/// Export move them between computers.

#pragma once

#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/shortcut_rules.h"

#include <QKeyCombination>
#include <QString>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class QCheckBox;
class QFrame;
class QGridLayout;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace gui {

class ShortcutRecorder;

/// @brief Settings > Keyboard Shortcuts
class ShortcutsPage : public SettingsPage {
    Q_OBJECT

public:
    /// @brief Constructor: the commands registered now
    /// @param parent Parent widget
    explicit ShortcutsPage(QWidget* parent = nullptr);

    /// @brief Select a command in the list (the search shows it if it hid it)
    /// @return Whether the list has the command
    bool selectCommand(const std::string& commandId);

    /// @brief Give the selected command keys, as when they are pressed in the field
    ///
    /// Refused keys and keys another command has bring a message, as in the field.
    void assignKeys(QKeyCombination keys);

    /// @brief The keys a command has on the page now (with the changes not applied yet)
    [[nodiscard]] KeyboardShortcut shortcutOf(const std::string& commandId) const;

    /// @brief Write the keys the user changed to a file (Export)
    /// @return Whether the file was written
    bool exportShortcuts(const QString& path);

    /// @brief Read the keys of a file in place of the changes on the page (Import)
    /// @return Whether it was a file of shortcuts
    bool importShortcuts(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief A command of the list
    struct Entry {
        std::string id;
        QString menu;          ///< The menu it is in ("File")
        QString label;         ///< Its name with the submenus ("Open › Open File...")
        bool available = true; ///< The program runs it already
        QTreeWidgetItem* item = nullptr;
    };

    /// @brief What a message under the field is about
    enum class MessageKind { Information, Warning, Error };

    void createSearch();
    void createList();
    void createDetail();
    void createButtons();

    /// @brief Fill the list: the commands in the order of the menus, then the fixed keys
    void buildList();

    /// @brief The command of an item, or nullptr for a group or the fixed keys
    [[nodiscard]] const Entry* entryOf(const QTreeWidgetItem* item) const;
    [[nodiscard]] const Entry* currentEntry() const;
    [[nodiscard]] const Entry* entryById(const std::string& id) const;

    /// @brief The fixed keys of an item, or nullptr
    /// @param group The group of the keys, also for the item of the group itself
    [[nodiscard]] const FixedKeys* fixedKeysOf(const QTreeWidgetItem* item,
                                               const FixedKeyGroup** group = nullptr) const;

    /// @brief "File › Save", for the messages
    [[nodiscard]] QString titleOf(const std::string& commandId) const;

    /// @brief The program's keys of a command
    [[nodiscard]] KeyboardShortcut defaultOf(const std::string& commandId) const;

    /// @brief The keys the commands have with the changes on the page
    [[nodiscard]] CommandRegistry::ShortcutMap resolved() const;

    /// @brief The command that has the keys, other than one; empty if none
    [[nodiscard]] std::string ownerOf(const KeyboardShortcut& keys,
                                      const std::string& exceptId) const;

    /// @brief The user's keys without those that are a command's own default keys
    [[nodiscard]] CommandRegistry::ShortcutMap withoutDefaults(
        const CommandRegistry::ShortcutMap& custom) const;

    /// @brief Give a command keys (its own default ones are no change)
    void setKeys(const std::string& commandId, const KeyboardShortcut& keys);

    /// @brief Show the keys of every command and filter the list again
    void refresh();

    /// @brief Hide what the search and "Only Changed" leave out
    void applyFilter();

    /// @brief Whether the search and "Only Changed" leave an item of a command or of fixed
    ///        keys in the list
    /// @param query The text searched for, as searchForm() makes it (empty: none)
    [[nodiscard]] bool isShown(const QTreeWidgetItem* item,
                               const CommandRegistry::ShortcutMap& keysNow,
                               const QString& query) const;

    /// @brief What the list says when nothing is left in it
    [[nodiscard]] QString noMatchText() const;

    /// @brief The first command or fixed keys the list shows, or nullptr
    [[nodiscard]] QTreeWidgetItem* firstShownItem() const;

    /// @brief Show the selected command or fixed keys under the list
    void showDetail();

    /// @brief Put the buttons of the keys beside the field, or under it where they would
    ///        leave it too little room for the longest keys of the list
    void arrangeKeysRow();

    /// @brief Why a command cannot have fixed keys (row: nullptr for the whole group)
    [[nodiscard]] QString explanationOf(const FixedKeyGroup& group, const FixedKeys* row) const;

    /// @brief Show a message under the field
    /// @param html The message (rich text: escape what is not)
    /// @param acceptText The text of a button that goes on (empty: no buttons)
    /// @param accept What the button does
    void showMessage(MessageKind kind, const QString& html, const QString& acceptText = {},
                     std::function<void()> accept = {});
    void hideMessage();

    /// @brief Scroll the page, if it scrolls, so the message is in sight
    void revealMessage();

    /// @brief Colors and icon of the message, of the current theme
    void styleMessage();

    /// @brief Icons of the current theme
    void updateIcons();

    /// @brief Record keys for the selected command (Change...)
    void startChange();
    void removeKeys();
    void restoreDefault();
    void restoreAllDefaults();
    void chooseExportFile();
    void chooseImportFile();

    /// @brief Turn search by keys on or off
    void setKeySearch(bool on);

    /// @brief Record keys in the search field (search by keys)
    void recordSearchKeys();

    /// @brief What the search field asks for, by text and by keys
    [[nodiscard]] static QString textSearchPrompt();
    [[nodiscard]] static QString keySearchPrompt();

    /// @brief Give the list the keys, with a command selected
    void focusList();

    ShortcutRules m_rules;
    std::vector<FixedKeyGroup> m_fixedGroups;
    std::vector<Entry> m_entries;
    CommandRegistry::ShortcutMap m_defaults;  ///< The program's keys of all registered commands
    CommandRegistry::ShortcutMap m_custom;    ///< The user's keys, with the changes on the page

    int m_widestKeys = 0;                         ///< The longest keys the field shows, in pixels
    bool m_keyButtonsUnder = false;               ///< The buttons of the keys are under the field
    bool m_keySearch = false;                     ///< Search by keys is on
    std::optional<QKeyCombination> m_searchKeys;  ///< The keys searched for
    QString m_textQuery;                          ///< The text searched for before it
    MessageKind m_messageKind = MessageKind::Information;
    std::function<void()> m_messageAccept;

    ShortcutRecorder* m_search = nullptr;
    QPushButton* m_byKeysButton = nullptr;
    QCheckBox* m_onlyChanged = nullptr;
    QTreeWidget* m_list = nullptr;
    QTreeWidgetItem* m_noMatchItem = nullptr;
    QWidget* m_detail = nullptr;
    QLabel* m_detailTitle = nullptr;
    QGridLayout* m_keysRow = nullptr;
    QLabel* m_keysLabel = nullptr;
    ShortcutRecorder* m_keysField = nullptr;
    QWidget* m_keyButtons = nullptr;
    QPushButton* m_changeButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_restoreButton = nullptr;
    QLabel* m_detailInfo = nullptr;
    QFrame* m_message = nullptr;
    QLabel* m_messageIcon = nullptr;
    QLabel* m_messageText = nullptr;
    QPushButton* m_messageAcceptButton = nullptr;
    QPushButton* m_messageCancelButton = nullptr;
    QPushButton* m_restoreAllButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QPushButton* m_exportButton = nullptr;
};

} // namespace gui
} // namespace kalahari
