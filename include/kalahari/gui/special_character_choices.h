/// @file special_character_choices.h
/// @brief The writer's own characters of Insert > Special Character...: the favorites and the
///        ones inserted last, kept in the settings

#pragma once

#include <QList>

namespace kalahari {
namespace gui {

/// @brief The setting with the favorite characters, in their order (texts of one character)
inline constexpr const char* FAVORITE_CHARACTERS_SETTING = "specialCharacters.favorites";

/// @brief The setting with the characters inserted last, the last first
inline constexpr const char* RECENT_CHARACTERS_SETTING = "specialCharacters.recent";

/// @brief The setting: whether the window stays open after Insert
inline constexpr const char* KEEP_CHARACTER_WINDOW_OPEN_SETTING = "specialCharacters.keepOpen";

/// @brief The setting with the view the window opens in, the one shown last: "grid" or "list"
inline constexpr const char* CHARACTER_WINDOW_VIEW_SETTING = "specialCharacters.view";

/// @brief How many characters inserted last the settings keep
inline constexpr int RECENT_CHARACTERS = 16;

/// @brief The favorite characters, in their order
[[nodiscard]] QList<char32_t> favoriteCharacters();

/// @brief Whether a character is a favorite
[[nodiscard]] bool isFavoriteCharacter(char32_t code);

/// @brief Make a character a favorite (the last one) or not
void setFavoriteCharacter(char32_t code, bool favorite);

/// @brief The characters inserted last, the last first
[[nodiscard]] QList<char32_t> recentCharacters();

/// @brief A character was inserted: the first of those inserted last
void addRecentCharacter(char32_t code);

} // namespace gui
} // namespace kalahari
