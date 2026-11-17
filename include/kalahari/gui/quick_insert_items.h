// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file quick_insert_items.h
/// @brief What the list of quick insert offers: the names of the book's dictionary and the
///        cards of its Workshop, the writer's text fragments and the typographic characters,
///        as Settings > Editor > Quick Insert chooses for @ and #

#ifndef KALAHARI_GUI_QUICK_INSERT_ITEMS_H
#define KALAHARI_GUI_QUICK_INSERT_ITEMS_H

#include <kalahari/editor/quick_insert.h>

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>

#include <nlohmann/json_fwd.hpp>

namespace kalahari::core {
class BookDictionary;
struct BookProject;
class BookTypeRegistry;
struct NameVariant;
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The settings of quick insert (Settings > Editor > Quick Insert)
namespace quick_insert_keys {
/// Whether @ and # typed at the start of a word open the list
inline constexpr const char* ENABLED = "editor.quickInsert.enabled";
/// What @ opens: a list of SOURCE_CARDS (the names of the book's dictionary and of the cards),
/// SOURCE_FRAGMENTS, SOURCE_CHARACTERS
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
///        of the kinds edited as cards), each named by its title, with its color, grouped by
///        kind, in the order of the Workshop
QList<editor::QuickInsertItem> cardItems(const core::BookProject& project,
                                         const core::BookTypeRegistry& registry);

/// @brief The forms of @p variant as the list shows them under Right: each once, with the
///        cases it is the form of ("dative, locative"), those of the singular first, then
///        those of the plural when @p withPlural; for a language without cases (only the
///        nominative), with the number ("singular"); none when it has a single form
QList<editor::QuickInsertForm> quickInsertForms(const core::NameVariant& variant,
                                                bool withPlural);

/// @brief The names of the book's dictionary that quick insert offers: each variant of each
///        of them in its order (the name itself, its other names with the name they belong to
///        beside them), with its forms, grouped by kind, the kinds in the order of their first
///        name
QList<editor::QuickInsertItem> dictionaryItems(const core::BookDictionary& dictionary);

/// @brief The characters that open the list as the settings are: "@#", "@", "#" or none
QString quickInsertTriggers();

/// @brief The items of the list opened by @p trigger ('@', '#'; QChar(): all of them, for
///        Insert > Quick Insert...), with the names of the dictionary and the cards of the book
///        open in @p book (a card whose name the dictionary has is its name there, and the
///        names of a card have its color)
QList<editor::QuickInsertItem> quickInsertItems(const core::ProjectManager& book, QChar trigger);

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_QUICK_INSERT_ITEMS_H
