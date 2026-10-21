/// @file special_character_coordinator.cpp
/// @brief Insert > Special Character... and Insert > Quick Character: the window and the
///        palette, the characters they insert in the text in front, the keys of the characters
///        and the replacements of auto-correct the window adds

#include "kalahari/gui/special_character_coordinator.h"
#include "kalahari/core/logger.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/auto_correct_replacements.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/character_dialogs.h"
#include "kalahari/gui/dialogs/special_character_dialog.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/special_character_choices.h"
#include "kalahari/gui/special_characters.h"
#include "kalahari/gui/widgets/quick_character_palette.h"

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QRect>
#include <QStatusBar>

#include <utility>

namespace kalahari {
namespace gui {

namespace {

/// @brief How long a message stays in the status bar (ms)
constexpr int MESSAGE_MS = 4000;

/// @brief The command that opens the window
constexpr const char* WINDOW_COMMAND = "insert.specialChar";

/// @brief The command that opens the palette
constexpr const char* PALETTE_COMMAND = "insert.quickCharacter";

/// @brief A character as the messages name it: its name and the character
QString characterTitle(char32_t code)
{
    const SpecialCharacter character = SpecialCharacters().character(code);
    const QString shown = character.mark.isEmpty() ? character.text() : character.mark;
    return QStringLiteral("%1 (%2)").arg(character.name, shown);
}

} // namespace

SpecialCharacterCoordinator::SpecialCharacterCoordinator(QWidget* window, QStatusBar* statusBar,
                                                         EditorInFront editorInFront,
                                                         InsertCharacters insertWithKeys,
                                                         TextCommands textCommands,
                                                         QObject* parent)
    : QObject(parent)
    , m_window(window)
    , m_statusBar(statusBar)
    , m_editorInFront(std::move(editorInFront))
    , m_insertWithKeys(std::move(insertWithKeys))
    , m_textCommands(std::move(textCommands))
{
}

SpecialCharacterCoordinator::~SpecialCharacterCoordinator()
{
    QObject::disconnect(m_focusConnection);
    // The commands outlive the coordinator
    for (const char* id : {WINDOW_COMMAND, PALETTE_COMMAND}) {
        if (Command* command = CommandRegistry::getInstance().getCommand(id)) {
            command->execute = nullptr;
        }
    }
    for (const Command& command : CommandRegistry::getInstance().getAllCommands()) {
        if (isCharacterCommand(command.id)) {
            if (Command* registered = CommandRegistry::getInstance().getCommand(command.id)) {
                registered->execute = nullptr;
            }
        }
    }
}

void SpecialCharacterCoordinator::connectCommands()
{
    if (Command* command = CommandRegistry::getInstance().getCommand(WINDOW_COMMAND)) {
        command->execute = [this]() { openWindow(); };
    }
    if (Command* command = CommandRegistry::getInstance().getCommand(PALETTE_COMMAND)) {
        command->execute = [this]() { openPalette(); };
    }
    const std::vector<std::string> ids = registerCharacterCommands(m_insertWithKeys);
    attachCommands(ids);
    if (!ids.empty()) {
        core::Logger::getInstance().info("Special characters: {} with the writer's keys",
                                         ids.size());
    }
}

void SpecialCharacterCoordinator::attachCommands(const std::vector<std::string>& ids)
{
    if (ids.empty()) {
        return;
    }
    // No menu has them: their keys work in the window that has their actions
    CommandRegistry& registry = CommandRegistry::getInstance();
    for (const std::string& id : ids) {
        if (QAction* action = registry.getAction(id); action != nullptr && m_window != nullptr) {
            m_window->addAction(action);
        }
    }
    if (m_textCommands) {
        m_textCommands(ids);
    }
}

// ============================================================================
// The window
// ============================================================================

void SpecialCharacterCoordinator::openWindow()
{
    if (m_dialog == nullptr) {
        auto* dialog = new dialogs::SpecialCharacterDialog(
            [this]() {
                dialogs::CharacterTarget target;
                target.whyNot = whyNotInsertable();
                if (const editor::BookEditor* text = m_editorInFront ? m_editorInFront()
                                                                     : nullptr) {
                    target.fontFamily = text->currentFontFamily();
                }
                return target;
            },
            [](char32_t code) { return keysOf(code); },
            [](char32_t code) {
                return autoCorrectSources(SpecialCharacters().character(code).text());
            },
            m_window);
        connect(dialog, &QObject::destroyed, this, [this]() { m_dialog = nullptr; });
        connect(dialog, &dialogs::SpecialCharacterDialog::insertRequested, this,
                [this](char32_t code) { insert(code); });
        connect(dialog, &dialogs::SpecialCharacterDialog::shortcutRequested, this,
                &SpecialCharacterCoordinator::chooseShortcut);
        connect(dialog, &dialogs::SpecialCharacterDialog::replacementRequested, this,
                &SpecialCharacterCoordinator::chooseReplacement);
        // Closed after Insert: the keys go back to the text
        connect(dialog, &QDialog::accepted, this, [this]() {
            if (m_lastEditor != nullptr) {
                m_lastEditor->window()->activateWindow();
                m_lastEditor->setFocus(Qt::OtherFocusReason);
            }
        });
        // Another chapter in front, a book open read-only: Insert follows it
        m_focusConnection = connect(qApp, &QApplication::focusChanged, this, [this]() {
            if (m_dialog != nullptr && m_dialog->isVisible()) {
                m_dialog->updateTarget();
            }
        });
        m_dialog = dialog;
    }
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
}

void SpecialCharacterCoordinator::openPalette()
{
    editor::BookEditor* text = m_editorInFront ? m_editorInFront() : nullptr;
    if (text == nullptr || text->isReadOnly()) {
        showMessage(whyNotInsertable());
        return;
    }
    if (m_palette != nullptr) {
        m_palette->close();
    }
    auto* palette = new QuickCharacterPalette(
        text->currentFontFamily(), [](char32_t code) { return keysOf(code); },
        [](char32_t code) {
            return autoCorrectSources(SpecialCharacters().character(code).text());
        },
        m_window);
    connect(palette, &QObject::destroyed, this, [this, palette]() {
        if (m_palette == palette) {
            m_palette = nullptr;
        }
    });
    connect(palette, &QuickCharacterPalette::insertRequested, this,
            [this](char32_t code) { insert(code); });
    connect(palette, &QuickCharacterPalette::allCharactersRequested, this,
            &SpecialCharacterCoordinator::openWindow);
    m_palette = palette;

    // Under the cursor of the text, in the coordinates of the screen
    const QRect cursor =
        text->inputMethodQuery(Qt::ImCursorRectangle).toRectF().toAlignedRect();
    palette->showAt(QRect(text->mapToGlobal(cursor.topLeft()), cursor.size()));
}

QString SpecialCharacterCoordinator::whyNotInsertable() const
{
    const editor::BookEditor* text = m_editorInFront ? m_editorInFront() : nullptr;
    if (text == nullptr) {
        return tr("No chapter or file is open");
    }
    return text->isReadOnly() ? readOnlyReason() : QString();
}

bool SpecialCharacterCoordinator::insert(char32_t code)
{
    editor::BookEditor* text = m_editorInFront ? m_editorInFront() : nullptr;
    if (code == 0 || text == nullptr || text->isReadOnly()) {
        return false;
    }
    const SpecialCharacter character = SpecialCharacters().character(code);
    text->insertCharacters(character.text());
    if (m_lastEditor != text) {
        QObject::disconnect(m_lastEditorGone);
        m_lastEditor = text;
        m_lastEditorGone =
            connect(text, &QObject::destroyed, this, [this]() { m_lastEditor = nullptr; });
    }
    addRecentCharacter(code);
    if (m_dialog != nullptr) {
        m_dialog->refresh();
    }
    //: %1 is a character with its name, as "Em dash (—)"
    showMessage(tr("Inserted: %1").arg(characterTitle(code)));
    return true;
}

// ============================================================================
// Keys and replacements
// ============================================================================

QString SpecialCharacterCoordinator::keysOf(char32_t code)
{
    CommandRegistry& registry = CommandRegistry::getInstance();
    const std::string id = characterCommandId(code);
    if (!registry.isCommandRegistered(id)) {
        return {};
    }
    const QAction* action = registry.getAction(id);
    const QKeySequence keys = action != nullptr ? action->shortcut() : QKeySequence();
    return keys.isEmpty() ? QString() : ShortcutRules::keysText(keys[0]);
}

void SpecialCharacterCoordinator::addCharacterCommand(char32_t code)
{
    if (registerCharacterCommand(code, m_insertWithKeys)) {
        attachCommands({characterCommandId(code)});
    }
}

void SpecialCharacterCoordinator::setCharacterKeys(char32_t code, const KeyboardShortcut& keys)
{
    const std::string id = characterCommandId(code);
    if (!keys.isEmpty()) {
        addCharacterCommand(code);
    }
    const std::string owner = ownerOfKeys(keys);
    saveCustomShortcuts(withKeys(id, keys));
    core::Logger::getInstance().info("Special characters: {} for {}",
                                     keys.isEmpty() ? std::string("no keys")
                                                    : portableKeys(keys),
                                     id);
    if (keys.isEmpty()) {
        //: %1 is a character with its name, as "Em dash (—)"
        showMessage(tr("%1 has no shortcut now.").arg(characterTitle(code)));
    } else if (!owner.empty() && owner != id) {
        const Command* command = CommandRegistry::getInstance().getCommand(owner);
        //: %1 are keys, %2 a character with its name, %3 the command that had the keys
        showMessage(tr("%1 inserts %2 now; %3 has no shortcut.")
                        .arg(keysOf(code), characterTitle(code),
                             command != nullptr ? QString::fromStdString(command->label)
                                                : QString::fromStdString(owner)));
    } else {
        //: %1 are keys, %2 a character with its name
        showMessage(tr("%1 inserts %2 now.").arg(keysOf(code), characterTitle(code)));
    }
    if (m_dialog != nullptr) {
        m_dialog->refresh();
    }
}

void SpecialCharacterCoordinator::chooseShortcut(char32_t code)
{
    if (code == 0) {
        return;
    }
    const std::string id = characterCommandId(code);
    KeyboardShortcut keysNow;
    if (CommandRegistry::getInstance().isCommandRegistered(id)) {
        const QAction* action = CommandRegistry::getInstance().getAction(id);
        if (action != nullptr && !action->shortcut().isEmpty()) {
            keysNow = KeyboardShortcut::fromQKeySequence(action->shortcut());
        }
    }
    dialogs::CharacterShortcutDialog dialog(code, id, keysNow, m_dialog);
    if (dialog.exec() == QDialog::Accepted && dialog.keys() != keysNow) {
        setCharacterKeys(code, dialog.keys());
    }
}

void SpecialCharacterCoordinator::chooseReplacement(char32_t code)
{
    if (code == 0) {
        return;
    }
    dialogs::ReplacementDialog dialog(code, m_dialog);
    if (dialog.exec() != QDialog::Accepted || dialog.word().isEmpty()) {
        return;
    }
    addReplacement(dialog.word(), dialog.replacement());
    core::Logger::getInstance().info("Special characters: replacement {} added",
                                     dialog.word().toStdString());
    //: %1 is a word, %2 a character with its name
    showMessage(tr("Auto-correct replaces “%1” with %2 now.")
                    .arg(dialog.word(), characterTitle(code)));
    if (m_dialog != nullptr) {
        m_dialog->refresh();
    }
}

void SpecialCharacterCoordinator::showMessage(const QString& message)
{
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(message, MESSAGE_MS);
    }
}

} // namespace gui
} // namespace kalahari
