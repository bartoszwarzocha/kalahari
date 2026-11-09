/// @file test_writing_goals.cpp
/// @brief The goals of a book, its deadlines and the reckoning of the words they need; the
/// record of writing in a database: the writer's journal and the book's project.db

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/project_database.h>
#include <kalahari/core/writing_goals.h>
#include <kalahari/core/writing_journal.h>

#include <QTemporaryDir>

using namespace kalahari::core;

namespace {

QDate day(int year, int month, int dayOfMonth)
{
    return QDate(year, month, dayOfMonth);
}

WritingActivity entry(const QDate& date, int slot, const QString& bookId, int written,
                      int deleted = 0, qint64 activeMs = 0, const QString& elementId = "ch-1")
{
    WritingActivity activity;
    activity.date = date;
    activity.slot = slot;
    activity.bookId = bookId;
    activity.elementId = elementId;
    activity.written = written;
    activity.deleted = deleted;
    activity.activeMs = activeMs;
    return activity;
}

}  // namespace

TEST_CASE("Writing goals: the goals of a book as the setting keeps them", "[core][statistics]")
{
    BookGoals goals;
    REQUIRE(goals.isEmpty());
    REQUIRE(BookGoals::fromJson({}).isEmpty());
    REQUIRE(BookGoals::fromJson("not json").isEmpty());

    goals.targetWords = 90000;
    goals.deadline = day(2027, 1, 31);
    goals.dailyGoalFromDeadline = true;
    goals.deadlines = {{"Wersja dla wydawnictwa", day(2027, 1, 31), {}, 90000},
                       {"Rozdziały 1–10 dla redaktorki", day(2026, 11, 30), "part-1", 40000},
                       {"Bez daty", {}, {}, 0}};
    goals.sortDeadlines();
    REQUIRE(goals.deadlines[0].name == "Rozdziały 1–10 dla redaktorki");
    REQUIRE(goals.deadlines[2].name == "Bez daty");

    const BookGoals read = BookGoals::fromJson(goals.toJson());
    REQUIRE(read == goals);
    REQUIRE_FALSE(read.isEmpty());

    REQUIRE(goals.nextDeadline(day(2026, 11, 12)) == 0);
    REQUIRE(goals.nextDeadline(day(2026, 11, 30)) == 0);
    REQUIRE(goals.nextDeadline(day(2026, 12, 1)) == 1);
    REQUIRE(goals.nextDeadline(day(2027, 2, 1)) == -1);
}

TEST_CASE("Writing goals: writing days and the words a day", "[core][statistics]")
{
    WritingRules rules;  // Monday to Friday
    REQUIRE(writingDaysBetween(rules, day(2026, 11, 13), day(2027, 1, 31)) == 56);
    REQUIRE(writingDaysBetween(rules, day(2026, 11, 14), day(2026, 11, 15)) == 0);
    REQUIRE(writingDaysBetween(rules, day(2026, 11, 15), day(2026, 11, 14)) == 0);
    REQUIRE(writingDaysBetween(rules, day(2026, 11, 9), day(2026, 11, 9)) == 1);
    rules.writingDays = 0;  // no writing days: every day is one
    REQUIRE(writingDaysBetween(rules, day(2026, 11, 9), day(2026, 11, 15)) == 7);

    REQUIRE(wordsPerWritingDay(27582, 56) == 493);
    REQUIRE(wordsPerWritingDay(560, 56) == 10);
    REQUIRE(wordsPerWritingDay(0, 56) == 0);
    REQUIRE(wordsPerWritingDay(-10, 56) == 0);
    REQUIRE(wordsPerWritingDay(300, 0) == 300);
}

TEST_CASE("Writing goals: the day the book will be written", "[core][statistics]")
{
    WritingRules rules;
    const QDate today = day(2026, 11, 12);  // Thursday
    REQUIRE(forecastDate(rules, today, 27582, 1058.0) == day(2026, 12, 21));
    REQUIRE(forecastDate(rules, today, 1000, 1000.0) == day(2026, 11, 13));
    REQUIRE(forecastDate(rules, today, 2000, 1000.0) == day(2026, 11, 16));  // over the weekend
    REQUIRE(forecastDate(rules, today, 5000, 1000.0) == day(2026, 11, 19));
    REQUIRE(forecastDate(rules, today, 0, 1000.0) == today);
    REQUIRE_FALSE(forecastDate(rules, today, 1000, 0.0).isValid());
    REQUIRE_FALSE(forecastDate(rules, today, 1000000, 0.001).isValid());
}

TEST_CASE("Writing goals: the daily goal of a deadline", "[core][statistics]")
{
    WritingRules rules;
    BookGoals goals;
    const QDate today = day(2026, 11, 12);
    REQUIRE(dailyGoalForDeadline(rules, goals, 61568, today) == 0);
    goals.targetWords = 90000;
    REQUIRE(dailyGoalForDeadline(rules, goals, 61568, today) == 0);
    goals.deadline = day(2027, 1, 31);
    // 28 432 words in the 57 writing days from today to the deadline
    REQUIRE(dailyGoalForDeadline(rules, goals, 61568, today) == 499);
    REQUIRE(dailyGoalForDeadline(rules, goals, 95000, today) == 0);
    REQUIRE(dailyGoalForDeadline(rules, goals, 61568, day(2027, 2, 1)) == 0);
}

TEST_CASE("Writing record: the writer's journal", "[core][statistics][database]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    WritingJournal journal;
    REQUIRE(journal.open(dir.filePath("journal/writing_journal.db")));

    REQUIRE(journal.add({entry(day(2026, 11, 12), 255, "book-a", 30, 2, 60000),
                         entry(day(2026, 11, 12), 255, "book-b", 10),
                         entry(day(2026, 11, 13), 12, "book-a", 5)}));
    // The same slot again grows by its counts
    REQUIRE(journal.add({entry(day(2026, 11, 12), 255, "book-a", 20, 1, 30000)}));
    // Nothing done is not written
    REQUIRE(journal.add({entry(day(2026, 11, 12), 256, "book-a", 0)}));

    const QList<WritingActivity> all = journal.activity(day(2026, 11, 1), day(2026, 11, 30));
    REQUIRE(all.size() == 3);
    const QList<WritingActivity> bookA =
        journal.activity(day(2026, 11, 12), day(2026, 11, 12), "book-a");
    REQUIRE(bookA.size() == 1);
    REQUIRE(bookA.first().written == 50);
    REQUIRE(bookA.first().deleted == 3);
    REQUIRE(bookA.first().activeMs == 90000);
    REQUIRE(bookA.first().elementId == "ch-1");

    // What it keeps stays after it is opened again
    journal.close();
    REQUIRE_FALSE(journal.isOpen());
    REQUIRE(journal.activity(day(2026, 11, 1), day(2026, 11, 30)).isEmpty());
    REQUIRE(journal.open(dir.filePath("journal/writing_journal.db")));
    REQUIRE(journal.activity(day(2026, 11, 1), day(2026, 11, 30)).size() == 3);
}

TEST_CASE("Writing record: the record of a book in its project.db", "[core][statistics][database]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    ProjectDatabase db;
    REQUIRE(db.open(dir.path()));
    REQUIRE(db.addWritingActivity({entry(day(2026, 11, 12), 100, "book", 40, 0, 120000),
                                   entry(day(2026, 11, 12), 100, "book", 2, 0, 0, "ch-2")}));
    REQUIRE(db.addWritingActivity({entry(day(2026, 11, 12), 100, "book", 10)}));
    const QList<WritingActivity> record = db.writingActivity(day(2026, 11, 12), day(2026, 11, 12));
    REQUIRE(record.size() == 2);
    int written = 0;
    for (const WritingActivity& item : record) {
        written += item.written;
    }
    REQUIRE(written == 52);

    // The goals of the book are a setting of its own
    BookGoals goals;
    goals.targetWords = 90000;
    db.setSetting(BookGoals::SETTING_KEY, goals.toJson());
    REQUIRE(BookGoals::fromJson(db.getSetting(BookGoals::SETTING_KEY).toString()) == goals);
    db.close();
}
