/// @file test_writing_stats_panel.cpp
/// @brief The Statistics panel: the list of views and the view in front, as narrow as a panel
/// can be

#include <catch2/catch_test_macros.hpp>

#include "../editor/editor_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/writing_journal.h"
#include "kalahari/gui/panels/writing_stats_panel.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QTemporaryDir>
#include <QToolButton>

using namespace kalahari;
using gui::WritingStatsModel;
using gui::WritingStatsPanel;
using View = WritingStatsPanel::View;
using gui::stats_painter::number;

namespace {

core::WritingActivity entry(const QDate& date, int hour, int minute, int written, int deleted = 0)
{
    core::WritingActivity activity;
    activity.date = date;
    activity.slot = core::activitySlotOf(QTime(hour, minute));
    activity.bookId = QStringLiteral("book");
    activity.elementId = QStringLiteral("ch-1");
    activity.written = written;
    activity.deleted = deleted;
    activity.activeMs = 5 * 60 * 1000;
    return activity;
}

/// Thursday, 12 November 2026, half past nine in the evening
const QDateTime NOW(QDate(2026, 11, 12), QTime(21, 30));

/// The writer's journal of the tests and a model of it at NOW: 150 words on Tuesday, 500 on
/// Wednesday and 480 today, in two sessions; and 100 words pasted today at 21:00
struct Journal {
    QTemporaryDir dir;
    WritingStatsModel model;

    Journal()
    {
        REQUIRE(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("journal.db"));
        {
            core::WritingJournal journal;
            REQUIRE(journal.open(path));
            core::WritingActivity pasted = entry(QDate(2026, 11, 12), 21, 0, 0);
            pasted.pasted = 100;
            pasted.activeMs = 0;
            REQUIRE(journal.add({entry(QDate(2026, 11, 10), 20, 0, 150),
                                 entry(QDate(2026, 11, 11), 20, 0, 500),
                                 entry(QDate(2026, 11, 12), 10, 0, 200, 20),
                                 entry(QDate(2026, 11, 12), 21, 0, 300), pasted}));
            journal.close();
        }
        model.setClock([]() { return NOW; });
        REQUIRE(model.openJournal(path));
    }
};

bool contains(const QStringList& texts, const QString& text)
{
    if (texts.contains(text)) {
        return true;
    }
    UNSCOPED_INFO("not among: " << texts.join(QStringLiteral(" | ")).toStdString());
    return false;
}

}  // namespace

TEST_CASE("Statistics panel: today, its numbers and its sessions", "[gui][statistics]")
{
    Journal journal;
    WritingStatsPanel panel;
    panel.setModel(&journal.model);
    panel.setTitleSources([](const QString& id) {
        return id == QLatin1String("ch-1") ? QStringLiteral("Chapter 1") : QString();
    }, []() { return QString(); });
    panel.show();

    CHECK(panel.view() == View::Today);
    CHECK(panel.titleText() == QStringLiteral("Today"));
    CHECK(contains(panel.numbers(), QStringLiteral("Words today: 480")));
    CHECK(contains(panel.numbers(), QStringLiteral("Time of writing: 0:10")));
    CHECK(contains(panel.numbers(), QStringLiteral("Daily goal: 48%")));
    CHECK(contains(panel.numbers(), QStringLiteral("Sessions: 2")));
    CHECK(contains(panel.details(), QStringLiteral("Written: 500")));
    CHECK(contains(panel.details(), QStringLiteral("Deleted: 20")));
    CHECK(contains(panel.details(), QStringLiteral("Pasted: 100, not counted")));
    CHECK(contains(panel.details(), QStringLiteral("To the daily goal: 520 words")));
    CHECK(contains(panel.details(), QStringLiteral("Sessions: 10:00–10:05, 21:00–21:05")));
    CHECK(contains(panel.details(), QStringLiteral("Texts: Chapter 1 (480)")));

    // The list: every view with its line of numbers
    REQUIRE(panel.viewList()->count() == 6);
    CHECK(panel.listTexts().first() ==
          QStringLiteral("Today: 480 of %1 words · 0:10").arg(number(1000)));
}

TEST_CASE("Statistics panel: the box of the text pasted counts it at once", "[gui][statistics]")
{
    auto& settings = core::SettingsManager::getInstance();
    Journal journal;
    WritingStatsPanel panel;
    panel.setModel(&journal.model);
    panel.show();
    QCheckBox* box = panel.countPastedBox();
    REQUIRE(box != nullptr);
    CHECK(box->isVisible());
    CHECK_FALSE(box->isChecked());
    CHECK(contains(panel.numbers(), QStringLiteral("Words today: 480")));

    // The box changes the setting, and the statistics are counted again
    box->click();
    CHECK(settings.get<bool>("stats.countPasted"));
    test::runEventLoop(WritingStatsModel::CHANGE_INTERVAL_MS * 2);
    CHECK(contains(panel.numbers(), QStringLiteral("Words today: 580")));
    CHECK(contains(panel.details(), QStringLiteral("Written: 600")));
    CHECK(contains(panel.details(), QStringLiteral("Pasted: 100, counted")));

    // The setting changed elsewhere (Settings > Counting) changes the box
    settings.set<bool>("stats.countPasted", false);
    test::runEventLoop(WritingStatsModel::CHANGE_INTERVAL_MS * 2);
    CHECK_FALSE(box->isChecked());
    CHECK(contains(panel.numbers(), QStringLiteral("Words today: 480")));

    // The foot can be put under the panel, out of it
    QWidget* footer = panel.takeFooter();
    REQUIRE(footer != nullptr);
    CHECK(footer->isAncestorOf(box));
    QWidget holder;
    footer->setParent(&holder);
    CHECK_FALSE(panel.isAncestorOf(box));
}

TEST_CASE("Statistics panel: the week and the weeks before it", "[gui][statistics]")
{
    Journal journal;
    WritingStatsPanel panel;
    panel.setModel(&journal.model);
    panel.show();

    // The arrows down the list choose the views
    panel.viewList()->setFocus();
    panel.viewList()->setCurrentRow(1);
    CHECK(panel.view() == View::Week);
    CHECK(panel.titleText() == QStringLiteral("Week"));
    CHECK(contains(panel.numbers(), QStringLiteral("This week: %1").arg(number(1130))));
    CHECK(contains(panel.numbers(), QStringLiteral("Weekly goal: 23%")));
    CHECK(contains(panel.details(), QStringLiteral("Days with the goal: 0 of 4")));
    CHECK(contains(panel.details(), QStringLiteral("Best day: Wednesday, 500 words")));
    CHECK(contains(panel.details(), QStringLiteral("Days off: Saturday, Sunday")));
    auto* later = panel.findChild<QToolButton*>();
    REQUIRE(later != nullptr);

    // The week before: nothing written, and the week after it is there again
    panel.setPeriodOffset(-1);
    CHECK(contains(panel.numbers(), QStringLiteral("In the week: 0")));
    panel.setPeriodOffset(0);
    CHECK(contains(panel.numbers(), QStringLiteral("This week: %1").arg(number(1130))));
    panel.setPeriodOffset(3);  // no weeks to come
    CHECK(panel.periodOffset() == 0);
}

TEST_CASE("Statistics panel: 30 days, the book and the times of writing", "[gui][statistics]")
{
    Journal journal;
    WritingStatsPanel panel;
    panel.setModel(&journal.model);
    panel.show();

    panel.setView(View::Month);
    CHECK(panel.titleText() == QStringLiteral("30 Days"));
    CHECK(contains(panel.numbers(), QStringLiteral("Total: %1").arg(number(1130))));
    CHECK(contains(panel.details(), QStringLiteral("Best day: Wednesday, 11 November – 500 words")));
    // Monday the 9th: the first day of the record is the 10th, so nothing is missed before it
    CHECK(contains(panel.details(), QStringLiteral("Writing days missed: none")));

    panel.setView(View::Book);
    CHECK(panel.subtitleText() == QStringLiteral("No book is open"));
    CHECK(panel.numbers().isEmpty());

    panel.setView(View::Hours);
    CHECK(panel.titleText() == QStringLiteral("Times of Writing"));
    CHECK(contains(panel.numbers(), QStringLiteral("Best time: 20–22")));
    // The share of the words written: 950 of 1,150 in the evening
    CHECK(contains(panel.details(), QStringLiteral("Evening (18–24): 83%")));
    CHECK(contains(panel.details(), QStringLiteral("Monday to Friday and weekend: 100% and 0%")));
}

TEST_CASE("Statistics panel: the settings choose the views of the list", "[gui][statistics]")
{
    auto& settings = core::SettingsManager::getInstance();
    settings.set<bool>(WritingStatsPanel::settingOf(View::Hours), false);
    settings.set<bool>(WritingStatsPanel::settingOf(View::Month), false);
    Journal journal;
    WritingStatsPanel panel;
    panel.setModel(&journal.model);
    panel.show();
    CHECK(panel.listedViews() == QList<View>{View::Today, View::Week, View::Book, View::Awards});
    CHECK(panel.viewList()->count() == 4);

    // A view chosen elsewhere is shown, though not in the list
    panel.setView(View::Hours);
    CHECK(panel.titleText() == QStringLiteral("Times of Writing"));
    CHECK(panel.viewList()->currentRow() == -1);
    settings.set<bool>(WritingStatsPanel::settingOf(View::Hours), true);
    settings.set<bool>(WritingStatsPanel::settingOf(View::Month), true);
}

TEST_CASE("Statistics panel: every view fits the narrowest panel", "[gui][statistics]")
{
    Journal journal;
    WritingStatsPanel panel;
    panel.setModel(&journal.model);
    panel.resize(200, 900);
    panel.show();
    QApplication::processEvents();
    for (View view : {View::Today, View::Week, View::Month, View::Book, View::Hours}) {
        panel.setView(view);
        QApplication::processEvents();
        INFO(panel.titleText().toStdString());
        CHECK(panel.minimumSizeHint().width() <= 200);
        // Nothing reaches out of the panel
        for (const QLabel* label : panel.findChildren<QLabel*>()) {
            if (label->isVisible()) {
                INFO(label->text().toStdString());
                CHECK(label->mapTo(&panel, QPoint(label->width(), 0)).x() <= panel.width());
            }
        }
    }
}

TEST_CASE("Statistics panel: without statistics", "[gui][statistics]")
{
    WritingStatsPanel panel;
    panel.show();
    CHECK(panel.titleText() == QStringLiteral("Today"));
    CHECK(panel.numbers().isEmpty());
    CHECK(panel.viewList()->count() == 6);
}
