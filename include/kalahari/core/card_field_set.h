// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_field_set.h
/// @brief The fields of the cards of a kind as a book has them: the fields of the kind in the
/// writer's order and where each is shown, and the fields the writer added

#ifndef KALAHARI_CORE_CARD_FIELD_SET_H
#define KALAHARI_CORE_CARD_FIELD_SET_H

#include "kalahari/core/book_type_package.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace kalahari::core {

/// @brief A field of the cards of a kind in the set of a book: a field of the kind (by its id),
/// shown where the writer chose, or a field the writer added (isOwn())
///
/// In the .klh file, in the Workshop, by the kind of the cards; a field of the kind has its id
/// and where it is shown, a field of the writer also what it is:
/// @code
/// "cardFields": { "kalahari.base:character": [
///   { "id": "role", "show": ["card", "properties", "list"] },
///   { "id": "own-1", "name": "Rasa", "type": "choice", "show": ["card"],
///     "choices": [ { "id": "c1", "name": "elf" }, { "id": "c2", "name": "krasnolud" } ] },
///   { "id": "own-2", "name": "Mentor", "type": "card", "card": "character",
///     "relation": "own-3", "inverse": "Uczeń", "show": ["card"] } ] }
/// @endcode
/// A field of the kind that the set does not list is hidden, after the fields listed; an
/// entry this version does not understand is kept and saved back.
struct BookCardField {
    /// @brief The start of the ids of the fields of the writer, which no field of a package
    /// has: "own-1"
    static constexpr const char* OWN_PREFIX = "own-";

    QString id;                 ///< A field of the kind: its id; of the writer: "own-1"
    bool onCard = false;        ///< Shown on the full card
    bool inProperties = false;  ///< Shown on the short card of the Properties panel
    bool inList = false;        ///< A column of the list of cards
    // A field of the writer:
    QString name;                    ///< Its name: "Rasa"
    CardFieldType type = CardFieldType::Text;  ///< What it holds
    QList<CardFieldChoice> choices;  ///< Choice: the values in order, each named in every
                                     ///< language (LocalizedText::plain())
    QString cardKind;                ///< Card: the id of the kind of the cards it names, in the
                                     ///< package of its kind: "location"
    QString relation;                ///< Card: the id of the writer's relation (OwnRelation) the
                                     ///< card it names has with the card; empty: none yet
    QString inverseName;             ///< Card: what the card is to the card it names: "Uczeń"
    QJsonObject extra;               ///< Fields this version does not know, saved back

    /// @brief Whether the writer added it
    [[nodiscard]] bool isOwn() const { return id.startsWith(QLatin1String(OWN_PREFIX)); }

    /// @brief Whether it is shown anywhere
    [[nodiscard]] bool isShown() const { return onCard || inProperties || inList; }

    bool operator==(const BookCardField&) const = default;
};

/// @brief The sets of the fields of the cards of a book, by the kind of the cards:
/// "kalahari.base:character" -> its fields in the order of the card; a kind without a set has
/// the fields of its package
using CardFieldSets = QMap<QString, QList<BookCardField>>;

/// @brief The sets of the fields of the cards: read, written, completed with the fields of
/// their kinds
namespace card_fields {

/// @brief The setting of the sets of the fields that new books start with (the same JSON as
/// "cardFields" of the .klh file); empty: the fields of the kinds
inline constexpr const char* DEFAULTS_SETTING = "cards.fields";

/// @brief The fields of kind @p kind as its package has them, each where the package shows it
[[nodiscard]] QList<BookCardField> ofKind(const ElementKind& kind);

/// @brief Every field of kind @p kind in set @p set: the fields @p set lists, in its order
/// (those of the writer, and those of the kind it has), then the fields of the kind it does
/// not list, hidden; an empty set gives the fields of the kind (ofKind())
[[nodiscard]] QList<BookCardField> complete(const ElementKind& kind,
                                            const QList<BookCardField>& set);

/// @brief The fields of the cards of kind @p kind with set @p set (complete()), as the cards
/// show them: a field of the kind with its name, type and values, where the set shows it; a
/// field of the writer as the set has it
[[nodiscard]] QList<CardField> effective(const ElementKind& kind,
                                         const QList<BookCardField>& set);

/// @brief Whether set @p set gives kind @p kind the fields of its package: the same fields,
/// in the same order, shown in the same places
[[nodiscard]] bool isKindSet(const ElementKind& kind, const QList<BookCardField>& set);

/// @brief The entries of @p before that @p kind does not know, which @p after keeps at its
/// end: fields of a newer package, saved back
[[nodiscard]] QList<BookCardField> keepUnknown(const ElementKind& kind,
                                               const QList<BookCardField>& before,
                                               const QList<BookCardField>& after);

/// @brief A new id of a field of the writer: "own-" and a number that neither the fields of
/// @p set nor @p taken (the ids of the values of the cards, kept after a field goes) have
[[nodiscard]] QString newOwnId(const QList<BookCardField>& set, const QStringList& taken);

/// @brief A new id of a value of a field of the writer with a list: "c" and a number that
/// none of @p choices has
[[nodiscard]] QString newChoiceId(const QList<CardFieldChoice>& choices);

/// @brief The sets of "cardFields" @p value; what is not understood is kept in the entries
[[nodiscard]] CardFieldSets read(const QJsonValue& value);

/// @brief "cardFields" of @p sets: an object, empty when there are none
[[nodiscard]] QJsonObject write(const CardFieldSets& sets);

/// @brief The sets that new books start with (DEFAULTS_SETTING)
[[nodiscard]] CardFieldSets defaults();

/// @brief Make @p sets the sets that new books start with (DEFAULTS_SETTING)
void setDefaults(const CardFieldSets& sets);

}  // namespace card_fields

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_CARD_FIELD_SET_H
