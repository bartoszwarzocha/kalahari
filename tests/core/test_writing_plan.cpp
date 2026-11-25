// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_writing_plan.cpp
/// @brief The schedule of a book: when each of its texts is written and when it is revised

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/writing_goals.h>
#include <kalahari/core/writing_plan.h>
#include <kalahari/core/writing_statistics.h>

using namespace kalahari::core;

namespace {

TextPlan planOf(const QString& id, const QDate& start, const QDate& end,
                const QDate& revisionStart = {}, const QDate& revisionEnd = {}, int words = 0)
{
    TextPlan plan;
    plan.elementId = id;
    plan.start = start;
    plan.end = end;
    plan.revisionStart = revisionStart;
    plan.revisionEnd = revisionEnd;
    plan.targetWords = words;
    return plan;
}

}  // namespace

TEST_CASE("Writing plan: writing and revisions of a text", "[core][statistics][plan]")
{
    const TextPlan writing = planOf("ch-1", QDate(2026, 11, 16), QDate(2026, 11, 20));
    CHECK(writing.hasWriting());
    CHECK_FALSE(writing.hasRevision());
    CHECK_FALSE(writing.isEmpty());
    CHECK(writing.firstDay() == QDate(2026, 11, 16));
    CHECK(writing.lastDay() == QDate(2026, 11, 20));

    // Revisions after writing: the plan ends with them
    const TextPlan both = planOf("ch-1", QDate(2026, 11, 16), QDate(2026, 11, 20),
                                 QDate(2026, 12, 7), QDate(2026, 12, 9));
    CHECK(both.hasRevision());
    CHECK(both.firstDay() == QDate(2026, 11, 16));
    CHECK(both.lastDay() == QDate(2026, 12, 9));

    // Revisions only, of a text written before the plan
    const TextPlan revision = planOf("ch-1", {}, {}, QDate(2026, 12, 7), QDate(2026, 12, 9));
    CHECK_FALSE(revision.hasWriting());
    CHECK(revision.firstDay() == QDate(2026, 12, 7));

    // An end before the start is no plan
    const TextPlan backwards = planOf("ch-1", QDate(2026, 11, 20), QDate(2026, 11, 16));
    CHECK_FALSE(backwards.hasWriting());
    CHECK(backwards.isEmpty());
    CHECK_FALSE(backwards.firstDay().isValid());

    // A length alone is a plan
    TextPlan length;
    length.elementId = QStringLiteral("ch-1");
    length.targetWords = 4000;
    CHECK_FALSE(length.isEmpty());

    // Moved by a week, both bars go together
    TextPlan moved = both;
    moved.shift(7);
    CHECK(moved.start == QDate(2026, 11, 23));
    CHECK(moved.end == QDate(2026, 11, 27));
    CHECK(moved.revisionStart == QDate(2026, 12, 14));
    CHECK(moved.revisionEnd == QDate(2026, 12, 16));
    TextPlan movedRevision = revision;
    movedRevision.shift(-1);
    CHECK_FALSE(movedRevision.start.isValid());
    CHECK(movedRevision.revisionStart == QDate(2026, 12, 6));
}

TEST_CASE("Writing plan: the plans of the texts of a book", "[core][statistics][plan]")
{
    WritingPlan plan;
    CHECK(plan.find(QStringLiteral("ch-1")) == nullptr);
    CHECK_FALSE(plan.firstDay().isValid());

    plan.set(planOf("ch-1", QDate(2026, 11, 16), QDate(2026, 11, 20)));
    plan.set(planOf("ch-2", QDate(2026, 11, 23), QDate(2026, 11, 27), QDate(2026, 12, 14),
                    QDate(2026, 12, 16)));
    REQUIRE(plan.texts.size() == 2);
    CHECK(plan.firstDay() == QDate(2026, 11, 16));
    CHECK(plan.lastDay() == QDate(2026, 12, 16));

    // Set again: the text keeps one plan
    plan.set(planOf("ch-1", QDate(2026, 11, 17), QDate(2026, 11, 21)));
    REQUIRE(plan.texts.size() == 2);
    REQUIRE(plan.find(QStringLiteral("ch-1")) != nullptr);
    CHECK(plan.find(QStringLiteral("ch-1"))->start == QDate(2026, 11, 17));

    // An empty plan removes the text's plan; a text without an id is no plan
    TextPlan empty;
    empty.elementId = QStringLiteral("ch-1");
    plan.set(empty);
    CHECK(plan.find(QStringLiteral("ch-1")) == nullptr);
    plan.set(planOf("", QDate(2026, 11, 16), QDate(2026, 11, 20)));
    CHECK(plan.texts.size() == 1);

    CHECK(plan.remove(QStringLiteral("ch-2")));
    CHECK_FALSE(plan.remove(QStringLiteral("ch-2")));
    CHECK(plan.texts.isEmpty());
}

TEST_CASE("Writing plan: as project.db keeps it", "[core][statistics][plan]")
{
    WritingPlan plan;
    plan.set(planOf("ch-1", QDate(2026, 11, 16), QDate(2026, 11, 20), QDate(2026, 12, 7),
                    QDate(2026, 12, 9), 4000));
    plan.set(planOf("ch-2", {}, {}, QDate(2026, 12, 10), QDate(2026, 12, 11)));
    const QString json = plan.toJson();
    CHECK(json.contains(QStringLiteral("\"start\":\"2026-11-16\"")));
    CHECK(json.contains(QStringLiteral("\"revisionEnd\":\"2026-12-09\"")));
    CHECK(json.contains(QStringLiteral("\"targetWords\":4000")));
    CHECK(WritingPlan::fromJson(json) == plan);

    // Empty and broken texts: an empty plan
    CHECK(WritingPlan::fromJson(QString()).texts.isEmpty());
    CHECK(WritingPlan::fromJson(QStringLiteral("{not json")).texts.isEmpty());
    CHECK(WritingPlan().toJson() == QStringLiteral("{}"));

    // A half of a bar is no bar; a text twice keeps its first plan; no id, no plan
    const WritingPlan read = WritingPlan::fromJson(QStringLiteral(
        R"({"texts":[{"elementId":"ch-1","start":"2026-11-16"},)"
        R"({"elementId":"ch-2","start":"2026-11-16","end":"2026-11-18","revisionEnd":"2026-12-01"},)"
        R"({"elementId":"ch-2","start":"2026-11-20","end":"2026-11-21"},)"
        R"({"start":"2026-11-16","end":"2026-11-18"},)"
        R"({"elementId":"ch-3","targetWords":-5}]})"));
    REQUIRE(read.texts.size() == 1);
    CHECK(read.texts.first().elementId == QStringLiteral("ch-2"));
    CHECK(read.texts.first().end == QDate(2026, 11, 18));
    CHECK_FALSE(read.texts.first().revisionEnd.isValid());
}

TEST_CASE("Writing plan: each volume of a series keeps its own", "[core][statistics][plan]")
{
    CHECK(WritingPlan::settingKeyOf(QString::fromLatin1(BookGoals::SETTING_KEY))
          == QStringLiteral("writing.plan"));
    CHECK(WritingPlan::settingKeyOf(QStringLiteral("writing.goals.vol-2"))
          == QStringLiteral("writing.plan.vol-2"));
    CHECK(WritingPlan::settingKeyOf(QString()) == QStringLiteral("writing.plan"));
}

TEST_CASE("Writing plan: the daily goal of the texts planned for today",
          "[core][statistics][plan][goal]")
{
    WritingRules rules;  // Monday to Friday
    const QDate monday(2026, 11, 23);
    WritingPlan plan;
    // In writing to Friday: 1,760 words left over five writing days
    plan.set(planOf("well", QDate(2026, 11, 16), QDate(2026, 11, 27), {}, {}, 3000));
    // Its bar ended on Friday: all it lacks is for today
    plan.set(planOf("market", QDate(2026, 11, 9), QDate(2026, 11, 20), {}, {}, 2500));
    // Not started yet; final; without a length; only revisions; not in the book; long enough
    plan.set(planOf("gorge", QDate(2026, 11, 30), QDate(2026, 12, 4), {}, {}, 2000));
    plan.set(planOf("storm", QDate(2026, 11, 2), QDate(2026, 11, 13), {}, {}, 3000));
    plan.set(planOf("epilogue", QDate(2026, 11, 16), QDate(2026, 11, 27)));
    plan.set(planOf("prologue", {}, {}, QDate(2026, 11, 23), QDate(2026, 11, 24), 900));
    plan.set(planOf("gone", QDate(2026, 11, 16), QDate(2026, 11, 27), {}, {}, 5000));
    plan.set(planOf("oasis", QDate(2026, 11, 16), QDate(2026, 11, 27), {}, {}, 1000));
    QHash<QString, PlannedText> texts;
    texts.insert(QStringLiteral("well"), {1240, true});
    texts.insert(QStringLiteral("market"), {1800, true});
    texts.insert(QStringLiteral("gorge"), {0, true});
    texts.insert(QStringLiteral("storm"), {3120, false});
    texts.insert(QStringLiteral("epilogue"), {0, true});
    texts.insert(QStringLiteral("prologue"), {400, true});
    texts.insert(QStringLiteral("oasis"), {1200, true});

    QStringList counted;
    CHECK(dailyGoalForPlan(rules, plan, texts, monday, &counted) == 352 + 700);
    CHECK(counted == QStringList{QStringLiteral("well"), QStringLiteral("market")});
    // On Friday the last day of writing of the well
    CHECK(dailyGoalForPlan(rules, plan, texts, QDate(2026, 11, 27)) == 1760 + 700);
    // A day off: no goal
    CHECK(dailyGoalForPlan(rules, plan, texts, QDate(2026, 11, 28), &counted) == 0);
    CHECK(counted.isEmpty());
    // A text in revision is written already
    texts[QStringLiteral("market")].beingWritten = false;
    CHECK(dailyGoalForPlan(rules, plan, texts, monday) == 352);
    CHECK(dailyGoalForPlan(rules, WritingPlan(), texts, monday) == 0);
}

TEST_CASE("Writing plan: texts spread over the writing days by the words they need",
          "[core][statistics][plan][spread]")
{
    WritingRules rules;  // Monday to Friday
    const QDate monday(2026, 11, 23);
    const QDate friday(2026, 12, 4);  // Ten writing days
    using Bars = QList<std::pair<QDate, QDate>>;

    // 500 words a day: two days, six days, two days
    CHECK(spreadOverWritingDays(rules, {1000, 3000, 1000}, monday, friday)
          == Bars{{QDate(2026, 11, 23), QDate(2026, 11, 24)},
                  {QDate(2026, 11, 25), QDate(2026, 12, 2)},
                  {QDate(2026, 12, 3), QDate(2026, 12, 4)}});
    // Texts shorter than a day share it
    CHECK(spreadOverWritingDays(rules, {100, 100, 100}, monday, monday.addDays(1))
          == Bars{{monday, monday}, {monday, monday.addDays(1)},
                  {monday.addDays(1), monday.addDays(1)}});
    // No words needed: the same share each
    CHECK(spreadOverWritingDays(rules, {0, 0}, monday, friday)
          == Bars{{QDate(2026, 11, 23), QDate(2026, 11, 27)},
                  {QDate(2026, 11, 30), QDate(2026, 12, 4)}});
    // A text that needs nothing has a day of its own
    CHECK(spreadOverWritingDays(rules, {0, 1000}, monday, friday)
          == Bars{{monday, monday}, {monday, friday}});
    // A weekend has no writing day; no texts; the days the wrong way round
    CHECK(spreadOverWritingDays(rules, {1000}, QDate(2026, 11, 28), QDate(2026, 11, 29)).isEmpty());
    CHECK(spreadOverWritingDays(rules, {}, monday, friday).isEmpty());
    CHECK(spreadOverWritingDays(rules, {1000}, friday, monday).isEmpty());
    // Without writing days every day is one
    rules.writingDays = 0;
    CHECK(spreadOverWritingDays(rules, {1000}, QDate(2026, 11, 28), QDate(2026, 11, 29))
          == Bars{{QDate(2026, 11, 28), QDate(2026, 11, 29)}});
}

TEST_CASE("Writing goals: the daily goal from the schedule is kept with the goals",
          "[core][statistics][plan][goal]")
{
    BookGoals goals;
    goals.dailyGoalFromPlan = true;
    const QString json = goals.toJson();
    CHECK(json.contains(QStringLiteral("\"dailyGoalFromPlan\":true")));
    CHECK(BookGoals::fromJson(json).dailyGoalFromPlan);
    // Not with the deadline: the deadline wins
    CHECK_FALSE(BookGoals::fromJson(QStringLiteral(
                    R"({"dailyGoalFromDeadline":true,"dailyGoalFromPlan":true})"))
                    .dailyGoalFromPlan);
    CHECK_FALSE(BookGoals::fromJson(QStringLiteral("{}")).dailyGoalFromPlan);
}
