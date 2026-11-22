// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_writing_schedule_tab.cpp
/// @brief The tab Writing Schedule: the chart, its mouse and its keys, the plan of a text in
/// its window, the plan in the book and the command that opens the tab

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/project_database.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/writing_activity.h"
#include "kalahari/core/writing_plan.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/diagnostic_menus.h"
#include "kalahari/gui/dialogs/spread_plan_dialog.h"
#include "kalahari/gui/dialogs/text_plan_dialog.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/panels/writing_goals_tab.h"
#include "kalahari/gui/panels/writing_schedule_tab.h"
#include "kalahari/gui/panels/writing_stats_panel.h"
#include "kalahari/gui/widgets/schedule_view.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/writing_stats_coordinator.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateEdit>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QRadioButton>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>

#include <algorithm>

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

using namespace kalahari;
using gui::ScheduleFacts;
using gui::ScheduleRow;
using gui::ScheduleView;
using gui::WritingScheduleTab;
using gui::WritingStatsModel;
using Bar = gui::ScheduleView::Bar;

namespace {

/// Monday, 23 November 2026, ten in the morning
const QDateTime NOW(QDate(2026, 11, 23), QTime(10, 0));

QDate day(int month, int dayOfMonth, int year = 2026)
{
    return {year, month, dayOfMonth};
}

core::TextPlan planOf(const QString& id, const QDate& start, const QDate& end,
                      const QDate& revisionStart = {}, const QDate& revisionEnd = {},
                      int words = 0)
{
    core::TextPlan plan;
    plan.elementId = id;
    plan.start = start;
    plan.end = end;
    plan.revisionStart = revisionStart;
    plan.revisionEnd = revisionEnd;
    plan.targetWords = words;
    return plan;
}

ScheduleRow textRow(const QString& id, const QString& title, const QString& status, int words,
                    const QString& partId = QString())
{
    ScheduleRow row;
    row.id = id;
    row.title = title;
    row.status = status;
    row.words = words;
    row.partId = partId;
    return row;
}

/// The book Caravan: a part with three chapters, two chapters outside it
QList<ScheduleRow> caravan()
{
    ScheduleRow book;
    book.kind = ScheduleRow::Kind::Book;
    book.title = QStringLiteral("Caravan");
    book.words = 6160;
    ScheduleRow part;
    part.kind = ScheduleRow::Kind::Part;
    part.id = QStringLiteral("part-1");
    part.title = QStringLiteral("Part I. Desert");
    part.words = 4360;
    return {book,
            part,
            textRow("ch-1", "Storm", "final", 3120, "part-1"),
            textRow("ch-2", "Well", "draft", 1240, "part-1"),
            textRow("ch-3", "Night in the Gorge", "draft", 0, "part-1"),
            textRow("ch-4", "Market", "draft", 1800),
            textRow("ch-5", "Epilogue", "draft", 0)};
}

/// The plan of Caravan: the first chapter written, the second in writing with its revisions
core::WritingPlan caravanPlan()
{
    core::WritingPlan plan;
    plan.set(planOf("ch-1", day(11, 2), day(11, 13), day(11, 16), day(11, 17), 3000));
    plan.set(planOf("ch-2", day(11, 16), day(11, 27), day(12, 3), day(12, 4), 3000));
    plan.set(planOf("ch-4", day(11, 9), day(11, 20), {}, {}, 2500));
    return plan;
}

ScheduleFacts caravanFacts()
{
    ScheduleFacts facts;
    facts.today = NOW.date();
    facts.bookTargetWords = 32000;
    facts.bookDeadline = day(1, 31, 2027);
    facts.forecast = day(1, 26, 2027);
    core::WritingDeadline part;
    part.name = QStringLiteral("Part I for the editor");
    part.date = day(12, 11);
    part.elementId = QStringLiteral("part-1");
    facts.deadlines = {part};
    facts.written[QStringLiteral("ch-2")] = {{day(11, 16), 420}, {day(11, 17), 380},
                                             {day(11, 19), 440}};
    return facts;
}

/// A chart of Caravan in a window, its axis by days
struct Chart {
    ScheduleView view;
    QList<core::WritingPlan> edited;
    QStringList opened;

    Chart()
    {
        view.resize(1800, 420);
        view.setRows(caravan());
        view.setFacts(caravanFacts());
        view.setPlan(caravanPlan());
        view.setScale(ScheduleView::Scale::Days);
        view.show();
        QApplication::processEvents();
        // The first week of November at the left
        view.showDate(day(11, 9));
        QObject::connect(&view, &ScheduleView::planEdited,
                         [this](const core::WritingPlan& plan) { edited << plan; });
        QObject::connect(&view, &ScheduleView::editRequested,
                         [this](const QString& id) { opened << id; });
    }

    [[nodiscard]] int row(const char* id) const { return view.rowOf(QString::fromLatin1(id)); }

    /// The plan of a text as the last change left it
    [[nodiscard]] core::TextPlan last(const char* id) const
    {
        REQUIRE_FALSE(edited.isEmpty());
        const core::TextPlan* plan = edited.last().find(QString::fromLatin1(id));
        return plan != nullptr ? *plan : core::TextPlan{};
    }

    void mouse(QEvent::Type type, const QPoint& at)
    {
        const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
        const Qt::MouseButtons buttons =
            type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
        QMouseEvent event(type, QPointF(at), QPointF(view.viewport()->mapToGlobal(at)), button,
                          buttons, Qt::NoModifier);
        QApplication::sendEvent(view.viewport(), &event);
    }

    /// A drag from @p from to @p to
    void drag(const QPoint& from, const QPoint& to)
    {
        mouse(QEvent::MouseButtonPress, from);
        mouse(QEvent::MouseMove, (from + to) / 2);
        mouse(QEvent::MouseMove, to);
        mouse(QEvent::MouseButtonRelease, to);
    }

    void key(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QKeyEvent press(QEvent::KeyPress, key, modifiers);
        QApplication::sendEvent(&view, &press);
    }

    /// The middle of the height of row @p row at @p x
    [[nodiscard]] QPoint at(int row, int x) const
    {
        return {x, view.rowRect(row).center().y()};
    }
};

/// A model at NOW with an empty journal, and a book of 6,160 words when it is opened
struct Schedule {
    QTemporaryDir dir;
    WritingStatsModel model;
    core::ProjectDatabase database;

    Schedule()
    {
        REQUIRE(dir.isValid());
        model.setClock([]() { return NOW; });
        REQUIRE(model.openJournal(dir.filePath(QStringLiteral("journal.db"))));
        model.setBookWordsSource([]() { return 6160; });
    }

    void openBook(bool readOnly = false)
    {
        if (!database.isOpen()) {
            REQUIRE(database.open(dir.path()));
        }
        model.setBook(&database, QStringLiteral("book"), readOnly);
    }
};

template <typename T>
T* child(const QWidget& parent, const char* name)
{
    auto* found = parent.findChild<T*>(QString::fromLatin1(name));
    REQUIRE(found != nullptr);
    return found;
}

/// The letter of the access key of @p text ("&Days": "d"); none: an empty string
QString accessKey(const QString& text)
{
    return QKeySequence::mnemonic(text).toString().right(1).toLower();
}

/// The access keys of the menus of the window, as they read now
QStringList menuKeys()
{
    QStringList keys;
    for (const char* menu : {"FILE", "EDIT", "BOOK", "INSERT", "FORMAT", "TABLE", "IMAGE", "TOOLS",
                             "ASSISTANT", "VIEW", "WINDOW", "HELP"}) {
        keys << accessKey(gui::MenuBuilder::menuText(menu));
    }
    keys << accessKey(gui::diagnosticsMenuTitle()) << accessKey(gui::devToolsMenuTitle());
    return keys;
}

/// The access keys of the buttons of the tab: each its own, none of a menu (a key of both
/// would only move the focus to the button)
void checkTabKeys(const WritingScheduleTab& tab)
{
    const QStringList menus = menuKeys();
    QStringList keys;
    for (const char* name : {"scheduleDays", "scheduleWeeks", "scheduleMonths"}) {
        keys << accessKey(child<QToolButton>(tab, name)->text());
    }
    for (const char* name : {"scheduleToday", "scheduleSpread", "scheduleGoals"}) {
        keys << accessKey(child<QPushButton>(tab, name)->text());
    }
    keys << accessKey(child<QCheckBox>(tab, "scheduleDaysOff")->text());
    keys.removeAll(QString());
    INFO(keys.join(QLatin1Char(' ')).toStdString());
    CHECK(keys.size() >= 7 - 1);
    for (const QString& key : keys) {
        CHECK(keys.count(key) == 1);
        CHECK_FALSE(menus.contains(key));
    }
}

}  // namespace

TEST_CASE("Writing schedule: the plan of a book is kept in its project.db",
          "[gui][statistics][schedule]")
{
    Schedule schedule;
    // Without a book: no plan to keep
    CHECK_FALSE(schedule.model.setPlan(caravanPlan()));
    schedule.openBook();
    CHECK(schedule.model.plan().texts.isEmpty());
    int changes = 0;
    QObject::connect(&schedule.model, &WritingStatsModel::planChanged, [&changes]() { ++changes; });
    REQUIRE(schedule.model.setPlan(caravanPlan()));
    CHECK(changes == 1);
    CHECK(schedule.model.plan() == caravanPlan());
    CHECK(core::WritingPlan::fromJson(
              schedule.database.getSetting(QString::fromLatin1(core::WritingPlan::SETTING_KEY))
                  .toString())
          == caravanPlan());
    // The same plan again: nothing changes
    REQUIRE(schedule.model.setPlan(caravanPlan()));
    CHECK(changes == 1);

    // Opened again, the book has it
    schedule.model.setBook(nullptr, QString());
    CHECK(schedule.model.plan().texts.isEmpty());
    schedule.openBook();
    CHECK(schedule.model.plan() == caravanPlan());

    // Another volume of a series has its own; the first one keeps its plan
    schedule.model.setVolume(QStringLiteral("writing.goals.vol-2"), {QStringLiteral("ch-9")});
    CHECK(schedule.model.plan().texts.isEmpty());
    core::WritingPlan second;
    second.set(planOf("ch-9", day(12, 1), day(12, 4)));
    REQUIRE(schedule.model.setPlan(second));
    CHECK_FALSE(schedule.database.getSetting(QStringLiteral("writing.plan.vol-2")).toString().isEmpty());
    schedule.model.setVolume(QString(), {});
    CHECK(schedule.model.plan() == caravanPlan());

    // Open read-only, the plan does not change
    schedule.openBook(true);
    CHECK_FALSE(schedule.model.setPlan(second));
    CHECK(schedule.model.plan() == caravanPlan());
}

TEST_CASE("Writing schedule: the bars of writing and revisions on the time axis",
          "[gui][statistics][schedule]")
{
    Chart chart;
    ScheduleView& view = chart.view;
    CHECK(view.dayWidth() == 30);
    CHECK(view.firstDay().dayOfWeek() == 1);
    CHECK(view.firstDay() <= day(11, 2));
    CHECK(view.lastDay() >= day(1, 31, 2027));

    // A bar from the left edge of its first day to the right edge of its last one
    const int well = chart.row("ch-2");
    const QRect writing = view.barRect(well, Bar::Writing);
    CHECK(writing.left() == view.xOf(day(11, 16)));
    CHECK(writing.left() + writing.width() == view.xOf(day(11, 28)));
    CHECK(view.dateAt(writing.left()) == day(11, 16));
    CHECK(view.dateAt(writing.right()) == day(11, 27));
    const QRect revision = view.barRect(well, Bar::Revision);
    CHECK(revision.left() == view.xOf(day(12, 3)));
    CHECK(revision.width() == 2 * view.dayWidth());
    CHECK(revision.top() == writing.top());
    // No bars: a text without a plan, a part, the book, a stage not planned
    CHECK(view.barRect(chart.row("ch-3"), Bar::Writing).isEmpty());
    CHECK(view.barRect(chart.row("part-1"), Bar::Writing).isEmpty());
    CHECK(view.barRect(chart.row(""), Bar::Writing).isEmpty());
    CHECK(view.barRect(chart.row("ch-4"), Bar::Revision).isEmpty());

    // The other scales: narrower days, the same days under the bars
    view.setScale(ScheduleView::Scale::Weeks);
    CHECK(view.dayWidth() == 10);
    CHECK(view.barRect(well, Bar::Writing).width() == 12 * 10);
    CHECK(view.dateAt(view.barRect(well, Bar::Writing).left()) == day(11, 16));
    view.setScale(ScheduleView::Scale::Months);
    CHECK(view.firstDay().day() == 1);
    CHECK(view.barRect(well, Bar::Writing).width() == 12 * view.dayWidth());
}

TEST_CASE("Writing schedule: late bars and what a row says", "[gui][statistics][schedule]")
{
    Chart chart;
    ScheduleView& view = chart.view;
    const int well = chart.row("ch-2");
    // Today, writing of Well is on time; Market is late: not as long as planned, a draft
    CHECK_FALSE(view.isLate(well, Bar::Writing));
    CHECK(view.isLate(chart.row("ch-4"), Bar::Writing));
    // Storm is final: never late
    CHECK_FALSE(view.isLate(chart.row("ch-1"), Bar::Writing));
    CHECK_FALSE(view.isLate(chart.row("ch-1"), Bar::Revision));

    const QString said = view.describe(well);
    CHECK(said.startsWith(QStringLiteral("Well: writing ")));
    CHECK(said.contains(QStringLiteral("(10 writing days)")));
    CHECK(said.contains(gui::stats_painter::number(1240) + QStringLiteral(" of ")
                        + gui::stats_painter::words(3000)));
    // 1,760 words left over the 5 writing days from today to Friday 27 November
    CHECK(said.contains(gui::stats_painter::words(1760) + QStringLiteral(" left, ")
                        + gui::stats_painter::words(352) + QStringLiteral(" a writing day")));
    CHECK(said.contains(QStringLiteral("revisions ")));
    CHECK(said.contains(QStringLiteral("written on 3 days")));
    CHECK(view.describe(chart.row("ch-4")).contains(
        QStringLiteral("late, ") + gui::stats_painter::words(700) + QStringLiteral(" missing")));
    CHECK(view.describe(chart.row("ch-3")).contains(QStringLiteral("no plan")));
    CHECK(view.describe(chart.row("part-1")).contains(QStringLiteral("Part I for the editor")));
    const QString book = view.describe(chart.row(""));
    CHECK(book.startsWith(QStringLiteral("Caravan: ")));
    CHECK(book.contains(QStringLiteral("deadline ")));
    CHECK(book.contains(QStringLiteral("at the present pace done about ")));

    // A week later, writing of Well is late; after its revisions, they are late too
    ScheduleFacts facts = caravanFacts();
    facts.today = day(11, 30);
    view.setFacts(facts);
    CHECK(view.isLate(well, Bar::Writing));
    CHECK_FALSE(view.isLate(well, Bar::Revision));
    facts.today = day(12, 7);
    view.setFacts(facts);
    CHECK(view.isLate(well, Bar::Revision));
    // In revision, its writing is done
    QList<ScheduleRow> rows = caravan();
    rows[3].status = QStringLiteral("revision");
    view.setRows(rows);
    CHECK_FALSE(view.isLate(well, Bar::Writing));
    CHECK(view.isLate(well, Bar::Revision));
}

TEST_CASE("Writing schedule: the mouse moves a bar, changes its end and plans a text",
          "[gui][statistics][schedule]")
{
    Chart chart;
    ScheduleView& view = chart.view;
    const int width = view.dayWidth();
    const int well = chart.row("ch-2");

    // The bar of writing moves three days; its revisions stay
    const QRect writing = view.barRect(well, Bar::Writing);
    REQUIRE(writing.left() > view.leftWidth());
    chart.drag(writing.center(), writing.center() + QPoint(3 * width, 0));
    REQUIRE(chart.edited.size() == 1);
    CHECK(chart.last("ch-2").start == day(11, 19));
    CHECK(chart.last("ch-2").end == day(11, 30));
    CHECK(chart.last("ch-2").revisionStart == day(12, 3));
    CHECK(chart.last("ch-2").targetWords == 3000);
    CHECK(view.currentRow() == well);

    // Its right edge: the last day two days later
    const QRect moved = view.barRect(well, Bar::Writing);
    const QPoint edge(moved.left() + moved.width(), moved.center().y());
    chart.drag(edge, edge + QPoint(2 * width, 0));
    REQUIRE(chart.edited.size() == 2);
    CHECK(chart.last("ch-2").start == day(11, 19));
    CHECK(chart.last("ch-2").end == day(12, 2));

    // The bar of revisions moves on its own
    const QRect revision = view.barRect(well, Bar::Revision);
    chart.drag(revision.center(), revision.center() + QPoint(4 * width, 0));
    REQUIRE(chart.edited.size() == 3);
    CHECK(chart.last("ch-2").revisionStart == day(12, 7));
    CHECK(chart.last("ch-2").revisionEnd == day(12, 8));
    CHECK(chart.last("ch-2").start == day(11, 19));

    // A drag in an empty row plans writing; in a row with writing only, revisions
    const int night = chart.row("ch-3");
    chart.drag(chart.at(night, view.xOf(day(12, 7)) + 5), chart.at(night, view.xOf(day(12, 18)) + 5));
    REQUIRE(chart.edited.size() == 4);
    CHECK(chart.last("ch-3").start == day(12, 7));
    CHECK(chart.last("ch-3").end == day(12, 18));
    CHECK_FALSE(chart.last("ch-3").hasRevision());
    const int market = chart.row("ch-4");
    chart.drag(chart.at(market, view.xOf(day(11, 25)) + 5), chart.at(market, view.xOf(day(11, 24)) + 5));
    REQUIRE(chart.edited.size() == 5);
    CHECK(chart.last("ch-4").revisionStart == day(11, 24));
    CHECK(chart.last("ch-4").revisionEnd == day(11, 25));

    // A click: the row is chosen, the plan stays as it is
    const int epilogue = chart.row("ch-5");
    chart.mouse(QEvent::MouseButtonPress, chart.at(epilogue, view.xOf(day(12, 1)) + 5));
    chart.mouse(QEvent::MouseButtonRelease, chart.at(epilogue, view.xOf(day(12, 1)) + 5));
    CHECK(chart.edited.size() == 5);
    CHECK(view.currentRow() == epilogue);
    CHECK(view.plan().find(QStringLiteral("ch-5")) == nullptr);

    // A double click opens the plan of the text
    QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(chart.at(epilogue, view.xOf(day(12, 1)))),
                      QPointF(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &twice);
    CHECK(chart.opened == QStringList{QStringLiteral("ch-5")});

    // Read-only: the bars do not move
    view.setReadOnly(true);
    const QRect still = view.barRect(well, Bar::Writing);
    chart.drag(still.center(), still.center() + QPoint(3 * width, 0));
    CHECK(chart.edited.size() == 5);
    CHECK(view.barRect(well, Bar::Writing) == still);
}

TEST_CASE("Writing schedule: the keys choose a text and change its plan",
          "[gui][statistics][schedule]")
{
    Chart chart;
    ScheduleView& view = chart.view;
    view.setCurrentRow(0);
    chart.key(Qt::Key_Down);
    chart.key(Qt::Key_Down);
    chart.key(Qt::Key_Down);
    CHECK(view.currentText() == QStringLiteral("ch-2"));
    chart.key(Qt::Key_Up);
    CHECK(view.currentText() == QStringLiteral("ch-1"));
    chart.key(Qt::Key_End);
    CHECK(view.currentText() == QStringLiteral("ch-5"));
    chart.key(Qt::Key_Home);
    CHECK(view.currentRow() == 0);
    CHECK(view.currentText().isEmpty());

    // Alt+Right moves the whole plan a day; Alt+Shift+Left the last day of writing
    view.setCurrentRow(chart.row("ch-2"));
    chart.key(Qt::Key_Right, Qt::AltModifier);
    REQUIRE(chart.edited.size() == 1);
    CHECK(chart.last("ch-2").start == day(11, 17));
    CHECK(chart.last("ch-2").end == day(11, 28));
    CHECK(chart.last("ch-2").revisionStart == day(12, 4));
    chart.key(Qt::Key_Left, Qt::AltModifier | Qt::ShiftModifier);
    REQUIRE(chart.edited.size() == 2);
    CHECK(chart.last("ch-2").start == day(11, 17));
    CHECK(chart.last("ch-2").end == day(11, 27));
    // Not before its first day
    for (int i = 0; i < 20; ++i) {
        chart.key(Qt::Key_Left, Qt::AltModifier | Qt::ShiftModifier);
    }
    CHECK(chart.last("ch-2").end == day(11, 17));

    // The arrows alone scroll the chart
    const int before = view.xOf(day(11, 17));
    chart.key(Qt::Key_Right);
    CHECK(view.xOf(day(11, 17)) == before - view.dayWidth());
    const int count = int(chart.edited.size());

    // Enter opens its plan, Delete removes it
    chart.key(Qt::Key_Return);
    CHECK(chart.opened == QStringList{QStringLiteral("ch-2")});
    chart.key(Qt::Key_Delete);
    REQUIRE(chart.edited.size() == count + 1);
    CHECK(chart.edited.last().find(QStringLiteral("ch-2")) == nullptr);

    // A text without a plan: Alt+arrows do nothing; the book or a part: nothing to open
    view.setCurrentRow(chart.row("ch-3"));
    chart.key(Qt::Key_Right, Qt::AltModifier);
    CHECK(chart.edited.size() == count + 1);
    view.setCurrentRow(chart.row("part-1"));
    chart.key(Qt::Key_Return);
    CHECK(chart.opened.size() == 1);
}

TEST_CASE("Writing schedule: the tab keeps the plan in the book, Undo takes it back",
          "[gui][statistics][schedule]")
{
    Schedule schedule;
    schedule.openBook();
    REQUIRE(schedule.model.setPlan(caravanPlan()));
    WritingScheduleTab tab(&schedule.model);
    tab.setRowsSource(caravan);
    tab.resize(1200, 600);
    tab.show();
    QApplication::processEvents();
    ScheduleView& view = *tab.view();
    REQUIRE(view.rows() == caravan());
    CHECK(view.plan() == caravanPlan());
    CHECK(view.facts().today == NOW.date());
    CHECK_FALSE(tab.canEditHere(gui::EditCommand::Undo));

    // A change of the chart goes to the book
    view.setCurrentRow(view.rowOf(QStringLiteral("ch-2")));
    QKeyEvent alt(QEvent::KeyPress, Qt::Key_Right, Qt::AltModifier);
    QApplication::sendEvent(&view, &alt);
    REQUIRE(schedule.model.plan().find(QStringLiteral("ch-2")) != nullptr);
    CHECK(schedule.model.plan().find(QStringLiteral("ch-2"))->start == day(11, 17));
    CHECK(child<QLabel>(tab, "scheduleDetails")->text().startsWith(QStringLiteral("Well: ")));

    // Edit > Undo and Redo
    REQUIRE(tab.canEditHere(gui::EditCommand::Undo));
    CHECK_FALSE(tab.canEditHere(gui::EditCommand::Redo));
    tab.editHere(gui::EditCommand::Undo);
    CHECK(schedule.model.plan() == caravanPlan());
    CHECK(view.plan() == caravanPlan());
    REQUIRE(tab.canEditHere(gui::EditCommand::Redo));
    tab.editHere(gui::EditCommand::Redo);
    CHECK(schedule.model.plan().find(QStringLiteral("ch-2"))->start == day(11, 17));

    // Edit > Delete removes the plan of the chosen text
    REQUIRE(tab.canEditHere(gui::EditCommand::Delete));
    tab.editHere(gui::EditCommand::Delete);
    CHECK(schedule.model.plan().find(QStringLiteral("ch-2")) == nullptr);
    CHECK_FALSE(tab.canEditHere(gui::EditCommand::Delete));
    CHECK(tab.takesEdit(gui::EditCommand::Undo));
    CHECK_FALSE(tab.takesEdit(gui::EditCommand::Copy));

    // Another book: nothing to undo of it
    schedule.model.setBook(nullptr, QString());
    CHECK_FALSE(tab.canEditHere(gui::EditCommand::Undo));
    CHECK(view.rows().isEmpty());
    CHECK(child<QLabel>(tab, "scheduleIntro")->text().contains(QStringLiteral("Open a book")));

    // Read-only: the plan shows, but does not change
    schedule.openBook(true);
    CHECK(view.isReadOnly());
    CHECK(view.rows() == caravan());
    view.setCurrentRow(view.rowOf(QStringLiteral("ch-1")));
    QApplication::sendEvent(&view, &alt);
    CHECK(schedule.model.plan().find(QStringLiteral("ch-1"))->start == day(11, 2));
    CHECK_FALSE(tab.canEditHere(gui::EditCommand::Delete));
    CHECK(child<QLabel>(tab, "scheduleIntro")->text().contains(QStringLiteral("read-only")));
}

TEST_CASE("Writing schedule: the access keys of the tab are not those of the menus",
          "[gui][statistics][schedule][keys]")
{
    Schedule schedule;
    WritingScheduleTab tab(&schedule.model);
    SECTION("in English") { checkTabKeys(tab); }
    SECTION("in Polish, the translation the program ships")
    {
        QTranslator polish;
        REQUIRE(polish.load(QStringLiteral(":/i18n/kalahari_pl.qm")));
        QCoreApplication::installTranslator(&polish);
        QApplication::processEvents();
        CHECK(child<QToolButton>(tab, "scheduleDays")->text() == QStringLiteral("Dn&i"));
        CHECK(WritingScheduleTab::tabTitle() == QStringLiteral("Harmonogram"));
        checkTabKeys(tab);
        QCoreApplication::removeTranslator(&polish);
        QApplication::processEvents();
    }
}

TEST_CASE("Writing schedule: the scale and the days off are kept for the next time",
          "[gui][statistics][schedule]")
{
    auto& settings = core::SettingsManager::getInstance();
    settings.removeKey(WritingScheduleTab::SCALE_SETTING);
    settings.removeKey(WritingScheduleTab::DAYS_OFF_SETTING);
    Schedule schedule;
    schedule.openBook();
    {
        WritingScheduleTab tab(&schedule.model);
        CHECK(tab.view()->scale() == ScheduleView::Scale::Weeks);
        CHECK(child<QToolButton>(tab, "scheduleWeeks")->isChecked());
        CHECK(tab.view()->daysOffShown());
        child<QToolButton>(tab, "scheduleMonths")->click();
        CHECK(tab.view()->scale() == ScheduleView::Scale::Months);
        child<QCheckBox>(tab, "scheduleDaysOff")->setChecked(false);
        CHECK_FALSE(tab.view()->daysOffShown());
    }
    WritingScheduleTab again(&schedule.model);
    CHECK(again.view()->scale() == ScheduleView::Scale::Months);
    CHECK(child<QToolButton>(again, "scheduleMonths")->isChecked());
    CHECK_FALSE(again.view()->daysOffShown());
    settings.removeKey(WritingScheduleTab::SCALE_SETTING);
    settings.removeKey(WritingScheduleTab::DAYS_OFF_SETTING);
}

TEST_CASE("Writing schedule: the plan of a text in its window", "[gui][statistics][schedule]")
{
    const core::WritingRules rules;  // Monday to Friday
    // A text without a plan: writing from today for two weeks, no revisions
    core::TextPlan none;
    none.elementId = QStringLiteral("ch-3");
    {
        gui::dialogs::TextPlanDialog dialog(QStringLiteral("Night"), none, 0, rules, NOW.date());
        CHECK(dialog.windowTitle() == QStringLiteral("Plan of the Text"));
        CHECK(child<QCheckBox>(dialog, "planWriting")->isChecked());
        CHECK_FALSE(child<QCheckBox>(dialog, "planRevision")->isChecked());
        CHECK_FALSE(child<QDateEdit>(dialog, "planRevisionStart")->isEnabled());
        CHECK(dialog.findChild<QPushButton*>(QStringLiteral("planRemove")) == nullptr);
        const core::TextPlan plan = dialog.plan();
        CHECK(plan.elementId == QStringLiteral("ch-3"));
        CHECK(plan.start == NOW.date());
        CHECK(plan.end == NOW.date().addDays(13));
        CHECK_FALSE(plan.hasRevision());
    }

    // A text with a plan: its days, revisions, length; the note says what a day needs
    const core::TextPlan well =
        planOf("ch-2", day(11, 16), day(11, 27), day(12, 3), day(12, 4), 3000);
    gui::dialogs::TextPlanDialog dialog(QStringLiteral("Well"), well, 1240, rules, NOW.date());
    CHECK(dialog.plan() == well);
    const QString note = child<QLabel>(dialog, "planNote")->text();
    CHECK(note.contains(QStringLiteral("Writing: 10 writing days.")));
    CHECK(note.contains(gui::stats_painter::words(352) + QStringLiteral(" a writing day")));
    CHECK(note.contains(QStringLiteral("Revisions: 2 writing days.")));
    // The last day not before the first
    child<QDateEdit>(dialog, "planStart")->setDate(day(12, 10));
    CHECK(child<QDateEdit>(dialog, "planEnd")->date() == day(12, 10));
    CHECK(child<QLabel>(dialog, "planNote")->text().contains(
        QStringLiteral("The revisions start before the writing ends.")));
    child<QCheckBox>(dialog, "planRevision")->setChecked(false);
    child<QSpinBox>(dialog, "planLength")->setValue(4000);
    const core::TextPlan changed = dialog.plan();
    CHECK(changed.start == day(12, 10));
    CHECK_FALSE(changed.hasRevision());
    CHECK(changed.targetWords == 4000);

    // Remove Plan
    child<QPushButton>(dialog, "planRemove")->click();
    CHECK(dialog.isRemoved());
    CHECK(dialog.plan().isEmpty());
    CHECK(dialog.plan().elementId == QStringLiteral("ch-2"));
}

TEST_CASE("Writing schedule: Tools > Writing Schedule opens one tab with the texts of the book",
          "[gui][statistics][schedule]")
{
    test::OpenNovel novel(QStringLiteral("Sand"));
    QStatusBar statusBar;
    QMainWindow window;
    gui::registerAllCommands(kalahari::test::commands(), gui::CommandCallbacks{});
    auto* docks = new gui::DockCoordinator(novel.pm, kalahari::test::commands(), &window, &window);
    docks->createDocks();
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QString opened;
    gui::WritingStatsCoordinator coordinator(
        novel.pm, kalahari::test::commands(), docks, nullptr,
        [&opened](const QString& page) { opened = page; }, nullptr,
        dir.filePath(QStringLiteral("journal.db")));
    coordinator.connectCommands();
    window.show();
    QApplication::processEvents();
    REQUIRE(coordinator.model()->hasBook());

    QTabWidget* tabs = docks->centralTabs();
    const int before = tabs->count();
    auto& registry = test::commands();
    REQUIRE(registry.executeCommand("tools.writingSchedule")
            == gui::CommandExecutionResult::Success);
    REQUIRE(tabs->count() == before + 1);
    auto* tab = qobject_cast<WritingScheduleTab*>(tabs->currentWidget());
    REQUIRE(tab != nullptr);
    CHECK(tabs->tabText(tabs->currentIndex()) == QStringLiteral("Schedule"));
    CHECK(tab->property("tabIconId").toString() == QStringLiteral("tools.writingSchedule"));

    // The book first, then its texts: the chapter added last at the end
    const QList<ScheduleRow>& rows = tab->view()->rows();
    REQUIRE(rows.size() >= 2);
    CHECK(rows.first().kind == ScheduleRow::Kind::Book);
    CHECK(rows.first().title == QStringLiteral("Sand"));
    const ScheduleRow& storm = rows.last();
    CHECK(storm.kind == ScheduleRow::Kind::Text);
    CHECK(storm.id == novel.chapterId);
    CHECK(storm.title == QStringLiteral("The Storm"));
    CHECK(storm.status == QStringLiteral("draft"));
    CHECK_FALSE(storm.iconId.isEmpty());
    int words = 0;
    for (const ScheduleRow& row : rows) {
        words += row.kind == ScheduleRow::Kind::Text ? row.words : 0;
    }
    CHECK(rows.first().words == words);

    // Again: the same tab, brought to the front
    tabs->setCurrentIndex(0);
    REQUIRE(registry.executeCommand("tools.writingSchedule")
            == gui::CommandExecutionResult::Success);
    CHECK(tabs->count() == before + 1);
    CHECK(tabs->currentWidget() == tab);

    // The gear: the colors; Goals and Deadlines: their tab
    child<QToolButton>(*tab, "scheduleSettings")->click();
    CHECK(opened == QString::fromLatin1(gui::WritingStatsCoordinator::SCHEDULE_SETTINGS_PAGE));
    child<QPushButton>(*tab, "scheduleGoals")->click();
    CHECK(qobject_cast<gui::WritingGoalsTab*>(tabs->currentWidget()) != nullptr);
}

TEST_CASE("Writing schedule: Plan the Texts lays texts one after another by their words",
          "[gui][statistics][schedule][spread]")
{
    using gui::dialogs::SpreadPlanDialog;
    using Texts = SpreadPlanDialog::Texts;
    const core::WritingRules rules;  // Monday to Friday
    const QDate monday = NOW.date();
    const QDate friday = day(12, 4);  // Ten writing days

    // The texts in Draft without a plan of writing, in the order of the book
    CHECK(SpreadPlanDialog::textsToPlan(caravan(), caravanPlan(), Texts::WithoutPlan)
          == QStringList{QStringLiteral("ch-3"), QStringLiteral("ch-5")});
    CHECK(SpreadPlanDialog::textsToPlan(caravan(), caravanPlan(), Texts::BeingWritten)
          == QStringList{QStringLiteral("ch-2"), QStringLiteral("ch-3"), QStringLiteral("ch-4"),
                         QStringLiteral("ch-5")});

    // Two empty texts of 2,000 words: a week each; they keep the length; the rest stays
    core::WritingPlan spread = SpreadPlanDialog::spread(caravan(), caravanPlan(),
                                                        Texts::WithoutPlan, monday, friday, 2000,
                                                        rules);
    CHECK(*spread.find(QStringLiteral("ch-3"))
          == planOf("ch-3", day(11, 23), day(11, 27), {}, {}, 2000));
    CHECK(*spread.find(QStringLiteral("ch-5"))
          == planOf("ch-5", day(11, 30), day(12, 4), {}, {}, 2000));
    for (const char* id : {"ch-1", "ch-2", "ch-4"}) {
        CHECK(*spread.find(QString::fromLatin1(id)) == *caravanPlan().find(QString::fromLatin1(id)));
    }

    // All in Draft: 1,760 + 2,000 + 700 + 2,000 words over ten days; the revisions stay
    spread = SpreadPlanDialog::spread(caravan(), caravanPlan(), Texts::BeingWritten, monday,
                                      friday, 2000, rules);
    CHECK(*spread.find(QStringLiteral("ch-2"))
          == planOf("ch-2", day(11, 23), day(11, 25), day(12, 3), day(12, 4), 3000));
    CHECK(*spread.find(QStringLiteral("ch-3"))
          == planOf("ch-3", day(11, 25), day(11, 30), {}, {}, 2000));
    CHECK(*spread.find(QStringLiteral("ch-4"))
          == planOf("ch-4", day(11, 30), day(12, 1), {}, {}, 2500));
    CHECK(*spread.find(QStringLiteral("ch-5"))
          == planOf("ch-5", day(12, 1), day(12, 4), {}, {}, 2000));
    CHECK(*spread.find(QStringLiteral("ch-1")) == *caravanPlan().find(QStringLiteral("ch-1")));
    // No writing day: nothing changes
    CHECK(SpreadPlanDialog::spread(caravan(), caravanPlan(), Texts::BeingWritten, day(11, 28),
                                   day(11, 29), 2000, rules)
          == caravanPlan());

    // The length of a text without its own: the book's over its texts
    core::BookGoals goals;
    CHECK(SpreadPlanDialog::defaultLengthOf(caravan(), goals) == 3000);
    goals.targetWords = 32000;
    CHECK(SpreadPlanDialog::defaultLengthOf(caravan(), goals) == 6400);

    // The window: to the deadline of the book, the words a day before anything changes
    goals.deadline = friday;
    SpreadPlanDialog dialog(caravan(), caravanPlan(), goals, rules, monday);
    CHECK(child<QRadioButton>(dialog, "spreadWithoutPlan")->isChecked());
    CHECK(child<QDateEdit>(dialog, "spreadFrom")->date() == monday);
    CHECK(child<QDateEdit>(dialog, "spreadTo")->date() == friday);
    CHECK(dialog.findChild<QLabel*>(QStringLiteral("spreadNoDeadline")) == nullptr);
    CHECK(child<QSpinBox>(dialog, "spreadLength")->value() == 6400);
    child<QSpinBox>(dialog, "spreadLength")->setValue(2000);
    auto* note = child<QLabel>(dialog, "spreadNote");
    CHECK(note->text().toStdString()
          == "2 texts lack 4,000 words, on 10 writing days: about 400 words a writing day.");
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.plan()
          == SpreadPlanDialog::spread(caravan(), caravanPlan(), Texts::WithoutPlan, monday,
                                      friday, 2000, rules));
    child<QRadioButton>(dialog, "spreadBeingWritten")->setChecked(true);
    CHECK(note->text().contains(QStringLiteral("The writing of 2 texts is planned anew.")));
    // A weekend only: nothing to plan on
    child<QDateEdit>(dialog, "spreadFrom")->setDate(day(11, 28));
    child<QDateEdit>(dialog, "spreadTo")->setDate(day(11, 29));
    CHECK(note->text() == QStringLiteral("There is no writing day between these days."));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    // Without a deadline: four weeks, and a word about it; every text planned: all of them
    core::WritingPlan full = caravanPlan();
    full.set(planOf("ch-3", day(11, 23), day(11, 24)));
    full.set(planOf("ch-5", day(11, 23), day(11, 24)));
    SpreadPlanDialog without(caravan(), full, core::BookGoals(), rules, monday);
    CHECK(child<QDateEdit>(without, "spreadTo")->date() == monday.addDays(27));
    CHECK(without.findChild<QLabel*>(QStringLiteral("spreadNoDeadline")) != nullptr);
    CHECK(child<QRadioButton>(without, "spreadBeingWritten")->isChecked());
}

TEST_CASE("Writing schedule: Plan Texts... on the tab, one step to undo",
          "[gui][statistics][schedule][spread]")
{
    Schedule schedule;
    schedule.openBook();
    REQUIRE(schedule.model.setPlan(caravanPlan()));
    WritingScheduleTab tab(&schedule.model);
    tab.setRowsSource(caravan);
    tab.resize(1200, 600);
    tab.show();
    QApplication::processEvents();
    auto* button = child<QPushButton>(tab, "scheduleSpread");
    REQUIRE(button->isEnabled());

    // The window answered with a length of 2,000 words to Friday next week
    int shown = 0;
    QTimer answer;
    answer.setInterval(10);
    QObject::connect(&answer, &QTimer::timeout, [&shown]() {
        auto* dialog =
            qobject_cast<gui::dialogs::SpreadPlanDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr || !dialog->isVisible()) {
            return;
        }
        ++shown;
        child<QDateEdit>(*dialog, "spreadTo")->setDate(day(12, 4));
        child<QSpinBox>(*dialog, "spreadLength")->setValue(2000);
        dialog->accept();
    });
    answer.start();
    button->click();
    answer.stop();
    CHECK(shown == 1);
    REQUIRE(schedule.model.plan().find(QStringLiteral("ch-3")) != nullptr);
    CHECK(*schedule.model.plan().find(QStringLiteral("ch-3"))
          == planOf("ch-3", day(11, 23), day(11, 27), {}, {}, 2000));
    CHECK(schedule.model.plan().find(QStringLiteral("ch-5"))->end == day(12, 4));

    // One step back
    REQUIRE(tab.canEditHere(gui::EditCommand::Undo));
    tab.editHere(gui::EditCommand::Undo);
    CHECK(schedule.model.plan() == caravanPlan());

    // A book read-only: nothing to plan
    schedule.openBook(true);
    CHECK_FALSE(button->isEnabled());
}

TEST_CASE("Writing schedule: the daily goal from the schedule, in the goals and the panel",
          "[gui][statistics][schedule][goal]")
{
    Schedule schedule;
    // 200 words of the well written this morning: it had 1,040 when the day started
    core::WritingActivity morning;
    morning.date = NOW.date();
    morning.slot = core::activitySlotOf(QTime(9, 0));
    morning.bookId = QStringLiteral("book");
    morning.elementId = QStringLiteral("ch-2");
    morning.written = 200;
    REQUIRE(schedule.database.open(schedule.dir.path()));
    REQUIRE(schedule.database.addWritingActivity({morning}));
    schedule.model.setPlannedTextsSource([]() {
        QHash<QString, core::PlannedText> texts;
        for (const ScheduleRow& row : caravan()) {
            if (row.kind == ScheduleRow::Kind::Text) {
                texts.insert(row.id, {row.words, row.status == QLatin1String("draft")});
            }
        }
        return texts;
    });
    schedule.openBook();
    core::WritingRules rules = schedule.model.rules();
    REQUIRE(rules.isWritingDay(NOW.date()));
    const int own = rules.dailyGoalOn(NOW.date());

    // Asked for, but nothing planned: the writer's own goal
    core::BookGoals goals;
    goals.dailyGoalFromPlan = true;
    REQUIRE(schedule.model.setGoals(goals));
    CHECK(schedule.model.dailyGoalSourceToday() == gui::DailyGoalSource::Own);
    CHECK(schedule.model.dailyGoalToday() == own);

    // The well: 1,960 words over five days; the market, its bar over: 700 today
    int goalChanges = 0;
    QObject::connect(&schedule.model, &WritingStatsModel::goalsChanged,
                     [&goalChanges]() { ++goalChanges; });
    REQUIRE(schedule.model.setPlan(caravanPlan()));
    CHECK(goalChanges == 1);
    CHECK(schedule.model.dailyGoalSourceToday() == gui::DailyGoalSource::Schedule);
    CHECK(schedule.model.dailyGoalToday() == 392 + 700);
    QStringList texts;
    CHECK(schedule.model.dailyGoalForPlanToday(&texts) == 392 + 700);
    CHECK(texts == QStringList{QStringLiteral("ch-2"), QStringLiteral("ch-4")});

    // The tab of the goals: the option, what it gives today, one of the two options
    gui::WritingGoalsTab goalsTab(&schedule.model);
    auto* fromPlan = child<QCheckBox>(goalsTab, "dailyGoalFromPlan");
    auto* fromDeadline = child<QCheckBox>(goalsTab, "dailyGoalFromDeadline");
    CHECK(fromPlan->isChecked());
    CHECK(fromPlan->isEnabled());
    CHECK_FALSE(child<QSpinBox>(goalsTab, "dailyGoal")->isEnabled());
    CHECK(fromPlan->text() == QStringLiteral("Set the daily goal from the s&chedule"));
    // The note beside the option (the deadline's needs a length and a deadline)
    const QString today = QStringLiteral("(today: %1)").arg(gui::stats_painter::words(1092));
    const QList<QLabel*> labels = goalsTab.findChildren<QLabel*>();
    CHECK(std::count_if(labels.cbegin(), labels.cend(),
                        [&today](const QLabel* label) { return label->text() == today; })
          == 1);

    // The deadline instead: the schedule is off
    goals.targetWords = 32000;
    goals.deadline = day(1, 31, 2027);
    REQUIRE(schedule.model.setGoals(goals));
    fromDeadline->setChecked(true);
    CHECK(schedule.model.goals().dailyGoalFromDeadline);
    CHECK_FALSE(schedule.model.goals().dailyGoalFromPlan);
    CHECK_FALSE(fromPlan->isChecked());
    CHECK(schedule.model.dailyGoalSourceToday() == gui::DailyGoalSource::Deadline);
    fromPlan->setChecked(true);
    CHECK_FALSE(schedule.model.goals().dailyGoalFromDeadline);
    CHECK(schedule.model.dailyGoalSourceToday() == gui::DailyGoalSource::Schedule);

    // The Statistics panel says where the goal of today comes from
    gui::WritingStatsPanel panel;
    panel.setModel(&schedule.model);
    panel.setTitleSources(
        [](const QString& id) {
            for (const ScheduleRow& row : caravan()) {
                if (row.id == id) {
                    return row.title;
                }
            }
            return QString();
        },
        []() { return QStringLiteral("Caravan"); });
    panel.show();
    CHECK(panel.details().contains(QStringLiteral("Daily goal from: the schedule: Well, Market")));
    fromDeadline->setChecked(true);
    panel.refresh();
    CHECK(panel.details().contains(QStringLiteral("Daily goal from: the deadline of the book")));
    // Nothing planned for today
    fromPlan->setChecked(true);
    core::WritingPlan later;
    later.set(planOf("ch-3", day(11, 30), day(12, 4), {}, {}, 2000));
    REQUIRE(schedule.model.setPlan(later));
    panel.refresh();
    CHECK(schedule.model.dailyGoalToday() == 0);
    CHECK(panel.details().contains(
        QStringLiteral("Daily goal from: the schedule – nothing planned for today")));
}

TEST_CASE("Writing schedule: pictures of the tab", "[.][shot]")
{
    // KALAHARI_SHOTS=<folder>: the tab in both themes at three scales, for the eyes
    const QString folder = qEnvironmentVariable("KALAHARI_SHOTS");
    if (folder.isEmpty()) {
        return;
    }
    Schedule schedule;
    schedule.openBook();
    core::BookGoals goals;
    goals.targetWords = 32000;
    goals.deadline = day(1, 31, 2027);
    core::WritingDeadline part;
    part.name = QStringLiteral("Part I for the editor");
    part.date = day(12, 11);
    part.elementId = QStringLiteral("part-1");
    goals.deadlines = {part};
    REQUIRE(schedule.model.setGoals(goals));
    REQUIRE(schedule.model.setPlan(caravanPlan()));
    QList<core::WritingActivity> activity;
    const QList<std::pair<QDate, int>> days{
        {day(11, 16), 420}, {day(11, 17), 380}, {day(11, 19), 640}, {day(11, 20), 210}};
    for (const auto& [date, words] : days) {
        core::WritingActivity entry;
        entry.date = date;
        entry.slot = core::activitySlotOf(QTime(10, 0));
        entry.bookId = QStringLiteral("book");
        entry.elementId = QStringLiteral("ch-2");
        entry.written = words;
        activity << entry;
    }
    REQUIRE(schedule.database.addWritingActivity(activity));
    schedule.openBook();
    WritingScheduleTab tab(&schedule.model);
    tab.setRowsSource(caravan);
    tab.resize(1280, 520);
    tab.show();
    QApplication::processEvents();
    tab.view()->setCurrentRow(tab.view()->rowOf(QStringLiteral("ch-2")));
    tab.view()->setFocus();
    const QList<std::pair<const char*, const char*>> scales{
        {"dni", "scheduleDays"}, {"tygodnie", "scheduleWeeks"}, {"miesiace", "scheduleMonths"}};
    auto& themes = core::ThemeManager::getInstance();
    const QString theme = QString::fromStdString(themes.getCurrentTheme().name);
    for (const char* shown : {"Light", "Dark"}) {
        themes.switchTheme(QString::fromLatin1(shown));
        tab.view()->refreshColors();
        for (const auto& [name, button] : scales) {
            child<QToolButton>(tab, button)->click();
            tab.view()->showDate(day(11, 2));
            QApplication::processEvents();
            tab.grab().save(folder + QStringLiteral("/harmonogram-%1-%2.png")
                                         .arg(QString::fromLatin1(shown), QString::fromLatin1(name)));
        }
    }
    themes.switchTheme(theme);
    gui::dialogs::TextPlanDialog dialog(
        QStringLiteral("Well"), planOf("ch-2", day(11, 16), day(11, 27), day(12, 3), day(12, 4), 3000),
        1240, schedule.model.rules(), NOW.date());
    dialog.show();
    QApplication::processEvents();
    dialog.grab().save(folder + QStringLiteral("/harmonogram-plan-tekstu.png"));
    core::BookGoals spreadGoals = goals;
    spreadGoals.deadline = day(12, 18);
    gui::dialogs::SpreadPlanDialog spread(caravan(), caravanPlan(), spreadGoals,
                                          schedule.model.rules(), NOW.date());
    spread.show();
    QApplication::processEvents();
    spread.grab().save(folder + QStringLiteral("/harmonogram-rozplanuj.png"));
}
