// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file avatar_words.cpp
/// @brief The look of a portrait of parts in words, for the field Appearance of a card

#include "kalahari/gui/avatar.h"

#include <QJsonArray>
#include <QJsonValue>

#include <array>

namespace kalahari::gui {

namespace {

/// A text with its comment for the translators, as QT_TRANSLATE_NOOP3 gives it
struct Text {
    const char* source;
    const char* comment;
};

/// A value of a part and its words in the description
struct Words {
    const char* key;
    const char* value;
    Text words;
};

/// The words of the values in the description of the look; a hairstyle has "%1" where the
/// color of the hair goes
constexpr std::array WORDS = {
    Words{"face", "oval", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "oval face", "look")},
    Words{"face", "round", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "round face", "look")},
    Words{"face", "square", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "square face", "look")},
    Words{"face", "long", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "long face", "look")},
    Words{"face", "heart",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "heart-shaped face", "look")},
    Words{"eyes", "round",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "round", "look: the shape of the eyes")},
    Words{"eyes", "almond", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "almond-shaped",
                                               "look: the shape of the eyes")},
    Words{"eyes", "narrow",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "narrow", "look: the shape of the eyes")},
    Words{"eyes", "big",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "big", "look: the shape of the eyes")},
    Words{"lashes", "long", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "long lashes", "look")},
    Words{"brows", "arched",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "arched brows", "look")},
    Words{"brows", "thick", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "thick brows", "look")},
    Words{"nose", "small", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "small nose", "look")},
    Words{"nose", "straight",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "straight nose", "look")},
    Words{"nose", "aquiline",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "aquiline nose", "look")},
    Words{"nose", "wide", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "wide nose", "look")},
    Words{"hair", "short", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "short %1 hair",
                                              "look; %1: the color of the hair")},
    Words{"hair", "fringe",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "short %1 hair with a fringe",
                             "look; %1: the color of the hair")},
    Words{"hair", "shoulder",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "shoulder-length %1 hair",
                             "look; %1: the color of the hair")},
    Words{"hair", "long", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "long %1 hair",
                                             "look; %1: the color of the hair")},
    Words{"hair", "bun", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "%1 hair in a bun",
                                            "look; %1: the color of the hair")},
    Words{"hair", "curly", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "curly %1 hair",
                                              "look; %1: the color of the hair")},
    Words{"hair", "bald", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "bald head", "look")},
    Words{"beard", "stubble", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "stubble", "look")},
    Words{"beard", "beard", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "beard", "look")},
    Words{"beard", "moustache",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "moustache", "look")},
    Words{"beard", "full",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "beard and moustache", "look")},
    Words{"glasses", "round",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "round wire-rimmed glasses", "look")},
    Words{"glasses", "rectangular",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "rectangular glasses", "look")},
    Words{"glasses", "dark", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "dark glasses", "look")},
    Words{"glasses", "monocle", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "a monocle", "look")},
    Words{"hat", "hat", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "a hat", "look")},
    Words{"hat", "hood", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "a hood", "look")},
    Words{"hat", "diadem", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "a diadem", "look")},
    Words{"extras", "freckles", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "freckles", "look")},
    Words{"extras", "blush", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "rosy cheeks", "look")},
    Words{"extras", "mole",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "a mole on the cheek", "look")},
    Words{"extras", "scar",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "a scar on the cheek", "look")},
    Words{"extras", "earrings", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "earrings", "look")},
    Words{"extras", "eyePatch",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "an eye patch", "look")},
    Words{"age", "middle",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "wrinkles around the eyes", "look")},
    Words{"age", "old", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "deep wrinkles", "look")},
    Words{"height", "short", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "short", "look: height")},
    Words{"height", "medium",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "of medium height", "look: height")},
    Words{"height", "tall", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "tall", "look: height")},
    Words{"build", "slim", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "slim build", "look")},
    Words{"build", "average",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "average build", "look")},
    Words{"build", "strong",
          QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "strong build", "look")},
};

/// Clothes, their words and the form of the word a color before them takes
struct Clothes {
    const char* key;
    const char* value;
    Text words;
    Text form;  ///< "m", "f" or "pl", as the language has them (AvatarPalettes::colorName())
};

/// The clothes in the description of the look
constexpr std::array CLOTHES = {
    Clothes{"clothes", "shirt", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "shirt", "look"),
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "m",
                               "the form of the color before 'shirt': m, f or pl")},
    Clothes{"clothes", "sweater", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "sweater", "look"),
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "m",
                               "the form of the color before 'sweater': m, f or pl")},
    Clothes{"clothes", "coat", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "coat", "look"),
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "m",
                               "the form of the color before 'coat': m, f or pl")},
    Clothes{"clothes", "robe", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "robe", "look"),
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "m",
                               "the form of the color before 'robe': m, f or pl")},
    Clothes{"clothes", "dress", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "dress", "look"),
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "m",
                               "the form of the color before 'dress': m, f or pl")},
    Clothes{"bottom", "trousers", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "trousers", "look"),
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "pl",
                               "the form of the color before 'trousers': m, f or pl")},
    Clothes{"bottom", "skirt", QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "skirt", "look"),
            QT_TRANSLATE_NOOP3("kalahari::gui::Avatar", "m",
                               "the form of the color before 'skirt': m, f or pl")},
};

/// The clothes @p value of part @p key; nullptr: none
const Clothes* clothesAt(QLatin1String key, const QString& value) {
    for (const Clothes& each : CLOTHES) {
        if (key == QLatin1String(each.key) && value == QLatin1String(each.value)) {
            return &each;
        }
    }
    return nullptr;
}

/// The text of @p text in the language of the program
QString translated(const Text& text) {
    return QCoreApplication::translate("kalahari::gui::Avatar", text.source, text.comment);
}

/// The words of value @p value of part @p key; empty when it has none
QString wordsOf(const QString& key, const QString& value) {
    for (const Words& each : WORDS) {
        if (key == QLatin1String(each.key) && value == QLatin1String(each.value)) {
            return translated(each.words);
        }
    }
    return {};
}

/// The words of the part @p key of @p avatar; empty when it has none
QString wordsOf(const QJsonObject& avatar, const char* key) {
    return wordsOf(QLatin1String(key), avatar.value(QLatin1String(key)).toString());
}

/// @p text with its first letter a capital one
QString capitalized(const QString& text) {
    return text.isEmpty() ? text : text.front().toUpper() + text.mid(1);
}

/// The eyes of @p avatar in words: their shape and their color
QString eyesOf(const QJsonObject& avatar) {
    const QString shape = wordsOf(avatar, "eyes");
    if (shape.isEmpty()) {
        return {};
    }
    const QString color = AvatarPalettes::colorName(
        QStringLiteral("eyes"), avatar.value(QLatin1String("eyeColor")).toString());
    return color.isEmpty() ? Avatar::tr("%1 eyes", "look; %1: their shape").arg(shape)
                           : Avatar::tr("%1 %2 eyes", "look; %1: their shape, %2: their color")
                                 .arg(shape, color);
}

/// The hair of @p avatar in words, with its color
QString hairOf(const QJsonObject& avatar) {
    QString words = wordsOf(avatar, "hair");
    if (!words.contains(QLatin1String("%1"))) {
        return words;
    }
    const QString color = AvatarPalettes::colorName(
        QStringLiteral("hair"), avatar.value(QLatin1String("hairColor")).toString());
    return words.arg(color).simplified();
}

/// The clothes @p key of @p avatar in words with their color @p colorKey; empty for others
QString clothesOf(const QJsonObject& avatar, const char* key, const char* colorKey) {
    const QString value = avatar.value(QLatin1String(key)).toString();
    if (QLatin1String(key) == QLatin1String("clothes") && value == QLatin1String("armor")) {
        return Avatar::tr("armor", "look");
    }
    const Clothes* found = clothesAt(QLatin1String(key), value);
    if (found == nullptr) {
        return {};
    }
    const QString color = AvatarPalettes::colorName(
        QStringLiteral("clothes"), avatar.value(QLatin1String(colorKey)).toString(),
        translated(found->form));
    return color.isEmpty() ? translated(found->words)
                           : Avatar::tr("%1 %2", "look; %1: the color of clothes, %2: them")
                                 .arg(color, translated(found->words));
}

/// The face of @p avatar in words: its shape, the skin, the eyes, the hair and what is on it
QStringList faceOf(const QJsonObject& avatar) {
    QStringList words = {
        wordsOf(avatar, "face"),
        AvatarPalettes::colorName(QStringLiteral("skin"),
                                  avatar.value(QLatin1String("skin")).toString()),
        eyesOf(avatar),
        wordsOf(avatar, "lashes"),
        wordsOf(avatar, "brows"),
        wordsOf(avatar, "nose"),
        hairOf(avatar),
        wordsOf(avatar, "beard"),
        wordsOf(avatar, "glasses"),
        wordsOf(avatar, "hat"),
    };
    for (const auto& extra : avatar.value(QLatin1String("extras")).toArray()) {
        words << wordsOf(QStringLiteral("extras"), extra.toString());
    }
    words << wordsOf(avatar, "age");
    words.removeAll(QString());
    return words;
}

}  // namespace

QString Avatar::described(const QJsonObject& avatar, bool full) {
    QStringList sentences;
    if (const QStringList face = faceOf(avatar); !face.isEmpty()) {
        sentences << capitalized(face.join(QStringLiteral(", "))) + QLatin1Char('.');
    }
    QStringList clothes = {clothesOf(avatar, "clothes", "clothesColor")};
    const QString garment = avatar.value(QLatin1String("clothes")).toString();
    const bool longGarment = garment == QLatin1String("robe") || garment == QLatin1String("dress");
    if (full && !longGarment) {
        clothes << clothesOf(avatar, "bottom", "bottomColor");
    }
    clothes.removeAll(QString());
    if (!clothes.isEmpty()) {
        sentences << tr("Clothes: %1.", "look").arg(clothes.join(QStringLiteral(", ")));
    }
    if (full) {
        QStringList body = {wordsOf(avatar, "height"), wordsOf(avatar, "build")};
        body.removeAll(QString());
        if (!body.isEmpty()) {
            sentences << capitalized(body.join(QStringLiteral(", "))) + QLatin1Char('.');
        }
    }
    return sentences.join(QLatin1Char(' '));
}

}  // namespace kalahari::gui
