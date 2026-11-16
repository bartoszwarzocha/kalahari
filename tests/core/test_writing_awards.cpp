// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_writing_awards.cpp
/// @brief The badges of writing and the writer's challenges: what each kind counts, the month,
/// the badges of a book and what the journal keeps

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/writing_awards.h>
#include <kalahari/core/writing_journal.h>

#include <QTemporaryDir>
#include <QTime>

#include <algorithm>

using namespace kalahari::core;

namespace {

/// Thursday, 12 November 2026
const QDate TODAY(2026, 11, 12);

WritingActivity entry(int dayOfMonth, int hour, int written)
{
    WritingActivity activity;
    activity.date = QDate(2026, 11, dayOfMonth);
    activity.slot = activitySlotOf(QTime(hour, 0));
    activity.bookId = QStringLiteral("book");
    activity.elementId = QStringLiteral("ch-1");
    activity.written = written;
    activity.activeMs = 10 * 60 * 1000;
    return activity;
}

/// Two weeks of November: the mornings and the nights of the first week, the second up to
/// today. Under the default rules (1,000 words a day from Monday to Friday, 100 words keep
/// the days in a row going)
QList<WritingActivity> record()
{
    return {entry(2, 9, 1200),  entry(3, 20, 1000), entry(4, 21, 50),  entry(5, 10, 300),
            entry(6, 22, 1100), entry(7, 11, 200),  entry(9, 8, 1500), entry(10, 23, 900),
            entry(11, 19, 1000), entry(12, 9, 400)};
}

AwardsInput input()
{
    AwardsInput input;
    input.today = TODAY;
    input.journal.setActivity(record());
    input.goals.setActivity(record());
    input.book.setActivity(record());
    return input;
}

const BadgeProgress& of(const QList<BadgeProgress>& all, BadgeKind kind)
{
    const auto found = std::find_if(all.cbegin(), all.cend(),
                                    [kind](const BadgeProgress& progress) { return progress.kind == kind; });
    REQUIRE(found != all.cend());
    return *found;
}

}  // namespace

TEST_CASE("Writing awards: fifteen kinds of badges in three groups", "[core][statistics][awards]")
{
    REQUIRE(BADGE_KINDS.size() == 15);
    int yours = 0;
    int book = 0;
    int month = 0;
    for (const BadgeKind kind : BADGE_KINDS) {
        REQUIRE(badgeKindOf(badgeIdOf(kind)) == kind);
        switch (badgeGroupOf(kind)) {
        case BadgeGroup::Yours: ++yours; break;
        case BadgeGroup::Book: ++book; break;
        case BadgeGroup::Month: ++month; break;
        }
    }
    CHECK(yours == 8);
    CHECK(book == 4);
    CHECK(month == 3);
    CHECK_FALSE(badgeKindOf(QStringLiteral("nothing")).has_value());

    // A kind of badges and its four thresholds
    BadgeProgress progress;
    progress.value = 30;
    progress.thresholds = {7, 30, 60, 100};
    CHECK(progress.level() == 2);
    CHECK(progress.share(1) == 1.0);
    CHECK(progress.share(2) == 0.5);
    progress.available = false;
    CHECK(progress.level() == 0);
}

TEST_CASE("Writing awards: the writer's own badges from the record", "[core][statistics][awards]")
{
    AwardsInput data = input();
    QList<BadgeProgress> all = badgeProgress(data);
    REQUIRE(all.size() == 15);
    CHECK(of(all, BadgeKind::Words).value == 7650);
    // Not Wednesday the 4th (50 words) nor Sunday the 8th
    CHECK(of(all, BadgeKind::WritingDays).value == 9);
    // From Thursday the 5th to today: Saturday counts, Sunday neither breaks nor adds
    CHECK(of(all, BadgeKind::Streak).value == 7);
    CHECK(longestStreak(data.journal, QDate(2026, 11, 2), TODAY) == 7);
    CHECK(of(all, BadgeKind::DailyGoals).value == 5);
    CHECK(of(all, BadgeKind::WeeklyGoals).value == 0);
    // Only the week over: more words at night than in the morning
    CHECK(of(all, BadgeKind::NightOwl).value == 1);
    CHECK(of(all, BadgeKind::EarlyBird).value == 0);
    CHECK(of(all, BadgeKind::FinishedBooks).value == 0);

    // The goals as they were then: higher on Tuesday the 3rd, lower in the first week
    data.dailyGoals.insert(QDate(2026, 11, 3), 1100);
    data.weeklyGoals.insert(QDate(2026, 11, 6), 3000);
    all = badgeProgress(data);
    CHECK(of(all, BadgeKind::DailyGoals).value == 4);
    CHECK(of(all, BadgeKind::WeeklyGoals).value == 1);
    CHECK(data.weeklyGoalOf(QDate(2026, 11, 2)) == 3000);
    CHECK(data.weeklyGoalOf(QDate(2026, 11, 9)) == 5000);
}

TEST_CASE("Writing awards: the badges of the open book", "[core][statistics][awards]")
{
    AwardsInput data = input();
    QList<BadgeProgress> all = badgeProgress(data);
    CHECK_FALSE(of(all, BadgeKind::BookGoal).available);
    CHECK_FALSE(of(all, BadgeKind::SteadyPace).available);

    data.hasBook = true;
    data.bookWords = 45000;
    data.bookTarget = 90000;
    data.chapters = 18;
    data.chaptersDone = 4;
    data.deadlinesKept = 1;
    data.weeklyGoals.insert(QDate(2026, 11, 6), 3000);
    data.weeklyGoals.insert(QDate(2026, 11, 12), 3000);
    all = badgeProgress(data);
    CHECK(of(all, BadgeKind::BookGoal).value == 50);
    CHECK(of(all, BadgeKind::BookGoal).level() == 2);
    // The first, a quarter, half and all of the 18 chapters
    CHECK(of(all, BadgeKind::ChaptersDone).thresholds == std::array<int, 4>{1, 5, 9, 18});
    CHECK(of(all, BadgeKind::ChaptersDone).level() == 1);
    CHECK(of(all, BadgeKind::Deadlines).level() == 1);
    // Two weeks in a row with the weekly goal in the book
    CHECK(of(all, BadgeKind::SteadyPace).value == 2);

    // Start Over: the badges of the book count from that day
    data.bookSince = QDate(2026, 11, 9);
    all = badgeProgress(data);
    CHECK(of(all, BadgeKind::SteadyPace).value == 1);
}

TEST_CASE("Writing awards: the badges of the month", "[core][statistics][awards]")
{
    // The weeks of a month are those of its Thursdays
    CHECK(weeksOfMonth(QDate(2026, 11, 20)) ==
          QList<QDate>{QDate(2026, 11, 2), QDate(2026, 11, 9), QDate(2026, 11, 16),
                       QDate(2026, 11, 23)});
    CHECK(weeksOfMonth(QDate(2026, 12, 1)).size() == 5);
    CHECK(weeksOfMonth(QDate(2026, 12, 1)).last() == QDate(2026, 12, 28));
    CHECK(monthScope(TODAY) == QStringLiteral("2026-11"));

    const AwardsInput data = input();
    QList<BadgeProgress> month = monthBadgeProgress(data, TODAY, TODAY);
    REQUIRE(month.size() == 3);
    // The writing days so far, today not before its goal: 5 of 8
    CHECK(of(month, BadgeKind::MonthGoalDays).done == 5);
    CHECK(of(month, BadgeKind::MonthGoalDays).total == 8);
    CHECK(of(month, BadgeKind::MonthGoalDays).value == 62);
    // The plan of November: 21 writing days of 1,000 words
    CHECK(of(month, BadgeKind::MonthWords).total == 21000);
    CHECK(of(month, BadgeKind::MonthWords).value == 36);
    CHECK(of(month, BadgeKind::MonthWeeks).thresholds == std::array<int, 4>{1, 2, 3, 4});
    CHECK(of(month, BadgeKind::MonthWeeks).value == 0);

    // The month over: all its writing days count
    month = monthBadgeProgress(data, TODAY, QDate(2026, 12, 1));
    CHECK(of(month, BadgeKind::MonthGoalDays).total == 21);
}

TEST_CASE("Writing awards: how far a challenge is", "[core][statistics][awards]")
{
    WritingStatistics goals;
    goals.setActivity(record());

    WritingChallenge words{1, ChallengeKind::Words, 5000, QDate(2026, 11, 9), QDate(2026, 11, 15), {}};
    ChallengeProgress progress = challengeProgress(words, goals, TODAY);
    CHECK(progress.value == 3800);
    CHECK_FALSE(progress.done);
    CHECK_FALSE(progress.over);
    CHECK(progress.daysLeft == 4);

    WritingChallenge inRow{2, ChallengeKind::DaysInRow, 30, QDate(2026, 11, 9), QDate(2026, 12, 8), {}};
    // The run of seven days began before the challenge: only its days since then count
    CHECK(challengeProgress(inRow, goals, TODAY).value == 4);

    WritingChallenge days{3, ChallengeKind::WritingDays, 5, QDate(2026, 11, 2), QDate(2026, 11, 8), {}};
    progress = challengeProgress(days, goals, TODAY);
    CHECK(progress.value == 5);
    CHECK(progress.done);
    CHECK(progress.daysLeft == 0);

    WritingChallenge missed{4, ChallengeKind::Words, 10000, QDate(2026, 11, 2), QDate(2026, 11, 8), {}};
    progress = challengeProgress(missed, goals, TODAY);
    CHECK(progress.over);
    CHECK(progress.daysLeft == 0);

    CHECK(challengeKindOf(challengeIdOf(ChallengeKind::DaysInRow)) == ChallengeKind::DaysInRow);
    CHECK(challengeKindOf(challengeIdOf(ChallengeKind::WritingDays)) == ChallengeKind::WritingDays);
    CHECK(challengeKindOf(QStringLiteral("?")) == ChallengeKind::Words);
}

TEST_CASE("Writing awards: the badges of a book as its project.db keeps them",
          "[core][statistics][awards]")
{
    BookBadges badges;
    badges.since = QDate(2026, 10, 1);
    badges.earned = {{BadgeKind::BookGoal, 1, QDate(2026, 11, 3), {}}};
    badges.history = {{BadgeKind::ChaptersDone, 0, QDate(2026, 9, 3), {}}};
    badges.keptDeadlines = {QStringLiteral("key")};
    CHECK(BookBadges::fromJson(badges.toJson()) == badges);
    CHECK(BookBadges::fromJson(QStringLiteral("not json")) == BookBadges{});
}

TEST_CASE("Writing awards: the journal keeps the badges, the goals of the days, the books "
          "finished and the challenges",
          "[core][statistics][awards]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("journal.db"));
    {
        WritingJournal journal;
        REQUIRE(journal.open(path));
        CHECK(journal.badges().isEmpty());
        REQUIRE(journal.addBadges({{BadgeKind::Streak, 0, QDate(2026, 11, 8), {}},
                                   {BadgeKind::MonthWords, 1, QDate(2026, 11, 10), QStringLiteral("2026-11")}}));
        // Won once: the day it was won stays
        REQUIRE(journal.addBadges({{BadgeKind::Streak, 0, QDate(2026, 11, 9), {}}}));
        REQUIRE(journal.setDayGoals(QDate(2026, 11, 3), 1100, 5500));
        REQUIRE(journal.setDayGoals(QDate(2026, 11, 3), 1200, 6000));
        REQUIRE(journal.addFinishedBook(QStringLiteral("book-1"), QDate(2026, 11, 5)));
        REQUIRE(journal.addFinishedBook(QStringLiteral("book-1"), QDate(2026, 11, 6)));
        const int first = journal.addChallenge(
            {0, ChallengeKind::Words, 5000, QDate(2026, 11, 9), QDate(2026, 11, 15), {}});
        const int second = journal.addChallenge(
            {0, ChallengeKind::DaysInRow, 30, QDate(2026, 11, 9), QDate(2026, 12, 8), {}});
        REQUIRE(first > 0);
        REQUIRE(second > first);
        REQUIRE(journal.setChallengeDone(first, QDate(2026, 11, 14)));
        REQUIRE(journal.removeChallenge(second));
        CHECK_FALSE(journal.removeChallenge(second));
    }
    WritingJournal journal;
    REQUIRE(journal.open(path));
    const QList<EarnedBadge> badges = journal.badges();
    REQUIRE(badges.size() == 2);
    CHECK(badges[0] == EarnedBadge{BadgeKind::Streak, 0, QDate(2026, 11, 8), {}});
    CHECK(badges[1].scope == QStringLiteral("2026-11"));
    CHECK(journal.dailyGoals() == QMap<QDate, int>{{QDate(2026, 11, 3), 1200}});
    CHECK(journal.weeklyGoals() == QMap<QDate, int>{{QDate(2026, 11, 3), 6000}});
    CHECK(journal.finishedBooks() == QStringList{QStringLiteral("book-1")});
    const QList<WritingChallenge> challenges = journal.challenges();
    REQUIRE(challenges.size() == 1);
    CHECK(challenges[0].kind == ChallengeKind::Words);
    CHECK(challenges[0].target == 5000);
    CHECK(challenges[0].done == QDate(2026, 11, 14));
}
