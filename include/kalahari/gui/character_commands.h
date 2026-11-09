/// @file character_commands.h
/// @brief The commands that insert a character with keys the writer gave it (Insert > Special
///        Character..., Assign Shortcut...)

#pragma once

#include <QString>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {

/// @brief The start of the ids of the commands of the characters: "insert.character.2014"
inline constexpr const char* CHARACTER_COMMAND_PREFIX = "insert.character.";

/// @brief Inserts characters in the book's text
using InsertCharacters = std::function<void(const QString& characters)>;

/// @brief The id of the command that inserts a character: for those of Insert > Formatting
///        Character the command of the menu, for the others "insert.character.2014"
[[nodiscard]] std::string characterCommandId(char32_t code);

/// @brief The character a command of characterCommandId() inserts; none for another command
[[nodiscard]] std::optional<char32_t> characterOfCommand(const std::string& id);

/// @brief Whether a command is one of the characters outside the menus
///        ("insert.character.2014")
[[nodiscard]] bool isCharacterCommand(const std::string& id);

/// @brief Register the command of a character outside the menus, unless it is registered
///
/// Its label is the name of the character with the character ("Em dash (—)"): Settings >
/// Keyboard Shortcuts lists it so, in the group of the characters.
/// @param code The character (not one of Insert > Formatting Character, which has its command)
/// @param insert Inserts it
/// @return Whether it was registered now
bool registerCharacterCommand(char32_t code, const InsertCharacters& insert);

/// @brief Register the commands of the characters the writer's keys in the settings name (at
///        startup, after the user's keys are given to the registry)
/// @return The ids of those registered now
std::vector<std::string> registerCharacterCommands(const InsertCharacters& insert);

} // namespace gui
} // namespace kalahari
