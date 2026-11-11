/// @file cover_material.cpp
/// @brief The materials of the cover: what each is and where it goes

#include <kalahari/core/cover_material.h>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/text_statistics.h>

namespace kalahari {
namespace core {

bool CoverData::isEmpty() const {
    return part.isEmpty() && place.isEmpty() && state.isEmpty() && author.isEmpty() &&
           role.isEmpty() && source.isEmpty() && date.isEmpty() && consent.isEmpty() &&
           license.isEmpty() && about.isEmpty() && targetCharacters == 0 && targetWords == 0 &&
           extra.isEmpty();
}

const QStringList& CoverMaterial::parts() {
    static const QStringList parts{
        QString::fromLatin1(BLURB),  QString::fromLatin1(TAGLINE),   QString::fromLatin1(OPINION),
        QString::fromLatin1(DESIGN), QString::fromLatin1(REFERENCE), QString::fromLatin1(NOTE)};
    return parts;
}

const QStringList& CoverMaterial::places() {
    static const QStringList places{
        QString::fromLatin1(FRONT),      QString::fromLatin1(SPINE),
        QString::fromLatin1(BACK),       QString::fromLatin1(FRONT_FLAP),
        QString::fromLatin1(BACK_FLAP),  QString::fromLatin1(WHOLE)};
    return places;
}

bool CoverMaterial::isText(const QString& part) {
    return part == QLatin1String(BLURB) || part == QLatin1String(TAGLINE) ||
           part == QLatin1String(OPINION) || part == QLatin1String(NOTE);
}

bool CoverMaterial::isPicture(const QString& part) {
    return part == QLatin1String(DESIGN) || part == QLatin1String(REFERENCE);
}

bool CoverMaterial::hasTarget(const QString& part) {
    return part == QLatin1String(BLURB) || part == QLatin1String(TAGLINE) ||
           part == QLatin1String(OPINION);
}

bool CoverMaterial::goesOnCover(const QString& part) {
    return part == QLatin1String(BLURB) || part == QLatin1String(TAGLINE) ||
           part == QLatin1String(OPINION) || part == QLatin1String(DESIGN);
}

bool CoverMaterial::choosesOne(const QString& part) {
    return part == QLatin1String(BLURB) || part == QLatin1String(TAGLINE) ||
           part == QLatin1String(DESIGN);
}

QString CoverMaterial::defaultPlaceOf(const QString& part) {
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

} // namespace core
} // namespace kalahari
