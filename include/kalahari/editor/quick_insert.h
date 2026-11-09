/// @file quick_insert.h
/// @brief Quick insert: what the list at the cursor offers and how the typed letters find it
///
/// Typed at the start of a word, @ or # opens a list at the cursor: the names of the cards of
/// the Workshop (characters, locations, items), the writer's text fragments and typographic
/// characters (Settings > Editor > Quick Insert says which list each character opens). The
/// letters typed after it narrow the list, and the item chosen takes the place of the
/// character and the letters (QuickInsertController).

#pragma once

#include <QChar>
#include <QList>
#include <QString>

namespace kalahari::editor {

/// @brief Where an item of quick insert comes from
enum class QuickInsertSource {
    Card,      ///< The name of a card of the Workshop: a character, a location, an item
    Fragment,  ///< A text fragment of the writer
    Character  ///< A typographic character (–, —, …, „”)
};

/// @brief An item of the list of quick insert
struct QuickInsertItem {
    QuickInsertSource source = QuickInsertSource::Fragment;
    QString name;    ///< What the letters find and the list shows: "Anna Wolska", "półpauza"
    QString text;    ///< What is inserted: the name of a card, the text of a fragment
    QString detail;  ///< Shown beside the name: the kind of a card, the text of a fragment
    QString group;   ///< The heading of its group in the list: "Characters", "Text Fragments"

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
/// "Żoliborz". The items whose name starts with the letters come first; otherwise the order
/// of @p items stays. Without letters: all the items.
QList<QuickInsertItem> matchQuickInsert(const QList<QuickInsertItem>& items, const QString& query);

/// @brief Whether @p text is a pair of marks that enclose a text ("„”", "()", "«»"): inserted,
///        it leaves the cursor between them
bool isEnclosingPair(const QString& text);

}  // namespace kalahari::editor
