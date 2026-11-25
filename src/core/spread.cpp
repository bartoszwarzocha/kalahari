// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

#include "kalahari/core/spread.h"

#include "kalahari/core/settings_manager.h"

#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <string>

namespace kalahari::core {

namespace {

/// @brief A number of the file, or @p fallback when it is not one
double numberOf(const QJsonValue& value, double fallback) {
    const double number = value.toDouble(fallback);
    return std::isfinite(number) ? number : fallback;
}

}  // namespace

SpreadData SpreadData::fromJson(const QJsonObject& json) {
    SpreadData data;
    QJsonObject rest = json;
    data.picture = rest.take(QLatin1String("picture")).toString();
    data.area = rest.take(QLatin1String("area")).toString(QLatin1String(MARGINS));
    data.fit = rest.take(QLatin1String("fit")).toString(QLatin1String(WHOLE));
    const QJsonValue crop = rest.take(QLatin1String("crop"));
    if (crop.isObject()) {
        const QJsonObject place = crop.toObject();
        data.cropX = numberOf(place.value(QLatin1String("x")), 0.0);
        data.cropY = numberOf(place.value(QLatin1String("y")), 0.0);
        data.zoom = static_cast<int>(
            std::lround(numberOf(place.value(QLatin1String("zoom")), MIN_ZOOM)));
    }
    data.gutter = numberOf(rest.take(QLatin1String("gutter")), 0.0);
    const QJsonValue caption = rest.take(QLatin1String("caption"));
    if (caption.isObject()) {
        const QJsonObject words = caption.toObject();
        data.caption = true;
        data.captionLabel = words.value(QLatin1String("label")).toString();
        data.captionText = words.value(QLatin1String("text")).toString();
    }
    data.pageNumbers = rest.take(QLatin1String("pageNumbers")).toBool(false);
    data.inContents = rest.take(QLatin1String("inContents")).toBool(false);
    data.extra = rest;
    return data.normalized();
}

QJsonObject SpreadData::toJson() const {
    const SpreadData data = normalized();
    QJsonObject json = data.extra;
    if (!data.picture.isEmpty()) {
        json.insert(QLatin1String("picture"), data.picture);
    }
    json.insert(QLatin1String("area"), data.area);
    json.insert(QLatin1String("fit"), data.fit);
    if (data.fills()) {
        json.insert(QLatin1String("crop"), QJsonObject{{QLatin1String("x"), data.cropX},
                                                       {QLatin1String("y"), data.cropY},
                                                       {QLatin1String("zoom"), data.zoom}});
    }
    json.insert(QLatin1String("gutter"), data.gutter);
    if (data.caption) {
        QJsonObject words;
        if (!data.captionLabel.isEmpty()) {
            words.insert(QLatin1String("label"), data.captionLabel);
        }
        words.insert(QLatin1String("text"), data.captionText);
        json.insert(QLatin1String("caption"), words);
    }
    json.insert(QLatin1String("pageNumbers"), data.pageNumbers);
    json.insert(QLatin1String("inContents"), data.inContents);
    return json;
}

SpreadData SpreadData::ofSettings() {
    const auto& settings = SettingsManager::getInstance();
    SpreadData data;
    data.area = QString::fromStdString(settings.get<std::string>(AREA_KEY, MARGINS));
    data.fit = QString::fromStdString(settings.get<std::string>(FIT_KEY, WHOLE));
    data.gutter = settings.get<double>(GUTTER_KEY, 0.0);
    data.caption = settings.get<bool>(CAPTION_KEY, false);
    data.captionLabel = QString::fromStdString(settings.get<std::string>(CAPTION_LABEL_KEY, ""));
    data.pageNumbers = settings.get<bool>(PAGE_NUMBERS_KEY, false);
    data.inContents = settings.get<bool>(IN_CONTENTS_KEY, false);
    return data.normalized();
}

SpreadData SpreadData::normalized() const {
    SpreadData data = *this;
    if (data.area != QLatin1String(EDGE)) {
        data.area = QString::fromLatin1(MARGINS);
    }
    if (data.fit != QLatin1String(FILL)) {
        data.fit = QString::fromLatin1(WHOLE);
    }
    data.cropX = std::clamp(std::isfinite(data.cropX) ? data.cropX : 0.0, -1.0, 1.0);
    data.cropY = std::clamp(std::isfinite(data.cropY) ? data.cropY : 0.0, -1.0, 1.0);
    data.zoom = std::clamp(data.zoom, MIN_ZOOM, MAX_ZOOM);
    data.gutter = std::clamp(std::isfinite(data.gutter) ? data.gutter : 0.0, 0.0, MAX_GUTTER);
    data.captionLabel = data.captionLabel.trimmed();
    return data;
}

}  // namespace kalahari::core
