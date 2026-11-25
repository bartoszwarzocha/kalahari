// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_look.h
/// @brief How timelines look and behave: the settings of Settings > Timelines and the colors
/// of the theme

#ifndef KALAHARI_TIMELINE_TIMELINE_LOOK_H
#define KALAHARI_TIMELINE_TIMELINE_LOOK_H

#include <kalahari/timeline/story_time.h>
#include <kalahari/timeline/timeline.h>

#include <QColor>
#include <QString>

#include <array>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace kalahari::timeline {

/// @brief The view a timeline opens on
enum class OpenMode {
    Axis,  ///< The axis
    List,  ///< The list
    Book,  ///< The time and the book
    Last   ///< The view it was left on
};

/// @brief The settings of the timelines, under "timelines."
struct TimelineSettings {
    static constexpr const char* NEW_SCALE = "timelines.newScale";  ///< "dates", "days", "order"
    static constexpr const char* NEW_LANES = "timelines.newLanes";  ///< "threads"... "one"
    static constexpr const char* NEW_LOOK = "timelines.newLook";    ///< "cards", "labels"
    static constexpr const char* NEW_COMPRESS = "timelines.newCompress";  ///< bool
    static constexpr const char* OPEN_ON = "timelines.openOn";  ///< "axis", "list", "book"...
    static constexpr const char* CARD_TIME = "timelines.cardTime";              ///< bool
    static constexpr const char* CARD_CHAPTERS = "timelines.cardChapters";      ///< bool
    static constexpr const char* CARD_CHARACTERS = "timelines.cardCharacters";  ///< bool
    static constexpr const char* CARD_PLACE = "timelines.cardPlace";            ///< bool
    static constexpr const char* DATES = "timelines.dates";  ///< "words", "numbers", "iso"
    static constexpr const char* PARTS = "timelines.parts";  ///< "words", "clock"

    /// @brief The settings and their default values, for core::settings_schema::addDefaults()
    [[nodiscard]] static std::map<std::string, nlohmann::json> defaults();
};

/// @brief The keys of the colors of the theme ("editor" section of the theme files)
struct TimelineColorKeys {
    /// The palette of the threads, shared with the mind maps: six colors
    static constexpr std::array<const char*, 6> PALETTE{
        "workshopPalette1", "workshopPalette2", "workshopPalette3",
        "workshopPalette4", "workshopPalette5", "workshopPalette6"};
    static constexpr const char* LANE = "timelineLane";
    static constexpr const char* LANE_ALTERNATE = "timelineLaneAlternate";
    static constexpr const char* TICK = "timelineTick";
    static constexpr const char* GAP = "timelineGap";
    static constexpr const char* DROP = "timelineDrop";
    static constexpr const char* FLASHBACK = "timelineFlashback";
};

/// @brief How the timelines look now: the settings and the colors of the theme in use
struct TimelineLook {
    bool showTime = true;         ///< The cards show the time
    bool showChapters = true;     ///< The cards show the chapters
    bool showCharacters = false;  ///< The cards show the characters
    bool showPlace = false;       ///< The cards show the place
    StoryTimeStyle style;         ///< How the times are written

    QColor lane;           ///< The background of a lane
    QColor laneAlternate;  ///< The background of every other lane
    QColor tick;           ///< The ticks of the axis, the guide of a lane
    QColor gap;            ///< The stripes of a gap drawn short
    QColor drop;           ///< Where a dragged event goes
    QColor flashback;      ///< The ring of an event told after later events
    QColor window;         ///< The heads of the lanes and of the axis
    QColor card;           ///< The background of a card
    QColor text;           ///< The text
    QColor mutedText;      ///< The hints, the times on the cards
    QColor mid;            ///< The frames
    QColor selection;      ///< The frame of the selected event
    QColor none;           ///< An event without a thread or a color
    std::array<QColor, 6> palette;

    /// @brief The look of the settings and of the theme in use
    [[nodiscard]] static TimelineLook current();

    /// @brief The color of palette key @p key ("1"..."6"); invalid for another key
    [[nodiscard]] QColor paletteColor(const QString& key) const;

    /// @brief The view of a new timeline, as the settings say
    [[nodiscard]] static TimelineView newView();

    /// @brief The view a timeline opens on, as the settings say
    [[nodiscard]] static OpenMode openMode();

    /// @brief The language of the times the program writes: that of the program
    [[nodiscard]] static TimeLanguage language();
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_LOOK_H
