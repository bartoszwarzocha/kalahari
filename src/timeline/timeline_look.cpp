// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_look.cpp
/// @brief How timelines look and behave: the settings and the colors of the theme

#include <kalahari/timeline/timeline_look.h>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>

namespace kalahari::timeline {

namespace {

std::string setting(const char* key, const char* fallback) {
    return core::SettingsManager::getInstance().get<std::string>(key, fallback);
}

bool flag(const char* key, bool fallback) {
    return core::SettingsManager::getInstance().get<bool>(key, fallback);
}

QColor themeColor(const char* key, const QColor& fallback) {
    return core::ThemeManager::getInstance().editorColor(key, fallback);
}

}  // namespace

std::map<std::string, nlohmann::json> TimelineSettings::defaults() {
    return {
        {NEW_SCALE, "dates"},     {NEW_LANES, "threads"},     {NEW_LOOK, "cards"},
        {NEW_COMPRESS, true},     {OPEN_ON, "last"},          {CARD_TIME, true},
        {CARD_CHAPTERS, true},    {CARD_CHARACTERS, false},   {CARD_PLACE, false},
        {DATES, "words"},         {PARTS, "words"},
    };
}

TimelineLook TimelineLook::current() {
    TimelineLook look;
    look.showTime = flag(TimelineSettings::CARD_TIME, true);
    look.showChapters = flag(TimelineSettings::CARD_CHAPTERS, true);
    look.showCharacters = flag(TimelineSettings::CARD_CHARACTERS, false);
    look.showPlace = flag(TimelineSettings::CARD_PLACE, false);
    look.style.language = language();
    const std::string dates = setting(TimelineSettings::DATES, "words");
    look.style.dates = dates == "numbers" ? DateStyle::Numbers
                       : dates == "iso"   ? DateStyle::Iso
                                          : DateStyle::Words;
    look.style.parts =
        setting(TimelineSettings::PARTS, "words") == "clock" ? PartStyle::Clock : PartStyle::Words;

    const core::Theme& theme = core::ThemeManager::getInstance().getCurrentTheme();
    look.text = theme.palette.text;
    look.mutedText = theme.palette.placeholderText.isValid() ? theme.palette.placeholderText
                                                             : theme.palette.mid;
    look.mid = theme.palette.mid;
    look.selection = theme.palette.highlight;
    look.window = theme.palette.window;
    look.card = theme.palette.base;
    look.none = look.mutedText;
    look.lane = themeColor(TimelineColorKeys::LANE, theme.palette.base);
    look.laneAlternate = themeColor(TimelineColorKeys::LANE_ALTERNATE, theme.palette.alternateBase);
    look.tick = themeColor(TimelineColorKeys::TICK, theme.palette.mid);
    look.gap = themeColor(TimelineColorKeys::GAP, theme.palette.midlight);
    look.drop = themeColor(TimelineColorKeys::DROP, theme.palette.highlight);
    look.flashback = themeColor(TimelineColorKeys::FLASHBACK, theme.palette.highlight);
    for (std::size_t i = 0; i < look.palette.size(); ++i) {
        look.palette.at(i) = themeColor(TimelineColorKeys::PALETTE.at(i), theme.palette.highlight);
    }
    return look;
}

QColor TimelineLook::paletteColor(const QString& key) const {
    bool number = false;
    const int index = key.toInt(&number) - 1;
    return number && index >= 0 && index < static_cast<int>(palette.size())
               ? palette.at(static_cast<std::size_t>(index))
               : QColor();
}

TimelineView TimelineLook::newView() {
    TimelineView view;
    view.scale = TimelineViewNames::scale(QString::fromStdString(
                     setting(TimelineSettings::NEW_SCALE, "dates")))
                     .value_or(TimelineView::Scale::Dates);
    view.lanes = TimelineViewNames::lanes(QString::fromStdString(
                     setting(TimelineSettings::NEW_LANES, "threads")))
                     .value_or(TimelineView::Lanes::Threads);
    view.look = TimelineViewNames::look(QString::fromStdString(
                    setting(TimelineSettings::NEW_LOOK, "cards")))
                    .value_or(TimelineView::Look::Cards);
    view.compress = flag(TimelineSettings::NEW_COMPRESS, true);
    return view;
}

OpenMode TimelineLook::openMode() {
    const std::string mode = setting(TimelineSettings::OPEN_ON, "last");
    return mode == "axis"   ? OpenMode::Axis
           : mode == "list" ? OpenMode::List
           : mode == "book" ? OpenMode::Book
                            : OpenMode::Last;
}

TimeLanguage TimelineLook::language() {
    return setting("ui.language", "en").rfind("pl", 0) == 0 ? TimeLanguage::Polish
                                                            : TimeLanguage::English;
}

}  // namespace kalahari::timeline
