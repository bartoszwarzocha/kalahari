/// @file test_writing_activity.cpp
/// @brief The record of writing in 5-minute slots, and the recorder that collects it

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/writing_activity.h>

using namespace kalahari::core;

namespace {

QDateTime at(int hour, int minute, int second = 0)
{
    return QDateTime(QDate(2026, 11, 12), QTime(hour, minute, second));
}

constexpr qint64 MINUTE = 60 * 1000;

}  // namespace

TEST_CASE("Writing activity: the slots of a day", "[core][statistics]")
{
    REQUIRE(ACTIVITY_SLOTS_PER_DAY == 288);
    REQUIRE(activitySlotOf(QTime(0, 0)) == 0);
    REQUIRE(activitySlotOf(QTime(0, 4, 59)) == 0);
    REQUIRE(activitySlotOf(QTime(0, 5)) == 1);
    REQUIRE(activitySlotOf(QTime(23, 59, 59)) == 287);
    REQUIRE(activitySlotOf(QTime()) == 0);

    const QDate day(2026, 11, 12);
    REQUIRE(activitySlotStart(day, 0) == QDateTime(day, QTime(0, 0)));
    REQUIRE(activitySlotStart(day, 255) == QDateTime(day, QTime(21, 15)));
    REQUIRE(activitySlotStart(day, ACTIVITY_SLOTS_PER_DAY) == QDateTime(day.addDays(1), QTime(0, 0)));
}

TEST_CASE("Writing recorder: words written and deleted", "[core][statistics]")
{
    WritingRecorder recorder;
    recorder.setPlace("book", "chapter-1");

    SECTION("The changes of one slot go to one entry")
    {
        REQUIRE(recorder.recordWords(12, at(21, 1)));
        REQUIRE(recorder.recordWords(-3, at(21, 4, 59)));
        REQUIRE(recorder.recordWords(5, at(21, 2)));
        REQUIRE(recorder.pending().size() == 1);
        const WritingActivity& entry = recorder.pending().first();
        REQUIRE(entry.date == QDate(2026, 11, 12));
        REQUIRE(entry.slot == 21 * 12);
        REQUIRE(entry.bookId == "book");
        REQUIRE(entry.elementId == "chapter-1");
        REQUIRE(entry.written == 17);
        REQUIRE(entry.deleted == 3);
        REQUIRE(recorder.written() == 17);
        REQUIRE(recorder.deleted() == 3);
    }

    SECTION("Another slot or another text is another entry")
    {
        recorder.recordWords(10, at(21, 1));
        recorder.recordWords(10, at(21, 5));
        recorder.setPlace("book", "chapter-2");
        recorder.recordWords(10, at(21, 6));
        recorder.setPlace("book", "chapter-1");
        recorder.recordWords(4, at(21, 7));
        REQUIRE(recorder.pending().size() == 3);
        REQUIRE(recorder.pending()[1].written == 14);
        REQUIRE(recorder.pending()[2].elementId == "chapter-2");
    }

    SECTION("A change of more words than the limit is not writing, in both directions")
    {
        recorder.setLargeChangeWords(500);
        REQUIRE(recorder.recordWords(500, at(21, 1)));
        REQUIRE_FALSE(recorder.recordWords(501, at(21, 2)));
        REQUIRE_FALSE(recorder.recordWords(-2000, at(21, 3)));
        REQUIRE(recorder.written() == 500);
        REQUIRE(recorder.deleted() == 0);

        recorder.setLargeChangeWords(0);
        REQUIRE(recorder.recordWords(-2000, at(21, 3)));
        REQUIRE(recorder.deleted() == 2000);
    }

    SECTION("No change and no time give no entry")
    {
        REQUIRE(recorder.recordWords(0, at(21, 1)));
        REQUIRE(recorder.recordWords(5, QDateTime()));
        REQUIRE(recorder.pending().isEmpty());
    }
}

TEST_CASE("Writing recorder: the time of writing", "[core][statistics]")
{
    WritingRecorder recorder;
    recorder.setPlace("book", "chapter-1");
    recorder.setIdleSeconds(5 * 60);

    SECTION("The time between two changes goes to the slots it passes through")
    {
        recorder.noteActivity(at(10, 3));
        recorder.noteActivity(at(10, 7));
        REQUIRE(recorder.activeMs() == 4 * MINUTE);
        REQUIRE(recorder.pending().size() == 2);
        REQUIRE(recorder.pending()[0].slot == 10 * 12);
        REQUIRE(recorder.pending()[0].activeMs == 2 * MINUTE);
        REQUIRE(recorder.pending()[1].slot == 10 * 12 + 1);
        REQUIRE(recorder.pending()[1].activeMs == 2 * MINUTE);
    }

    SECTION("A pause longer than the idle time is not counted")
    {
        recorder.noteActivity(at(10, 0));
        recorder.noteActivity(at(10, 5));      // 5 minutes: counted
        recorder.noteActivity(at(10, 10, 1));  // 5 minutes and a second: a pause
        recorder.noteActivity(at(10, 11));     // 59 seconds: counted
        REQUIRE(recorder.activeMs() == 5 * MINUTE + 59 * 1000);
    }

    SECTION("A pause starts a new stretch of writing")
    {
        recorder.noteActivity(at(10, 0));
        recorder.pause();
        recorder.noteActivity(at(10, 1));
        recorder.noteActivity(at(10, 2));
        REQUIRE(recorder.activeMs() == 1 * MINUTE);
    }

    SECTION("A clock set back counts no time")
    {
        recorder.noteActivity(at(10, 5));
        recorder.noteActivity(at(10, 1));
        REQUIRE(recorder.activeMs() == 0);
        recorder.noteActivity(at(10, 2));
        REQUIRE(recorder.activeMs() == 1 * MINUTE);
    }

    SECTION("The time of writing passes midnight")
    {
        recorder.noteActivity(QDateTime(QDate(2026, 11, 12), QTime(23, 58)));
        recorder.noteActivity(QDateTime(QDate(2026, 11, 13), QTime(0, 1)));
        REQUIRE(recorder.pending().size() == 2);
        REQUIRE(recorder.pending()[0].date == QDate(2026, 11, 12));
        REQUIRE(recorder.pending()[0].slot == 287);
        REQUIRE(recorder.pending()[0].activeMs == 2 * MINUTE);
        REQUIRE(recorder.pending()[1].date == QDate(2026, 11, 13));
        REQUIRE(recorder.pending()[1].slot == 0);
        REQUIRE(recorder.pending()[1].activeMs == 1 * MINUTE);
    }
}

TEST_CASE("Writing recorder: taking what is collected", "[core][statistics]")
{
    WritingRecorder recorder;
    recorder.setPlace("book", "chapter-1");
    recorder.recordWords(30, at(9, 0));
    recorder.noteActivity(at(9, 0));
    recorder.noteActivity(at(9, 2));

    const QList<WritingActivity> taken = recorder.takePending();
    REQUIRE(taken.size() == 1);
    REQUIRE(taken.first().written == 30);
    REQUIRE(taken.first().activeMs == 2 * MINUTE);
    REQUIRE(recorder.pending().isEmpty());
    // The totals stay until they are reset
    REQUIRE(recorder.written() == 30);

    // The same slot later is a new entry, added up when it is saved
    recorder.recordWords(5, at(9, 3));
    REQUIRE(recorder.pending().size() == 1);
    REQUIRE(recorder.pending().first().written == 5);

    recorder.resetTotals();
    REQUIRE(recorder.written() == 0);
    REQUIRE(recorder.activeMs() == 0);
    REQUIRE(recorder.pending().size() == 1);
}
