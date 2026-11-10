/// @file test_writing_goals_tab.cpp
/// @brief The tab Writing Goals and Deadlines: the writer's goals in the settings, the goals of
/// the book in the book, at once, and the window of a deadline

#include <catch2/catch_test_macros.hpp>

#include "../editor/editor_test_utils.h"
#include "kalahari/core/project_database.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/writing_activity.h"
#include "kalahari/core/writing_goals.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/deadline_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/panels/writing_goals_tab.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/writing_stats_coordinator.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <functional>

using namespace kalahari;
using gui::WritingGoalsTab;
using gui::WritingStatsModel;
using gui::dialogs::BookPart;
using gui::dialogs::DeadlineDialog;
using gui::stats_painter::number;

namespace {

/// Thursday, 12 November 2026, half past nine in the evening
const QDateTime NOW(QDate(2026, 11, 12), QTime(21, 30));

/// The words of the book in the tests
constexpr int BOOK_WORDS = 53600;

/// A model at NOW with an empty journal, and a book of BOOK_WORDS words when it is opened
struct Goals {
    QTemporaryDir dir;
    WritingStatsModel model;
    core::ProjectDatabase database;

    Goals()
    {
        REQUIRE(dir.isValid());
        model.setClock([]() { return NOW; });
        REQUIRE(model.openJournal(dir.filePath(QStringLiteral("journal.db"))));
        model.setBookWordsSource([]() { return BOOK_WORDS; });
    }

    void openBook()
    {
        REQUIRE(database.open(dir.path()));
        model.setBook(&database, QStringLiteral("book"));
    }

    /// The parts of the book: a part with two chapters
    static WritingGoalsTab::BookSource source()
    {
        WritingGoalsTab::BookSource source;
        source.title = []() { return QStringLiteral("Caravan"); };
        source.parts = []() {
            return QList<BookPart>{{QStringLiteral("part-1"), QStringLiteral("Part I. Desert"), 0},
                                   {QStringLiteral("ch-1"), QStringLiteral("Chapter 1"), 1},
                                   {QStringLiteral("ch-2"), QStringLiteral("Chapter 2"), 1}};
        };
        source.words = [](const QString& id) {
            return id == QLatin1String("part-1") ? 36400 : 0;
        };
        return source;
    }
};

template <typename T>
T* child(const QWidget& parent, const char* name)
{
    auto* found = parent.findChild<T*>(QString::fromLatin1(name));
    REQUIRE(found != nullptr);
    return found;
}

/// The texts of the row @p row of the list of the deadlines
QStringList rowTexts(const QTreeWidget& list, int row)
{
    QStringList texts;
    const QTreeWidgetItem* item = list.topLevelItem(row);
    REQUIRE(item != nullptr);
    for (int column = 0; column < list.columnCount(); ++column) {
        texts << item->text(column);
    }
    return texts;
}

/// Answer the window shown next by @p answer, once
void whenShown(const std::function<void(QDialog&)>& answer)
{
    auto* timer = new QTimer();
    timer->setInterval(10);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, answer]() {
        if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            timer->stop();
            timer->deleteLater();
            answer(*dialog);
        }
    });
    timer->start();
}

/// The mnemonic letters of the buttons and labels of @p widget that are shown
QStringList accessKeys(const QWidget& widget)
{
    QStringList keys;
    auto keyOf = [](const QString& text) {
        for (qsizetype i = 0; i + 1 < text.size(); ++i) {
            if (text[i] == QLatin1Char('&')) {
                if (text[i + 1] != QLatin1Char('&')) {
                    return QString(text[i + 1].toUpper());
                }
                ++i;
            }
        }
        return QString();
    };
    for (const QAbstractButton* button : widget.findChildren<QAbstractButton*>()) {
        if (!keyOf(button->text()).isEmpty()) {
            keys << keyOf(button->text());
        }
    }
    for (const QLabel* label : widget.findChildren<QLabel*>()) {
        if (label->buddy() != nullptr && !keyOf(label->text()).isEmpty()) {
            keys << keyOf(label->text());
        }
    }
    return keys;
}

}  // namespace

TEST_CASE("Writing goals: the writer's goals change at once and follow the settings",
          "[gui][statistics][goals]")
{
    auto& settings = core::SettingsManager::getInstance();
    Goals goals;
    WritingGoalsTab tab(&goals.model);
    tab.setBookSource(Goals::source());
    tab.show();

    // The goals of the settings: 1,000 words on Monday to Friday, the week their sum
    auto* daily = child<QSpinBox>(tab, "dailyGoal");
    CHECK(daily->value() == 1000);
    for (int day = 1; day <= 7; ++day) {
        const auto name = QStringLiteral("writingDay%1").arg(day).toLatin1();
        CHECK(child<QToolButton>(tab, name.constData())->isChecked() == (day <= 5));
    }
    auto* sum = child<QRadioButton>(tab, "weeklyGoalSum");
    auto* own = child<QRadioButton>(tab, "weeklyGoalOwn");
    CHECK(sum->isChecked());
    CHECK(sum->text() == QStringLiteral("The &sum of the daily goals: %1 words").arg(number(5000)));

    // Without a book its goals are off
    CHECK_FALSE(child<QSpinBox>(tab, "bookLength")->isEnabled());
    CHECK_FALSE(child<QTreeWidget>(tab, "deadlines")->isEnabled());
    CHECK(child<QLabel>(tab, "bookCalculation")->text() ==
          QStringLiteral("Open a book to set its length and deadline."));

    // A change goes to the settings at once, and the sum of the week follows it
    daily->setValue(1200);
    CHECK(settings.get<int>("stats.dailyGoal") == 1200);
    child<QToolButton>(tab, "writingDay6")->click();
    CHECK(settings.get<int>("stats.writingDays") == 0x3f);
    test::runEventLoop(WritingStatsModel::CHANGE_INTERVAL_MS * 2);
    CHECK(goals.model.rules().dailyGoal == 1200);
    CHECK(sum->text() == QStringLiteral("The &sum of the daily goals: %1 words").arg(number(7200)));

    // The writer's own weekly goal, and back to the sum
    child<QSpinBox>(tab, "weeklyGoal")->setValue(6000);
    CHECK(own->isChecked());
    CHECK(settings.get<int>("stats.weeklyGoal") == 6000);
    sum->click();
    CHECK(settings.get<int>("stats.weeklyGoal") == 0);

    // A goal changed in another place shows here
    settings.set<int>("stats.dailyGoal", 800);
    test::runEventLoop(WritingStatsModel::CHANGE_INTERVAL_MS * 2);
    CHECK(daily->value() == 800);

    // Every field and button has a key of its own
    QStringList keys = accessKeys(tab);
    const auto count = keys.size();
    keys.removeDuplicates();
    CHECK(keys.size() == count);
}

TEST_CASE("Writing goals: the length and the deadline of the book, and what they need",
          "[gui][statistics][goals]")
{
    Goals goals;
    goals.openBook();
    WritingGoalsTab tab(&goals.model);
    tab.setBookSource(Goals::source());
    tab.show();
    auto* length = child<QSpinBox>(tab, "bookLength");
    auto* calculation = child<QLabel>(tab, "bookCalculation");
    REQUIRE(length->isEnabled());
    CHECK(calculation->text() ==
          QStringLiteral("The book has %1 words. Set its length to see how much is left.")
              .arg(number(BOOK_WORDS)));

    // The length is kept in the book at once
    length->setValue(90000);
    CHECK(goals.model.goals().targetWords == 90000);
    CHECK(core::BookGoals::fromJson(
              goals.database.getSetting(QString::fromLatin1(core::BookGoals::SETTING_KEY)).toString())
              .targetWords == 90000);
    CHECK(calculation->text() ==
          QStringLiteral("The book has %1 words and %2 words are left. The forecast shows after "
                         "a few days of writing.")
              .arg(number(BOOK_WORDS), number(90000 - BOOK_WORDS)));

    // The deadline: the words of a writing day it needs
    auto* fromDeadline = child<QCheckBox>(tab, "dailyGoalFromDeadline");
    CHECK_FALSE(fromDeadline->isEnabled());
    child<QDateEdit>(tab, "bookDeadline")->setDate(QDate(2027, 1, 31));
    CHECK(child<QCheckBox>(tab, "bookHasDeadline")->isChecked());
    CHECK(goals.model.goals().deadline == QDate(2027, 1, 31));
    const core::WritingRules& rules = goals.model.rules();
    const int days = core::writingDaysBetween(rules, NOW.date(), QDate(2027, 1, 31));
    const int needed = core::wordsPerWritingDay(90000 - BOOK_WORDS, days);
    CHECK(calculation->text().startsWith(
        QStringLiteral("The book has %1 words, %2 words are left and %3 writing days to the "
                       "deadline, that is <b>%4 words on a writing day</b>.")
            .arg(number(BOOK_WORDS), number(90000 - BOOK_WORDS), number(days), number(needed))));

    // The daily goal from the deadline: the words of today, and the writer's goal is off
    REQUIRE(fromDeadline->isEnabled());
    fromDeadline->click();
    CHECK(goals.model.goals().dailyGoalFromDeadline);
    CHECK(goals.model.dailyGoalToday() == needed);
    CHECK_FALSE(child<QSpinBox>(tab, "dailyGoal")->isEnabled());

    // No deadline: the book's goal without it
    child<QCheckBox>(tab, "bookHasDeadline")->click();
    CHECK_FALSE(goals.model.goals().deadline.isValid());
    CHECK(child<QSpinBox>(tab, "dailyGoal")->isEnabled());

    // The goals of another book, or of none, show at once
    goals.model.setBook(nullptr, QString());
    CHECK_FALSE(length->isEnabled());
    CHECK(length->value() == 0);
}

TEST_CASE("Writing goals: the daily goal from the deadline counts from the book of the morning",
          "[gui][statistics][goals]")
{
    // Regression: with "All the words written", the words written and deleted again today
    // made the book of the morning shorter, and the daily goal bigger; a deadline passed or a
    // length taken away kept the daily goal at 0, while the tab let the writer's goal be set
    Goals goals;
    REQUIRE(goals.database.open(goals.dir.path()));
    core::WritingActivity morning;
    morning.date = NOW.date();
    morning.slot = core::activitySlotOf(QTime(10, 0));
    morning.bookId = QStringLiteral("book");
    morning.elementId = QStringLiteral("ch-1");
    morning.written = 100;
    morning.deleted = 100;
    morning.pasted = 300;
    REQUIRE(goals.database.addWritingActivity({morning}));
    goals.model.setBook(&goals.database, QStringLiteral("book"));
    // The book grew by the 300 words pasted today
    CHECK(goals.model.bookWordsAtStartOfToday() == BOOK_WORDS - 300);

    core::BookGoals book;
    book.targetWords = 90000;
    book.deadline = QDate(2027, 1, 31);
    book.dailyGoalFromDeadline = true;
    REQUIRE(goals.model.setGoals(book));
    const int days = core::writingDaysBetween(goals.model.rules(), NOW.date(), book.deadline);
    const int needed = core::wordsPerWritingDay(90000 - (BOOK_WORDS - 300), days);
    auto& settings = core::SettingsManager::getInstance();
    for (const char* counting : {"written", "net"}) {
        INFO(counting);
        settings.set<std::string>("stats.goalCounting", counting);
        goals.model.reloadRules();
        CHECK(goals.model.dailyGoalToday() == needed);
    }

    // Without a length, or with the deadline passed: the writer's own daily goal
    const int own = goals.model.rules().dailyGoalOn(NOW.date());
    REQUIRE(own > 0);
    book.targetWords = 0;
    REQUIRE(goals.model.setGoals(book));
    CHECK(goals.model.dailyGoalToday() == own);
    book.targetWords = 90000;
    book.deadline = NOW.date().addDays(-1);
    REQUIRE(goals.model.setGoals(book));
    CHECK(goals.model.dailyGoalToday() == own);
}

TEST_CASE("Writing goals: a book open read-only keeps its goals", "[gui][statistics][goals]")
{
    // A book opens read-only on a copy of its database, which goes when it closes: its goals
    // do not change, so that no change is lost; the writer's own goals do
    Goals goals;
    REQUIRE(goals.database.open(goals.dir.path()));
    core::BookGoals kept;
    kept.targetWords = 90000;
    goals.database.setSetting(QString::fromLatin1(core::BookGoals::SETTING_KEY), kept.toJson());
    goals.model.setBook(&goals.database, QStringLiteral("book"), true);
    CHECK(goals.model.isBookReadOnly());
    CHECK(goals.model.goals().targetWords == 90000);
    core::BookGoals changed = kept;
    changed.targetWords = 50000;
    CHECK_FALSE(goals.model.setGoals(changed));
    CHECK(goals.model.goals().targetWords == 90000);

    WritingGoalsTab tab(&goals.model);
    tab.setBookSource(Goals::source());
    tab.show();
    CHECK_FALSE(child<QSpinBox>(tab, "bookLength")->isEnabled());
    CHECK(child<QSpinBox>(tab, "dailyGoal")->isEnabled());
    CHECK(child<QLabel>(tab, "goalsIntro")->text().contains(QStringLiteral("read-only")));

    // Open for writing again, it changes
    goals.model.setBook(&goals.database, QStringLiteral("book"));
    CHECK_FALSE(goals.model.isBookReadOnly());
    CHECK(child<QSpinBox>(tab, "bookLength")->isEnabled());
    CHECK(goals.model.setGoals(changed));
}

TEST_CASE("Writing goals: the deadlines of the book in their window", "[gui][statistics][goals]")
{
    Goals goals;
    goals.openBook();
    core::BookGoals book;
    book.targetWords = 90000;
    book.deadline = QDate(2027, 1, 31);
    book.deadlines = {{QStringLiteral("For the publisher"), QDate(2027, 1, 31), QString(), 90000}};
    REQUIRE(goals.model.setGoals(book));
    WritingGoalsTab tab(&goals.model);
    tab.setBookSource(Goals::source());
    tab.show();
    auto* list = child<QTreeWidget>(tab, "deadlines");
    REQUIRE(list->topLevelItemCount() == 1);
    CHECK(rowTexts(*list, 0) == QStringList{QStringLiteral("For the publisher"),
                                            QLocale().toString(QDate(2027, 1, 31), QLocale::ShortFormat),
                                            QStringLiteral("the whole book"),
                                            QStringLiteral("%1 words").arg(number(90000)),
                                            QStringLiteral("%1 (60%)").arg(number(BOOK_WORDS))});
    CHECK(list->topLevelItem(0)->font(0).bold());
    CHECK_FALSE(child<QPushButton>(tab, "changeDeadline")->isEnabled());

    // Add Deadline: the window with the parts of the book
    whenShown([](QDialog& shown) {
        auto& dialog = dynamic_cast<DeadlineDialog&>(shown);
        CHECK_FALSE(dialog.acceptButton()->isEnabled());  // not without a name
        auto* part = child<QComboBox>(dialog, "deadlinePart");
        CHECK(part->count() == 4);
        CHECK(part->currentText() == QStringLiteral("the whole book"));
        child<QLineEdit>(dialog, "deadlineName")->setText(QStringLiteral("Chapters for the editor"));
        child<QDateEdit>(dialog, "deadlineDate")->setDate(QDate(2026, 11, 30));
        part->setCurrentIndex(1);
        child<QSpinBox>(dialog, "deadlineWords")->setValue(40000);
        REQUIRE(dialog.acceptButton()->isEnabled());
        dialog.acceptButton()->click();
    });
    child<QPushButton>(tab, "addDeadline")->click();
    REQUIRE(goals.model.goals().deadlines.size() == 2);
    // By date: the new one first, chosen and the next one
    REQUIRE(list->topLevelItemCount() == 2);
    CHECK(rowTexts(*list, 0) == QStringList{QStringLiteral("Chapters for the editor"),
                                            QLocale().toString(QDate(2026, 11, 30), QLocale::ShortFormat),
                                            QStringLiteral("Part I. Desert"),
                                            QStringLiteral("%1 words").arg(number(40000)),
                                            QStringLiteral("%1 (91%)").arg(number(36400))});
    CHECK(list->currentItem() == list->topLevelItem(0));
    CHECK(list->topLevelItem(0)->font(0).bold());
    CHECK_FALSE(list->topLevelItem(1)->font(0).bold());
    CHECK(child<QPushButton>(tab, "changeDeadline")->isEnabled());

    // Change: the window with the deadline chosen
    whenShown([](QDialog& shown) {
        auto& dialog = dynamic_cast<DeadlineDialog&>(shown);
        auto* name = child<QLineEdit>(dialog, "deadlineName");
        CHECK(name->text() == QStringLiteral("Chapters for the editor"));
        CHECK(child<QComboBox>(dialog, "deadlinePart")->currentText().endsWith(
            QStringLiteral("Part I. Desert")));
        name->setText(QStringLiteral("Part I for the editor"));
        dialog.acceptButton()->click();
    });
    child<QPushButton>(tab, "changeDeadline")->click();
    CHECK(goals.model.goals().deadlines.front().name == QStringLiteral("Part I for the editor"));
    CHECK(list->topLevelItem(0)->text(0) == QStringLiteral("Part I for the editor"));

    // Delete on the list: after asking
    whenShown([](QDialog& shown) {
        auto& message = dynamic_cast<gui::dialogs::MessageDialog&>(shown);
        message.acceptButton()->click();
    });
    list->setFocus();
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(list, &press);
    REQUIRE(goals.model.goals().deadlines.size() == 1);
    CHECK(goals.model.goals().deadlines.front().name == QStringLiteral("For the publisher"));
    CHECK(list->topLevelItemCount() == 1);
}

TEST_CASE("Writing goals: Tools > Writing Goals and Deadlines opens one tab in the middle",
          "[gui][statistics][goals]")
{
    QStatusBar statusBar;
    QMainWindow window;
    gui::registerAllCommands(gui::CommandCallbacks{});
    auto* docks = new gui::DockCoordinator(&window, &window);
    docks->createDocks();
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QString opened;
    gui::WritingStatsCoordinator coordinator(
        docks, nullptr, [&opened](const QString& page) { opened = page; }, nullptr,
        dir.filePath(QStringLiteral("journal.db")));
    coordinator.connectCommands();
    window.show();

    QTabWidget* tabs = docks->centralTabs();
    const int before = tabs->count();
    auto& registry = gui::CommandRegistry::getInstance();
    REQUIRE(registry.executeCommand("tools.writingGoals") == gui::CommandExecutionResult::Success);
    REQUIRE(tabs->count() == before + 1);
    auto* tab = qobject_cast<WritingGoalsTab*>(tabs->currentWidget());
    REQUIRE(tab != nullptr);
    CHECK(tabs->tabText(tabs->currentIndex()) == QStringLiteral("Writing Goals and Deadlines"));
    CHECK(tab->property("tabIconId").toString() == QStringLiteral("tools.writingGoals"));

    // Again: the same tab, brought to the front
    tabs->setCurrentIndex(0);
    REQUIRE(registry.executeCommand("tools.writingGoals") == gui::CommandExecutionResult::Success);
    CHECK(tabs->count() == before + 1);
    CHECK(tabs->currentWidget() == tab);

    // Statistics Settings: how the words count toward the goals
    child<QPushButton>(*tab, "statsSettings")->click();
    CHECK(opened == QString::fromLatin1(gui::WritingStatsCoordinator::GOALS_SETTINGS_PAGE));
}

TEST_CASE("Writing goals: the page fills the whole width of the window",
          "[gui][statistics][goals]")
{
    Goals goals;
    goals.openBook();
    WritingGoalsTab tab(&goals.model);
    tab.setBookSource(Goals::source());
    for (const int width : {700, 1900}) {
        tab.resize(width, 800);
        tab.show();
        QApplication::processEvents();
        // The intro over the whole page reaches near the right edge, on a wide window too
        const auto* intro = tab.findChild<QWidget*>(QStringLiteral("goalsIntro"));
        REQUIRE(intro != nullptr);
        INFO("width " << width << ", intro " << intro->width());
        CHECK(intro->mapTo(&tab, QPoint(intro->width(), 0)).x() >= width - 60);
    }
}
