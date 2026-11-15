// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

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

/// @brief A portrait of a card: a picture among the materials of the book, with its label
struct CardPortrait {
    QString image;      ///< Id of the material whose picture it is
    QString label;      ///< The writer's label, "youth"; empty: none
    QJsonObject extra;  ///< Fields this version does not know, saved back

    /// @brief Whether it is the same portrait, with the same label and fields
    bool operator==(const CardPortrait&) const = default;
};

/// @brief What a card has beside its text, saved with the card as "card"
///
/// In the .klh file:
/// @code
/// "card": { "values": { "role": "main", "age": "29" }, "color": "#c0392b",
///           "portraits": [ { "image": "e31", "label": "youth" } ] }
/// @endcode
/// The text of the card (a .kchapter file) is its long description; its short description
/// ("In short") is the description of the element, its name the element's title.
struct CardData {
    /// @brief Field id (CardField::id) -> value: the text, or the id of the choice of a field
    /// with a list
    QMap<QString, QString> values;
    QString color;                  ///< "#RRGGBB"; empty: none
    QList<CardPortrait> portraits;  ///< In order; the first one is shown on the card
    QJsonObject extra;              ///< Fields this version does not know, saved back

    /// @brief Whether it says nothing
    [[nodiscard]] bool isEmpty() const;

    /// @brief Value of field @p fieldId; empty when it has none
    [[nodiscard]] QString value(const QString& fieldId) const { return values.value(fieldId); }

    /// @brief Id of the material of the portrait shown on the card; empty when it has none
    [[nodiscard]] QString portraitImage() const;

    /// @brief Whether a portrait shows the picture of material @p elementId
    [[nodiscard]] bool hasPortrait(const QString& elementId) const;

    /// @brief Whether it says the same: the values, the color, the portraits and the fields
    bool operator==(const CardData&) const = default;
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
