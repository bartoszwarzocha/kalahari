// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_writing_plan.cpp
/// @brief The schedule of a book: when each of its texts is written and when it is revised

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/writing_goals.h>
#include <kalahari/core/writing_plan.h>

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
