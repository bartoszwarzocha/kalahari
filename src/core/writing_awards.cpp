/// @file writing_awards.cpp
/// @brief The badges of writing and the writer's challenges

#include "kalahari/core/writing_awards.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace kalahari::core {

namespace {

const QString DAY_FORMAT = QStringLiteral("yyyy-MM-dd");

/// The names of the kinds in the records, in the order of BadgeKind
const std::array<const char*, 15> BADGE_IDS{
    "words",      "writingDays",  "streak",    "dailyGoals",    "weeklyGoals",
    "earlyBird",  "nightOwl",     "finishedBooks", "bookGoal",  "chaptersDone",
    "deadlines",  "steadyPace",   "monthGoalDays", "monthWords", "monthWeeks"};

/// The Monday of the week of @p day
QDate mondayOf(const QDate& day)
{
    return day.addDays(1 - day.dayOfWeek());
}

/// The words of a writing day that count toward the goals
int countedOn(const WritingStatistics& statistics, const QDate& day)
{
    return statistics.day(day).counted(statistics.rules().counting);
}

/// The words of the writing days from @p from to @p to that count toward the goals
int countedIn(const WritingStatistics& statistics, const QDate& from, const QDate& to)
{
    return from <= to ? statistics.period(from, to).counted(statistics.rules().counting) : 0;
}

/// The weeks from the week of @p from to the week of @p today whose weekly goal is reached;
/// with @p longestRun, the longest run of them in a row (the week of today does not break it
/// before it is over)
int weeksWithGoal(const AwardsInput& input, const WritingStatistics& statistics,
                  const QDate& from, const QDate& today, bool longestRun)
{
    if (!from.isValid() || from > today) {
        return 0;
    }
    int count = 0;
    int run = 0;
    int best = 0;
    for (QDate monday = mondayOf(from); monday <= today; monday = monday.addDays(7)) {
        const QDate sunday = monday.addDays(6);
        const int goal = input.weeklyGoalOf(monday);
        const bool reached =
            goal > 0 && countedIn(statistics, std::max(monday, from), std::min(sunday, today)) >= goal;
        if (reached) {
            ++count;
            best = std::max(best, ++run);
        } else if (sunday < today) {
            run = 0;
        }
    }
    return longestRun ? best : count;
}

/// The weeks over, from the week of @p from, whose words were written mostly at a time of
/// the day: in the morning (@p morning) or in the evening and at night
int weeksWrittenMostly(const WritingStatistics& statistics, const QDate& from,
                       const QDate& today, bool morning)
{
    if (!from.isValid()) {
        return 0;
    }
    int weeks = 0;
    for (QDate monday = mondayOf(from); monday.addDays(6) < today; monday = monday.addDays(7)) {
        qint64 mornings = 0;
        qint64 afternoons = 0;
        qint64 nights = 0;
        for (QDate day = monday; day <= monday.addDays(6); day = day.addDays(1)) {
            const auto entries = statistics.record().find(day);
            if (entries == statistics.record().cend()) {
                continue;
            }
            for (const WritingActivity& entry : *entries) {
                const int words = statistics.counted(entry).written;
                const int hour = entry.slot * ACTIVITY_SLOT_MINUTES / 60;
                if (hour >= 5 && hour < 12) {
                    mornings += words;
                } else if (hour >= 12 && hour < 18) {
                    afternoons += words;
                } else if (hour >= 18 || hour < 4) {
                    nights += words;
                }
            }
        }
        if (morning ? (mornings > afternoons && mornings > nights)
                    : (nights > mornings && nights > afternoons)) {
            ++weeks;
        }
    }
    return weeks;
}

/// A whole number of percent of @p part in @p whole, not more than 1000
int percentOf(qint64 part, qint64 whole)
{
    return whole > 0 ? int(std::clamp<qint64>(100 * part / whole, 0, 1000)) : 0;
}

QJsonArray badgesToJson(const QList<EarnedBadge>& badges)
{
    QJsonArray array;
    for (const EarnedBadge& badge : badges) {
        array.append(QJsonObject{{QStringLiteral("badge"), badgeIdOf(badge.kind)},
                                 {QStringLiteral("level"), badge.level},
                                 {QStringLiteral("date"), badge.date.toString(DAY_FORMAT)}});
    }
    return array;
}

QList<EarnedBadge> badgesFromJson(const QJsonArray& array)
{
    QList<EarnedBadge> badges;
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject object = array.at(i).toObject();
        const std::optional<BadgeKind> kind = badgeKindOf(object.value(QStringLiteral("badge")).toString());
        const int level = object.value(QStringLiteral("level")).toInt(-1);
        if (!kind || level < 0 || level >= BADGE_LEVELS) {
            continue;
        }
        badges.append({*kind, level,
                       QDate::fromString(object.value(QStringLiteral("date")).toString(), DAY_FORMAT),
                       QString()});
    }
    return badges;
}

}  // namespace

// =============================================================================
// The kinds
// =============================================================================

BadgeGroup badgeGroupOf(BadgeKind kind)
{
    switch (kind) {
    case BadgeKind::BookGoal:
    case BadgeKind::ChaptersDone:
    case BadgeKind::Deadlines:
    case BadgeKind::SteadyPace:
        return BadgeGroup::Book;
    case BadgeKind::MonthGoalDays:
    case BadgeKind::MonthWords:
    case BadgeKind::MonthWeeks:
        return BadgeGroup::Month;
    default:
        return BadgeGroup::Yours;
    }
}

QString badgeIdOf(BadgeKind kind)
{
    return QString::fromLatin1(BADGE_IDS[std::size_t(kind)]);
}

std::optional<BadgeKind> badgeKindOf(const QString& id)
{
    for (std::size_t i = 0; i < BADGE_IDS.size(); ++i) {
        if (id == QLatin1String(BADGE_IDS[i])) {
            return BadgeKind(i);
        }
    }
    return std::nullopt;
}

int BadgeProgress::level() const
{
    if (!available) {
        return 0;
    }
    int reached = 0;
    for (const int threshold : thresholds) {
        if (threshold > 0 && value >= threshold) {
            ++reached;
        }
    }
    return reached;
}

double BadgeProgress::share(int level) const
{
    if (level < 0 || level >= BADGE_LEVELS || thresholds[std::size_t(level)] <= 0 || !available) {
        return 0.0;
    }
    return std::clamp(double(value) / thresholds[std::size_t(level)], 0.0, 1.0);
}

// =============================================================================
// Counting
// =============================================================================

int AwardsInput::dailyGoalOf(const QDate& day) const
{
    const auto kept = dailyGoals.find(day);
    return kept != dailyGoals.cend() ? *kept : goals.rules().dailyGoalOn(day);
}

int AwardsInput::weeklyGoalOf(const QDate& monday) const
{
    // The goal of its last day kept
    auto kept = weeklyGoals.upperBound(monday.addDays(6));
    if (kept != weeklyGoals.cbegin()) {
        --kept;
        if (kept.key() >= monday) {
            return *kept;
        }
    }
    return goals.rules().weeklyGoalWords();
}

int longestStreak(const WritingStatistics& statistics, const QDate& from, const QDate& today)
{
    if (!from.isValid() || from > today) {
        return 0;
    }
    int run = 0;
    int best = 0;
    for (QDate day = from; day <= today; day = day.addDays(1)) {
        if (statistics.countsToStreak(day)) {
            best = std::max(best, ++run);
        } else if (statistics.rules().isWritingDay(day) && day != today) {
            run = 0;
        }
    }
    return best;
}

QList<QDate> weeksOfMonth(const QDate& month)
{
    QList<QDate> mondays;
    const QDate first(month.year(), month.month(), 1);
    // The first Thursday of the month, and the Mondays of the weeks of the Thursdays
    QDate thursday = first.addDays((4 - first.dayOfWeek() + 7) % 7);
    for (; thursday.month() == month.month(); thursday = thursday.addDays(7)) {
        mondays.append(thursday.addDays(-3));
    }
    return mondays;
}

QString monthScope(const QDate& day)
{
    return day.toString(QStringLiteral("yyyy-MM"));
}

QList<BadgeProgress> monthBadgeProgress(const AwardsInput& input, const QDate& month,
                                        const QDate& asOf)
{
    const QDate first(month.year(), month.month(), 1);
    const QDate last = first.addMonths(1).addDays(-1);
    const bool over = asOf > last;
    const QDate end = over ? last : asOf;

    // The writing days of the month so far with the daily goal reached; today counts once
    // it is reached
    BadgeProgress goalDays;
    goalDays.kind = BadgeKind::MonthGoalDays;
    goalDays.thresholds = {50, 75, 90, 100};
    for (QDate day = first; day <= end; day = day.addDays(1)) {
        const int goal = input.dailyGoalOf(day);
        if (goal <= 0) {
            continue;
        }
        const bool reached = countedOn(input.goals, day) >= goal;
        if (day == asOf && !over && !reached) {
            continue;
        }
        ++goalDays.total;
        goalDays.done += reached ? 1 : 0;
    }
    goalDays.value = percentOf(goalDays.done, goalDays.total);
    goalDays.available = goalDays.total > 0;

    // The words of the month against its plan: the daily goal of each of its days
    BadgeProgress words;
    words.kind = BadgeKind::MonthWords;
    words.thresholds = {50, 75, 100, 125};
    qint64 plan = 0;
    for (QDate day = first; day <= last; day = day.addDays(1)) {
        plan += input.dailyGoalOf(day);
    }
    words.done = countedIn(input.goals, first, end);
    words.total = int(std::min<qint64>(plan, 2'000'000'000));
    words.value = percentOf(words.done, plan);
    words.available = plan > 0;

    // The weeks of the month with the weekly goal
    BadgeProgress weeks;
    weeks.kind = BadgeKind::MonthWeeks;
    const QList<QDate> mondays = weeksOfMonth(first);
    for (const QDate& monday : mondays) {
        if (monday > end) {
            continue;
        }
        const int goal = input.weeklyGoalOf(monday);
        if (goal > 0 && countedIn(input.goals, monday, std::min(monday.addDays(6), asOf)) >= goal) {
            ++weeks.value;
        }
    }
    weeks.done = weeks.value;
    weeks.total = int(mondays.size());
    weeks.thresholds = {1, 2, 3, int(mondays.size())};
    return {goalDays, words, weeks};
}

QList<BadgeProgress> badgeProgress(const AwardsInput& input)
{
    const QDate today = input.today;
    const WritingRules& rules = input.goals.rules();
    QList<BadgeProgress> all;
    auto add = [&all](BadgeKind kind, int value, std::array<int, BADGE_LEVELS> thresholds) {
        BadgeProgress progress;
        progress.kind = kind;
        progress.value = value;
        progress.thresholds = thresholds;
        all.append(progress);
        return &all.last();
    };

    // The writer's own
    const QDate first = input.journal.firstDay();
    const QDate goalsFirst = input.goals.firstDay();
    int words = 0;
    int writingDays = 0;
    if (first.isValid() && first <= today) {
        words = input.journal.period(first, today).written;
        for (QDate day = first; day <= today; day = day.addDays(1)) {
            if (countedOn(input.journal, day) >= std::max(1, rules.streakMinimumWords)) {
                ++writingDays;
            }
        }
    }
    int dailyGoals = 0;
    if (goalsFirst.isValid()) {
        for (QDate day = goalsFirst; day <= today; day = day.addDays(1)) {
            const int goal = input.dailyGoalOf(day);
            if (goal > 0 && countedOn(input.goals, day) >= goal) {
                ++dailyGoals;
            }
        }
    }
    add(BadgeKind::Words, words, {10000, 50000, 100000, 500000});
    add(BadgeKind::WritingDays, writingDays, {7, 30, 100, 365});
    add(BadgeKind::Streak, longestStreak(input.journal, first, today), {7, 30, 60, 100});
    add(BadgeKind::DailyGoals, dailyGoals, {10, 50, 100, 250});
    add(BadgeKind::WeeklyGoals, weeksWithGoal(input, input.goals, goalsFirst, today, false),
        {4, 12, 26, 52});
    add(BadgeKind::EarlyBird, weeksWrittenMostly(input.journal, first, today, true),
        {1, 4, 12, 26});
    add(BadgeKind::NightOwl, weeksWrittenMostly(input.journal, first, today, false),
        {1, 4, 12, 26});
    add(BadgeKind::FinishedBooks, input.finishedBooks, {1, 3, 5, 10});

    // The open book's
    BadgeProgress* bookGoal = add(BadgeKind::BookGoal, percentOf(input.bookWords, input.bookTarget),
                                  {25, 50, 75, 100});
    bookGoal->done = input.bookWords;
    bookGoal->total = input.bookTarget;
    bookGoal->available = input.hasBook && input.bookTarget > 0;
    const int chapters = std::max(1, input.chapters);
    BadgeProgress* done = add(BadgeKind::ChaptersDone, input.chaptersDone,
                              {1, std::max(1, (chapters + 3) / 4), std::max(1, (chapters + 1) / 2),
                               chapters});
    done->done = input.chaptersDone;
    done->total = input.chapters;
    done->available = input.hasBook && input.chapters > 0;
    add(BadgeKind::Deadlines, input.deadlinesKept, {1, 2, 3, 5})->available = input.hasBook;
    const QDate bookFrom = input.bookSince.isValid() ? input.bookSince : input.book.firstDay();
    add(BadgeKind::SteadyPace,
        input.hasBook ? weeksWithGoal(input, input.book, bookFrom, today, true) : 0,
        {2, 4, 8, 12})
        ->available = input.hasBook;

    // The month's
    all.append(monthBadgeProgress(input, today, today));
    return all;
}

// =============================================================================
// The badges of a book
// =============================================================================

QString BookBadges::toJson() const
{
    QJsonObject object;
    if (since.isValid()) {
        object.insert(QStringLiteral("since"), since.toString(DAY_FORMAT));
    }
    object.insert(QStringLiteral("earned"), badgesToJson(earned));
    if (!history.isEmpty()) {
        object.insert(QStringLiteral("history"), badgesToJson(history));
    }
    if (!keptDeadlines.isEmpty()) {
        object.insert(QStringLiteral("kept"), QJsonArray::fromStringList(keptDeadlines));
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

BookBadges BookBadges::fromJson(const QString& text)
{
    BookBadges badges;
    const QJsonObject object = QJsonDocument::fromJson(text.toUtf8()).object();
    badges.since = QDate::fromString(object.value(QStringLiteral("since")).toString(), DAY_FORMAT);
    badges.earned = badgesFromJson(object.value(QStringLiteral("earned")).toArray());
    badges.history = badgesFromJson(object.value(QStringLiteral("history")).toArray());
    const QJsonArray kept = object.value(QStringLiteral("kept")).toArray();
    for (qsizetype i = 0; i < kept.size(); ++i) {
        const QJsonValue value = kept.at(i);
        if (value.isString()) {
            badges.keptDeadlines.append(value.toString());
        }
    }
    return badges;
}

// =============================================================================
// Challenges
// =============================================================================

QString challengeIdOf(ChallengeKind kind)
{
    switch (kind) {
    case ChallengeKind::DaysInRow:
        return QStringLiteral("daysInRow");
    case ChallengeKind::WritingDays:
        return QStringLiteral("writingDays");
    default:
        return QStringLiteral("words");
    }
}

ChallengeKind challengeKindOf(const QString& id)
{
    if (id == QLatin1String("daysInRow")) {
        return ChallengeKind::DaysInRow;
    }
    if (id == QLatin1String("writingDays")) {
        return ChallengeKind::WritingDays;
    }
    return ChallengeKind::Words;
}

ChallengeProgress challengeProgress(const WritingChallenge& challenge,
                                    const WritingStatistics& goals, const QDate& today)
{
    ChallengeProgress progress;
    const QDate end = std::min(challenge.end, today);
    switch (challenge.kind) {
    case ChallengeKind::Words:
        progress.value = countedIn(goals, challenge.start, end);
        break;
    case ChallengeKind::DaysInRow: {
        // The days in a row, of those since the challenge started: the run may have begun
        // before it
        int since = 0;
        for (QDate day = challenge.start; day <= end; day = day.addDays(1)) {
            since += goals.countsToStreak(day) ? 1 : 0;
        }
        progress.value = end >= challenge.start ? std::min(goals.streak(end), since) : 0;
        break;
    }
    case ChallengeKind::WritingDays:
        for (QDate day = challenge.start; day <= end; day = day.addDays(1)) {
            if (countedOn(goals, day) >= std::max(1, goals.rules().streakMinimumWords)) {
                ++progress.value;
            }
        }
        break;
    }
    progress.done = challenge.done.isValid() ||
                    (challenge.target > 0 && progress.value >= challenge.target);
    progress.over = !progress.done && today > challenge.end;
    progress.daysLeft = progress.done || progress.over ? 0 : int(today.daysTo(challenge.end)) + 1;
    return progress;
}

}  // namespace kalahari::core
