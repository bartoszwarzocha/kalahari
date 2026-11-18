// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_data.cpp
/// @brief What a card of the Workshop has beside its text, and the colors of the cards

#include <kalahari/core/card_data.h>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/settings_schema.h>

#include <QColor>

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace kalahari::core {

bool CardData::isEmpty() const {
    return values.isEmpty() && color.isEmpty() && portraits.isEmpty() && extra.isEmpty();
}

const CardPortrait* CardData::mainPortrait() const {
    return portraits.isEmpty() ? nullptr : &portraits.constFirst();
}

QString CardData::portraitImage() const {
    return portraits.isEmpty() ? QString() : portraits.constFirst().image;
}

bool CardData::hasPortrait(const QString& elementId) const {
    return std::any_of(portraits.cbegin(), portraits.cend(),
                       [&elementId](const CardPortrait& each) { return each.image == elementId; });
}

QStringList CardColors::defaultPalette() {
    QStringList colors;
    if (const nlohmann::json* listed = settings_schema::defaultValue(PALETTE_SETTING)) {
        for (const nlohmann::json& each : *listed) {
            colors << QString::fromStdString(each.get<std::string>());
        }
    }
    return colors;
}

QStringList CardColors::palette() {
    const auto stored =
        SettingsManager::getInstance().get<std::vector<std::string>>(PALETTE_SETTING);
    QStringList colors;
    for (const std::string& each : stored) {
        const QString name = QString::fromStdString(each);
        if (QColor::isValidColorName(name)) {
            colors << QColor(name).name();
        }
    }
    return colors.isEmpty() ? defaultPalette() : colors;
}

QString CardColors::next(const QStringList& palette, const QStringList& used) {
    QString best;
    qsizetype fewest = -1;
    for (const QString& color : palette) {
        const qsizetype times =
            std::count_if(used.cbegin(), used.cend(), [&color](const QString& each) {
                return each.compare(color, Qt::CaseInsensitive) == 0;
            });
        if (fewest < 0 || times < fewest) {
            best = color;
            fewest = times;
        }
    }
    return best;
}

}  // namespace kalahari::core
