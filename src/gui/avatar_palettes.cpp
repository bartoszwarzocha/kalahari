// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file avatar_palettes.cpp
/// @brief The palettes of the portraits of parts and the names of their colors

#include "kalahari/gui/avatar.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/settings_schema.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include <nlohmann/json_fwd.hpp>

#include <array>
#include <string>

namespace kalahari::gui {

namespace {

/// A text with its comment for the translators, as QT_TRANSLATE_NOOP3 gives it
struct Text {
    const char* source;
    const char* comment;
};

/// A palette and its name
struct PaletteName {
    const char* name;
    Text title;
};

/// The palettes, in order
constexpr std::array PALETTES = {
    PaletteName{"skin", QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "Skin", "palette")},
    PaletteName{"skinOther", QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes",
                                                "Skin, Not Natural", "palette")},
    PaletteName{"eyes", QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "Eyes", "palette")},
    PaletteName{"eyesOther", QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes",
                                                "Eyes, Not Natural", "palette")},
    PaletteName{"hair", QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "Hair", "palette")},
    PaletteName{"hairOther", QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes",
                                                "Hair, Not Natural", "palette")},
    PaletteName{"clothes",
                QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "Clothes", "palette")},
};

/// No text
constexpr Text NONE{nullptr, nullptr};

/// A color of the program and its name
struct ColorName {
    const char* palette;  ///< The palette of the program it is in: "skinOther"
    int index;            ///< Its place in that palette
    Text name;            ///< Its name, for the clothes before a word of the form "m"
    Text feminine;        ///< For the clothes: before a word of the form "f"
    Text plural;          ///< For the clothes: before a word of the form "pl"
};

/// The names of the colors of the palettes of the program ("cards.portraitPalettes" in the
/// schema of the settings), by their places there, for the description of the look. The skin has a
/// name of its own ("fair skin"); the colors of the eyes and of the hair are words before
/// "eyes" and "hair", those of the clothes before the name of the clothes, in its form.
constexpr std::array COLOR_NAMES = {
    ColorName{"skin", 0,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "very fair skin", "skin"),
              NONE, NONE},
    ColorName{"skin", 1,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "fair skin", "skin"),
              NONE, NONE},
    ColorName{"skin", 2,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "peach skin", "skin"),
              NONE, NONE},
    ColorName{"skin", 3,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "olive skin", "skin"),
              NONE, NONE},
    ColorName{"skin", 4,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "tanned skin", "skin"),
              NONE, NONE},
    ColorName{"skin", 5,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "brown skin", "skin"),
              NONE, NONE},
    ColorName{"skin", 6,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "dark skin", "skin"),
              NONE, NONE},
    ColorName{"skin", 7,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "very dark skin", "skin"),
              NONE, NONE},
    ColorName{"skinOther", 0,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "greenish skin", "skin"),
              NONE, NONE},
    ColorName{"skinOther", 1,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "bluish skin", "skin"),
              NONE, NONE},
    ColorName{"skinOther", 2,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "ashen skin", "skin"),
              NONE, NONE},
    ColorName{"skinOther", 3,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "lilac skin", "skin"),
              NONE, NONE},
    ColorName{"eyes", 0,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "brown", "eyes"),
              NONE, NONE},
    ColorName{"eyes", 1,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "dark brown", "eyes"),
              NONE, NONE},
    ColorName{"eyes", 2,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "hazel", "eyes"),
              NONE, NONE},
    ColorName{"eyes", 3,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "green", "eyes"),
              NONE, NONE},
    ColorName{"eyes", 4,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "blue", "eyes"),
              NONE, NONE},
    ColorName{"eyes", 5,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "grey", "eyes"),
              NONE, NONE},
    ColorName{"eyes", 6,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "amber", "eyes"),
              NONE, NONE},
    ColorName{"eyesOther", 0,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "violet", "eyes"),
              NONE, NONE},
    ColorName{"eyesOther", 1,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "red", "eyes"),
              NONE, NONE},
    ColorName{"eyesOther", 2,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "silver", "eyes"),
              NONE, NONE},
    ColorName{"hair", 0,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "black", "hair"),
              NONE, NONE},
    ColorName{"hair", 1,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "dark brown", "hair"),
              NONE, NONE},
    ColorName{"hair", 2,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "brown", "hair"),
              NONE, NONE},
    ColorName{"hair", 3,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "auburn", "hair"),
              NONE, NONE},
    ColorName{"hair", 4,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "red", "hair"),
              NONE, NONE},
    ColorName{"hair", 5,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "dark blond", "hair"),
              NONE, NONE},
    ColorName{"hair", 6,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "light blond", "hair"),
              NONE, NONE},
    ColorName{"hair", 7,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "platinum", "hair"),
              NONE, NONE},
    ColorName{"hair", 8,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "grey", "hair"),
              NONE, NONE},
    ColorName{"hair", 9,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "white", "hair"),
              NONE, NONE},
    ColorName{"hairOther", 0,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "blue", "hair"),
              NONE, NONE},
    ColorName{"hairOther", 1,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "pink", "hair"),
              NONE, NONE},
    ColorName{"hairOther", 2,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "green", "hair"),
              NONE, NONE},
    ColorName{"hairOther", 3,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "purple", "hair"),
              NONE, NONE},
    ColorName{"clothes", 0,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "navy",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "navy",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "navy",
                                 "clothes, form pl")},
    ColorName{"clothes", 1,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "grey",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "grey",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "grey",
                                 "clothes, form pl")},
    ColorName{"clothes", 2,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "black",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "black",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "black",
                                 "clothes, form pl")},
    ColorName{"clothes", 3,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "maroon",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "maroon",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "maroon",
                                 "clothes, form pl")},
    ColorName{"clothes", 4,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "green",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "green",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "green",
                                 "clothes, form pl")},
    ColorName{"clothes", 5,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "brown",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "brown",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "brown",
                                 "clothes, form pl")},
    ColorName{"clothes", 6,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "beige",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "beige",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "beige",
                                 "clothes, form pl")},
    ColorName{"clothes", 7,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "white",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "white",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "white",
                                 "clothes, form pl")},
    ColorName{"clothes", 8,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "mustard",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "mustard",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "mustard",
                                 "clothes, form pl")},
    ColorName{"clothes", 9,
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "light blue",
                                 "clothes, form m"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "light blue",
                                 "clothes, form f"),
              QT_TRANSLATE_NOOP3("kalahari::gui::AvatarPalettes", "light blue",
                                 "clothes, form pl")},
};

/// The text of @p text in the language of the program; empty for none
QString translated(const Text& text) {
    return text.source == nullptr ? QString()
                                  : QCoreApplication::translate("kalahari::gui::AvatarPalettes",
                                                                text.source, text.comment);
}

/// The name of color @p index of palette @p palette of the program; nullptr: none
const ColorName* colorNameAt(const QString& palette, qsizetype index) {
    for (const ColorName& each : COLOR_NAMES) {
        if (palette == QLatin1String(each.palette) && index == each.index) {
            return &each;
        }
    }
    return nullptr;
}

/// The valid colors of @p json, as "#rrggbb"
QStringList colorsOf(const QJsonValue& json) {
    QStringList colors;
    for (const auto& each : json.toArray()) {
        if (QColor::isValidColorName(each.toString())) {
            colors << QColor(each.toString()).name();
        }
    }
    return colors;
}

/// @p json as a QJsonObject; empty when it is not an object
QJsonObject objectOf(const nlohmann::json& json) {
    if (!json.is_object()) {
        return {};
    }
    return QJsonDocument::fromJson(QByteArray::fromStdString(json.dump())).object();
}

}  // namespace

QStringList AvatarPalettes::names() {
    QStringList names;
    for (const PaletteName& each : PALETTES) {
        names << QLatin1String(each.name);
    }
    return names;
}

QString AvatarPalettes::title(const QString& name) {
    for (const PaletteName& each : PALETTES) {
        if (name == QLatin1String(each.name)) {
            return translated(each.title);
        }
    }
    return name;
}

AvatarPalettes AvatarPalettes::defaults() {
    AvatarPalettes palettes;
    if (const nlohmann::json* listed = core::settings_schema::defaultValue(SETTING)) {
        palettes.m_palettes = objectOf(*listed);
    }
    return palettes;
}

AvatarPalettes AvatarPalettes::current() {
    return fromSetting(
        core::SettingsManager::getInstance().get<nlohmann::json>(SETTING, nlohmann::json()));
}

AvatarPalettes AvatarPalettes::fromSetting(const nlohmann::json& value) {
    const AvatarPalettes fallback = defaults();
    const QJsonObject stored = objectOf(value);
    AvatarPalettes palettes;
    for (const QString& name : names()) {
        const QStringList colors = colorsOf(stored.value(name));
        palettes.setColors(name, colors.isEmpty() ? fallback.colors(name) : colors);
    }
    return palettes;
}

nlohmann::json AvatarPalettes::toSetting() const {
    const QByteArray json = QJsonDocument(m_palettes).toJson(QJsonDocument::Compact);
    return nlohmann::json::parse(json.toStdString());
}

void AvatarPalettes::save() const {
    core::SettingsManager::getInstance().set<nlohmann::json>(SETTING, toSetting());
}

QStringList AvatarPalettes::colors(const QString& name) const {
    return colorsOf(m_palettes.value(name));
}

void AvatarPalettes::setColors(const QString& name, const QStringList& colors) {
    m_palettes.insert(name, QJsonArray::fromStringList(colors));
}

QString AvatarPalettes::colorName(const QString& name, const QString& color,
                                  const QString& form) {
    // The color in the palette of the program, or in the one of the colors that are not
    // natural that goes with it
    const QString other = QStringLiteral("Other");
    const QString natural = name.endsWith(other) ? name.chopped(other.size()) : name;
    static const AvatarPalettes program = defaults();
    const QString wanted = QColor(color).name();
    QString palette = natural;
    qsizetype index = program.colors(natural).indexOf(wanted);
    if (index < 0) {
        palette = natural + other;
        index = program.colors(palette).indexOf(wanted);
    }
    const ColorName* found = colorNameAt(palette, index);
    if (found == nullptr) {
        return {};
    }
    if (form == QLatin1String("f")) {
        return translated(found->feminine);
    }
    return form == QLatin1String("pl") ? translated(found->plural) : translated(found->name);
}

}  // namespace kalahari::gui
