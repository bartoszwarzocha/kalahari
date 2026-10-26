/// @file text_lists.h
/// @brief Bulleted and numbered lists: the list a paragraph is an item of, its level, the
///        formats of the levels and the numbers of the items
///
/// A list item is a paragraph whose block format names its list (an id unique in the
/// document) and its level (0 to 8). The items of a list need not follow one another: a
/// paragraph outside the list between two of its items leaves the numbering going on, as
/// Format > Continue Numbering asks. Each item carries the formats of all nine levels of its
/// list, so an item moved to another level takes that level's format. An item may set its
/// own number (the next ones go on from it) or have none: a paragraph in the list's indent
/// without a number or bullet (Backspace at the start of an item).
///
/// The layout shows the numbers (KalahariTextDocumentLayout); KML writes the lists as
/// <ol>, <ul> and <li>, a list of a lower level inside an item (docs/kml_format.md).

#pragma once

#include <QString>
#include <QTextFormat>

#include <array>
#include <optional>
#include <vector>

class QTextBlock;
class QTextBlockFormat;
class QTextDocument;
class QXmlStreamAttributes;

namespace kalahari::editor {

// =============================================================================
// Properties of the block format
// =============================================================================

/// @brief The id of the list the paragraph is an item of (int, greater than 0)
inline constexpr int LIST_ID_PROPERTY = QTextFormat::UserProperty + 140;

/// @brief Level of the item: 0 is the first (int, 0 to LIST_LEVELS - 1)
inline constexpr int LIST_LEVEL_PROPERTY = QTextFormat::UserProperty + 141;

/// @brief The formats of the levels of the list (QString, ListDefinition::toString())
inline constexpr int LIST_DEFINITION_PROPERTY = QTextFormat::UserProperty + 142;

/// @brief Number the item has (int): the next items of its level go on from it
inline constexpr int LIST_VALUE_PROPERTY = QTextFormat::UserProperty + 143;

/// @brief The item has no number or bullet (bool): a paragraph in the indent of the list
inline constexpr int LIST_UNNUMBERED_PROPERTY = QTextFormat::UserProperty + 144;

/// @brief Number of levels of a list
inline constexpr int LIST_LEVELS = 9;

/// @brief Smallest and largest number of an item
inline constexpr int MIN_LIST_NUMBER = 0;
inline constexpr int MAX_LIST_NUMBER = 32767;

// =============================================================================
// Formats of the levels
// =============================================================================

/// @brief How a level marks its items
enum class ListNumberFormat {
    Decimal,     ///< 1, 2, 3
    LowerAlpha,  ///< a, b, c ... z, aa, bb
    UpperAlpha,  ///< A, B, C
    LowerRoman,  ///< i, ii, iii
    UpperRoman,  ///< I, II, III
    Bullet       ///< A character, the same for every item
};

/// @brief Where the number stands at the place of its indent
enum class ListMarkerAlignment { Left, Center, Right };

/// @brief Default distance from the start of the text to the number of the first level,
///        and from one level to the next one: 0.635 cm (a quarter of an inch)
inline constexpr qreal LIST_INDENT_STEP = 18.0;

/// @brief The bullets offered in Format > Bullets and Numbering
inline constexpr char16_t BULLET_DISC = u'•';    ///< •
inline constexpr char16_t BULLET_CIRCLE = u'◦';  ///< ◦
inline constexpr char16_t BULLET_SQUARE = u'▪';  ///< ▪
inline constexpr char16_t BULLET_DASH = u'–';    ///< –
inline constexpr char16_t BULLET_CHECK = u'✓';   ///< ✓
inline constexpr char16_t BULLET_ARROW = u'➢';   ///< ➢

/// @brief The format of one level of a list
struct ListLevel {
    ListNumberFormat format = ListNumberFormat::Decimal;
    QString suffix = QStringLiteral(".");  ///< After the number: "." or ")"
    QString bullet;           ///< The bullet (ListNumberFormat::Bullet)
    int start = 1;            ///< Number of the first item
    qreal markerIndent = LIST_INDENT_STEP;      ///< Points from the start of the text to
                                                ///< the number
    qreal textIndent = 2.0 * LIST_INDENT_STEP;  ///< Points from the start of the text to
                                                ///< the text of the item
    ListMarkerAlignment alignment = ListMarkerAlignment::Left;

    /// @brief Whether the level has bullets instead of numbers
    bool isBullet() const { return format == ListNumberFormat::Bullet; }

    bool operator==(const ListLevel&) const = default;
};

/// @brief The kinds of lists
enum class ListKind { Numbered, Bulleted };

/// @brief The formats of the levels of a list
struct ListDefinition {
    std::array<ListLevel, LIST_LEVELS> levels;

    /// @brief A numbered list: 1., a), i. and again from the fourth level on
    static ListDefinition numbered();

    /// @brief A bulleted list: •, ◦, ▪ and again from the fourth level on
    static ListDefinition bulleted();

    /// @brief The default list of a kind
    static ListDefinition ofKind(ListKind kind);

    /// @brief The default format of a level in a list of a kind (with the indents of the
    ///        level)
    static ListLevel defaultLevel(ListKind kind, int level);

    /// @brief The kind of the list: the kind of its first level
    ListKind kind() const;

    /// @brief The definition as a block format property keeps it
    QString toString() const;

    /// @brief A definition from toString(); the levels it lacks or cannot read are those of a
    ///        numbered list
    static ListDefinition fromString(const QString& text);

    bool operator==(const ListDefinition&) const = default;
};

/// @brief The text of a number in a format, without its suffix: "3", "c", "C", "iii", "III"
///        (empty for Bullet; an alphabetic number past z goes on aa, bb...)
QString listNumberText(int number, ListNumberFormat format);

/// @brief The mark of an item of a level: its number with the suffix, or the bullet
QString listMarkerText(const ListLevel& level, int number);

// =============================================================================
// The items of a document
// =============================================================================

/// @brief A paragraph's place in a list
struct ListItem {
    int id = 0;                  ///< The list (greater than 0)
    int level = 0;               ///< 0 to LIST_LEVELS - 1
    ListDefinition definition;   ///< The formats of the list's levels
    std::optional<int> value;    ///< Its own number: the next ones go on from it
    bool numbered = true;        ///< False: no number or bullet

    /// @brief The format of the item's level
    const ListLevel& levelFormat() const { return definition.levels[static_cast<size_t>(level)]; }

    bool operator==(const ListItem&) const = default;
};

/// @brief The list item a block format is; nothing for a paragraph outside a list
std::optional<ListItem> listItemOf(const QTextBlockFormat& format);

/// @brief Whether a block format is a list item
bool isListItem(const QTextBlockFormat& format);

/// @brief Make a block format a list item (an empty value and numbered leave no property)
void setListItem(QTextBlockFormat& format, const ListItem& item);

/// @brief Take a block format out of its list
void clearListItem(QTextBlockFormat& format);

/// @brief An id no list of the document has
int newListId(const QTextDocument& document);

/// @brief Give the lists of @p content ids the lists of @p target do not have, before
///        @p content goes into @p target (a paste): its items do not join a list of the
///        target
void giveListsNewIds(QTextDocument& content, const QTextDocument& target);

/// @brief The marks of the items of a document, by block number: the number with its suffix
///        or the bullet; empty for a paragraph outside a list and an item without a number
///
/// The numbers of a list go on over the paragraphs outside it. An item of a level starts
/// its level's numbering again after an item of a lower level (1., a), b), 2., a)).
/// @param hasItems Receives whether the document has a list item (also one without a number)
std::vector<QString> listMarkers(const QTextDocument& document, bool* hasItems = nullptr);

/// @brief The number of each item of a document, by block number, as listMarkers() counts
///        them: -1 outside a list and for an item without a number (a bulleted item counts
///        as a numbered one)
std::vector<int> listNumbers(const QTextDocument& document);

// =============================================================================
// KML
// =============================================================================

/// @brief KML element of a level: "ol" for numbers, "ul" for bullets
QString kmlListElementOf(const ListLevel& level);

/// @brief The format of a level from the attributes of its <ol> or <ul> element
///
/// <ol>: type ("1", "a", "A", "i", "I"; "1" without one), suffix ("." without one), start.
/// <ul>: type ("disc", "circle", "square", "dash", "check", "arrow" or the bullet itself;
/// "disc" without one). Both: marker-indent, text-indent (points; without them those of the
/// level), marker-align ("left", "center", "right").
/// @param level 0 to LIST_LEVELS - 1: the default indents
ListLevel listLevelFromKml(bool numbered, const QXmlStreamAttributes& attributes, int level);

/// @brief The attributes of a level's <ol> or <ul> element (each with a leading space)
/// @param level 0 to LIST_LEVELS - 1: the indents of the level are not written
QString listLevelToKml(const ListLevel& level, int levelIndex);

}  // namespace kalahari::editor
