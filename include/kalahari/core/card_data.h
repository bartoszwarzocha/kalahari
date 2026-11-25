// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file card_data.h
/// @brief What a card of the Workshop (a character, a location, an item) has beside its text:
/// the values of its fields, its color and its portraits

#ifndef KALAHARI_CORE_CARD_DATA_H
#define KALAHARI_CORE_CARD_DATA_H

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace kalahari::core {

/// @brief A portrait of a card, with its label: a picture among the materials of the book, or
/// one put together from parts (gui::Avatar), which the program draws in any size
struct CardPortrait {
    QString image;       ///< Id of the material whose picture it is; empty: put together
    QString label;       ///< The writer's label, "youth"; empty: none
    QString style;       ///< How a portrait of parts is drawn: "flat" or "line"; empty: flat
    QJsonObject avatar;  ///< The parts and colors of a portrait of parts, with those this
                         ///< version does not know; empty: a picture
    QJsonObject extra;   ///< Fields this version does not know, saved back

    /// @brief Whether it is put together from parts, not a picture
    [[nodiscard]] bool isComposed() const { return image.isEmpty() && !avatar.isEmpty(); }

    /// @brief Whether it is the same portrait, with the same label and fields
    bool operator==(const CardPortrait&) const = default;
};

/// @brief What a card has beside its text, saved with the card as "card"
///
/// In the .klh file:
/// @code
/// "card": { "values": { "role": "main", "age": "29" }, "color": "#c0392b",
///           "portraits": [ { "label": "main", "style": "flat",
///                            "avatar": { "face": "oval", "hair": "fringe", ... } },
///                          { "image": "e31", "label": "youth" } ] }
/// @endcode
/// The text of the card (a .kchapter file) is its long description; its short description
/// ("In short") is the description of the element, its name the element's title.
struct CardData {
    /// @brief Field id (CardField::id) -> value: the text, or the id of the choice of a field
    /// with a list
    QMap<QString, QString> values;
    QString color;                  ///< "#RRGGBB"; empty: none
    QList<CardPortrait> portraits;  ///< In order; the first one, the main one, is shown on
                                    ///< the card, in Properties and in the lists
    QJsonObject extra;              ///< Fields this version does not know, saved back

    /// @brief Whether it says nothing
    [[nodiscard]] bool isEmpty() const;

    /// @brief Value of field @p fieldId; empty when it has none
    [[nodiscard]] QString value(const QString& fieldId) const { return values.value(fieldId); }

    /// @brief The main portrait, shown on the card; nullptr when it has none
    [[nodiscard]] const CardPortrait* mainPortrait() const;

    /// @brief Id of the material of the main portrait when it is a picture; empty when the card
    /// has no portrait or its main portrait is put together from parts
    [[nodiscard]] QString portraitImage() const;

    /// @brief Whether a portrait shows the picture of material @p elementId
    [[nodiscard]] bool hasPortrait(const QString& elementId) const;

    /// @brief Whether it says the same: the values, the color, the portraits and the fields
    bool operator==(const CardData&) const = default;
};

/// @brief A relation between cards that the writer named, kept in the Workshop for the next
/// relations: "creditor" and "debtor"
///
/// Each one has its inverse, which the other card has (CardRelation); a relation that is the
/// same both ways is its own inverse. In the .klh file:
/// @code
/// "relations": [ { "id": "own-1", "kind": "kalahari.base:character",
///                  "other": "kalahari.base:character", "name": "creditor", "inverse": "own-2" } ]
/// @endcode
struct OwnRelation {
    /// @brief The start of the ids of the relations of the writer, which no relation of a
    /// package has: "own-1"
    static constexpr const char* ID_PREFIX = "own-";

    QString id;         ///< Unique among the relations of the Workshop: "own-1"
    QString kind;       ///< The kind of the card that has it: "kalahari.base:character"
    QString otherKind;  ///< The kind of the other card
    QString name;       ///< What the other card is to the card: "creditor"
    QString inverse;    ///< The id of the relation of the other card; its own id when it is the
                        ///< same both ways
    QJsonObject extra;  ///< Fields this version does not know, saved back

    /// @brief Whether it is the same relation, with the same fields
    bool operator==(const OwnRelation&) const = default;
};

/// @brief The colors of the cards
struct CardColors {
    /// @brief The setting of the palette that new cards take their colors from, in order: a
    /// list of "#RRGGBB"
    static constexpr const char* PALETTE_SETTING = "cards.palette";

    /// @brief The setting of whether the Navigator shows the color of a card beside its name
    static constexpr const char* NAVIGATOR_DOT_SETTING = "cards.navigatorColorDot";

    /// @brief The palette of the program, which the setting starts with
    [[nodiscard]] static QStringList defaultPalette();

    /// @brief The palette of the setting; the program's when the setting has no color
    [[nodiscard]] static QStringList palette();

    /// @brief The color of a new card: the first color of @p palette that the fewest of
    /// @p used have; empty when the palette is empty
    [[nodiscard]] static QString next(const QStringList& palette, const QStringList& used);
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_CARD_DATA_H
