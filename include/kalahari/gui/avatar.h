// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file avatar.h
/// @brief A portrait put together from parts: the parts and their values the writer chooses
/// from, the colors of the palettes, what a new portrait starts with, the drawing of chance and
/// the description of the look

#ifndef KALAHARI_GUI_AVATAR_H
#define KALAHARI_GUI_AVATAR_H

#include "kalahari/gui/avatar_art.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <nlohmann/json_fwd.hpp>

class QColor;
class QRandomGenerator;

namespace kalahari::core {
struct CardPortrait;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The palettes of the portraits of parts (Settings > Cards > Portrait Palettes...)
///
/// Each one is a list of "#rrggbb"; the colors that are not natural ones ("skinOther": the
/// skin of elves and ghosts) follow those of their palette.
struct AvatarPalettes {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::AvatarPalettes)

public:
    /// @brief The setting of the palettes: an object of the lists by their names
    static constexpr const char* SETTING = "cards.portraitPalettes";

    /// @brief The names of the palettes, in order: "skin", "skinOther", "eyes", "eyesOther",
    /// "hair", "hairOther", "clothes"
    [[nodiscard]] static QStringList names();

    /// @brief The name of palette @p name to show ("Skin")
    [[nodiscard]] static QString title(const QString& name);

    /// @brief The palettes of the program
    [[nodiscard]] static AvatarPalettes defaults();

    /// @brief The palettes of the setting; the program's for a palette it has no color of
    [[nodiscard]] static AvatarPalettes current();

    /// @brief The palettes of @p value, a value of the setting; the program's for a palette it
    /// has no color of
    [[nodiscard]] static AvatarPalettes fromSetting(const nlohmann::json& value);

    /// @brief The palettes as the setting keeps them
    [[nodiscard]] nlohmann::json toSetting() const;

    /// @brief Keep them in the setting
    void save() const;

    /// @brief The colors of palette @p name
    [[nodiscard]] QStringList colors(const QString& name) const;

    /// @brief Give palette @p name the colors @p colors
    void setColors(const QString& name, const QStringList& colors);

    /// @brief The name of @p color of the palette @p name, a color of the program; empty for
    /// another color. For the clothes, @p form is the form of the word the color goes with:
    /// "m" (a sweater), "f" (a shirt) or "pl" (trousers), as the language has them.
    [[nodiscard]] static QString colorName(const QString& name, const QString& color,
                                           const QString& form = QString());

    /// @brief Whether they have the same colors
    bool operator==(const AvatarPalettes&) const = default;

private:
    QJsonObject m_palettes;  ///< The lists by the names
};

/// @brief The parts of a portrait of parts and what the writer does with them
///
/// A portrait is a JSON object of its choices (CardPortrait::avatar): each part ("face",
/// "hair") with the id of its value ("oval", "fringe"), "extras" with a list of them, each
/// color ("skin", "hairColor") with its "#rrggbb", and "sign" whether the corner has the sign
/// of the gender. A value this version does not know stays in the object and is drawn as the
/// default one.
class Avatar {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::Avatar)

public:
    /// @brief A part the writer chooses
    struct Part {
        QString key;          ///< In the portrait: "hair"
        QStringList values;   ///< The ids of its values, in order
        QString fallback;     ///< The value of a new portrait
        bool many = false;    ///< Whether several values go together ("extras")
        bool full = false;    ///< Whether only the whole figure shows it
        bool closeUp = false; ///< Whether its tiles show the face close up
    };

    /// @brief A color the writer chooses
    struct Color {
        QString key;       ///< In the portrait: "hairColor"
        QString palette;   ///< The palette it is chosen from: "hair"
        int fallback = 0;  ///< The color of a new portrait: its place in the palette of the
                           ///< program (AvatarPalettes::defaults())
        bool full = false; ///< Whether only the whole figure shows it
    };

    /// @brief A group of the parts and the colors in the window Portrait
    struct Group {
        QString id;           ///< "face"
        QStringList parts;    ///< Part::key, in order
        QStringList colors;   ///< Color::key, in order
        bool full = false;    ///< Whether only the whole figure shows it
        bool sign = false;    ///< Whether it has the sign of the gender
    };

    /// @brief The key of the sign of the gender in the corner: true or false
    static constexpr const char* SIGN = "sign";

    /// @brief The setting of the style of new portraits: AvatarLook::FLAT (the default) or
    /// AvatarLook::LINE
    static constexpr const char* STYLE_SETTING = "cards.portraitStyle";

    /// @brief The setting of the frame the window Portrait opens with: FRAME_BUST (the
    /// default) or FRAME_FULL
    static constexpr const char* FRAME_SETTING = "cards.portraitFrame";
    static constexpr const char* FRAME_BUST = "bust";  ///< The head and the shoulders
    static constexpr const char* FRAME_FULL = "full";  ///< The whole figure

    /// @brief The parts, in the order of the groups
    [[nodiscard]] static const QList<Part>& parts();

    /// @brief The colors, in the order of the groups
    [[nodiscard]] static const QList<Color>& colors();

    /// @brief The groups, in order
    [[nodiscard]] static const QList<Group>& groups();

    /// @brief Part @p key; nullptr when there is none
    [[nodiscard]] static const Part* part(const QString& key);

    /// @brief Color @p key; nullptr when there is none
    [[nodiscard]] static const Color* color(const QString& key);

    /// @brief The color of a new portrait for @p color, "#rrggbb"
    [[nodiscard]] static QString fallbackOf(const Color& color);

    /// @brief The name of group @p id ("Hair and Beard")
    [[nodiscard]] static QString groupName(const QString& id);

    /// @brief The name of part @p key ("Hairstyle")
    [[nodiscard]] static QString partName(const QString& key);

    /// @brief The name of color @p key ("Hair and Beard Color")
    [[nodiscard]] static QString colorName(const QString& key);

    /// @brief The name of value @p value of part @p key on its tile ("with a fringe")
    [[nodiscard]] static QString valueName(const QString& key, const QString& value);

    /// @brief The portrait a card starts with: each part and color with its value
    [[nodiscard]] static QJsonObject defaults();

    /// @brief A new portrait for a card: the defaults, with the hair, the lashes and the age
    /// suggested by the fields Gender (@p gender: "woman", "man", ...) and Age (@p age, the
    /// text of the field)
    [[nodiscard]] static QJsonObject proposal(const QString& gender, const QString& age);

    /// @brief @p avatar with the values of the drawing: each part and color, a value it does
    /// not know or lacks replaced by the default; what the drawing does not know kept
    [[nodiscard]] static QJsonObject resolved(const QJsonObject& avatar);

    /// @brief @p avatar with the parts and the natural colors of @p palettes drawn by chance,
    /// as gender @p gender suits (a woman gets no beard, a man no dress); the face is calm,
    /// the age, the background and the sign stay
    [[nodiscard]] static QJsonObject randomized(const QJsonObject& avatar, const QString& gender,
                                                const AvatarPalettes& palettes,
                                                QRandomGenerator& random);

    /// @brief The look of @p avatar in words ("Oval face, fair skin, ... Clothes: a navy
    /// sweater."), for the field Appearance; @p full adds the build and the height. A value
    /// the program does not know and a color without a name are left out.
    [[nodiscard]] static QString described(const QJsonObject& avatar, bool full);

    /// @brief The sign of gender @p gender for the corner of a portrait: "♀", "♂" or "⚲"
    [[nodiscard]] static QString signOf(const QString& gender);

    /// @brief The style of new portraits (STYLE_SETTING)
    [[nodiscard]] static QString newStyle();

    /// @brief How portrait @p portrait of a card is drawn in @p frame: in its style, on @p card,
    /// the color of the card, with the sign of gender @p gender when it has the sign
    [[nodiscard]] static AvatarLook lookOf(const core::CardPortrait& portrait, AvatarFrame frame,
                                           const QColor& card, const QString& gender);

    /// @brief Portrait @p portrait of a card drawn as an image of @p pixels (lookOf())
    [[nodiscard]] static QImage imageOf(const core::CardPortrait& portrait, AvatarFrame frame,
                                        const QColor& card, const QString& gender,
                                        const QSize& pixels);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_AVATAR_H
