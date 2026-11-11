/// @file test_writing_statistics.cpp
/// @brief The statistics of writing: the writing days, the graph of a day, the days in a row,
/// the hours of writing and the temperature of work

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/writing_statistics.h>

using namespace kalahari::core;
using Catch::Approx;

namespace {

constexpr qint64 MINUTE = 60 * 1000;

// November 2026: the 2nd, 9th and 16th are Mondays
QDate nov(int day)
{
    return QDate(2026, 11, day);
}

WritingActivity entry(const QDate& date, const QTime& time, int written, int deleted = 0,
                      qint64 activeMs = 0, const QString& elementId = "chapter-1")
{
    WritingActivity activity;
    activity.date = date;
    activity.slot = activitySlotOf(time);
    activity.bookId = "book";
    activity.elementId = elementId;
    activity.written = written;
    activity.deleted = deleted;
    activity.activeMs = activeMs;
    return activity;
}

}  // namespace

TEST_CASE("Writing rules: writing days and goals", "[core][statistics]")
{
    WritingRules rules;
    REQUIRE(rules.isWritingDay(nov(9)));         // Monday
    REQUIRE(rules.isWritingDay(nov(13)));        // Friday
    REQUIRE_FALSE(rules.isWritingDay(nov(14)));  // Saturday
    REQUIRE_FALSE(rules.isWritingDay(QDate()));
    REQUIRE(rules.dailyGoalOn(nov(10)) == 1000);
    REQUIRE(rules.dailyGoalOn(nov(15)) == 0);
    REQUIRE(rules.weeklyGoalWords() == 5000);

    rules.writingDays = 0x7f;
    rules.dailyGoal = 500;
    REQUIRE(rules.weeklyGoalWords() == 3500);
    rules.weeklyGoal = 4000;
    REQUIRE(rules.weeklyGoalWords() == 4000);
}

TEST_CASE("Writing rules: from the settings", "[core][statistics][settings]")
{
    auto& settings = SettingsManager::getInstance();

    SECTION("The defaults")
    {
        const WritingRules defaults;
        const WritingRules rules = WritingRules::fromSettings();
        REQUIRE(rules.dayStartMinutes == defaults.dayStartMinutes);
        REQUIRE(rules.idleMinutes == defaults.idleMinutes);
        REQUIRE(rules.largeChangeWords == defaults.largeChangeWords);
        REQUIRE(rules.countPasted == defaults.countPasted);
        REQUIRE_FALSE(rules.countPasted);
        REQUIRE(rules.counting == defaults.counting);
        REQUIRE(rules.allBooks == defaults.allBooks);
        REQUIRE(rules.writingDays == defaults.writingDays);
        REQUIRE(rules.dailyGoal == defaults.dailyGoal);
        REQUIRE(rules.weeklyGoal == defaults.weeklyGoal);
        REQUIRE(rules.streakRule == defaults.streakRule);
        REQUIRE(rules.streakMinimumWords == defaults.streakMinimumWords);
    }

    SECTION("The choices of the writer")
    {
        settings.set<int>("stats.dayStart", 3 * 60);
        settings.set<bool>("stats.skipLargeChanges", false);
        settings.set<bool>("stats.countPasted", true);
        settings.set<std::string>("stats.goalCounting", "written");
        settings.set<std::string>("stats.streakRule", "dailyGoal");
        settings.set<int>("stats.writingDays", 0x7f);

        const WritingRules rules = WritingRules::fromSettings();
        REQUIRE(rules.dayStartMinutes == 180);
        REQUIRE(rules.largeChangeWords == 0);
        REQUIRE(rules.countPasted);
        REQUIRE(rules.counting == GoalCounting::Written);
        REQUIRE(rules.streakRule == StreakRule::DailyGoal);
        REQUIRE(rules.writingDays == 0x7f);

        settings.set<int>("stats.dayStart", 240);
        settings.set<bool>("stats.skipLargeChanges", true);
        settings.set<bool>("stats.countPasted", false);
        settings.set<std::string>("stats.goalCounting", "net");
        settings.set<std::string>("stats.streakRule", "minimumWords");
        settings.set<int>("stats.writingDays", 31);
    }
}

TEST_CASE("Writing statistics: the writing day ends in the morning", "[core][statistics]")
{
    WritingStatistics stats;
    REQUIRE(stats.writingDayOf(QDateTime(nov(13), QTime(3, 59))) == nov(12));
    REQUIRE(stats.writingDayOf(QDateTime(nov(13), QTime(4, 0))) == nov(13));
    REQUIRE(stats.writingDayOf(nov(13), 0) == nov(12));

    stats.setActivity({entry(nov(12), QTime(22, 0), 300, 20, 30 * MINUTE),
                       entry(nov(13), QTime(1, 30), 200, 0, 20 * MINUTE),
                       entry(nov(13), QTime(9, 0), 100)});
    REQUIRE(stats.firstDay() == nov(12));
    REQUIRE(stats.day(nov(12)).written == 500);
    REQUIRE(stats.day(nov(12)).deleted == 20);
    REQUIRE(stats.day(nov(12)).net() == 480);
    REQUIRE(stats.day(nov(12)).activeMs == 50 * MINUTE);
    REQUIRE(stats.day(nov(13)).written == 100);
    REQUIRE(stats.period(nov(1), nov(30)).written == 600);
    REQUIRE(stats.period(nov(13), nov(13)).written == 100);

    SECTION("Another time counts the whole record again")
    {
        WritingRules rules;
        rules.dayStartMinutes = 0;
        stats.setRules(rules);
        REQUIRE(stats.day(nov(12)).written == 300);
        REQUIRE(stats.day(nov(13)).written == 300);
    }

    SECTION("The end of a day is taken to a whole slot")
    {
        WritingRules rules;
        rules.dayStartMinutes = 92;  // 1:32 is 1:30
        stats.setRules(rules);
        REQUIRE(stats.rules().dayStartMinutes == 90);
        REQUIRE(stats.day(nov(13)).written == 300);
    }

    SECTION("The record not saved yet adds to it")
    {
        stats.addActivity({entry(nov(13), QTime(3, 55), 40)});
        REQUIRE(stats.day(nov(12)).written == 540);
    }
}

TEST_CASE("Writing statistics: the graph of a day", "[core][statistics]")
{
    WritingStatistics stats;
    stats.setActivity({entry(nov(12), QTime(4, 5), 10, 2),
                       entry(nov(12), QTime(4, 7), 5, 0, 0, "chapter-2"),
                       entry(nov(12), QTime(22, 0), 50),
                       entry(nov(13), QTime(1, 0), 30, 10)});

    const QList<int> graph = stats.dayGraph(nov(12), 10);
    REQUIRE(graph.size() == 144);
    REQUIRE(graph[0] == 13);     // 4:00-4:10, net
    REQUIRE(graph[108] == 50);   // 22:00 is 18 hours after 4:00
    REQUIRE(graph[126] == 20);   // 1:00 of the next morning
    int sum = 0;
    for (int words : graph) {
        sum += words;
    }
    REQUIRE(sum == 83);

    SECTION("All the words written")
    {
        WritingRules rules;
        rules.counting = GoalCounting::Written;
        stats.setRules(rules);
        REQUIRE(stats.dayGraph(nov(12), 60)[0] == 15);
        REQUIRE(stats.dayGraph(nov(12), 60)[21] == 30);
    }

    SECTION("Pieces that are not whole slots of a day are made so")
    {
        REQUIRE(stats.dayGraph(nov(12), 7).size() == 288);  // 5 minutes
        REQUIRE(stats.dayGraph(nov(12), 25).size() == 72);  // 20 minutes
        REQUIRE(stats.dayGraph(nov(12), 0).size() == 288);
        REQUIRE(stats.dayGraph(nov(12), 5000).size() == 1);
    }

    SECTION("The texts of the days")
    {
        const QHash<QString, WritingTotals> texts = stats.texts(nov(12), nov(12));
        REQUIRE(texts.size() == 2);
        REQUIRE(texts.value("chapter-1").written == 90);
        REQUIRE(texts.value("chapter-1").deleted == 12);
        REQUIRE(texts.value("chapter-2").written == 5);
    }
}

TEST_CASE("Writing statistics: the words pasted count only when the writer says so",
          "[core][statistics]")
{
    // 100 words written and 300 pasted at 10:00; at 11:00 a paste of 50 words undone
    WritingActivity morning = entry(nov(12), QTime(10, 0), 100, 10, 5 * MINUTE);
    morning.pasted = 300;
    WritingActivity undone = entry(nov(12), QTime(11, 0), 20, 0, MINUTE);
    undone.pasted = -50;
    WritingStatistics stats;
    stats.setActivity({morning, undone});

    // Not counted: only the words written, the words pasted apart
    WritingTotals today = stats.day(nov(12));
    REQUIRE(today.written == 120);
    REQUIRE(today.deleted == 10);
    REQUIRE(today.pasted == 250);
    REQUIRE(stats.dayGraph(nov(12), 60)[6] == 90);  // 10:00 is 6 hours after 4:00
    REQUIRE(stats.texts(nov(12), nov(12)).value("chapter-1").written == 120);
    REQUIRE(stats.counted(morning).written == 100);
    REQUIRE(stats.counted(morning).pasted == 300);

    // Counted: the words pasted are written, the paste undone is deleted; at once
    WritingRules rules;
    rules.countPasted = true;
    stats.setRules(rules);
    today = stats.day(nov(12));
    REQUIRE(today.written == 420);
    REQUIRE(today.deleted == 60);
    REQUIRE(today.pasted == 250);
    REQUIRE(today.net() == 360);
    REQUIRE(stats.dayGraph(nov(12), 60)[6] == 390);
    REQUIRE(stats.dayGraph(nov(12), 60)[7] == -30);
    REQUIRE(stats.texts(nov(12), nov(12)).value("chapter-1").written == 420);
    REQUIRE(stats.hourMap(nov(12), nov(12)).words[3][10] == Approx(400.0));
    REQUIRE(stats.counted(undone).deleted == 50);

    // And back
    stats.setRules(WritingRules());
    REQUIRE(stats.day(nov(12)).written == 120);
}

TEST_CASE("Writing statistics: a paste and its undo in one slot count as the record keeps them",
          "[core][statistics]")
{
    // Regression: saved apart, a paste of 100 words and its undo in one slot counted as 100
    // written and 100 deleted, but the record adds them up, so after a restart they were
    // nothing: the words written changed with the moment they were saved
    WritingRules rules;
    rules.countPasted = true;
    WritingActivity pasted = entry(nov(12), QTime(10, 0), 10, 0, MINUTE);
    pasted.pasted = 100;
    WritingActivity undone = entry(nov(12), QTime(10, 2), 0, 0, MINUTE);
    undone.pasted = -100;
    WritingActivity together = pasted;
    together.pasted = 0;
    together.activeMs = 2 * MINUTE;

    WritingStatistics apart;
    apart.setRules(rules);
    apart.addActivity({pasted});
    apart.addActivity({undone});
    WritingStatistics once;
    once.setRules(rules);
    once.setActivity({together});
    for (const WritingStatistics* stats : {&apart, &once}) {
        const WritingTotals today = stats->day(nov(12));
        CHECK(today.written == 10);
        CHECK(today.deleted == 0);
        CHECK(today.pasted == 0);
        CHECK(today.activeMs == 2 * MINUTE);
    }
    // Other slots and other texts stay apart
    apart.addActivity({entry(nov(12), QTime(10, 5), 7), entry(nov(12), QTime(10, 0), 3, 0, 0,
                                                              QStringLiteral("chapter-2"))});
    CHECK(apart.day(nov(12)).written == 20);
    CHECK(apart.texts(nov(12), nov(12)).value(QStringLiteral("chapter-1")).written == 17);
    CHECK(apart.texts(nov(12), nov(12)).value(QStringLiteral("chapter-2")).written == 3);
}

TEST_CASE("Writing statistics: the days in a row", "[core][statistics]")
{
    WritingRules rules;  // Monday to Friday, at least 100 words
    WritingStatistics stats(rules);

    SECTION("A day off without writing neither breaks nor adds a day")
    {
        stats.setActivity({entry(nov(5), QTime(10, 0), 150),    // Thursday
                           entry(nov(6), QTime(10, 0), 120),    // Friday
                           entry(nov(9), QTime(10, 0), 300)});  // Monday
        REQUIRE(stats.streak(nov(9)) == 3);
    }

    SECTION("Writing on a day off adds a day")
    {
        stats.setActivity({entry(nov(6), QTime(10, 0), 120),
                           entry(nov(7), QTime(10, 0), 400),  // Saturday
                           entry(nov(9), QTime(10, 0), 300)});
        REQUIRE(stats.streak(nov(9)) == 3);
    }

    SECTION("A writing day with too few words breaks them")
    {
        stats.setActivity({entry(nov(3), QTime(10, 0), 500),
                           entry(nov(4), QTime(10, 0), 99),  // Wednesday
                           entry(nov(5), QTime(10, 0), 100)});
        REQUIRE(stats.streak(nov(5)) == 1);
    }

    SECTION("Today does not break them before it is over")
    {
        stats.setActivity({entry(nov(5), QTime(10, 0), 150), entry(nov(6), QTime(10, 0), 150)});
        REQUIRE(stats.streak(nov(9)) == 2);
        stats.addActivity({entry(nov(9), QTime(10, 0), 40)});
        REQUIRE(stats.streak(nov(9)) == 2);
        stats.addActivity({entry(nov(9), QTime(11, 0), 60)});
        REQUIRE(stats.countsToStreak(nov(9)));
        REQUIRE(stats.streak(nov(9)) == 3);
        // Tuesday without writing is over: Wednesday starts from nothing
        REQUIRE(stats.streak(nov(11)) == 0);
    }

    SECTION("The growth of the text counts, not the words written")
    {
        stats.setActivity({entry(nov(9), QTime(10, 0), 300, 250)});
        REQUIRE_FALSE(stats.countsToStreak(nov(9)));
        rules.counting = GoalCounting::Written;
        stats.setRules(rules);
        REQUIRE(stats.countsToStreak(nov(9)));
    }

    SECTION("The daily goal keeps them going")
    {
        rules.streakRule = StreakRule::DailyGoal;
        rules.dailyGoal = 500;
        stats.setRules(rules);
        REQUIRE(stats.streakWords() == 500);
        stats.setActivity({entry(nov(6), QTime(10, 0), 600), entry(nov(9), QTime(10, 0), 450)});
        REQUIRE(stats.streak(nov(9)) == 1);
        REQUIRE(stats.streak(nov(10)) == 0);

        rules.dailyGoal = 0;  // no daily goal: the minimum
        stats.setRules(rules);
        REQUIRE(stats.streakWords() == 100);
        REQUIRE(stats.streak(nov(9)) == 2);
    }

    SECTION("No record, no days")
    {
        REQUIRE(stats.streak(nov(9)) == 0);
        rules.writingDays = 0;
        stats.setRules(rules);
        stats.setActivity({entry(nov(1), QTime(10, 0), 150)});
        REQUIRE(stats.streak(nov(9)) == 1);
    }
}

TEST_CASE("Writing statistics: the average of a writing day", "[core][statistics]")
{
    WritingStatistics stats;
    // Monday 2 to Sunday 8: five writing days, and words on Saturday too
    stats.setActivity({entry(nov(2), QTime(10, 0), 1000),
                       entry(nov(4), QTime(10, 0), 1500),
                       entry(nov(7), QTime(10, 0), 500)});
    REQUIRE(stats.averagePerWritingDay(nov(2), nov(8)) == Approx(600.0));
    // The days before the record started do not count
    REQUIRE(stats.averagePerWritingDay(QDate(2026, 10, 1), nov(8)) == Approx(600.0));
    // Only days off
    REQUIRE(stats.averagePerWritingDay(nov(7), nov(8)) == Approx(0.0));
    REQUIRE(WritingStatistics().averagePerWritingDay(nov(2), nov(8)) == Approx(0.0));
}

TEST_CASE("Writing statistics: the hours of writing", "[core][statistics]")
{
    WritingStatistics stats;
    // Two Wednesdays in the evening, a Monday morning and a night after Friday
    stats.setActivity({entry(nov(4), QTime(20, 10), 400),
                       entry(nov(4), QTime(21, 0), 200),
                       entry(nov(11), QTime(20, 30), 600),
                       entry(nov(9), QTime(8, 0), 100),
                       entry(nov(7), QTime(1, 0), 300)});  // Saturday 1:00 is Friday's night

    const WritingHourMap map = stats.hourMap(nov(1), nov(14));
    REQUIRE(map.words[2][20] == Approx(500.0));
    REQUIRE(map.words[2][21] == Approx(100.0));
    // The record starts on Wednesday the 4th: one Monday, two Wednesdays and two Fridays
    REQUIRE(map.words[0][8] == Approx(100.0));
    REQUIRE(map.words[4][1] == Approx(150.0));
    REQUIRE(map.words[5][1] == Approx(0.0));

    REQUIRE(map.bestDayOfWeek() == 3);
    REQUIRE(map.bestHours(2) == 20);
    REQUIRE(map.bestTime(2) == std::pair<int, int>{3, 20});
    // 500 + 100 + 150 of 800 words a day of the week, on average
    REQUIRE(map.shareOfHours(18, 4) == Approx(750.0 / 850.0));
    REQUIRE(map.shareOfHours(0, 24) == Approx(1.0));
    REQUIRE(map.shareOfDays(1, 5) == Approx(1.0));
    REQUIRE(map.shareOfDays(6, 7) == Approx(0.0));
    REQUIRE(map.shareOfDays(3, 3) == Approx(600.0 / 850.0));

    const WritingHourMap empty;
    REQUIRE(empty.bestDayOfWeek() == 0);
    REQUIRE(empty.bestHours() == -1);
    REQUIRE(empty.bestTime() == std::pair<int, int>{0, -1});
    REQUIRE(empty.shareOfHours(0, 24) == Approx(0.0));
}

TEST_CASE("Writing statistics: the temperature of work", "[core][statistics]")
{
    // Four weeks of usual work: an hour a day, 600 words written and 100 deleted
    QList<WritingActivity> record;
    for (int day = 0; day < 28; ++day) {
        const QDate date = nov(12).addDays(-28 + day);
        for (int slot = 0; slot < 12; ++slot) {
            record.append(entry(date, QTime(9, 0).addSecs(slot * 300), 50, 0, 5 * MINUTE));
        }
        record.last().deleted = 100;
    }
    // Usually: 250 words that count in 30 minutes, 700 / 60 changes in a minute
    WritingStatistics stats;
    stats.setActivity(record);
    const QDateTime now(nov(12), QTime(21, 40));

    auto writeNow = [&](int written, int deleted, int activeMinutes) {
        WritingStatistics current = stats;
        // Spread over the slots of the last half hour
        current.addActivity({entry(nov(12), QTime(21, 15), written / 2, deleted / 2, activeMinutes * MINUTE / 2),
                             entry(nov(12), QTime(21, 40), written - written / 2,
                                   deleted - deleted / 2, activeMinutes * MINUTE - activeMinutes * MINUTE / 2)});
        return current.temperature(now);
    };

    REQUIRE(writeNow(320, 20, 25) == WritingTemperature::MuchFast);   // 300 words, 13.6 a minute
    REQUIRE(writeNow(300, 0, 30) == WritingTemperature::MuchSlow);    // 300 words, 10 a minute
    REQUIRE(writeNow(200, 150, 25) == WritingTemperature::LittleFast);
    REQUIRE(writeNow(100, 0, 30) == WritingTemperature::LittleSlow);
    REQUIRE(stats.temperature(now) == WritingTemperature::Idle);

    SECTION("Words before the window do not count")
    {
        WritingStatistics current = stats;
        current.addActivity({entry(nov(12), QTime(21, 5), 1000, 0, 30 * MINUTE)});
        REQUIRE(current.temperature(now) == WritingTemperature::Idle);
    }

    SECTION("Too little usual work to compare with")
    {
        WritingStatistics fresh;
        fresh.setActivity({entry(nov(11), QTime(9, 0), 500, 0, 59 * MINUTE),
                           entry(nov(12), QTime(21, 40), 300, 0, 10 * MINUTE)});
        REQUIRE(fresh.temperature(now) == WritingTemperature::Unknown);
    }
}

TEST_CASE("Writing totals", "[core][statistics]")
{
    WritingTotals totals;
    REQUIRE(totals.isEmpty());
    REQUIRE(totals.wordsPerHour(GoalCounting::Net) == Approx(0.0));
    totals.add(entry(nov(12), QTime(10, 0), 700, 100, 30 * MINUTE));
    REQUIRE(totals.net() == 600);
    REQUIRE(totals.counted(GoalCounting::Written) == 700);
    REQUIRE(totals.wordsPerHour(GoalCounting::Net) == Approx(1200.0));
    WritingTotals more;
    more.written = 300;
    totals += more;
    REQUIRE(totals.written == 1000);
}
