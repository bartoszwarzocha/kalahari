/// @file test_writing_stats_bar.cpp
/// @brief The statistics bar above the tabs of the texts, its model and its coordinator

#include <catch2/catch_test_macros.hpp>

#include "../editor/editor_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/writing_journal.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/widgets/writing_stats_bar.h"
#include "kalahari/gui/writing_stats_coordinator.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QToolButton>

#include <cmath>
#include <utility>

using namespace kalahari;
using gui::WritingStatsBar;
using gui::WritingStatsModel;
namespace stats_painter = gui::stats_painter;

namespace {

core::WritingActivity entry(const QDate& date, int hour, int minute, int written, int deleted = 0,
                            qint64 activeMs = 5 * 60 * 1000)
{
    core::WritingActivity activity;
    activity.date = date;
    activity.slot = core::activitySlotOf(QTime(hour, minute));
    activity.bookId = QStringLiteral("book");
    activity.elementId = QStringLiteral("ch-1");
    activity.written = written;
    activity.deleted = deleted;
    activity.activeMs = activeMs;
    return activity;
}

/// Thursday, 12 November 2026, half past nine in the evening
const QDateTime NOW(QDate(2026, 11, 12), QTime(21, 30));

/// A journal with three days of writing, the last of them today: 480 words today
void fillJournal(const QString& path)
{
    core::WritingJournal journal;
    REQUIRE(journal.open(path));
    REQUIRE(journal.add({entry(QDate(2026, 11, 10), 20, 0, 150),
                         entry(QDate(2026, 11, 11), 20, 0, 500),
                         entry(QDate(2026, 11, 12), 10, 0, 200, 20),
                         entry(QDate(2026, 11, 12), 21, 0, 300)}));
    journal.close();
}

/// A model of the journal at @p path, at NOW
void openModel(WritingStatsModel& model, const QString& path)
{
    model.setClock([]() { return NOW; });
    REQUIRE(model.openJournal(path));
}

}  // namespace

TEST_CASE("Statistics bar: the round tops of the scales", "[gui][statistics]")
{
    CHECK(stats_painter::niceMax(0) == 10);
    CHECK(stats_painter::niceMax(83) == 100);
    CHECK(stats_painter::niceMax(101) == 120);
    CHECK(stats_painter::niceMax(1200) == 1200);
    CHECK(stats_painter::niceMax(1201) == 1500);
    CHECK(stats_painter::niceMax(7100) == 8000);
    CHECK(stats_painter::hoursMinutes(155 * 60 * 1000) == QStringLiteral("2:35"));
    CHECK(stats_painter::hoursMinutes(-5) == QStringLiteral("0:00"));
}

TEST_CASE("Statistics bar: the pieces of a writing day", "[gui][statistics]")
{
    core::WritingRules rules;  // the day starts at 4:00
    core::WritingStatistics statistics(rules);
    statistics.setActivity({entry(QDate(2026, 11, 12), 10, 0, 200, 20),
                            entry(QDate(2026, 11, 12), 10, 5, 10),
                            entry(QDate(2026, 11, 13), 1, 0, 7),    // after midnight: the 12th
                            entry(QDate(2026, 11, 13), 5, 0, 99)}); // the next day
    const QList<gui::DayPiece> pieces =
        WritingStatsModel::dayPieces(statistics, QDate(2026, 11, 12), 10);
    REQUIRE(pieces.size() == 144);
    CHECK(pieces[36].written == 210);  // 10:00 is 6 hours after 4:00
    CHECK(pieces[36].deleted == 20);
    CHECK(pieces[36].counted(core::GoalCounting::Net) == 190);
    CHECK(pieces[126].written == 7);   // 1:00 is 21 hours after 4:00
    int all = 0;
    for (const gui::DayPiece& piece : pieces) {
        all += piece.written;
    }
    CHECK(all == 217);
    // A piece that does not divide the day is made one that does
    CHECK(WritingStatsModel::dayPieces(statistics, QDate(2026, 11, 12), 7).size() == 288);
    CHECK(WritingStatsModel::dayPieces(statistics, QDate(2026, 11, 12), 50).size() == 32);
}

TEST_CASE("Statistics bar: the hours of the graph of a writing day", "[gui][statistics]")
{
    // A writing day from 4:00 to 4:00, in pieces of 10 minutes, nothing written yet; minute 0
    // is 4:00, minute 1050 is 21:30
    const QList<gui::DayPiece> none(144);
    const auto shown = [&](int fromHour, int toHour, int now) {
        return WritingStatsModel::shownMinutes(none, 10, 240, fromHour, toHour, now);
    };
    // 6:00 to midnight, as by default
    CHECK(shown(6, 24, 1050) == std::pair<int, int>(120, 1200));
    // Regression: hours across the start of the writing day showed only their last part
    // (00:00 to 24:00 gave 21:00 to 4:00 at 21:30); now the whole day
    CHECK(shown(0, 24, 1050) == std::pair<int, int>(0, 1440));
    CHECK(shown(22, 6, 1050) == std::pair<int, int>(0, 1440));
    // Up to the end of the writing day
    CHECK(shown(22, 4, 1100) == std::pair<int, int>(1080, 1440));
    // Wider for the time now
    CHECK(shown(6, 12, 1050) == std::pair<int, int>(120, 1080));
    // A day from midnight: the whole clock
    CHECK(WritingStatsModel::shownMinutes(none, 10, 0, 0, 24, 600) ==
          std::pair<int, int>(0, 1440));
}

TEST_CASE("Statistics bar: the minutes of a writing day by the clock", "[gui][statistics]")
{
    auto& settings = core::SettingsManager::getInstance();
    const int before = settings.get<int>("stats.dayStart", 4 * 60);
    settings.set<int>("stats.dayStart", 3 * 60 + 33);
    {
        WritingStatsModel model;
        const QDate day(2026, 3, 28);
        // Regression: a day from 3:33 put the time now 3 minutes off its graph, which starts
        // on the slot of 3:30, as the record rounds it
        CHECK(model.minutesIntoDay(day, QDateTime(day, QTime(3, 30))) == 0);
        CHECK(model.minutesIntoDay(day, QDateTime(day, QTime(21, 30))) == 1080);
        CHECK(model.minutesIntoDay(day, QDateTime(day.addDays(1), QTime(3, 0))) == 1410);
        // Regression: on the night the clock goes forward (29 March 2026, from 2:00 to 3:00),
        // the time now was an hour before its slot on the graph
        const QTimeZone warsaw(QByteArray("Europe/Warsaw"));
        if (warsaw.isValid()) {
            CHECK(model.minutesIntoDay(day, QDateTime(day.addDays(1), QTime(3, 0), warsaw)) ==
                  1410);
        }
    }
    settings.set<int>("stats.dayStart", before);
}

TEST_CASE("Statistics bar: the model counts the journal by the writer's rules",
          "[gui][statistics][database]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("writing_journal.db"));
    fillJournal(path);

    WritingStatsModel model;
    openModel(model, path);
    CHECK(model.today() == QDate(2026, 11, 12));
    const core::WritingStatistics statistics = model.goalStatistics();
    CHECK(statistics.day(model.today()).counted(core::GoalCounting::Net) == 480);
    CHECK(statistics.streak(model.today()) == 3);
    CHECK(model.dailyGoalToday() == 1000);
    CHECK_FALSE(model.hasBook());
    CHECK(model.bookWords() == 0);

    // New rules count at once: the model takes them in its slot, queued by the settings
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("stats.goalCounting", "written");
    settings.set<int>("stats.dailyGoal", 600);
    QCoreApplication::processEvents();
    CHECK(model.goalStatistics().day(model.today()).counted(model.rules().counting) == 500);
    CHECK(model.dailyGoalToday() == 600);
    // A day off has no daily goal
    model.setClock([]() { return QDateTime(QDate(2026, 11, 14), QTime(12, 0)); });
    CHECK(model.dailyGoalToday() == 0);
}

TEST_CASE("Statistics bar: the graph, its texts and its buttons", "[gui][statistics]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("writing_journal.db"));
    fillJournal(path);
    WritingStatsModel model;
    openModel(model, path);

    WritingStatsBar bar(&model);
    bar.resize(900, bar.sizeHint().height());
    bar.show();
    test::runEventLoop(WritingStatsModel::CHANGE_INTERVAL_MS * 2);

    // At least 83 pixels high, and higher when the settings say so
    CHECK_FALSE(bar.isFolded());
    CHECK(bar.sizeHint().height() == WritingStatsBar::MIN_GRAPH_HEIGHT);
    auto& settings = core::SettingsManager::getInstance();
    settings.set<int>("stats.bar.height", 50);
    bar.refresh();
    CHECK(bar.sizeHint().height() == WritingStatsBar::MIN_GRAPH_HEIGHT);
    settings.set<int>("stats.bar.height", 120);
    bar.refresh();
    CHECK(bar.sizeHint().height() == 120);
    settings.set<int>("stats.bar.height", 83);
    bar.refresh();
    bar.resize(900, bar.sizeHint().height());

    // The words of today with the goal, the temperature, the time and the days in a row
    const QStringList texts = bar.texts();
    REQUIRE(texts.size() >= 3);
    CHECK(texts.first() ==
          QStringLiteral("%1 of %2 words")
              .arg(stats_painter::number(480), stats_painter::number(1000)));
    CHECK(texts.contains(QStringLiteral("0:10 of writing")));
    CHECK(texts.contains(QStringLiteral("3 days in a row")));

    // The texts and the buttons do not cover the data
    const QRectF plot = bar.plotRect();
    const QRectF textsRect = bar.textsRect();
    CHECK(plot.right() <= textsRect.left());
    CHECK(textsRect.right() <= bar.foldButton()->x());
    CHECK(textsRect.bottom() <= bar.height());
    CHECK(textsRect.top() >= 0);

    // The texts on the left
    settings.set<std::string>("stats.bar.textSide", "left");
    bar.refresh();
    CHECK(bar.textsRect().right() <= bar.plotRect().left());

    // Without the words, the texts are only the time and the days in a row
    settings.set<bool>("stats.bar.showWords", false);
    bar.refresh();
    CHECK(bar.texts() == QStringList{QStringLiteral("0:10 of writing"),
                                     QStringLiteral("3 days in a row")});
}

TEST_CASE("Statistics bar: folded to one line, with the fold button in the same place",
          "[gui][statistics]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("writing_journal.db"));
    fillJournal(path);
    WritingStatsModel model;
    openModel(model, path);

    WritingStatsBar bar(&model);
    bar.resize(900, bar.sizeHint().height());
    bar.show();
    QToolButton* fold = bar.foldButton();
    const QPoint graphPlace = fold->geometry().topRight();
    CHECK(graphPlace == QPoint(bar.width() - 2, 2));

    fold->click();
    CHECK(bar.isFolded());
    CHECK(core::SettingsManager::getInstance().get<std::string>("stats.bar.mode") == "line");
    CHECK(bar.sizeHint().height() == WritingStatsBar::LINE_HEIGHT);
    bar.resize(900, bar.sizeHint().height());
    CHECK(fold->geometry().topRight() == graphPlace);
    CHECK(bar.miniGraphButton()->isVisible());

    // One line: the words with the temperature, the time and the days in a row
    const QStringList texts = bar.texts();
    REQUIRE(texts.size() == 3);
    CHECK(texts[0].startsWith(QStringLiteral("%1 of %2 words")
                                  .arg(stats_painter::number(480), stats_painter::number(1000))));

    // The mini graph goes on from 7 days to 30, today and 7 again
    CHECK(bar.miniGraph() == WritingStatsBar::MiniGraph::Week);
    CHECK(bar.miniGraphButton()->accessibleName() ==
          QStringLiteral("7 days: %1").arg(stats_painter::number(1130)));
    bar.miniGraphButton()->click();
    CHECK(bar.miniGraph() == WritingStatsBar::MiniGraph::Month);
    bar.miniGraphButton()->click();
    CHECK(bar.miniGraph() == WritingStatsBar::MiniGraph::Today);
    CHECK(bar.miniGraphButton()->accessibleName() ==
          QStringLiteral("today: %1").arg(stats_painter::number(480)));
    bar.miniGraphButton()->click();
    CHECK(core::SettingsManager::getInstance().get<std::string>("stats.bar.miniGraph") == "7");

    fold->click();
    CHECK_FALSE(bar.isFolded());
}

TEST_CASE("Statistics bar: View > Show Statistics Bar turns it on and off", "[gui][statistics]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    gui::WritingStatsCoordinator coordinator(nullptr, nullptr, nullptr, nullptr,
                                             dir.filePath(QStringLiteral("journal.db")));
    REQUIRE(coordinator.bar() != nullptr);
    CHECK(coordinator.model()->journal().isOpen());
    CHECK_FALSE(coordinator.bar()->isHidden());

    coordinator.toggleBar();
    auto& settings = core::SettingsManager::getInstance();
    CHECK_FALSE(settings.get<bool>(gui::WritingStatsCoordinator::SHOW_BAR_SETTING));
    CHECK(coordinator.bar()->isHidden());
    coordinator.toggleBar();
    CHECK(settings.get<bool>(gui::WritingStatsCoordinator::SHOW_BAR_SETTING));
    CHECK_FALSE(coordinator.bar()->isHidden());
    // The setting changed elsewhere reaches the bar through the coordinator's queued slot
    settings.set<bool>(gui::WritingStatsCoordinator::SHOW_BAR_SETTING, false);
    QCoreApplication::processEvents();
    CHECK(coordinator.bar()->isHidden());
    settings.set<bool>(gui::WritingStatsCoordinator::SHOW_BAR_SETTING, true);
    QCoreApplication::processEvents();
    CHECK_FALSE(coordinator.bar()->isHidden());
    coordinator.bar()->hide();
}

TEST_CASE("Statistics bar: the settings of the statistics from their command",
          "[gui][statistics]")
{
    gui::registerAllCommands(gui::CommandCallbacks{});
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QString opened;
    gui::WritingStatsCoordinator coordinator(
        nullptr, nullptr, [&opened](const QString& page) { opened = page; }, nullptr,
        dir.filePath(QStringLiteral("journal.db")));
    coordinator.connectCommands();
    CHECK(gui::CommandRegistry::getInstance().executeCommand("tools.statsSettings") ==
          gui::CommandExecutionResult::Success);
    CHECK(opened == QString::fromLatin1(gui::WritingStatsCoordinator::SETTINGS_PAGE));
    coordinator.bar()->hide();
}

TEST_CASE("Statistics bar: now in the middle, from the button on the graph, remembered",
          "[gui][statistics]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("writing_journal.db"));
    fillJournal(path);
    WritingStatsModel model;
    openModel(model, path);
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("stats.bar.mode", "graph");
    settings.set<std::string>("stats.bar.graphMode", "day");
    settings.set<std::string>("stats.bar.textSide", "right");
    settings.set<bool>("stats.bar.showWords", true);

    WritingStatsBar bar(&model);
    bar.resize(900, bar.sizeHint().height());
    bar.show();
    QApplication::processEvents();
    QToolButton* mode = bar.graphModeButton();
    REQUIRE(mode != nullptr);
    CHECK(mode->isVisible());
    CHECK_FALSE(mode->isChecked());
    CHECK(mode->accessibleName() ==
          QStringLiteral("Now in the middle: the graph moves to the left"));
    CHECK(bar.graphMode() == WritingStatsBar::GraphMode::Day);
    const auto [dayFrom, dayTo] = bar.shownMinutes();
    CHECK(bar.xOfMinute(dayFrom) == bar.plotRect().left());

    // Now in the middle: the same hours wide, half before now and half after
    mode->click();
    CHECK(bar.graphMode() == WritingStatsBar::GraphMode::NowInMiddle);
    CHECK(mode->isChecked());
    CHECK(settings.get<std::string>("stats.bar.graphMode") == "now");
    const auto [from, to] = bar.shownMinutes();
    CHECK(to - from == dayTo - dayFrom);
    CHECK(std::abs(bar.xOfMinute(double(bar.nowMinute())) - bar.plotRect().center().x()) < 1.0);

    // The button covers neither the data nor the texts
    const QRectF button(mode->geometry());
    CHECK(bar.plotRect().right() <= button.left());
    CHECK_FALSE(button.intersects(bar.textsRect()));
    CHECK(button.right() <= bar.foldButton()->x());

    // Remembered: a bar made again shows now in the middle
    WritingStatsBar again(&model);
    CHECK(again.graphMode() == WritingStatsBar::GraphMode::NowInMiddle);
    CHECK(again.graphModeButton()->isChecked());

    // Folded, the line has no graph and no button for it
    bar.foldButton()->click();
    CHECK_FALSE(mode->isVisible());
    bar.foldButton()->click();
    CHECK(mode->isVisible());

    mode->click();
    CHECK(bar.graphMode() == WritingStatsBar::GraphMode::Day);
    CHECK(settings.get<std::string>("stats.bar.graphMode") == "day");
    CHECK(bar.shownMinutes() == std::pair<double, double>(dayFrom, dayTo));
}

TEST_CASE("Statistics bar: the tooltip of the texts says what they mean", "[gui][statistics]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("writing_journal.db"));
    fillJournal(path);
    WritingStatsModel model;
    openModel(model, path);
    core::SettingsManager::getInstance().set<bool>("stats.bar.showWords", true);

    WritingStatsBar bar(&model);
    const QString tip = bar.textsToolTip();
    INFO(tip.toStdString());
    CHECK(tip.contains(QStringLiteral("Words today: %1 of %2 (the daily goal)")
                           .arg(stats_painter::number(480), stats_painter::number(1000))));
    // Ten minutes of writing on the days before: too little to compare with
    CHECK(tip.contains(
        QStringLiteral("The pace of work shows after an hour of writing on the days before.")));
    CHECK(tip.contains(QStringLiteral("Time of writing today: 0:10")));
    CHECK(tip.contains(QStringLiteral("Writing days in a row: 3")));
}
