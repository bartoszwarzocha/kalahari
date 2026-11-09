/// @file special_character_choices.cpp
/// @brief The writer's own characters of Insert > Special Character...: the favorites and the
///        ones inserted last, kept in the settings

#include "kalahari/gui/special_character_choices.h"
#include "kalahari/core/settings_manager.h"

#include <QString>

#include <nlohmann/json.hpp>

#include <string>

namespace kalahari {
namespace gui {

namespace {

/// @brief The characters of a setting: its texts of one character, each once; other values
///        (a word, a number from a file edited by hand) are left out
QList<char32_t> charactersOf(const char* key)
{
    const auto texts = core::SettingsManager::getInstance().get<nlohmann::json>(key);
    QList<char32_t> characters;
    if (!texts.is_array()) {
        return characters;
    }
    for (const nlohmann::json& text : texts) {
        if (!text.is_string()) {
            continue;
        }
        const QList<uint> codes = QString::fromStdString(text.get<std::string>()).toUcs4();
        const char32_t code = codes.size() == 1 ? static_cast<char32_t>(codes.first()) : 0;
        if (code != 0 && !characters.contains(code)) {
            characters.append(code);
        }
    }
    return characters;
}

/// @brief Keep characters in a setting, as texts of one character
void setCharacters(const char* key, const QList<char32_t>& characters)
{
    nlohmann::json texts = nlohmann::json::array();
    for (const char32_t code : characters) {
        texts.push_back(QString::fromUcs4(&code, 1).toStdString());
    }
    core::SettingsManager::getInstance().set<nlohmann::json>(key, texts);
}

} // namespace

QList<char32_t> favoriteCharacters()
{
    return charactersOf(FAVORITE_CHARACTERS_SETTING);
}

bool isFavoriteCharacter(char32_t code)
{
    return favoriteCharacters().contains(code);
}

void setFavoriteCharacter(char32_t code, bool favorite)
{
    QList<char32_t> favorites = favoriteCharacters();
    if (favorites.contains(code) == favorite) {
        return;
    }
    if (favorite) {
        favorites.append(code);
    } else {
        favorites.removeAll(code);
    }
    setCharacters(FAVORITE_CHARACTERS_SETTING, favorites);
}

QList<char32_t> recentCharacters()
{
    QList<char32_t> recent = charactersOf(RECENT_CHARACTERS_SETTING);
    if (recent.size() > RECENT_CHARACTERS) {
        recent.resize(RECENT_CHARACTERS);
    }
    return recent;
}

void addRecentCharacter(char32_t code)
{
    QList<char32_t> recent = recentCharacters();
    if (!recent.isEmpty() && recent.first() == code) {
        return;
    }
    recent.removeAll(code);
    recent.prepend(code);
    if (recent.size() > RECENT_CHARACTERS) {
        recent.resize(RECENT_CHARACTERS);
    }
    setCharacters(RECENT_CHARACTERS_SETTING, recent);
}

} // namespace gui
} // namespace kalahari
