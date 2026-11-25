// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file quick_insert.h
/// @brief Quick insert: what the list at the cursor offers and how the typed letters find it
///
/// Typed at the start of a word, @ or # opens a list at the cursor: the names of the book's
/// dictionary with their forms and the names of the cards of the Workshop (characters,
/// locations, items), the writer's text fragments, typographic characters and the own fields
/// of the book, inserted as fields (Settings >
/// Editor > Quick Insert says which list each character opens). The letters typed after it
/// narrow the list, and the item chosen takes the place of the character and the letters
/// (QuickInsertController).

#ifndef KALAHARI_EDITOR_QUICK_INSERT_H
#define KALAHARI_EDITOR_QUICK_INSERT_H

#include <QChar>
#include <QList>
#include <QString>

namespace kalahari::editor {

/// @brief Where an item of quick insert comes from
enum class QuickInsertSource {
    Name,      ///< A name of the book's dictionary, with its forms
    Card,      ///< The name of a card of the Workshop: a character, a location, an item
    Fragment,  ///< A text fragment of the writer
    Character,  ///< A typographic character (–, —, …, „”)
    Field       ///< An own field of the book: it goes in as a field that shows its value
};

/// @brief A form of a name in the list of quick insert
struct QuickInsertForm {
    QString text;   ///< The form: "Annę Wolską"
    QString cases;  ///< The cases it is the form of: "accusative", "dative, locative"

    bool operator==(const QuickInsertForm&) const = default;
};

/// @brief An item of the list of quick insert
struct QuickInsertItem {
    QuickInsertSource source = QuickInsertSource::Fragment;
    QString name;    ///< What the letters find and the list shows: "Anna Wolska", "półpauza"
    QString text;    ///< What is inserted: the name of a card, the text of a fragment
    QString detail;  ///< Shown beside the name: the kind of a card, the text of a fragment
    QString group;   ///< The heading of its group in the list: "Characters", "Text Fragments"
    QString owner;   ///< The name it is another name of ("Anna Wolska" for "Ania"), or empty
    QList<QuickInsertForm> forms;  ///< The forms of a name (→ shows them), its own first
    QString color{};  ///< The color of the card it is the name of ("#c0504d"); empty: none
    QString field{};  ///< QuickInsertSource::Field: the id of the own field (TextField::key)

    bool operator==(const QuickInsertItem&) const = default;
};

/// @brief @p text as quick insert compares it: in lower case, without the marks of the
///        letters ("Żółć" -> "zolc", "Łódź" -> "lodz")
QString foldForSearch(const QString& text);

/// @brief Whether a trigger typed after @p before (the text of its paragraph before it)
///        starts a word: at the start of the paragraph, after a space, a dash, an opening
///        quotation mark or bracket. In a word ("jan@poczta.pl", "C#") it does not open the
///        list.
bool startsQuickInsertWord(const QString& before);

/// @brief Whether @p character can be in the letters typed after a trigger: a letter, a digit,
///        a hyphen, an underscore or an apostrophe. Any other one (a space, a comma, a period)
///        closes the list and stays in the text as typed.
bool continuesQuickInsertQuery(QChar character);

/// @brief The items that the letters @p query find, the best first
///
/// A letter typed finds the words of an item's name that start with it, in any case and
/// without the marks of the letters: "an" finds "Anna Wolska" and "Jan Anioł", "zol" finds
/// "Żoliborz". The items whose name is the letters come first, then those whose name starts
/// with them; otherwise the order of @p items stays. Without letters: all the items.
///
/// The letters that the name of an item does not match but one of its forms does find the
/// form as an item of its own (formItem()): "annę" finds "Annę Wolską". The letters as typed
/// come before the letters without their marks, so "wolską" finds "Annę Wolską", not "Anna
/// Wolska".
QList<QuickInsertItem> matchQuickInsert(const QList<QuickInsertItem>& items, const QString& query);

/// @brief The item that inserts the form @p form of @p item: named by it, with its cases and
///        the name it is a form of beside it ("accusative · Anna (Anna Wolska)")
QuickInsertItem formItem(const QuickInsertItem& item, const QuickInsertForm& form);

/// @brief Whether @p text is a pair of marks that enclose a text ("„”", "()", "«»"): inserted,
///        it leaves the cursor between them
bool isEnclosingPair(const QString& text);

}  // namespace kalahari::editor

#endif  // KALAHARI_EDITOR_QUICK_INSERT_H
