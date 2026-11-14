// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file badge_painter.cpp
/// @brief The look of the badges and their texts

#include "kalahari/gui/widgets/badge_painter.h"

#include "kalahari/gui/widgets/stats_painter.h"

#include <QCoreApplication>
#include <QHash>
#include <QLocale>
#include <QPainter>
#include <QSvgRenderer>

#include <algorithm>
#include <cmath>
#include <memory>

namespace kalahari::gui::badge_painter {

namespace {

/// @brief The context of the texts in the translations
struct BadgePainter {
    Q_DECLARE_TR_FUNCTIONS(BadgePainter)
};

using stats_painter::number;

/// @brief The marks of the kinds, 16 by 16, drawn in the color of the badge
const char* glyphOf(core::BadgeKind kind)
{
    static constexpr const char* PEN =
        R"(<path d="M10.8 2.7 13.3 5.2 5.6 12.9 2.6 13.4l.5-3z" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linejoin="round"/>)";
    static constexpr const char* CALENDAR =
        R"(<rect x="2.5" y="3.5" width="11" height="10" rx="1" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M2.5 6.5h11M5.5 2v3M10.5 2v3" stroke="currentColor" stroke-width="1.2"/>)";
    static constexpr const char* FLAME =
        R"(<path d="M8 1.5c.4 2.2 3.8 3.7 3.8 7.4A3.8 3.8 0 0 1 8 12.8a3.8 3.8 0 0 1-3.8-3.9c0-1.7.9-2.8 1.6-3.6.2 1.2.8 1.9 1.4 2.2C7.3 5.6 7.2 3.4 8 1.5Z" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"/>)";
    static constexpr const char* TARGET =
        R"(<circle cx="8" cy="8" r="6" fill="none" stroke="currentColor" stroke-width="1.3"/><circle cx="8" cy="8" r="3.2" fill="none" stroke="currentColor" stroke-width="1.3"/><circle cx="8" cy="8" r="1" fill="currentColor"/>)";
    static constexpr const char* CHECK =
        R"(<path d="M3 8.5 6.5 12 13 4.5" fill="none" stroke="currentColor" stroke-width="1.6"/>)";
    static constexpr const char* SUN =
        R"(<circle cx="8" cy="8" r="2.8" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M8 1.5v2M8 12.5v2M1.5 8h2M12.5 8h2M3.4 3.4l1.4 1.4M11.2 11.2l1.4 1.4M3.4 12.6l1.4-1.4M11.2 4.8l1.4-1.4" stroke="currentColor" stroke-width="1.2"/>)";
    static constexpr const char* MOON =
        R"(<path d="M10.5 2.5a5.8 5.8 0 1 0 3 9.4 5 5 0 0 1-3-9.4Z" fill="none" stroke="currentColor" stroke-width="1.2"/>)";
    static constexpr const char* BOOK =
        R"(<path d="M3 2.5h7.5a2 2 0 0 1 2 2v9H5a2 2 0 0 1-2-2v-9Zm0 9a2 2 0 0 1 2-2h7.5" fill="none" stroke="currentColor" stroke-width="1.2"/>)";
    static constexpr const char* DOCUMENT =
        R"(<path d="M4 1.5h5.5L12.5 4.5v10h-8.5zM9.5 1.5v3h3M6 8h4.5M6 10.5h4.5" fill="none" stroke="currentColor" stroke-width="1.2"/>)";
    static constexpr const char* RUNNER =
        R"(<circle cx="9.5" cy="2.8" r="1.3" fill="none" stroke="currentColor" stroke-width="1.1"/><path d="M5 14l2.2-4 2 1.6V14M7.2 10 8 6.2l2.6 2.3 2.4.4M8 6.2 5.6 7 4.4 9" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linejoin="round"/>)";
    switch (kind) {
    case core::BadgeKind::Words:
    case core::BadgeKind::MonthWords:
        return PEN;
    case core::BadgeKind::WritingDays:
    case core::BadgeKind::Deadlines:
        return CALENDAR;
    case core::BadgeKind::Streak:
        return FLAME;
    case core::BadgeKind::DailyGoals:
    case core::BadgeKind::MonthGoalDays:
        return TARGET;
    case core::BadgeKind::WeeklyGoals:
    case core::BadgeKind::MonthWeeks:
        return CHECK;
    case core::BadgeKind::EarlyBird:
        return SUN;
    case core::BadgeKind::NightOwl:
        return MOON;
    case core::BadgeKind::FinishedBooks:
    case core::BadgeKind::BookGoal:
        return BOOK;
    case core::BadgeKind::ChaptersDone:
        return DOCUMENT;
    case core::BadgeKind::SteadyPace:
        return RUNNER;
    }
    return PEN;
}

constexpr const char* TROPHY =
    R"(<path d="M5 2.5h6v3.8a3 3 0 0 1-6 0V2.5ZM5 3.5H2.8c0 2 .9 3 2.4 3.2M11 3.5h2.2c0 2-.9 3-2.4 3.2M8 9.3v2.4M5.5 13.5h5" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linejoin="round"/>)";

/// @brief A mark in @p color in @p rect; the renderers are kept, there are few of them
void drawSvg(QPainter& painter, const char* body, const QColor& color, const QRectF& rect)
{
    static QHash<QString, std::shared_ptr<QSvgRenderer>> renderers;
    const QString svg =
        QStringLiteral(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">%1</svg>)")
            .arg(QString::fromLatin1(body))
            .replace(QLatin1String("currentColor"), color.name(QColor::HexRgb));
    std::shared_ptr<QSvgRenderer>& renderer = renderers[svg];
    if (!renderer) {
        renderer = std::make_shared<QSvgRenderer>(svg.toUtf8());
    }
    painter.save();
    painter.setOpacity(painter.opacity() * color.alphaF());
    renderer->render(&painter, rect);
    painter.restore();
}

/// @brief The value of a threshold or of the kind now, with the unit of the kind
QString withUnit(core::BadgeKind kind, int value)
{
    switch (kind) {
    // One has a text of its own: the source language has no plural forms of its own
    case core::BadgeKind::Words:
        //: In Polish: 1 słowo
        return value == 1 ? BadgePainter::tr("1 word")
                          //: In Polish: %1 słowa / %1 słów
                          : BadgePainter::tr("%1 words", nullptr, value).arg(number(value));
    case core::BadgeKind::WritingDays:
    case core::BadgeKind::Streak:
    case core::BadgeKind::DailyGoals:
        //: In Polish: 1 dzień
        return value == 1 ? BadgePainter::tr("1 day")
                          //: In Polish: %1 dni
                          : BadgePainter::tr("%1 days", nullptr, value).arg(number(value));
    case core::BadgeKind::WeeklyGoals:
    case core::BadgeKind::EarlyBird:
    case core::BadgeKind::NightOwl:
    case core::BadgeKind::SteadyPace:
        //: In Polish: 1 tydzień
        return value == 1 ? BadgePainter::tr("1 week")
                          //: In Polish: %1 tygodnie / %1 tygodni
                          : BadgePainter::tr("%1 weeks", nullptr, value).arg(number(value));
    case core::BadgeKind::FinishedBooks:
        //: In Polish: 1 książka
        return value == 1 ? BadgePainter::tr("1 book")
                          //: In Polish: %1 książki / %1 książek
                          : BadgePainter::tr("%1 books", nullptr, value).arg(number(value));
    case core::BadgeKind::Deadlines:
        //: In Polish: 1 termin
        return value == 1 ? BadgePainter::tr("1 deadline")
                          //: In Polish: %1 terminy / %1 terminów
                          : BadgePainter::tr("%1 deadlines", nullptr, value).arg(number(value));
    case core::BadgeKind::BookGoal:
    case core::BadgeKind::MonthGoalDays:
    case core::BadgeKind::MonthWords:
        //: A share in percent. In Polish: %1%
        return BadgePainter::tr("%1%").arg(value);
    case core::BadgeKind::ChaptersDone:
    case core::BadgeKind::MonthWeeks:
        break;
    }
    return number(value);
}

}  // namespace

QString name(core::BadgeKind kind)
{
    switch (kind) {
    case core::BadgeKind::Words:
        //: The badges of all the words written. In Polish: Słowa
        return BadgePainter::tr("Words");
    case core::BadgeKind::WritingDays:
        //: In Polish: Dni pisania
        return BadgePainter::tr("Writing Days");
    case core::BadgeKind::Streak:
        //: The longest run of days in a row. In Polish: Seria
        return BadgePainter::tr("Streak");
    case core::BadgeKind::DailyGoals:
        //: In Polish: Cel dnia
        return BadgePainter::tr("Daily Goal");
    case core::BadgeKind::WeeklyGoals:
        //: In Polish: Cel tygodnia
        return BadgePainter::tr("Weekly Goal");
    case core::BadgeKind::EarlyBird:
        //: In Polish: Ranny ptaszek
        return BadgePainter::tr("Early Bird");
    case core::BadgeKind::NightOwl:
        //: In Polish: Nocny marek
        return BadgePainter::tr("Night Owl");
    case core::BadgeKind::FinishedBooks:
        //: In Polish: Ukończone książki
        return BadgePainter::tr("Finished Books");
    case core::BadgeKind::BookGoal:
        //: In Polish: Cel książki
        return BadgePainter::tr("Book Goal");
    case core::BadgeKind::ChaptersDone:
        //: In Polish: Rozdziały gotowe
        return BadgePainter::tr("Chapters Done");
    case core::BadgeKind::Deadlines:
        //: In Polish: Terminy
        return BadgePainter::tr("Deadlines");
    case core::BadgeKind::SteadyPace:
        //: In Polish: Równe tempo
        return BadgePainter::tr("Steady Pace");
    case core::BadgeKind::MonthGoalDays:
        //: In Polish: Miesiąc z celem
        return BadgePainter::tr("Month with the Goal");
    case core::BadgeKind::MonthWords:
        //: In Polish: Słowa miesiąca
        return BadgePainter::tr("Words of the Month");
    case core::BadgeKind::MonthWeeks:
        //: In Polish: Tygodnie z celem
        return BadgePainter::tr("Weeks with the Goal");
    }
    return QString();
}

QString description(const core::BadgeProgress& progress)
{
    switch (progress.kind) {
    case core::BadgeKind::Words:
        //: In Polish: Wszystkie słowa napisane w programie, we wszystkich książkach.
        return BadgePainter::tr("All the words written in the program, in all the books.");
    case core::BadgeKind::WritingDays:
        //: In Polish: Dni, w których powstało co najmniej minimum słów dnia.
        return BadgePainter::tr("The days with at least the minimum words of a day written.");
    case core::BadgeKind::Streak:
        //: In Polish: Najdłuższa seria dni z rzędu; dni wolne jej nie przerywają.
        return BadgePainter::tr("The longest run of days in a row; days off do not break it.");
    case core::BadgeKind::DailyGoals:
        //: In Polish: Dni z osiągniętym celem dziennym – takim, jaki był wtedy ustawiony.
        return BadgePainter::tr("The days with the daily goal reached – the goal as it was then.");
    case core::BadgeKind::WeeklyGoals:
        //: In Polish: Tygodnie z osiągniętym celem tygodniowym.
        return BadgePainter::tr("The weeks with the weekly goal reached.");
    case core::BadgeKind::EarlyBird:
        //: In Polish: Tygodnie, w których najwięcej słów powstało rano (5:00–12:00).
        return BadgePainter::tr("The weeks with the most words written in the morning (5:00–12:00).");
    case core::BadgeKind::NightOwl:
        //: In Polish: Tygodnie, w których najwięcej słów powstało wieczorem i w nocy
        //: (18:00–4:00).
        return BadgePainter::tr("The weeks with the most words written in the evening and at night "
                  "(18:00–4:00).");
    case core::BadgeKind::FinishedBooks:
        //: In Polish: Książki, które osiągnęły swoją docelową długość.
        return BadgePainter::tr("The books that reached their target length.");
    case core::BadgeKind::BookGoal:
        if (progress.total > 0) {
            //: In Polish: Postęp celu długości książki: %1 słów.
            return BadgePainter::tr("The progress toward the target length of the book: %1 words.", nullptr, progress.total)
                .arg(number(progress.total));
        }
        //: In Polish: Postęp celu długości książki. Długość ustawia się na karcie „Cele i terminy
        //: pisania”.
        return BadgePainter::tr("The progress toward the target length of the book. The length is set on the "
                  "tab Writing Goals and Deadlines.");
    case core::BadgeKind::ChaptersDone:
        //: In Polish: Rozdziały w stanie „Gotowy”: pierwszy, ćwierć, połowa, wszystkie.
        return BadgePainter::tr("The chapters with the status Final: the first, a quarter, half, all.");
    case core::BadgeKind::Deadlines:
        //: In Polish: Terminy z karty „Cele i terminy pisania” dotrzymane przed datą.
        return BadgePainter::tr("The deadlines from the tab Writing Goals and Deadlines kept before their day.");
    case core::BadgeKind::SteadyPace:
        //: In Polish: Tygodnie z rzędu z osiągniętym celem tygodniowym, przy tej książce.
        return BadgePainter::tr("The weeks in a row with the weekly goal reached, in this book.");
    case core::BadgeKind::MonthGoalDays:
        //: In Polish: Dni pisania tego miesiąca z osiągniętym celem dziennym. Odznaka przychodzi
        //: na koniec miesiąca.
        return BadgePainter::tr("The writing days of this month with the daily goal reached. The badge comes "
                  "when the month is over.");
    case core::BadgeKind::MonthWords:
        if (progress.total > 0) {
            //: In Polish: Słowa wobec planu miesiąca: cel dnia w każdym dniu pisania (%1 słów).
            return BadgePainter::tr("The words against the plan of the month: the daily goal of each writing "
                      "day (%1 words).", nullptr, progress.total)
                .arg(number(progress.total));
        }
        //: In Polish: Słowa wobec planu miesiąca: cel dnia w każdym dniu pisania.
        return BadgePainter::tr("The words against the plan of the month: the daily goal of each writing day.");
    case core::BadgeKind::MonthWeeks:
        //: In Polish: Tygodnie tego miesiąca z osiągniętym celem tygodniowym.
        return BadgePainter::tr("The weeks of this month with the weekly goal reached.");
    }
    return QString();
}

QString levelName(int level)
{
    switch (level) {
    case 0:
        //: A badge of the first threshold. In Polish: brązowa
        return BadgePainter::tr("bronze");
    case 1:
        //: In Polish: srebrna
        return BadgePainter::tr("silver");
    case 2:
        //: In Polish: złota
        return BadgePainter::tr("gold");
    default:
        //: In Polish: platynowa
        return BadgePainter::tr("platinum");
    }
}

QString thresholdText(const core::BadgeProgress& progress, int level)
{
    level = std::clamp(level, 0, core::BADGE_LEVELS - 1);
    if (progress.kind == core::BadgeKind::ChaptersDone) {
        switch (level) {
        case 0:
            //: The first chapter done. In Polish: pierwszy
            return BadgePainter::tr("the first");
        case 1:
            //: A quarter of the chapters. In Polish: ćwierć
            return BadgePainter::tr("a quarter");
        case 2:
            //: Half of the chapters. In Polish: połowa
            return BadgePainter::tr("half");
        default:
            //: All the chapters. In Polish: wszystkie
            return BadgePainter::tr("all");
        }
    }
    if (progress.kind == core::BadgeKind::MonthWeeks) {
        switch (level) {
        case 0:
            //: One week of the month. In Polish: jeden
            return BadgePainter::tr("one");
        case 1:
            //: In Polish: dwa
            return BadgePainter::tr("two");
        case 2:
            //: In Polish: trzy
            return BadgePainter::tr("three");
        default:
            //: All the weeks of the month. In Polish: wszystkie
            return BadgePainter::tr("all weeks");
        }
    }
    return withUnit(progress.kind, progress.thresholds[std::size_t(level)]);
}

QString title(const core::BadgeProgress& progress, int level)
{
    level = std::clamp(level, 0, core::BADGE_LEVELS - 1);
    if (progress.kind == core::BadgeKind::Streak) {
        const int days = progress.thresholds[std::size_t(level)];
        //: In Polish: Seria: %1 dni z rzędu
        return BadgePainter::tr("Streak: %1 days in a row", nullptr, days).arg(number(days));
    }
    //: The name of a badge and its threshold. In Polish: %1: %2
    return BadgePainter::tr("%1: %2").arg(name(progress.kind), thresholdText(progress, level));
}

QString valueText(const core::BadgeProgress& progress)
{
    switch (progress.kind) {
    case core::BadgeKind::BookGoal:
    case core::BadgeKind::MonthWords:
        if (progress.total > 0) {
            //: In Polish: %1 z %2 słów (%3%)
            return BadgePainter::tr("%1 of %2 words (%3%)", nullptr, progress.total)
                .arg(number(progress.done), number(progress.total))
                .arg(progress.value);
        }
        break;
    case core::BadgeKind::MonthGoalDays:
        if (progress.total > 0) {
            //: In Polish: %1 z %2 dni (%3%)
            return BadgePainter::tr("%1 of %2 days (%3%)", nullptr, progress.total)
                .arg(number(progress.done), number(progress.total))
                .arg(progress.value);
        }
        break;
    case core::BadgeKind::ChaptersDone:
    case core::BadgeKind::MonthWeeks:
        //: Done of all. In Polish: %1 z %2
        return BadgePainter::tr("%1 of %2").arg(number(progress.done), number(progress.total));
    default:
        break;
    }
    return withUnit(progress.kind, progress.value);
}

QString wonOn(const QDate& date)
{
    //: The day a badge was won. In Polish: zdobyta %1
    return BadgePainter::tr("won on %1").arg(QLocale().toString(date, QStringLiteral("d MMMM yyyy")));
}

QString toolTip(const core::BadgeProgress& progress, int level,
                const std::optional<core::EarnedBadge>& won, int timesWon)
{
    QString state;
    if (won) {
        //: In Polish: %1 – %2: zdobyta %3.
        state = BadgePainter::tr("%1 – %2: won on %3.")
                    .arg(name(progress.kind), thresholdText(progress, level),
                         QLocale().toString(won->date, QStringLiteral("d MMMM yyyy")));
    } else if (progress.available) {
        const int percent = int(std::floor(progress.share(level) * 100.0));
        //: In Polish: %1 – %2: jeszcze nie, %3% (teraz %4).
        state = BadgePainter::tr("%1 – %2: not yet, %3% (now %4).")
                    .arg(name(progress.kind), thresholdText(progress, level))
                    .arg(percent)
                    .arg(valueText(progress));
    } else {
        //: In Polish: %1 – %2: jeszcze nie.
        state = BadgePainter::tr("%1 – %2: not yet.").arg(name(progress.kind), thresholdText(progress, level));
    }
    QString tip = state + QLatin1Char(' ') + description(progress);
    if (timesWon > 1) {
        //: The months a badge of the month was won in, 2 or more. In Polish: Zdobyta %n razy.
        tip += QLatin1Char(' ') + BadgePainter::tr("Won %n times.", nullptr, timesWon);
    }
    return tip;
}

QColor colorOf(const StatsColors& colors, int level, bool won, const QColor& locked)
{
    return won ? colors.badge(level) : locked;
}

void drawBadge(QPainter& painter, const QRectF& circle, core::BadgeKind kind, const QColor& color,
               bool won, double ring)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF inner = circle.adjusted(ring / 2.0, ring / 2.0, -ring / 2.0, -ring / 2.0);
    QPen pen(color, ring);
    if (!won) {
        pen.setStyle(Qt::DashLine);
        pen.setDashPattern({2.0, 1.6});
    }
    QColor fill = color;
    fill.setAlphaF(won ? 0.16F : 0.0F);
    painter.setPen(pen);
    painter.setBrush(won ? QBrush(fill) : QBrush(Qt::NoBrush));
    painter.drawEllipse(inner);
    const double side = circle.width() / 2.0;
    drawSvg(painter, glyphOf(kind), color,
            QRectF(circle.center().x() - side / 2.0, circle.center().y() - side / 2.0, side, side));
    painter.restore();
}

void drawTrophy(QPainter& painter, const QRectF& rect, const QColor& color)
{
    drawSvg(painter, TROPHY, color, rect);
}

QString challengeTitle(const core::WritingChallenge& challenge)
{
    const int target = challenge.target;
    switch (challenge.kind) {
    case core::ChallengeKind::Words:
        return withUnit(core::BadgeKind::Words, target);
    case core::ChallengeKind::DaysInRow:
        //: A challenge. In Polish: 1 dzień z rzędu
        return target == 1 ? BadgePainter::tr("1 day in a row")
                           //: A challenge. In Polish: %1 dni z rzędu
                           : BadgePainter::tr("%1 days in a row", nullptr, target)
                                 .arg(number(target));
    case core::ChallengeKind::WritingDays:
        //: A challenge. In Polish: 1 dzień pisania
        return target == 1 ? BadgePainter::tr("1 writing day")
                           //: A challenge. In Polish: %1 dni pisania
                           : BadgePainter::tr("%1 writing days", nullptr, target)
                                 .arg(number(target));
    }
    return QString();
}

QString challengePeriod(const core::WritingChallenge& challenge)
{
    //: The last day of a challenge. In Polish: do %1
    return BadgePainter::tr("until %1")
        .arg(QLocale().toString(challenge.end, QStringLiteral("d MMMM")));
}

QString challengeValue(const core::WritingChallenge& challenge, int value)
{
    if (challenge.kind == core::ChallengeKind::Words) {
        //: Words done of a challenge. In Polish: %1 z %2
        return BadgePainter::tr("%1 of %2").arg(number(value), number(challenge.target));
    }
    //: Days done of a challenge. In Polish: %1 z %2 dni
    return BadgePainter::tr("%1 of %2 days", nullptr, challenge.target)
        .arg(number(value), number(challenge.target));
}

QString challengeState(const core::WritingChallenge& challenge,
                       const core::ChallengeProgress& progress)
{
    if (progress.done) {
        const QDate day = challenge.done.isValid() ? challenge.done : QDate();
        if (!day.isValid()) {
            //: A challenge done. In Polish: ukończone
            return BadgePainter::tr("done");
        }
        //: In Polish: ukończone %1
        return BadgePainter::tr("done on %1")
            .arg(QLocale().toString(day, QStringLiteral("d MMMM")));
    }
    if (progress.over) {
        //: A challenge whose last day has passed without it done. In Polish: czas minął
        return BadgePainter::tr("time is up");
    }
    if (progress.daysLeft <= 1) {
        //: In Polish: ostatni dzień
        return BadgePainter::tr("the last day");
    }
    //: In Polish: został %n dzień / zostały %n dni / zostało %n dni
    return BadgePainter::tr("%n days left", nullptr, progress.daysLeft);
}

}  // namespace kalahari::gui::badge_painter
