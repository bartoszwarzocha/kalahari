/// @file character_commands.cpp
/// @brief The commands that insert a character with keys the writer gave it

#include "kalahari/gui/character_commands.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/special_characters.h"

#include <QCoreApplication>

#include <array>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

/// The characters of Insert > Formatting Character and their commands
constexpr std::array<std::pair<char32_t, const char*>, 5> MENU_CHARACTERS = {{
    {0x00A0, "insert.noBreakSpace"},
    {0x2011, "insert.nonBreakingHyphen"},
    {0x00AD, "insert.optionalHyphen"},
    {0x2013, "insert.enDash"},
    {0x2014, "insert.emDash"},
}};

constexpr int HEXADECIMAL = 16;
constexpr int CODE_DIGITS = 4;
constexpr char32_t LAST_CODE = 0x10FFFF;

} // namespace

std::string characterCommandId(char32_t code)
{
    for (const auto& [character, id] : MENU_CHARACTERS) {
        if (character == code) {
            return id;
        }
    }
    return std::string(CHARACTER_COMMAND_PREFIX) +
           QString::number(static_cast<uint>(code), HEXADECIMAL)
               .toUpper()
               .rightJustified(CODE_DIGITS, QLatin1Char('0'))
               .toStdString();
}

std::optional<char32_t> characterOfCommand(const std::string& id)
{
    for (const auto& [character, menuId] : MENU_CHARACTERS) {
        if (id == menuId) {
            return character;
        }
    }
    const std::string prefix = CHARACTER_COMMAND_PREFIX;
    if (id.size() <= prefix.size() || id.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }
    const QString digits = QString::fromStdString(id.substr(prefix.size()));
    bool number = false;
    const uint code = digits.toUInt(&number, HEXADECIMAL);
    // Only the id the character has: "insert.character.00E9", not "insert.character.e9",
    // nor one of a character of the menu
    if (!number || code == 0 || code > LAST_CODE ||
        characterCommandId(static_cast<char32_t>(code)) != id) {
        return std::nullopt;
    }
    return static_cast<char32_t>(code);
}

bool isCharacterCommand(const std::string& id)
{
    const std::string prefix = CHARACTER_COMMAND_PREFIX;
    return id.compare(0, prefix.size(), prefix) == 0 && characterOfCommand(id).has_value();
}

bool registerCharacterCommand(char32_t code, const InsertCharacters& insert)
{
    CommandRegistry& registry = CommandRegistry::getInstance();
    const std::string id = characterCommandId(code);
    if (!isCharacterCommand(id) || registry.isCommandRegistered(id)) {
        return false;
    }
    const SpecialCharacter character = SpecialCharacters().character(code);
    const QString shown = character.mark.isEmpty() ? character.text() : character.mark;
    //: The name of a command that inserts a character (Settings > Keyboard Shortcuts, the
    //: group of the characters): its name and the character, as "Em dash (—)"
    const QString label = QCoreApplication::translate("kalahari::gui::CharacterCommands",
                                                      "%1 (%2)")
                              .arg(character.name, shown);
    QString menuText = label;
    menuText.replace(QLatin1Char('&'), QStringLiteral("&&"));

    Command command;
    command.id = id;
    command.label = label.toStdString();
    command.menuText = menuText.toStdString();
    command.tooltip = command.label;
    command.category = "CHARACTERS";
    command.showInMenu = false;
    command.showInToolbar = false;
    const QString text = character.text();
    command.execute = [insert, text]() {
        if (insert) {
            insert(text);
        }
    };
    registry.registerCommand(std::move(command));
    return true;
}

std::vector<std::string> registerCharacterCommands(const InsertCharacters& insert)
{
    std::vector<std::string> registered;
    for (const auto& [id, keys] : CommandRegistry::getInstance().customShortcuts()) {
        const std::optional<char32_t> code =
            isCharacterCommand(id) ? characterOfCommand(id) : std::nullopt;
        if (keys.isEmpty() || !code.has_value()) {
            continue;
        }
        if (registerCharacterCommand(*code, insert)) {
            registered.push_back(id);
        }
    }
    return registered;
}

} // namespace gui
} // namespace kalahari
