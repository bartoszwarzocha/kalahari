// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file caption_settings.cpp
/// @brief The settings of new images and of the captions of new books

#include "kalahari/gui/caption_settings.h"

#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"

#include <QCoreApplication>

#include <algorithm>

#include <nlohmann/json.hpp>

namespace kalahari::gui {

namespace {

QString translated(const char* text, const char* comment) {
    return QCoreApplication::translate("CaptionSettings", text, comment);
}

}  // namespace

QStringList defaultCaptionLabels() {
    return {translated("Figure", "A label of captions. In Polish: Rysunek"),
            translated("Photo", "A label of captions. In Polish: Fotografia"),
            translated("Illustration", "A label of captions. In Polish: Ilustracja"),
            translated("Map", "A label of captions. In Polish: Mapa"),
            translated("Diagram", "A label of captions. In Polish: Schemat")};
}

QStringList settingsCaptionLabels() {
    const nlohmann::json stored = core::SettingsManager::getInstance().get<nlohmann::json>(
        CAPTION_LABELS_KEY, nlohmann::json::array());
    QStringList labels;
    if (stored.is_array()) {
        for (const nlohmann::json& item : stored) {
            if (!item.is_string()) {
                continue;
            }
            const QString label = editor::normalizedLabel(QString::fromStdString(item.get<std::string>()));
            if (!label.isEmpty() && !labels.contains(label)) {
                labels << label;
            }
        }
    }
    return labels.isEmpty() ? defaultCaptionLabels() : labels;
}

core::BookCaptions settingsBookCaptions() {
    const auto& settings = core::SettingsManager::getInstance();
    core::BookCaptions captions;
    captions.labels = settingsCaptionLabels();
    editor::CaptionNumbering numbering;
    numbering.format = editor::numberFormatOfName(
        QString::fromStdString(settings.get<std::string>(CAPTION_FORMAT_KEY, "1")));
    numbering.withChapter = settings.get<bool>(CAPTION_WITH_CHAPTER_KEY, false);
    numbering.perChapter = numbering.withChapter;
    for (const QString& label : captions.labels) {
        captions.numbering << bookNumberingOf(label, numbering);
    }
    captions.reference = editor::referenceShowsName(editor::referenceShowsOfName(
        QString::fromStdString(settings.get<std::string>(CAPTION_REFERENCE_KEY, "number"))));
    return captions;
}

core::BookCaptions currentBookCaptions(const core::ProjectManager& book) {
    if (const std::optional<core::BookCaptions> own = book.bookCaptions()) {
        return *own;
    }
    return settingsBookCaptions();
}

editor::CaptionNumbering numberingOf(const core::CaptionLabelNumbering& numbering) {
    editor::CaptionNumbering result;
    result.format = editor::numberFormatOfName(numbering.format);
    result.perChapter = numbering.perChapter;
    result.withChapter = numbering.withChapter;
    result.separator = numbering.separator;
    return editor::normalizedNumbering(result);
}

core::CaptionLabelNumbering bookNumberingOf(const QString& label,
                                            const editor::CaptionNumbering& numbering) {
    const editor::CaptionNumbering normal = editor::normalizedNumbering(numbering);
    core::CaptionLabelNumbering result;
    result.label = label;
    result.format = editor::numberFormatName(normal.format);
    result.perChapter = normal.perChapter;
    result.withChapter = normal.withChapter;
    result.separator = normal.separator;
    return result;
}

QHash<QString, editor::CaptionNumbering> numberingsOf(const core::BookCaptions& captions) {
    QHash<QString, editor::CaptionNumbering> numberings;
    for (const core::CaptionLabelNumbering& numbering : captions.numbering) {
        numberings.insert(numbering.label, numberingOf(numbering));
    }
    return numberings;
}

editor::ReferenceShows referenceShowsOf(const core::BookCaptions& captions) {
    return editor::referenceShowsOfName(captions.reference);
}

editor::BookEditor::NewImageOptions settingsNewImageOptions() {
    const auto& settings = core::SettingsManager::getInstance();
    editor::BookEditor::NewImageOptions options;
    const std::string alignment = settings.get<std::string>(IMAGE_ALIGNMENT_KEY, "center");
    options.alignment = alignment == "left"    ? Qt::Alignment(Qt::AlignLeft)
                        : alignment == "right" ? Qt::Alignment(Qt::AlignRight)
                                               : Qt::Alignment(Qt::AlignHCenter);
    const int width = settings.get<int>(IMAGE_WIDTH_KEY, 0);
    options.widthPercent = width > 0 && width <= 100 ? width : 0;
    if (settings.get<bool>(IMAGE_CAPTION_KEY, true)) {
        const QString label = editor::normalizedLabel(
            QString::fromStdString(settings.get<std::string>(IMAGE_CAPTION_LABEL_KEY, "")));
        options.captionLabel = label.isEmpty() ? settingsCaptionLabels().constFirst() : label;
    }
    if (settings.get<bool>(IMAGE_DOWNSCALE_KEY, false)) {
        options.maxDpi = std::clamp(settings.get<int>(IMAGE_MAX_DPI_KEY, 300), IMAGE_MIN_MAX_DPI,
                                    IMAGE_MAX_MAX_DPI);
    }
    return options;
}

}  // namespace kalahari::gui
