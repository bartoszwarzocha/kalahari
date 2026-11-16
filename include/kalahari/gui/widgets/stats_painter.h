// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file stats_painter.h
/// @brief The colors and the drawing that the statistics bar, the Statistics panel and the
/// goals share: graphs on a fine grid, thin lines with a translucent fill, as in the Task
/// Manager of Windows 11

#pragma once

#include <kalahari/core/writing_statistics.h>

#include <QColor>
#include <QList>
#include <QRectF>
#include <QString>

#include <string>
#include <vector>

class QPainter;

namespace kalahari::gui {

/// @brief The colors of the statistics in the theme in use (Settings > Statistics and Goals >
/// Colors); each theme keeps its own, in the "editor" section of its file. The grid, the frame,
/// the lines and the times of writing follow the theme's color, unless the writer chose their
/// own.
struct StatsColors {
    QColor background;  ///< The background of the graphs
    QColor grid;        ///< The grid
    QColor frame;       ///< The frame of the graphs
    QColor line;        ///< The lines of the graphs and their fill
    QColor goal;        ///< The line of the goal
    QColor done;        ///< The goal reached
    QColor muchFast;    ///< The pace of work: more and faster than usual
    QColor muchSlow;    ///< More than usual, but slower
    QColor littleFast;  ///< Mostly corrections
    QColor littleSlow;  ///< Less and slower than usual
    QColor hoursLow;    ///< When you write: the fewest words
    QColor hoursHigh;   ///< The most words
    QColor text;        ///< The texts on the graphs
    QColor bronze;      ///< The badges won: the first threshold
    QColor silver;      ///< The second
    QColor gold;        ///< The third
    QColor platinum;    ///< The fourth
    int fillOpacity = 20;  ///< The fill under the lines, in percent

    /// @brief One color of the list below, by its key ("statsLine")
    QColor color(const std::string& key) const;

    /// @brief The fill under the lines: the line color with fillOpacity
    QColor fill() const;

    /// @brief The color of a pace of work; the line color when it is unknown or idle
    QColor temperature(core::WritingTemperature temperature) const;

    /// @brief The color of the badges won of @p level: 0 bronze ... 3 platinum
    QColor badge(int level) const;

    /// @brief The colors of the theme in use, with the writer's own
    static StatsColors current();

    /// @brief Whether the color of @p key follows the theme's color (the first color of the
    /// icons, as the writer set it): the grid, the frame, the lines and the times of writing
    static bool followsTheme(const std::string& key);

    /// @brief The color of @p key that follows the theme: @p themeColor over @p under (the
    /// background of the graphs) in its share; invalid for a color of its own
    static QColor themeDefault(const std::string& key, const QColor& themeColor,
                               const QColor& under);

    /// @brief The keys of the colors (in the theme files, or the settings of those that follow
    /// the theme's color), in the order of the Colors page
    static const std::vector<std::string>& keys();

    /// @brief The setting of the fill of a theme: "stats.fillOpacity.<theme>"
    static std::string fillOpacitySetting(const std::string& themeName);

    /// @brief The fill of a theme that the writer did not change: lighter on a light theme
    static int defaultFillOpacity(bool darkTheme);
};

/// @brief The drawing of the graphs
namespace stats_painter {

/// @brief A round top of a scale for @p value: 1, 1.2, 1.5, 2, 2.5, 3, 4, 5, 6, 8 or 10 times a
/// power of ten, not below @p value; at least @p minimum
double niceMax(double value, double minimum = 10.0);

/// @brief A number in the writer's language, with the separators of thousands
QString number(qint64 value);

/// @brief A number of words: "1 word", "1,250 words", in the forms the language has
QString words(qint64 count);

/// @brief A time of writing as hours and minutes: "2:35"
QString hoursMinutes(qint64 ms);

/// @brief The name of a pace of work (core::WritingTemperature), as the writer reads it:
/// "more and faster than usual"; empty when it is unknown
QString temperatureName(core::WritingTemperature temperature);

/// @brief The fine grid: @p columns vertical and @p rows horizontal lines inside @p rect,
/// without the lines on its edges
void drawGrid(QPainter& painter, const QRectF& rect, int columns, int rows, const QColor& color);

/// @brief A line through @p values over the width of @p rect, with a translucent fill under
/// it; @p max is the top of the scale
void drawArea(QPainter& painter, const QRectF& rect, const QList<double>& values, double max,
              const QColor& line, const QColor& fill);

/// @brief The small marks before the texts on the graphs, @p size pixels high
enum class Glyph { Clock, Flame, Gauge, Check };
void drawGlyph(QPainter& painter, Glyph glyph, const QRectF& rect, const QColor& color);

}  // namespace stats_painter

}  // namespace kalahari::gui
