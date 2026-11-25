// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cover_material.cpp
/// @brief The materials of the cover: what each is and where it goes

#include <kalahari/core/cover_material.h>

#include <kalahari/core/package_json.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/text_statistics.h>

#include <QJsonArray>
#include <QRegularExpression>

#include <algorithm>

namespace kalahari::core {

namespace {

using package_json::addProblem;

/// The path of @p key of the object @p field, for the problems
QString pathOf(const QString& field, const char* key) {
    return field.isEmpty() ? QString::fromLatin1(key)
                           : QStringLiteral("%1.%2").arg(field, QLatin1String(key));
}

/// Whether @p text is a color "#RRGGBB"
bool isColorText(const QString& text) {
    static const QRegularExpression color(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return color.match(text).hasMatch();
}

/// The text of @p key, taken out of @p rest; empty without it or when it is not one
QString takeText(QJsonObject& rest, const char* key, const QString& field,
                 QStringList& problems) {
    const QJsonValue value = rest.take(QLatin1String(key));
    if (value.isUndefined()) {
        return {};
    }
    if (!value.isString()) {
        addProblem(problems, pathOf(field, key), QStringLiteral("must be a text"));
        return {};
    }
    return value.toString();
}

/// The color "#RRGGBB" of @p key, taken out of @p rest; empty without it or when it is not one
QString takeColor(QJsonObject& rest, const char* key, const QString& field,
                  QStringList& problems) {
    QString text = takeText(rest, key, field, problems);
    if (!text.isEmpty() && !isColorText(text)) {
        addProblem(problems, pathOf(field, key), QStringLiteral("must be a color: \"#RRGGBB\""));
        return {};
    }
    return text;
}

/// The yes or no of @p key, taken out of @p rest; @p fallback without it
bool takeBool(QJsonObject& rest, const char* key, bool fallback, const QString& field,
              QStringList& problems) {
    const QJsonValue value = rest.take(QLatin1String(key));
    if (value.isUndefined()) {
        return fallback;
    }
    if (!value.isBool()) {
        addProblem(problems, pathOf(field, key), QStringLiteral("must be true or false"));
        return fallback;
    }
    return value.toBool();
}

/// @p text under @p key of @p json, or no @p key when it is empty
void setOrRemove(QJsonObject& json, const char* key, const QString& text) {
    if (text.isEmpty()) {
        json.remove(QLatin1String(key));
    } else {
        json.insert(QLatin1String(key), text);
    }
}

}  // namespace

bool CoverData::isEmpty() const {
    return part.isEmpty() && place.isEmpty() && state.isEmpty() && author.isEmpty() &&
           role.isEmpty() && source.isEmpty() && date.isEmpty() && consent.isEmpty() &&
           license.isEmpty() && about.isEmpty() && targetCharacters == 0 && targetWords == 0 &&
           extra.isEmpty();
}

const QStringList& CoverMaterial::parts() {
    static const QStringList parts{
        QString::fromLatin1(BLURB),        QString::fromLatin1(TAGLINE),
        QString::fromLatin1(OPINION),      QString::fromLatin1(BIO),
        QString::fromLatin1(DESIGN),       QString::fromLatin1(AUTHOR_PHOTO),
        QString::fromLatin1(REFERENCE),    QString::fromLatin1(NOTE)};
    return parts;
}

const QStringList& CoverMaterial::places() {
    static const QStringList places = coverPlaces() + jacketPlaces();
    return places;
}

const QStringList& CoverMaterial::coverPlaces() {
    static const QStringList places{
        QString::fromLatin1(FRONT),      QString::fromLatin1(SPINE),
        QString::fromLatin1(BACK),       QString::fromLatin1(FRONT_FLAP),
        QString::fromLatin1(BACK_FLAP),  QString::fromLatin1(WHOLE)};
    return places;
}

const QStringList& CoverMaterial::jacketPlaces() {
    static const QStringList places{
        QString::fromLatin1(JACKET_FRONT),      QString::fromLatin1(JACKET_SPINE),
        QString::fromLatin1(JACKET_BACK),       QString::fromLatin1(JACKET_FRONT_FLAP),
        QString::fromLatin1(JACKET_BACK_FLAP),  QString::fromLatin1(JACKET_WHOLE)};
    return places;
}

bool CoverMaterial::isJacketPlace(const QString& place) {
    return jacketPlaces().contains(place);
}

bool CoverMaterial::isCoverFlap(const QString& place) {
    return place == QLatin1String(FRONT_FLAP) || place == QLatin1String(BACK_FLAP);
}

bool CoverMaterial::isWhole(const QString& place) {
    return place == QLatin1String(WHOLE) || place == QLatin1String(JACKET_WHOLE);
}

bool CoverMaterial::isOn(const QString& place, const CoverParts& parts) {
    if (isJacketPlace(place)) {
        return parts.jacket;
    }
    return !isCoverFlap(place) || parts.flaps;
}

bool CoverMaterial::isText(const QString& part) {
    return part == QLatin1String(BLURB) || part == QLatin1String(TAGLINE) ||
           part == QLatin1String(OPINION) || part == QLatin1String(BIO) ||
           part == QLatin1String(NOTE);
}

bool CoverMaterial::isPicture(const QString& part) {
    return part == QLatin1String(DESIGN) || part == QLatin1String(AUTHOR_PHOTO) ||
           part == QLatin1String(REFERENCE);
}

bool CoverMaterial::hasTarget(const QString& part) {
    return part == QLatin1String(BLURB) || part == QLatin1String(TAGLINE) ||
           part == QLatin1String(OPINION) || part == QLatin1String(BIO);
}

bool CoverMaterial::goesOnCover(const QString& part) {
    return part == QLatin1String(BLURB) || part == QLatin1String(TAGLINE) ||
           part == QLatin1String(OPINION) || part == QLatin1String(BIO) ||
           part == QLatin1String(DESIGN) || part == QLatin1String(AUTHOR_PHOTO);
}

bool CoverMaterial::choosesOne(const QString& part) {
    return part == QLatin1String(BLURB) || part == QLatin1String(TAGLINE) ||
           part == QLatin1String(BIO) || part == QLatin1String(DESIGN) ||
           part == QLatin1String(AUTHOR_PHOTO);
}

QString CoverMaterial::defaultPlaceOf(const QString& part) {
    return defaultPlaceOf(part, CoverParts{});
}

QString CoverMaterial::defaultPlaceOf(const QString& part, const CoverParts& parts) {
    if (part == QLatin1String(BIO) || part == QLatin1String(AUTHOR_PHOTO)) {
        if (parts.jacket) {
            return QString::fromLatin1(JACKET_BACK_FLAP);
        }
        return QString::fromLatin1(parts.flaps ? BACK_FLAP : BACK);
    }
    if (part == QLatin1String(BLURB) || part == QLatin1String(OPINION)) {
        return QString::fromLatin1(BACK);
    }
    if (part == QLatin1String(TAGLINE)) {
        return QString::fromLatin1(FRONT);
    }
    if (part == QLatin1String(DESIGN)) {
        return QString::fromLatin1(WHOLE);
    }
    return {};
}

bool CoverMaterial::canChange(const QString& from, const QString& to) {
    return (isText(from) && isText(to)) || (isPicture(from) && isPicture(to));
}

bool CoverMaterial::countsWords() {
    return SettingsManager::getInstance().get<std::string>(UNIT_SETTING, "characters") ==
           "words";
}

std::string CoverMaterial::targetSettingOf(const QString& part, bool words) {
    return std::string(TARGETS_SETTING) + '.' + part.toStdString() +
           (words ? ".words" : ".characters");
}

int CoverMaterial::defaultTargetOf(const QString& part, bool words) {
    if (!hasTarget(part)) {
        return 0;
    }
    const int target = SettingsManager::getInstance().get<int>(targetSettingOf(part, words), 0);
    return target > 0 ? target : 0;
}

int CoverMaterial::targetOf(const CoverData& cover, bool words) {
    if (!hasTarget(cover.part)) {
        return 0;
    }
    const int own = words ? cover.targetWords : cover.targetCharacters;
    return own > 0 ? own : defaultTargetOf(cover.part, words);
}

int CoverMaterial::lengthOf(QStringView text, bool words) {
    const TextCounts counts = countText(text, wordCountRules());
    return words ? counts.words : counts.characters;
}

// =============================================================================================
// Endpaper
// =============================================================================================

const QStringList& Endpaper::patterns() {
    static const QStringList patterns{
        QString::fromLatin1(DOTS),     QString::fromLatin1(STRIPES), QString::fromLatin1(GRID),
        QString::fromLatin1(DIAGONAL), QString::fromLatin1(CHECKER), QString::fromLatin1(WAVES)};
    return patterns;
}

const QStringList& Endpaper::fills() {
    static const QStringList fills{QString::fromLatin1(PICTURE), QString::fromLatin1(TILE),
                                   QString::fromLatin1(PATTERN), QString::fromLatin1(COLOR),
                                   QString()};
    return fills;
}

QString Endpaper::shownPattern() const {
    return patterns().contains(pattern) ? pattern : QString::fromLatin1(DOTS);
}

QJsonObject Endpaper::toJson() const {
    QJsonObject json = extra;
    setOrRemove(json, "fill", fill);
    setOrRemove(json, "picture", picture);
    if (scale == DEFAULT_SCALE) {
        json.remove(QLatin1String("scale"));
    } else {
        json.insert(QLatin1String("scale"), scale);
    }
    setOrRemove(json, "pattern", pattern);
    if (patternColor.isEmpty() && paperColor.isEmpty()) {
        json.remove(QLatin1String("colors"));
    } else {
        json.insert(QLatin1String("colors"), QJsonArray{patternColor, paperColor});
    }
    setOrRemove(json, "color", color);
    if (sameAsFront) {
        json.insert(QLatin1String("sameAsFront"), true);
    } else {
        json.remove(QLatin1String("sameAsFront"));
    }
    return json;
}

Endpaper Endpaper::fromJson(const QJsonObject& json, const QString& field,
                            QStringList& problems) {
    QJsonObject rest = json;
    Endpaper endpaper;
    endpaper.fill = takeText(rest, "fill", field, problems);
    endpaper.picture = takeText(rest, "picture", field, problems);
    if (const QJsonValue scale = rest.take(QLatin1String("scale")); !scale.isUndefined()) {
        const double value = scale.toDouble(-1);
        if (!scale.isDouble() || value != static_cast<int>(value) || value < MIN_SCALE ||
            value > MAX_SCALE) {
            addProblem(problems, pathOf(field, "scale"),
                       QStringLiteral("must be a whole number from %1 to %2")
                           .arg(MIN_SCALE)
                           .arg(MAX_SCALE));
        } else {
            endpaper.scale = static_cast<int>(value);
        }
    }
    endpaper.pattern = takeText(rest, "pattern", field, problems);
    if (const QJsonValue colors = rest.take(QLatin1String("colors")); !colors.isUndefined()) {
        const QJsonArray array = colors.toArray();
        const bool good = colors.isArray() && array.size() == 2 &&
                          std::all_of(array.begin(), array.end(), [](const QJsonValue& each) {
                              return each.isString() && (each.toString().isEmpty() ||
                                                         isColorText(each.toString()));
                          });
        if (good) {
            endpaper.patternColor = array.at(0).toString();
            endpaper.paperColor = array.at(1).toString();
        } else {
            addProblem(problems, pathOf(field, "colors"),
                       QStringLiteral("must be a list of two colors: \"#RRGGBB\""));
        }
    }
    endpaper.color = takeColor(rest, "color", field, problems);
    endpaper.sameAsFront = takeBool(rest, "sameAsFront", false, field, problems);
    endpaper.extra = rest;
    return endpaper;
}

// =============================================================================================
// CoverParts
// =============================================================================================

QJsonObject CoverParts::toJson() const {
    QJsonObject json = extra;
    json.insert(QLatin1String("flaps"), flaps);
    json.insert(QLatin1String("jacket"), jacket);
    json.insert(QLatin1String("endpapers"), endpapers);
    for (const auto& [key, endpaper] : {std::pair{"frontEndpaper", &front},
                                        std::pair{"backEndpaper", &back}}) {
        const QJsonObject each = endpaper->toJson();
        if (each.isEmpty()) {
            json.remove(QLatin1String(key));
        } else {
            json.insert(QLatin1String(key), each);
        }
    }
    return json;
}

CoverParts CoverParts::fromJson(const QJsonObject& json, const QString& field,
                                QStringList& problems) {
    QJsonObject rest = json;
    CoverParts parts;
    parts.flaps = takeBool(rest, "flaps", true, field, problems);
    parts.jacket = takeBool(rest, "jacket", false, field, problems);
    parts.endpapers = takeBool(rest, "endpapers", false, field, problems);
    for (const auto& [key, endpaper] : {std::pair{"frontEndpaper", &parts.front},
                                        std::pair{"backEndpaper", &parts.back}}) {
        const QJsonValue value = rest.take(QLatin1String(key));
        if (value.isObject()) {
            *endpaper = Endpaper::fromJson(value.toObject(), pathOf(field, key), problems);
        } else if (!value.isUndefined()) {
            addProblem(problems, pathOf(field, key), QStringLiteral("must be an object"));
        }
    }
    parts.extra = rest;
    return parts;
}

} // namespace kalahari::core
