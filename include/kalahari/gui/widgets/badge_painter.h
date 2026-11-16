// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file badge_painter.h
/// @brief The look of the badges and their texts, shared by the tab Challenges and Badges, the
/// Statistics panel and the messages: a round badge with the mark of its kind, in the color of
/// its level when won and in grey, dashed, when not yet

#pragma once

#include <kalahari/core/writing_awards.h>

#include <QColor>
#include <QDate>
#include <QRectF>
#include <QString>

#include <optional>

class QPainter;

namespace kalahari::gui {

struct StatsColors;

namespace badge_painter {

/// @brief The name of @p kind: "Streak"
QString name(core::BadgeKind kind);

/// @brief What @p kind counts, for the tooltip of its badges
QString description(const core::BadgeProgress& progress);

/// @brief The name of the badge @p level: "bronze", "silver", "gold", "platinum"
QString levelName(int level);

/// @brief The threshold of the badge @p level: "10,000 words", "25%", "a quarter"
QString thresholdText(const core::BadgeProgress& progress, int level);

/// @brief The name of a badge, as a message says it: "Streak: 30 days in a row"
QString title(const core::BadgeProgress& progress, int level);

/// @brief How far the kind is now: "34 days", "5 of 6 days (83%)", "4 of 18"
QString valueText(const core::BadgeProgress& progress);

/// @brief The day a badge was won: "won on 8 November 2026"
QString wonOn(const QDate& date);

/// @brief The tooltip of the badge @p level: what it is, whether it is won and what it counts;
/// @p timesWon is the months a badge of the month was won in
QString toolTip(const core::BadgeProgress& progress, int level,
                const std::optional<core::EarnedBadge>& won, int timesWon = 0);

/// @brief The color of a badge: its level's when won, @p locked when not
QColor colorOf(const StatsColors& colors, int level, bool won, const QColor& locked);

/// @brief A badge in @p circle: a ring in @p color with the mark of @p kind; won, it has a
/// solid ring and a light fill; not yet, a dashed ring; @p ring is its width in pixels
void drawBadge(QPainter& painter, const QRectF& circle, core::BadgeKind kind, const QColor& color,
               bool won, double ring);

/// @brief The cup of the Achievements, as on the button of the tab
void drawTrophy(QPainter& painter, const QRectF& rect, const QColor& color);

/// @brief A challenge as the list says it: "5,000 words", "30 days in a row"
QString challengeTitle(const core::WritingChallenge& challenge);

/// @brief Its last day: "until 15 November"
QString challengePeriod(const core::WritingChallenge& challenge);

/// @brief How far it is: "3,200 of 5,000", "7 of 30 days"
QString challengeValue(const core::WritingChallenge& challenge, int value);

/// @brief What is left of it: "3 days left", "the last day", "done on 12 November", "time is up"
QString challengeState(const core::WritingChallenge& challenge,
                       const core::ChallengeProgress& progress);

}  // namespace badge_painter

}  // namespace kalahari::gui
