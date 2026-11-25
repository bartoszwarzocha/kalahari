// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file spread.h
/// @brief A spread: one picture over two pages of the book, a left one and a right one (a map
///        of the world, a family tree)
///
/// A spread is an element of the book of the kind "spread" (kalahari.base), placed as a
/// chapter is: in the front part, between the chapters, in a part or in the back part. It has
/// no text of its own: its settings are kept with the element in the .klh file, and its
/// picture is a material of the Workshop (a picture, or a picture of the cover):
/// @code
/// "spread": { "picture": "e-31", "area": "margins", "fit": "fill",
///             "crop": { "x": 0, "y": -0.2, "zoom": 120 }, "gutter": 4,
///             "caption": { "label": "Map", "text": "The route of the caravan, 1912" },
///             "pageNumbers": false, "inContents": true }
/// @endcode
/// A spread always starts on a left page, so that its halves lie side by side in the printed
/// book; a page before it is left empty when needed.

#ifndef KALAHARI_CORE_SPREAD_H
#define KALAHARI_CORE_SPREAD_H

#include <QJsonObject>
#include <QString>

namespace kalahari::core {

/// @brief The settings of a spread, saved with its element as "spread"
struct SpreadData {
    /// @brief The kind of the spreads in the base package, and the editor of their tabs
    static constexpr const char* KIND = "spread";
    static constexpr const char* EDITOR = "spread";

    /// @name Where the picture lies (area)
    /// @{
    static constexpr const char* MARGINS = "margins";  ///< Within the top, bottom and outer
                                                       ///< margins, across the spine
    static constexpr const char* EDGE = "edge";        ///< Over both pages to the edges of paper
    /// @}

    /// @name How the picture fits its area (fit)
    /// @{
    static constexpr const char* WHOLE = "whole";  ///< The whole picture, the rest left white
    static constexpr const char* FILL = "fill";    ///< It fills the area, its edges cut off
    /// @}

    /// @brief The most room at the spine, in millimetres
    static constexpr double MAX_GUTTER = 15.0;

    /// @brief The size of a picture that fills its area, in percent of the smallest that does
    static constexpr int MIN_ZOOM = 100;
    static constexpr int MAX_ZOOM = 300;

    /// @name The settings of new spreads (Settings > Editor > Images, Spreads)
    /// @{
    static constexpr const char* AREA_KEY = "editor.spread.area";
    static constexpr const char* FIT_KEY = "editor.spread.fit";
    static constexpr const char* GUTTER_KEY = "editor.spread.gutter";
    static constexpr const char* CAPTION_KEY = "editor.spread.caption";
    static constexpr const char* CAPTION_LABEL_KEY = "editor.spread.captionLabel";
    static constexpr const char* PAGE_NUMBERS_KEY = "editor.spread.pageNumbers";
    static constexpr const char* IN_CONTENTS_KEY = "editor.spread.inContents";
    /// @}

    QString picture;          ///< The material whose picture it shows; empty: none yet
    QString area = MARGINS;   ///< MARGINS or EDGE
    QString fit = WHOLE;      ///< WHOLE or FILL
    /// Of a picture that fills its area: where it lies, from -1 (its left or top edge at the
    /// area's) through 0 (in the middle) to 1 (its right or bottom edge at the area's)
    double cropX = 0.0;
    double cropY = 0.0;
    int zoom = MIN_ZOOM;      ///< Of a picture that fills its area, MIN_ZOOM-MAX_ZOOM
    double gutter = 0.0;      ///< The room at the spine, 0-MAX_GUTTER mm: each half of the
                              ///< picture moves away from the fold by half of it, and the strip
                              ///< at the spine shows on both pages
    bool caption = false;     ///< A caption under the picture, on the right page
    QString captionLabel;     ///< Its label with a number ("Map 1."); empty: its text alone
    QString captionText;      ///< Its text
    bool pageNumbers = false;  ///< The numbers of its pages and the running head show on them
    bool inContents = false;  ///< Its title is a line of the table of contents
    QJsonObject extra;        ///< Fields this version does not know, saved back

    bool operator==(const SpreadData&) const = default;

    /// @brief Whether the picture fills its area (FILL), its edges cut off
    [[nodiscard]] bool fills() const { return fit == QLatin1String(FILL); }

    /// @brief Whether the picture goes to the edges of paper (EDGE)
    [[nodiscard]] bool toEdges() const { return area == QLatin1String(EDGE); }

    /// @brief The settings of "spread" in the .klh file, each value clamped to what it allows
    [[nodiscard]] static SpreadData fromJson(const QJsonObject& json);

    /// @brief The settings as the .klh file keeps them
    [[nodiscard]] QJsonObject toJson() const;

    /// @brief The settings of a new spread, as the Settings give them
    [[nodiscard]] static SpreadData ofSettings();

    /// @brief The settings clamped to what they allow (an unknown area or fit: the default)
    [[nodiscard]] SpreadData normalized() const;
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_SPREAD_H
