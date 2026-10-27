/// @file quick_insert_items.h
/// @brief What the list of quick insert offers: the cards of the book's Workshop, the writer's
///        text fragments and the typographic characters, as Settings > Editor > Quick Insert
///        chooses for @ and #

#pragma once

#include <kalahari/editor/quick_insert.h>

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>

#include <nlohmann/json_fwd.hpp>

namespace kalahari::core {
struct BookProject;
class BookTypeRegistry;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The settings of quick insert (Settings > Editor > Quick Insert)
namespace quick_insert_keys {
/// Whether @ and # typed at the start of a word open the list
inline constexpr const char* ENABLED = "editor.quickInsert.enabled";
/// What @ opens: a list of SOURCE_CARDS, SOURCE_FRAGMENTS, SOURCE_CHARACTERS
inline constexpr const char* AT_SOURCES = "editor.quickInsert.at";
/// What # opens, as AT_SOURCES
inline constexpr const char* HASH_SOURCES = "editor.quickInsert.hash";
/// The fragments and characters: [{"name", "text", "kind": "fragment" | "character"}]; none
/// (null): the characters of defaultQuickInsertCharacters()
inline constexpr const char* ITEMS = "editor.quickInsert.items";

inline constexpr const char* SOURCE_CARDS = "cards";
inline constexpr const char* SOURCE_FRAGMENTS = "fragments";
inline constexpr const char* SOURCE_CHARACTERS = "characters";
}  // namespace quick_insert_keys

/// @brief The typographic characters quick insert has before the writer changes them, named in
///        the language of the program
QList<editor::QuickInsertItem> defaultQuickInsertCharacters();

/// @brief The fragments and characters of the settings (ITEMS), with their groups and details
QList<editor::QuickInsertItem> quickInsertFragments();

/// @brief Keep @p items as the fragments and characters of the settings; the characters the
///        program has at first are kept as none (null), so they follow the program's language
void setQuickInsertFragments(const QList<editor::QuickInsertItem>& items);

/// @brief The fragments and characters of a value of ITEMS (none, not a list: the characters
///        the program has at first); entries without a name or a text are left out
QList<editor::QuickInsertItem> quickInsertFragmentsOf(const nlohmann::json& value);

/// @brief The value of ITEMS that keeps @p items
nlohmann::json quickInsertFragmentsValue(const QList<editor::QuickInsertItem>& items);

/// @brief The item of a fragment or a character with the group and the detail the list shows
editor::QuickInsertItem fragmentItem(editor::QuickInsertSource source, const QString& name,
                                     const QString& text);

/// @brief The cards of the Workshop of @p project (characters, locations, items: the elements
///        of the kinds edited as cards), each named by its title, grouped by kind, in the order
///        of the Workshop
QList<editor::QuickInsertItem> cardItems(const core::BookProject& project,
                                         const core::BookTypeRegistry& registry);

/// @brief The characters that open the list as the settings are: "@#", "@", "#" or none
QString quickInsertTriggers();

/// @brief The items of the list opened by @p trigger ('@', '#'; QChar(): all of them, for
///        Insert > Quick Insert...), with the cards of the book open
QList<editor::QuickInsertItem> quickInsertItems(QChar trigger);

}  // namespace kalahari::gui
