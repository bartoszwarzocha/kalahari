/// @file shortcut_settings.h
/// @brief The user's keys of the commands: in the settings and in the files of Export and Import
///
/// The settings keep only the keys the user changed: command id → the keys in the portable
/// form ("Ctrl+Shift+F"), an empty text for a command left without keys. The program
/// reads them at startup and after Apply in Settings > Keyboard Shortcuts, and gives them
/// to CommandRegistry::setCustomShortcuts().

#pragma once

#include "kalahari/gui/command_registry.h"

#include <QString>
#include <QStringList>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace kalahari {
namespace gui {

class ShortcutRules;

/// @brief The setting with the user's keys
inline constexpr const char* SHORTCUTS_SETTING = "keyboard.shortcuts";

/// @brief The keys in the portable form the settings and the files keep ("" for no keys)
[[nodiscard]] std::string portableKeys(const KeyboardShortcut& keys);

/// @brief The user's keys as JSON: command id → the keys in the portable form
[[nodiscard]] nlohmann::json shortcutsToJson(const CommandRegistry::ShortcutMap& custom);

/// @brief The user's keys read from JSON
/// @param json An object of command ids and keys
/// @param unreadable The ids whose keys are not keys of one key press (appended to)
[[nodiscard]] CommandRegistry::ShortcutMap shortcutsFromJson(const nlohmann::json& json,
                                                             QStringList* unreadable = nullptr);

/// @brief The user's keys without those a command cannot have
///
/// Leaves out the keys the rules refuse (e.g. keys from the settings of another system)
/// and the keys of a command given the same keys as a command before it by id.
/// @param custom The user's keys
/// @param rules The rules of the system
/// @param dropped Why each key was left out (appended to)
[[nodiscard]] CommandRegistry::ShortcutMap validShortcuts(
    const CommandRegistry::ShortcutMap& custom, const ShortcutRules& rules,
    QStringList* dropped = nullptr);

/// @brief The user's keys from the settings, without those a command cannot have
[[nodiscard]] CommandRegistry::ShortcutMap loadCustomShortcuts();

/// @brief The contents of the file Export writes
[[nodiscard]] nlohmann::json shortcutsFile(const CommandRegistry::ShortcutMap& custom);

/// @brief The keys of a file Export wrote (or of an object of command ids and keys)
/// @return The object of command ids and keys, or nothing for another file
[[nodiscard]] std::optional<nlohmann::json> shortcutsOfFile(const nlohmann::json& file);

} // namespace gui
} // namespace kalahari
