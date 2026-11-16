// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file time_axis.h
/// @brief The axis of a timeline: days measured in pixels, where long gaps without events are
/// drawn short, so that events years apart fit beside events hours apart

#ifndef KALAHARI_CORE_TIME_AXIS_H
#define KALAHARI_CORE_TIME_AXIS_H

#include <QList>

namespace kalahari::core {

/// @brief The axis of a timeline
///
/// Days are numbers: Julian days with the part of the day as a fraction (StoryTime::start()).
/// The axis goes from the first event to the last, with room before and after each; where the
/// room of one event ends long before the room of the next, the gap between them is drawn
/// short, with a fixed width, when the axis compresses gaps.
class TimeAxis {
public:
    /// @brief When an event is, in days
    struct Span {
        double start = 0.0;
        double end = 0.0;  ///< Its end; start for a moment
    };

    /// @brief Days drawn to scale
    struct Segment {
        double from = 0.0;  ///< Its first day
        double to = 0.0;    ///< Its last day
        double x = 0.0;     ///< Where it starts, in pixels
    };

    /// @brief A long gap without events, drawn short
    struct Gap {
        double from = 0.0;   ///< The day it starts, after the room of the events before it
        double to = 0.0;     ///< The day it ends, before the room of the events after it
        double x = 0.0;      ///< Where it starts, in pixels
        double width = 0.0;  ///< Its width, in pixels
        double days = 0.0;   ///< Days from the last event before it to the first after it
    };

    /// @brief How the axis measures
    struct Options {
        double pixelsPerDay = 30.0;  ///< The scale of the days
        double roomBefore = 30.0;    ///< Pixels before the start of each event
        double roomAfter = 186.0;    ///< Pixels after the end of each event: the width of its
                                     ///< card or label
        bool compress = true;        ///< Long gaps are drawn short
        double shortFrom = 90.0;     ///< A gap wider than this, in pixels, is drawn short
        double gapWidth = 44.0;      ///< The width of a gap drawn short, in pixels
    };

    /// @param spans The events, in any order
    /// @param emptyDay Where an axis without events is: around this day
    TimeAxis(const QList<Span>& spans, const Options& options, double emptyDay);

    /// @brief The pixels per day of zoom step @p step: 0 (a year at a glance) to 6 (hours)
    [[nodiscard]] static double pixelsPerDayOf(int step);

    /// @brief The number of zoom steps
    [[nodiscard]] static int zoomSteps();

    /// @brief Where day @p day is, in pixels from the start of the axis
    [[nodiscard]] double x(double day) const;

    /// @brief The day at @p x pixels from the start of the axis
    [[nodiscard]] double dayAt(double x) const;

    /// @brief The width of the axis, in pixels
    [[nodiscard]] double width() const { return m_width; }

    [[nodiscard]] double pixelsPerDay() const { return m_pixelsPerDay; }
    [[nodiscard]] const QList<Segment>& segments() const { return m_segments; }
    [[nodiscard]] const QList<Gap>& gaps() const { return m_gaps; }

private:
    double m_pixelsPerDay = 1.0;
    double m_width = 0.0;
    QList<Segment> m_segments;
    QList<Gap> m_gaps;
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_TIME_AXIS_H
