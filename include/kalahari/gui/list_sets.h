// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file list_sets.h
/// @brief The sets of list formats: the formats of all nine levels of a list at once
///
/// A set is the program's own (built-in: numbered, multilevel and bulleted lists), the writer's
/// of the book (saved in the .klh file, like the book's own paragraph styles, so it goes with
/// the book) or the writer's of the program (saved in the Settings, for every book). Format >
/// Bullets and Numbering... chooses one on its tab Sets and saves the formats of a list as a
/// set (Save as Set...); Format > List Sets and the arrows of the buttons Bullets and Numbering
/// give one to the selected paragraphs. F12 and Shift+F12 start a list with the default set of
/// its kind (Settings > Editor > Lists).

#ifndef KALAHARI_GUI_LIST_SETS_H
#define KALAHARI_GUI_LIST_SETS_H

#include "kalahari/editor/text_lists.h"

#include <QJsonArray>
#include <QList>
#include <QString>

#include <optional>

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief Where a set of list formats is kept
enum class ListSetPlace {
    BuiltIn,  ///< The program's own, the same everywhere
    Book,     ///< The writer's, in the book (BookProject::listSets)
    Program,  ///< The writer's, in the Settings, for every book
};

/// @brief The group a set is shown in
enum class ListSetGroup {
    Numbered,    ///< Built-in: one number on each level
    Multilevel,  ///< Built-in: the numbers of the levels above too, or an outline
    Bulleted,    ///< Built-in: bullets
    Own,         ///< The writer's (ListSetPlace::Book or Program)
};

/// @brief A set of list formats
struct ListSet {
    QString id;    ///< Unique among the sets
    QString name;  ///< The writer's name of an own set; empty for a built-in one
    editor::ListDefinition definition;
    ListSetPlace place = ListSetPlace::BuiltIn;
    ListSetGroup group = ListSetGroup::Own;

    /// @brief Its name as the windows show it: the writer's, or the marks of its first three
    ///        levels ("1. – a) – i.")
    [[nodiscard]] QString displayName() const;

    /// @brief The kind of the list it gives: of its first level
    [[nodiscard]] editor::ListKind kind() const { return definition.kind(); }

    bool operator==(const ListSet&) const = default;
};

namespace list_sets {

/// @brief The setting of the writer's sets of the program (a list of { id, name, definition })
inline constexpr const char* PROGRAM_SETS_KEY = "editor.lists.sets";
/// @brief The setting of the id of the set F12 gives (a numbered list)
inline constexpr const char* NUMBERED_DEFAULT_KEY = "editor.lists.numberedSet";
/// @brief The setting of the id of the set Shift+F12 gives (a bulleted list)
inline constexpr const char* BULLETED_DEFAULT_KEY = "editor.lists.bulletedSet";

/// @brief The ids of the built-in sets that are the defaults: 1. – a) – i. and • – ◦ – ▪
inline constexpr const char* NUMBERED_ID = "numbered";
inline constexpr const char* BULLETED_ID = "bulleted";

/// @brief The program's own sets, in the order of their groups: numbered, multilevel, bulleted
[[nodiscard]] QList<ListSet> builtIn();

/// @brief The writer's sets of the program (the Settings)
[[nodiscard]] QList<ListSet> program();

/// @brief Give the program these sets of the writer (the Settings)
void setProgram(const QList<ListSet>& sets);

/// @brief The writer's sets of the open book; none without a book
[[nodiscard]] QList<ListSet> ofBook(const core::ProjectManager& book);

/// @brief Give the open book these sets of the writer and save it
/// @return false without a book, in a book open read-only or when it cannot be saved
bool setOfBook(core::ProjectManager& book, const QList<ListSet>& sets);

/// @brief Every set: the built-in ones, the book's (when @p book is not null) and the
///        program's
[[nodiscard]] QList<ListSet> all(const core::ProjectManager* book);

/// @brief The set of an id among all(); nothing for an id no set has
[[nodiscard]] std::optional<ListSet> find(const QString& id, const core::ProjectManager* book);

/// @brief The first of @p sets with these formats; nothing when none has them (a list changed
///        by hand)
[[nodiscard]] std::optional<ListSet> matching(const editor::ListDefinition& definition,
                                              const QList<ListSet>& sets);

/// @brief The id of the default set of a kind (F12, Shift+F12), as the Settings have it
[[nodiscard]] QString defaultId(editor::ListKind kind);

/// @brief Make a set the default of a kind (the kind of its first level)
void setDefault(editor::ListKind kind, const QString& id);

/// @brief The formats a new list of a kind gets: those of the default set of the kind; the
///        built-in default when the set is gone or of the other kind
[[nodiscard]] editor::ListDefinition defaultDefinition(editor::ListKind kind,
                                                       const core::ProjectManager* book);

/// @brief An id no set has yet
[[nodiscard]] QString newId();

/// @brief The title of a group of sets in the windows and menus ("Numbered", "In This Book")
[[nodiscard]] QString groupTitle(const ListSet& set);

/// @brief The sets as a file of exported sets or the Settings keep them
[[nodiscard]] QJsonArray toJson(const QList<ListSet>& sets);

/// @brief The sets of a list toJson() wrote, given @p place; the entries without formats are
///        left out
[[nodiscard]] QList<ListSet> fromJson(const QJsonArray& array, ListSetPlace place);

/// @brief Write the sets to a file of exported sets (JSON)
/// @return false when the file cannot be written
bool exportTo(const QString& path, const QList<ListSet>& sets);

/// @brief The sets of a file exportTo() wrote; nothing for a file that is not one
[[nodiscard]] std::optional<QList<ListSet>> importFrom(const QString& path);

}  // namespace list_sets

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_LIST_SETS_H
