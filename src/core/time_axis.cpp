// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file time_axis.cpp
/// @brief The axis of a timeline: its stretches of days and its gaps drawn short

#include <kalahari/core/time_axis.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace kalahari::core {

namespace {

/// Pixels per day of the zoom steps: a year at a glance ... hours
constexpr std::array<double, 7> ZOOM_STEPS{0.6, 2.0, 5.0, 12.0, 30.0, 70.0, 160.0};

/// Days before and after the empty day of an axis without events
constexpr double EMPTY_ROOM = 5.0;

/// A tolerance for days compared after arithmetic
constexpr double EPSILON = 1e-9;

/// Events whose rooms meet, as one stretch of days
struct Cluster {
    double from = 0.0;       ///< With the room before its first event
    double to = 0.0;         ///< With the room after its last event
    double firstDay = 0.0;   ///< The start of its first event
    double lastDay = 0.0;    ///< The end of its last event
};

QList<Cluster> clustersOf(const QList<TimeAxis::Span>& spans, const TimeAxis::Options& options,
                          double pixelsPerDay) {
    QList<Cluster> clusters;
    for (const TimeAxis::Span& span : spans) {
        const double end = std::max(span.end, span.start);
        clusters.append(Cluster{span.start - options.roomBefore / pixelsPerDay,
                                end + options.roomAfter / pixelsPerDay, span.start, end});
    }
    std::sort(clusters.begin(), clusters.end(),
              [](const Cluster& a, const Cluster& b) { return a.from < b.from; });
    QList<Cluster> joined;
    for (const Cluster& cluster : clusters) {
        if (!joined.isEmpty() && cluster.from <= joined.last().to) {
            joined.last().to = std::max(joined.last().to, cluster.to);
            joined.last().lastDay = std::max(joined.last().lastDay, cluster.lastDay);
        } else {
            joined.append(cluster);
        }
    }
    return joined;
}

}  // namespace

TimeAxis::TimeAxis(const QList<Span>& spans, const Options& options, double emptyDay)
    : m_pixelsPerDay(std::max(options.pixelsPerDay, EPSILON)) {
    QList<Cluster> clusters = clustersOf(spans, options, m_pixelsPerDay);
    if (clusters.isEmpty()) {
        clusters.append(
            Cluster{emptyDay - EMPTY_ROOM, emptyDay + EMPTY_ROOM, emptyDay, emptyDay});
    }
    double x = 0.0;
    for (qsizetype i = 0; i < clusters.size(); ++i) {
        const Cluster& cluster = clusters.at(i);
        if (i > 0) {
            const Cluster& before = clusters.at(i - 1);
            const double gap = cluster.from - before.to;
            if (options.compress && gap * m_pixelsPerDay > options.shortFrom) {
                m_gaps.append(Gap{before.to, cluster.from, x, options.gapWidth,
                                  cluster.firstDay - before.lastDay});
                x += options.gapWidth;
            } else {
                m_segments.append(Segment{before.to, cluster.from, x});
                x += gap * m_pixelsPerDay;
            }
        }
        m_segments.append(Segment{cluster.from, cluster.to, x});
        x += (cluster.to - cluster.from) * m_pixelsPerDay;
    }
    m_width = x;
}

double TimeAxis::pixelsPerDayOf(int step) {
    const auto index =
        static_cast<std::size_t>(std::clamp(step, 0, static_cast<int>(ZOOM_STEPS.size()) - 1));
    return ZOOM_STEPS.at(index);
}

int TimeAxis::zoomSteps() {
    return static_cast<int>(ZOOM_STEPS.size());
}

double TimeAxis::x(double day) const {
    for (const Segment& segment : m_segments) {
        if (day >= segment.from - EPSILON && day <= segment.to + EPSILON) {
            return segment.x + (day - segment.from) * m_pixelsPerDay;
        }
    }
    for (const Gap& gap : m_gaps) {
        if (day > gap.from && day < gap.to) {
            return gap.x + (day - gap.from) / (gap.to - gap.from) * gap.width;
        }
    }
    return day < m_segments.first().from ? 0.0 : m_width;
}

double TimeAxis::dayAt(double x) const {
    for (const Segment& segment : m_segments) {
        const double end = segment.x + (segment.to - segment.from) * m_pixelsPerDay;
        if (x >= segment.x && x <= end) {
            return segment.from + (x - segment.x) / m_pixelsPerDay;
        }
    }
    for (const Gap& gap : m_gaps) {
        if (x > gap.x && x < gap.x + gap.width) {
            return gap.from + (x - gap.x) / gap.width * (gap.to - gap.from);
        }
    }
    return x < 0.0 ? m_segments.first().from : m_segments.last().to;
}

}  // namespace kalahari::core
