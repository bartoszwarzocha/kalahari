// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file avatar.cpp
/// @brief The parts of a portrait of parts, their names and what the writer does with them

#include "kalahari/gui/avatar.h"

#include "kalahari/core/card_data.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/avatar_art.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QJsonValue>
#include <QPalette>
#include <QRandomGenerator>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace kalahari::gui {

namespace {

/// The parts, in the order of the groups
const QList<Avatar::Part>& partList() {
    static const QList<Avatar::Part> parts = {
        {QStringLiteral("face"), {QStringLiteral("oval"), QStringLiteral("round"),
          QStringLiteral("square"), QStringLiteral("long"), QStringLiteral("heart")},
         QStringLiteral("oval")},
        {QStringLiteral("age"), {QStringLiteral("young"), QStringLiteral("adult"),
          QStringLiteral("middle"), QStringLiteral("old")},
         QStringLiteral("adult"), false, false, true},
        {QStringLiteral("eyes"), {QStringLiteral("round"), QStringLiteral("almond"),
          QStringLiteral("narrow"), QStringLiteral("big")},
         QStringLiteral("round"), false, false, true},
        {QStringLiteral("lashes"), {QStringLiteral("plain"), QStringLiteral("long")},
         QStringLiteral("plain"), false, false, true},
        {QStringLiteral("brows"), {QStringLiteral("straight"), QStringLiteral("arched"),
          QStringLiteral("thick"), QStringLiteral("raised"), QStringLiteral("frowning")},
         QStringLiteral("straight"), false, false, true},
        {QStringLiteral("nose"), {QStringLiteral("small"), QStringLiteral("straight"),
          QStringLiteral("aquiline"), QStringLiteral("wide")},
         QStringLiteral("straight"), false, false, true},
        {QStringLiteral("mouth"), {QStringLiteral("neutral"), QStringLiteral("serious"),
          QStringLiteral("sad"), QStringLiteral("surprised"), QStringLiteral("smile")},
         QStringLiteral("neutral"), false, false, true},
        {QStringLiteral("hair"), {QStringLiteral("short"), QStringLiteral("fringe"),
          QStringLiteral("shoulder"), QStringLiteral("long"), QStringLiteral("bun"),
          QStringLiteral("curly"), QStringLiteral("bald")},
         QStringLiteral("short")},
        {QStringLiteral("beard"), {QStringLiteral("none"), QStringLiteral("stubble"),
          QStringLiteral("beard"), QStringLiteral("moustache"), QStringLiteral("full")},
         QStringLiteral("none")},
        {QStringLiteral("glasses"), {QStringLiteral("none"), QStringLiteral("round"),
          QStringLiteral("rectangular"), QStringLiteral("dark"), QStringLiteral("monocle")},
         QStringLiteral("none")},
        {QStringLiteral("hat"), {QStringLiteral("none"), QStringLiteral("hat"),
          QStringLiteral("hood"), QStringLiteral("diadem")},
         QStringLiteral("none")},
        {QStringLiteral("extras"), {QStringLiteral("freckles"), QStringLiteral("blush"),
          QStringLiteral("mole"), QStringLiteral("scar"), QStringLiteral("earrings"),
          QStringLiteral("eyePatch")},
         QString(), true},
        {QStringLiteral("clothes"), {QStringLiteral("shirt"), QStringLiteral("sweater"),
          QStringLiteral("coat"), QStringLiteral("robe"), QStringLiteral("armor"),
          QStringLiteral("dress")},
         QStringLiteral("shirt")},
        {QStringLiteral("bottom"), {QStringLiteral("trousers"), QStringLiteral("skirt")},
         QStringLiteral("trousers"), false, true},
        {QStringLiteral("background"), {QStringLiteral("card"), QStringLiteral("gradient"),
          QStringLiteral("light"), QStringLiteral("none")},
         QStringLiteral("card")},
        {QStringLiteral("build"), {QStringLiteral("slim"), QStringLiteral("average"),
          QStringLiteral("strong")},
         QStringLiteral("average"), false, true},
        {QStringLiteral("height"), {QStringLiteral("short"), QStringLiteral("medium"),
          QStringLiteral("tall")},
         QStringLiteral("medium"), false, true},
    };
    return parts;
}

/// The colors, in the order of the groups
const QList<Avatar::Color>& colorList() {
    static const QList<Avatar::Color> colors = {
        // Fair skin, brown eyes and hair, grey clothes and navy trousers
        {QStringLiteral("skin"), QStringLiteral("skin"), 1},
        {QStringLiteral("eyeColor"), QStringLiteral("eyes"), 0},
        {QStringLiteral("hairColor"), QStringLiteral("hair"), 2},
        {QStringLiteral("clothesColor"), QStringLiteral("clothes"), 1},
        {QStringLiteral("bottomColor"), QStringLiteral("clothes"), 0, true},
    };
    return colors;
}

/// A text with its comment for the translators, as QT_TRANSLATE_NOOP3 gives it
struct Text {
    const char* source;
    const char* comment;
};

/// A value of a part and its name on its tile
struct ValueName {
    const char* key;
    const char* value;
    Text name;
};

/// The names of the values of the parts on their tiles
constexpr std::array VALUE_NAMES = {
    ValueName{"face", "oval", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "oval", "face shape")},
    ValueName{"face", "round", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "round", "face shape")},
    ValueName{"face", "square",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "square", "face shape")},
    ValueName{"face", "long", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "long", "face shape")},
    ValueName{"face", "heart", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "heart", "face shape")},
    ValueName{"age", "young",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "under 20", "apparent age")},
    ValueName{"age", "adult",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "20 to 40", "apparent age")},
    ValueName{"age", "middle",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "40 to 60", "apparent age")},
    ValueName{"age", "old", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "over 60", "apparent age")},
    ValueName{"eyes", "round", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "round", "eyes")},
    ValueName{"eyes", "almond", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "almond", "eyes")},
    ValueName{"eyes", "narrow", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "narrow", "eyes")},
    ValueName{"eyes", "big", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "big", "eyes")},
    ValueName{"lashes", "plain", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "plain", "lashes")},
    ValueName{"lashes", "long", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "long", "lashes")},
    ValueName{"brows", "straight",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "straight", "brows")},
    ValueName{"brows", "arched", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "arched", "brows")},
    ValueName{"brows", "thick", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "thick", "brows")},
    ValueName{"brows", "raised", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "raised", "brows")},
    ValueName{"brows", "frowning",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "frowning", "brows")},
    ValueName{"nose", "small", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "small", "nose")},
    ValueName{"nose", "straight", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "straight", "nose")},
    ValueName{"nose", "aquiline", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "aquiline", "nose")},
    ValueName{"nose", "wide", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "wide", "nose")},
    ValueName{"mouth", "neutral", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "neutral", "mouth")},
    ValueName{"mouth", "serious", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "serious", "mouth")},
    ValueName{"mouth", "sad", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "sad", "mouth")},
    ValueName{"mouth", "surprised",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "surprised", "mouth")},
    ValueName{"mouth", "smile", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "smiling", "mouth")},
    ValueName{"hair", "short", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "short", "hairstyle")},
    ValueName{"hair", "fringe",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "with a fringe", "hairstyle")},
    ValueName{"hair", "shoulder",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "shoulder-length", "hairstyle")},
    ValueName{"hair", "long", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "long", "hairstyle")},
    ValueName{"hair", "bun", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "bun", "hairstyle")},
    ValueName{"hair", "curly", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "curly", "hairstyle")},
    ValueName{"hair", "bald", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "bald", "hairstyle")},
    ValueName{"beard", "none", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "none", "beard")},
    ValueName{"beard", "stubble", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "stubble", "beard")},
    ValueName{"beard", "beard", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "beard", "beard")},
    ValueName{"beard", "moustache",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "moustache", "beard")},
    ValueName{"beard", "full",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "beard and moustache", "beard")},
    ValueName{"glasses", "none", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "none", "glasses")},
    ValueName{"glasses", "round", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "round", "glasses")},
    ValueName{"glasses", "rectangular",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "rectangular", "glasses")},
    ValueName{"glasses", "dark", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "dark", "glasses")},
    ValueName{"glasses", "monocle",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "monocle", "glasses")},
    ValueName{"hat", "none", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "none", "headwear")},
    ValueName{"hat", "hat", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "hat", "headwear")},
    ValueName{"hat", "hood", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "hood", "headwear")},
    ValueName{"hat", "diadem", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "diadem", "headwear")},
    ValueName{"extras", "freckles",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "freckles", "extras")},
    ValueName{"extras", "blush",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "rosy cheeks", "extras")},
    ValueName{"extras", "mole", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "mole", "extras")},
    ValueName{"extras", "scar", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "scar", "extras")},
    ValueName{"extras", "earrings",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "earrings", "extras")},
    ValueName{"extras", "eyePatch",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "eye patch", "extras")},
    ValueName{"clothes", "shirt", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "shirt", "clothes")},
    ValueName{"clothes", "sweater",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "sweater", "clothes")},
    ValueName{"clothes", "coat", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "coat", "clothes")},
    ValueName{"clothes", "robe", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "robe", "clothes")},
    ValueName{"clothes", "armor", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "armor", "clothes")},
    ValueName{"clothes", "dress", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "dress", "clothes")},
    ValueName{"bottom", "trousers",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "trousers", "lower clothes")},
    ValueName{"bottom", "skirt",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "skirt", "lower clothes")},
    ValueName{"background", "card",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "card color", "background")},
    ValueName{"background", "gradient",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "gradient", "background")},
    ValueName{"background", "light",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "light", "background")},
    ValueName{"background", "none",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "none", "background")},
    ValueName{"build", "slim", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "slim", "build")},
    ValueName{"build", "average", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "average", "build")},
    ValueName{"build", "strong", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "strong", "build")},
    ValueName{"height", "short", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "short", "height")},
    ValueName{"height", "medium",
              QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "medium", "height")},
    ValueName{"height", "tall", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "tall", "height")},
};

/// A key and its name
struct KeyName {
    const char* key;
    Text name;
};

/// The names of the groups in the window Portrait
constexpr std::array GROUP_NAMES = {
    KeyName{"face", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Face and Skin", "group")},
    KeyName{"eyes", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Eyes and Brows", "group")},
    KeyName{"nose", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Nose and Mouth", "group")},
    KeyName{"hair", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Hair and Beard", "group")},
    KeyName{"extras",
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Glasses and Extras", "group")},
    KeyName{"clothes", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Clothes", "group")},
    KeyName{"background", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Background", "group")},
    KeyName{"body", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Figure",
                                       "group: the build and the height of the whole figure")},
};

/// The names of the parts above their tiles
constexpr std::array PART_NAMES = {
    KeyName{"face", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Face Shape", "part")},
    KeyName{"age", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Looks Like", "part: the age")},
    KeyName{"eyes", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Eyes", "part")},
    KeyName{"lashes", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Lashes", "part")},
    KeyName{"brows", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Brows", "part")},
    KeyName{"nose", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Nose", "part")},
    KeyName{"mouth", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Mouth", "part")},
    KeyName{"hair", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Hairstyle", "part")},
    KeyName{"beard", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Beard", "part")},
    KeyName{"glasses", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Glasses", "part")},
    KeyName{"hat", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Headwear", "part")},
    KeyName{"extras", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Extras", "part")},
    KeyName{"clothes", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Clothes", "part")},
    KeyName{"bottom", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Lower Clothes", "part")},
    KeyName{"background", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Background", "part")},
    KeyName{"build", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Build", "part")},
    KeyName{"height", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Height", "part")},
};

/// The names of the colors above their palettes
constexpr std::array COLOR_NAMES = {
    KeyName{"skin", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Skin Color", "color")},
    KeyName{"eyeColor", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Eye Color", "color")},
    KeyName{"hairColor",
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Hair and Beard Color", "color")},
    KeyName{"clothesColor", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Clothes Color", "color")},
    KeyName{"bottomColor",
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "Lower Clothes Color", "color")},
};

/// The name of @p key in @p names, in the language of the program; @p key without one
template <std::size_t N>
QString nameIn(const std::array<KeyName, N>& names, const QString& key) {
    for (const KeyName& each : names) {
        if (key == QLatin1String(each.key)) {
            return QCoreApplication::translate("kalahari::gui::Avatar", each.name.source,
                                               each.name.comment);
        }
    }
    return key;
}

/// The text of @p text in the language of the program
QString translated(const Text& text) {
    return QCoreApplication::translate("kalahari::gui::Avatar", text.source, text.comment);
}

/// The value of @p key in @p avatar as the drawing has it: a value of the part, else its
/// default
QString valueOf(const QJsonObject& avatar, const Avatar::Part& part) {
    const QString value = avatar.value(part.key).toString();
    return part.values.contains(value) ? value : part.fallback;
}

/// The values of the part of many values @p part in @p avatar that the program knows
QStringList valuesOf(const QJsonObject& avatar, const Avatar::Part& part) {
    QStringList values;
    for (const auto& each : avatar.value(part.key).toArray()) {
        if (part.values.contains(each.toString()) && !values.contains(each.toString())) {
            values << each.toString();
        }
    }
    return values;
}

/// The age a portrait shows for the text @p age of the field Age
QString ageOf(const QString& age) {
    constexpr int YOUNG = 20;
    constexpr int ADULT = 40;
    constexpr int MIDDLE = 60;
    bool number = false;
    const int years = age.trimmed().toInt(&number);
    if (!number) {
        return QStringLiteral("adult");
    }
    if (years < YOUNG) {
        return QStringLiteral("young");
    }
    if (years < ADULT) {
        return QStringLiteral("adult");
    }
    return years < MIDDLE ? QStringLiteral("middle") : QStringLiteral("old");
}

/// How likely, in percent, a portrait drawn by chance gets: the calm mouth (else the serious
/// one), long lashes for a woman, glasses, a hat, each of the extras and a skirt for a woman
constexpr int CALM_MOUTH = 70;
constexpr int LONG_LASHES = 80;
constexpr int GLASSES = 35;
constexpr int HEADWEAR = 20;
constexpr int EACH_EXTRA = 15;
constexpr int SKIRT = 40;
constexpr int ALL = 100;

/// Draws the values of a portrait by chance
class Chance {
public:
    explicit Chance(QRandomGenerator& random) : m_random(&random) {}

    /// One of @p values; empty when there is none
    [[nodiscard]] QString pick(const QStringList& values) const {
        return values.isEmpty() ? QString()
                                : values.at(static_cast<qsizetype>(
                                      m_random->bounded(static_cast<quint32>(values.size()))));
    }

    /// One of the values of part @p key, but @p without
    [[nodiscard]] QString pickOf(const QString& key, const QStringList& without = {}) const {
        const Avatar::Part* part = Avatar::part(key);
        QStringList values = part != nullptr ? part->values : QStringList();
        for (const QString& each : without) {
            values.removeAll(each);
        }
        return pick(values);
    }

    /// Whether something that comes @p percent times in a hundred comes now
    [[nodiscard]] bool happens(int percent) const { return m_random->bounded(ALL) < percent; }

    /// The values of the part of many values @p key, each one @p percent times in a hundred
    [[nodiscard]] QJsonArray someOf(const QString& key, int percent) const {
        QJsonArray values;
        const Avatar::Part* part = Avatar::part(key);
        for (const QString& each : part != nullptr ? part->values : QStringList()) {
            if (happens(percent)) {
                values.append(each);
            }
        }
        return values;
    }

    /// Give each color of @p avatar one of its palette in @p palettes, when it has some
    void pickColors(QJsonObject& avatar, const AvatarPalettes& palettes) const {
        for (const Avatar::Color& each : Avatar::colors()) {
            if (const QString color = pick(palettes.colors(each.palette)); !color.isEmpty()) {
                avatar.insert(each.key, color);
            }
        }
    }

private:
    QRandomGenerator* m_random;  ///< Not null: the generator of the caller
};

/// The hair, the beard and the lashes of @p drawn by @p chance, as a woman (@p woman) or a man
/// (@p man) suits: a woman gets no bald head, only a man a beard
void pickHair(QJsonObject& drawn, const Chance& chance, bool woman, bool man) {
    const QString none = QStringLiteral("none");
    drawn.insert(QStringLiteral("hair"),
                 chance.pickOf(QStringLiteral("hair"),
                               woman ? QStringList{QStringLiteral("bald")} : QStringList()));
    drawn.insert(QStringLiteral("beard"), man ? chance.pickOf(QStringLiteral("beard")) : none);
    drawn.insert(QStringLiteral("lashes"), woman && chance.happens(LONG_LASHES)
                                               ? QStringLiteral("long")
                                               : QStringLiteral("plain"));
}

/// What @p drawn wears, by @p chance, as a woman (@p woman) or a man (@p man) suits: the
/// glasses, the headwear, the extras and the clothes; a man gets no dress, only a woman a skirt
void pickWorn(QJsonObject& drawn, const Chance& chance, bool woman, bool man) {
    const QString none = QStringLiteral("none");
    drawn.insert(QStringLiteral("glasses"), chance.happens(GLASSES)
                                                ? chance.pickOf(QStringLiteral("glasses"), {none})
                                                : none);
    drawn.insert(QStringLiteral("hat"), chance.happens(HEADWEAR)
                                            ? chance.pickOf(QStringLiteral("hat"), {none})
                                            : none);
    drawn.insert(QStringLiteral("extras"), chance.someOf(QStringLiteral("extras"), EACH_EXTRA));
    drawn.insert(QStringLiteral("clothes"),
                 chance.pickOf(QStringLiteral("clothes"),
                               man ? QStringList{QStringLiteral("dress")} : QStringList()));
    drawn.insert(QStringLiteral("bottom"), woman && chance.happens(SKIRT)
                                               ? QStringLiteral("skirt")
                                               : QStringLiteral("trousers"));
}

}  // namespace

const QList<Avatar::Part>& Avatar::parts() {
    return partList();
}

const QList<Avatar::Color>& Avatar::colors() {
    return colorList();
}

const QList<Avatar::Group>& Avatar::groups() {
    static const QList<Group> groups = {
        {QStringLiteral("face"), {QStringLiteral("face"), QStringLiteral("age")},
         {QStringLiteral("skin")}},
        {QStringLiteral("eyes"),
         {QStringLiteral("eyes"), QStringLiteral("lashes"), QStringLiteral("brows")},
         {QStringLiteral("eyeColor")}},
        {QStringLiteral("nose"), {QStringLiteral("nose"), QStringLiteral("mouth")}, {}},
        {QStringLiteral("hair"), {QStringLiteral("hair"), QStringLiteral("beard")},
         {QStringLiteral("hairColor")}},
        {QStringLiteral("extras"),
         {QStringLiteral("glasses"), QStringLiteral("hat"), QStringLiteral("extras")},
         {}},
        {QStringLiteral("clothes"), {QStringLiteral("clothes"), QStringLiteral("bottom")},
         {QStringLiteral("clothesColor"), QStringLiteral("bottomColor")}},
        {QStringLiteral("background"), {QStringLiteral("background")}, {}, false, true},
        {QStringLiteral("body"), {QStringLiteral("build"), QStringLiteral("height")}, {}, true},
    };
    return groups;
}

const Avatar::Part* Avatar::part(const QString& key) {
    const auto& all = parts();
    const auto found = std::find_if(all.cbegin(), all.cend(),
                                     [&key](const Part& each) { return each.key == key; });
    return found == all.cend() ? nullptr : &*found;
}

const Avatar::Color* Avatar::color(const QString& key) {
    const auto& all = colors();
    const auto found = std::find_if(all.cbegin(), all.cend(),
                                     [&key](const Color& each) { return each.key == key; });
    return found == all.cend() ? nullptr : &*found;
}

QString Avatar::fallbackOf(const Color& color) {
    static const AvatarPalettes program = AvatarPalettes::defaults();
    return program.colors(color.palette).value(color.fallback);
}

QString Avatar::groupName(const QString& id) {
    return nameIn(GROUP_NAMES, id);
}

QString Avatar::partName(const QString& key) {
    return nameIn(PART_NAMES, key);
}

QString Avatar::colorName(const QString& key) {
    return nameIn(COLOR_NAMES, key);
}

QString Avatar::valueName(const QString& key, const QString& value) {
    for (const ValueName& each : VALUE_NAMES) {
        if (key == QLatin1String(each.key) && value == QLatin1String(each.value)) {
            return translated(each.name);
        }
    }
    return value;
}

QJsonObject Avatar::defaults() {
    QJsonObject avatar;
    for (const Part& each : parts()) {
        if (each.many) {
            avatar.insert(each.key, QJsonArray());
        } else {
            avatar.insert(each.key, each.fallback);
        }
    }
    for (const Color& each : colors()) {
        avatar.insert(each.key, fallbackOf(each));
    }
    avatar.insert(QLatin1String(SIGN), false);
    return avatar;
}

QJsonObject Avatar::proposal(const QString& gender, const QString& age) {
    QJsonObject avatar = defaults();
    avatar.insert(QStringLiteral("age"), ageOf(age));
    if (gender == QLatin1String("woman")) {
        avatar.insert(QStringLiteral("hair"), QStringLiteral("shoulder"));
        avatar.insert(QStringLiteral("lashes"), QStringLiteral("long"));
    }
    return avatar;
}

QJsonObject Avatar::resolved(const QJsonObject& avatar) {
    QJsonObject drawn = avatar;
    for (const Part& each : parts()) {
        if (each.many) {
            drawn.insert(each.key, QJsonArray::fromStringList(valuesOf(avatar, each)));
        } else {
            drawn.insert(each.key, valueOf(avatar, each));
        }
    }
    for (const Color& each : colors()) {
        const QString color = avatar.value(each.key).toString();
        drawn.insert(each.key, QColor::isValidColorName(color) ? QColor(color).name()
                                                                : fallbackOf(each));
    }
    drawn.insert(QLatin1String(SIGN), avatar.value(QLatin1String(SIGN)).toBool());
    return drawn;
}

QJsonObject Avatar::randomized(const QJsonObject& avatar, const QString& gender,
                               const AvatarPalettes& palettes, QRandomGenerator& random) {
    const Chance chance(random);
    const bool woman = gender == QLatin1String("woman");
    const bool man = gender == QLatin1String("man");
    QJsonObject drawn = avatar;
    for (const char* key : {"face", "eyes", "brows", "nose", "build", "height"}) {
        drawn.insert(QLatin1String(key), chance.pickOf(QLatin1String(key)));
    }
    drawn.insert(QStringLiteral("mouth"), chance.happens(CALM_MOUTH)
                                              ? QStringLiteral("neutral")
                                              : QStringLiteral("serious"));
    pickHair(drawn, chance, woman, man);
    pickWorn(drawn, chance, woman, man);
    chance.pickColors(drawn, palettes);
    return drawn;
}

QString Avatar::newStyle() {
    const QString style = QString::fromStdString(
        core::SettingsManager::getInstance().get<std::string>(STYLE_SETTING, AvatarLook::FLAT));
    return style == QLatin1String(AvatarLook::LINE) ? style : QString::fromLatin1(AvatarLook::FLAT);
}

AvatarLook Avatar::lookOf(const core::CardPortrait& portrait, AvatarFrame frame,
                          const QColor& card, const QString& gender) {
    AvatarLook look;
    look.style = portrait.style == QLatin1String(AvatarLook::LINE)
                     ? portrait.style
                     : QString::fromLatin1(AvatarLook::FLAT);
    look.frame = frame;
    // A card without a color: a grey of the theme, as its initials have
    look.card = card.isValid() ? card : QApplication::palette().color(QPalette::Mid);
    if (portrait.avatar.value(QLatin1String(SIGN)).toBool()) {
        look.sign = signOf(gender);
    }
    return look;
}

QImage Avatar::imageOf(const core::CardPortrait& portrait, AvatarFrame frame, const QColor& card,
                       const QString& gender, const QSize& pixels) {
    return AvatarArt::drawn().image(resolved(portrait.avatar),
                                    lookOf(portrait, frame, card, gender), pixels);
}

QString Avatar::signOf(const QString& gender) {
    if (gender == QLatin1String("woman")) {
        return QStringLiteral("\u2640");
    }
    return gender == QLatin1String("man") ? QStringLiteral("\u2642") : QStringLiteral("\u26b2");
}

}  // namespace kalahari::gui
