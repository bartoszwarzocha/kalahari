/// @file test_writing_awards_tab.cpp
/// @brief The badges and challenges in the window: what the tracker wins and says, the tab
/// Challenges and Badges, Add Challenge, the view Achievements of the Statistics panel, the
/// messages at the bottom, the status bar and the summary of the session

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/project_database.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/writing_journal.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/challenge_dialog.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/session_summary_dialog.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/panels/writing_awards_tab.h"
#include "kalahari/gui/panels/writing_stats_panel.h"
#include "kalahari/gui/widgets/badge_painter.h"
#include "kalahari/gui/widgets/info_bar.h"
#include "kalahari/gui/widgets/stats_painter.h"
#include "kalahari/gui/widgets/status_bar_writing.h"
#include "kalahari/gui/writing_awards_tracker.h"
#include "kalahari/gui/writing_stats_coordinator.h"
#include "kalahari/gui/writing_stats_model.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateEdit>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>
#include <functional>

using namespace kalahari;
using core::BadgeKind;
using core::ChallengeKind;
using core::EarnedBadge;
using core::WritingChallenge;
using gui::WritingAwardsTab;
using gui::WritingAwardsTracker;
using gui::WritingStatsModel;
using gui::WritingStatsPanel;
using gui::stats_painter::number;

namespace {

/// Thursday, 12 November 2026, half past nine in the evening
const QDateTime NOW(QDate(2026, 11, 12), QTime(21, 30));
const QDate TODAY = NOW.date();

/// The settings the tests count on, as they are by default; put back when the test ends
struct DefaultSettings {
    DefaultSettings() { apply(); }
    ~DefaultSettings()
    {
        try {
            apply();
        } catch (...) {
            // Left as the test made them; the next tests set what they need
            std::fputs("The statistics settings could not be put back\n", stderr);
        }
    }
    DefaultSettings(const DefaultSettings&) = delete;
    DefaultSettings& operator=(const DefaultSettings&) = delete;

    static void apply()
    {
        auto& settings = core::SettingsManager::getInstance();
        settings.set<int>("stats.dailyGoal", 1000);
        settings.set<int>("stats.weeklyGoal", 0);
        settings.set<int>("stats.writingDays", 31);
        settings.set<bool>("stats.badges", true);
        settings.set<bool>("stats.challenges", false);
        settings.set<bool>("stats.notify.dailyGoal", true);
        settings.set<bool>("stats.notify.weeklyGoal", true);
        settings.set<bool>("stats.notify.deadline", true);
        settings.set<bool>("stats.statusBar", true);
        settings.set<bool>("stats.sessionSummary", true);
        settings.set<bool>("stats.countPasted", false);
        settings.set<bool>("stats.panel.awards", true);
    }
};

core::WritingActivity entry(const QDate& date, int hour, int written)
{
    core::WritingActivity activity;
    activity.date = date;
    activity.slot = core::activitySlotOf(QTime(hour, 0));
    activity.bookId = QStringLiteral("book");
    activity.elementId = QStringLiteral("ch-1");
    activity.written = written;
    activity.activeMs = 5 * 60 * 1000;
    return activity;
}

/// The writer's journal of the tests and a model of it at NOW: 1,200 words in the morning of
/// each writing day from Monday 2 November to Wednesday 11, and 1,200 today, at nine in the
/// evening: 10,800 words on nine writing days in a row
struct Journal {
    DefaultSettings defaults;
    QTemporaryDir dir;
    WritingStatsModel model;

    explicit Journal(int todayWords = 1200)
    {
        REQUIRE(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("journal.db"));
        {
            core::WritingJournal journal;
            REQUIRE(journal.open(path));
            QList<core::WritingActivity> record;
            for (const int day : {2, 3, 4, 5, 6, 9, 10, 11}) {
                record.append(entry(QDate(2026, 11, day), 9, 1200));
            }
            if (todayWords > 0) {
                record.append(entry(TODAY, 21, todayWords));
            }
            REQUIRE(journal.add(record));
            journal.close();
        }
        model.setClock([]() { return NOW; });
        REQUIRE(model.openJournal(path));
    }
};

bool hasBadge(const QList<EarnedBadge>& badges, BadgeKind kind, int level)
{
    return std::any_of(badges.cbegin(), badges.cend(), [&](const EarnedBadge& badge) {
        return badge.kind == kind && badge.level == level;
    });
}

template <typename T>
T* child(const QWidget& parent, const char* name)
{
    auto* found = parent.findChild<T*>(QString::fromLatin1(name));
    REQUIRE(found != nullptr);
    return found;
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

/// Press the button that does the job, as a person does
void pressAccept(QDialog& dialog)
{
    if (auto* window = qobject_cast<gui::dialogs::KalahariDialog*>(&dialog)) {
        window->acceptButton()->click();
    } else {
        dialog.accept();
    }
}

/// The mnemonic letters of the buttons of @p widget
QStringList accessKeys(const QWidget& widget)
{
    QStringList keys;
    for (const QAbstractButton* button : widget.findChildren<QAbstractButton*>()) {
        const QString text = button->text();
        for (qsizetype i = 0; i + 1 < text.size(); ++i) {
            if (text[i] == QLatin1Char('&')) {
                if (text[i + 1] != QLatin1Char('&')) {
                    keys << QString(text[i + 1].toUpper());
                    break;
                }
                ++i;
            }
        }
    }
    return keys;
}

}  // namespace

// =============================================================================
// The tracker
// =============================================================================

TEST_CASE("Writing awards: the badges won are kept once and said once",
          "[gui][statistics][awards]")
{
    Journal journal;
    WritingAwardsTracker tracker(&journal.model);
    // How far the badges are is known at once, before they are counted
    CHECK(tracker.progressOf(BadgeKind::Words).value == 10800);
    CHECK_FALSE(tracker.counted());

    QList<QList<EarnedBadge>> said;
    QObject::connect(&tracker, &WritingAwardsTracker::badgesWon,
                     [&said](const QList<EarnedBadge>& badges) { said.append(badges); });
    tracker.evaluate();
    CHECK(tracker.counted());
    REQUIRE(said.size() == 1);
    const QList<EarnedBadge>& won = said.first();
    CHECK(hasBadge(won, BadgeKind::Words, 0));
    CHECK(hasBadge(won, BadgeKind::WritingDays, 0));
    CHECK(hasBadge(won, BadgeKind::Streak, 0));
    CHECK_FALSE(hasBadge(won, BadgeKind::Streak, 1));
    CHECK_FALSE(hasBadge(won, BadgeKind::DailyGoals, 0));  // 9 days of 10
    // 10,800 of the plan of November, 21 writing days of 1,000 words: 51%
    CHECK(hasBadge(won, BadgeKind::MonthWords, 0));
    CHECK_FALSE(hasBadge(won, BadgeKind::MonthWords, 1));
    // The week of 2 November had 6,000 words of 5,000
    CHECK(hasBadge(won, BadgeKind::MonthWeeks, 0));
    // The month's share of days with the goal comes when the month is over
    CHECK_FALSE(hasBadge(won, BadgeKind::MonthGoalDays, 0));
    for (const EarnedBadge& badge : won) {
        CHECK(badge.date == TODAY);
    }

    CHECK(tracker.earned(BadgeKind::Streak, 0).has_value());
    CHECK_FALSE(tracker.earned(BadgeKind::Streak, 1).has_value());
    CHECK(tracker.shownBadges().size() == won.size());
    REQUIRE(tracker.lastBadge().has_value());
    CHECK(tracker.lastBadge()->date == TODAY);
    // Kept in the journal: the writer's and the month's
    CHECK(journal.model.journal().badges().size() == won.size());

    // Nothing new: nothing said
    tracker.evaluate();
    CHECK(said.size() == 1);

    // Another tracker reads them from the journal and wins none again
    WritingAwardsTracker again(&journal.model);
    QObject::connect(&again, &WritingAwardsTracker::badgesWon,
                     [&said](const QList<EarnedBadge>& badges) { said.append(badges); });
    again.evaluate();
    CHECK(said.size() == 1);
    CHECK(again.shownBadges().size() == won.size());
}

TEST_CASE("Writing awards: with the badges off none are won", "[gui][statistics][awards]")
{
    Journal journal;
    core::SettingsManager::getInstance().set<bool>("stats.badges", false);
    WritingAwardsTracker tracker(&journal.model);
    int said = 0;
    QObject::connect(&tracker, &WritingAwardsTracker::badgesWon, [&said]() { ++said; });
    tracker.evaluate();
    CHECK(said == 0);
    CHECK(tracker.shownBadges().isEmpty());
    CHECK(journal.model.journal().badges().isEmpty());
}

TEST_CASE("Writing awards: the challenges are kept, done once and removed",
          "[gui][statistics][awards]")
{
    Journal journal;
    WritingAwardsTracker tracker(&journal.model);
    tracker.evaluate();
    QList<std::pair<WritingChallenge, int>> done;
    QObject::connect(&tracker, &WritingAwardsTracker::challengeDone,
                     [&done](const WritingChallenge& challenge, int value) {
                         done.append({challenge, value});
                     });

    // A challenge turns the challenges on; 4,800 words since Monday 9 November, of 4,000
    const int id = tracker.addChallenge(
        {0, ChallengeKind::Words, 4000, QDate(2026, 11, 9), QDate(2026, 11, 15), {}});
    REQUIRE(id > 0);
    CHECK(core::SettingsManager::getInstance().get<bool>("stats.challenges", false));
    REQUIRE(done.size() == 1);
    CHECK(done.first().first.id == id);
    CHECK(done.first().second == 4800);
    REQUIRE(tracker.challenges().size() == 1);
    CHECK(tracker.challenges().first().done == TODAY);
    CHECK(tracker.challengeProgress(tracker.challenges().first()).done);
    // Done once
    tracker.evaluate();
    CHECK(done.size() == 1);

    // One not done yet: how far it is
    const int month = tracker.addChallenge(
        {0, ChallengeKind::Words, 50000, QDate(2026, 11, 1), QDate(2026, 11, 30), {}});
    REQUIRE(month > 0);
    CHECK(done.size() == 1);
    const core::ChallengeProgress progress = tracker.challengeProgress(tracker.challenges().last());
    CHECK(progress.value == 10800);
    CHECK(progress.daysLeft == 19);

    // Kept in the journal: another tracker has them
    WritingAwardsTracker again(&journal.model);
    CHECK(again.challenges().size() == 2);

    CHECK(tracker.removeChallenge(id));
    CHECK(tracker.challenges().size() == 1);
    CHECK(journal.model.journal().challenges().size() == 1);
}

TEST_CASE("Writing awards: the daily goal is said when it is reached, not at the start",
          "[gui][statistics][awards]")
{
    Journal journal;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<int>("stats.dailyGoal", 2000);
    journal.model.reloadRules();
    WritingAwardsTracker tracker(&journal.model);
    QList<std::pair<int, int>> reached;
    QObject::connect(&tracker, &WritingAwardsTracker::dailyGoalReached,
                     [&reached](int words, int goal) { reached.append({words, goal}); });
    tracker.evaluate();
    CHECK(reached.isEmpty());

    // A lower goal: reached just now
    settings.set<int>("stats.dailyGoal", 1000);
    journal.model.reloadRules();
    tracker.evaluate();
    REQUIRE(reached.size() == 1);
    CHECK(reached.first() == std::pair<int, int>{1200, 1000});
    // Once a day
    tracker.evaluate();
    CHECK(reached.size() == 1);
}

// =============================================================================
// The texts of the badges
// =============================================================================

TEST_CASE("Writing awards: the texts of the badges", "[gui][statistics][awards]")
{
    Journal journal;
    WritingAwardsTracker tracker(&journal.model);
    namespace look = gui::badge_painter;
    const core::BadgeProgress words = tracker.progressOf(BadgeKind::Words);
    CHECK(look::name(BadgeKind::Words) == QStringLiteral("Words"));
    CHECK(look::thresholdText(words, 0) == QStringLiteral("%1 words").arg(number(10000)));
    CHECK(look::levelName(0) == QStringLiteral("bronze"));
    CHECK(look::levelName(3) == QStringLiteral("platinum"));
    // One has its own text
    CHECK(look::thresholdText(tracker.progressOf(BadgeKind::EarlyBird), 0) ==
          QStringLiteral("1 week"));
    CHECK(look::thresholdText(tracker.progressOf(BadgeKind::FinishedBooks), 0) ==
          QStringLiteral("1 book"));
    CHECK(look::thresholdText(tracker.progressOf(BadgeKind::ChaptersDone), 1) ==
          QStringLiteral("a quarter"));
    CHECK(look::title(tracker.progressOf(BadgeKind::Streak), 1) ==
          QStringLiteral("Streak: 30 days in a row"));
    CHECK(look::valueText(tracker.progressOf(BadgeKind::Streak)) == QStringLiteral("9 days"));

    const QString tip = look::toolTip(tracker.progressOf(BadgeKind::Streak), 1, std::nullopt);
    CHECK(tip.startsWith(QStringLiteral("Streak – 30 days: not yet, 30% (now 9 days).")));
    tracker.evaluate();
    const QString won =
        look::toolTip(tracker.progressOf(BadgeKind::Streak), 0, tracker.earned(BadgeKind::Streak, 0));
    CHECK(won.startsWith(QStringLiteral("Streak – 7 days: won on ")));

    const WritingChallenge challenge{1, ChallengeKind::DaysInRow, 30, TODAY, TODAY.addDays(26), {}};
    CHECK(look::challengeTitle(challenge) == QStringLiteral("30 days in a row"));
    CHECK(look::challengeValue(challenge, 4) == QStringLiteral("4 of 30 days"));
    core::ChallengeProgress progress;
    progress.daysLeft = 27;
    CHECK(look::challengeState(challenge, progress) == QStringLiteral("27 days left"));
    progress.daysLeft = 1;
    CHECK(look::challengeState(challenge, progress) == QStringLiteral("the last day"));
}

// =============================================================================
// The tab Challenges and Badges
// =============================================================================

TEST_CASE("Writing awards: the tab shows the challenges and the badges in three parts",
          "[gui][statistics][awards]")
{
    Journal journal;
    WritingAwardsTracker tracker(&journal.model);
    WritingAwardsTab tab(&tracker, &journal.model);
    tab.resize(900, 700);
    tab.show();
    // The tab counts what it shows
    CHECK(tracker.counted());

    auto* yours = child<QListWidget>(tab, "yourBadges");
    auto* month = child<QListWidget>(tab, "monthBadges");
    auto* book = child<QListWidget>(tab, "bookBadges");
    CHECK(yours->count() == 32);
    CHECK(month->count() == 12);
    // Without a book its badges are not there
    CHECK(book->isHidden());
    CHECK(child<QPushButton>(tab, "startBookOver")->isHidden());
    CHECK_FALSE(child<QLabel>(tab, "bookBadgesNote")->isHidden());

    const int won = int(tracker.shownBadges().size());
    CHECK(child<QLabel>(tab, "badgesWon")->text() == QStringLiteral("Badges won: %1 of 60").arg(won));
    // The badge of a streak of seven days, won, and of thirty, not yet
    QListWidgetItem* streak7 = yours->item(8);
    QListWidgetItem* streak30 = yours->item(9);
    REQUIRE(streak7 != nullptr);
    REQUIRE(streak30 != nullptr);
    CHECK(streak7->text() == QStringLiteral("Streak – 7 days"));
    CHECK(streak7->toolTip().contains(QStringLiteral("won on")));
    CHECK(streak30->toolTip().contains(QStringLiteral("not yet")));

    // The challenges are off until one is added
    auto* note = child<QLabel>(tab, "challengesNote");
    CHECK_FALSE(note->isHidden());
    auto* list = child<QListWidget>(tab, "challenges");
    CHECK(list->isHidden());

    // Add Challenge asks for the window
    int asked = 0;
    QObject::connect(&tab, &WritingAwardsTab::addChallengeRequested, [&asked]() { ++asked; });
    child<QPushButton>(tab, "addChallenge")->click();
    CHECK(asked == 1);

    tracker.addChallenge({0, ChallengeKind::Words, 50000, QDate(2026, 11, 1), QDate(2026, 11, 30), {}});
    REQUIRE(list->count() == 1);
    CHECK_FALSE(list->isHidden());
    CHECK(note->isHidden());
    CHECK(list->item(0)->text().startsWith(QStringLiteral("%1 words").arg(number(50000))));
    CHECK(list->item(0)->text().contains(QStringLiteral("19 days left")));

    // The buttons have keys of their own
    QStringList keys = accessKeys(tab);
    const auto count = keys.size();
    keys.removeDuplicates();
    CHECK(keys.size() == count);

    // Delete removes the chosen challenge, after asking
    auto* remove = child<QPushButton>(tab, "removeChallenge");
    CHECK_FALSE(remove->isEnabled());
    list->setCurrentRow(0);
    CHECK(remove->isEnabled());
    list->setFocus();
    whenShown([](QDialog& dialog) { pressAccept(dialog); });
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(list, &press);
    CHECK(tracker.challenges().isEmpty());
    CHECK(list->count() == 0);
}

TEST_CASE("Writing awards: the badges of the book and Start Over", "[gui][statistics][awards]")
{
    Journal journal;
    core::ProjectDatabase database;
    REQUIRE(database.open(journal.dir.path()));
    journal.model.setBook(&database, QStringLiteral("book"));
    journal.model.setBookWordsSource([]() { return 30000; });
    core::BookGoals goals;
    goals.targetWords = 90000;
    REQUIRE(journal.model.setGoals(goals));

    WritingAwardsTracker tracker(&journal.model);
    tracker.setBookSource({[]() { return std::pair<int, int>{2, 8}; },
                           [](const QString&) { return 0; }});
    tracker.evaluate();
    // A third of the length: the first badge; two chapters of eight: the first and a quarter
    CHECK(tracker.earned(BadgeKind::BookGoal, 0).has_value());
    CHECK_FALSE(tracker.earned(BadgeKind::BookGoal, 1).has_value());
    CHECK(tracker.earned(BadgeKind::ChaptersDone, 0).has_value());
    CHECK(tracker.earned(BadgeKind::ChaptersDone, 1).has_value());
    CHECK_FALSE(tracker.earned(BadgeKind::ChaptersDone, 2).has_value());
    // Kept in the book
    const core::BookBadges kept = core::BookBadges::fromJson(
        database.getSetting(QString::fromLatin1(core::BookBadges::SETTING_KEY)).toString());
    CHECK(kept.earned.size() == 3);

    WritingAwardsTab tab(&tracker, &journal.model);
    tab.setBookTitleSource([]() { return QStringLiteral("Caravan"); });
    tab.show();
    auto* book = child<QListWidget>(tab, "bookBadges");
    CHECK_FALSE(book->isHidden());
    CHECK(book->count() == 16);
    auto* startOver = child<QPushButton>(tab, "startBookOver");
    CHECK_FALSE(startOver->isHidden());
    bool titled = false;
    for (const QGroupBox* group : tab.findChildren<QGroupBox*>()) {
        titled = titled || group->title() == QStringLiteral("Book “Caravan”");
    }
    CHECK(titled);

    // Start Over: from today; those won go to the history of the book and are won again as
    // the book has them now
    whenShown([](QDialog& dialog) { pressAccept(dialog); });
    startOver->click();
    CHECK(tracker.bookBadges().since == TODAY);
    CHECK(tracker.bookBadges().history.size() == 3);
    CHECK_FALSE(child<QLabel>(tab, "bookBadgesNote")->isHidden());
    journal.model.setBook(nullptr, QString());
}

// =============================================================================
// Add Challenge
// =============================================================================

TEST_CASE("Writing awards: the window of Add Challenge", "[gui][statistics][awards]")
{
    gui::dialogs::ChallengeDialog dialog(TODAY);
    auto* kind = child<QComboBox>(dialog, "challengeKind");
    auto* target = child<QSpinBox>(dialog, "challengeTarget");
    auto* end = child<QComboBox>(dialog, "challengeEnd");
    auto* day = child<QDateEdit>(dialog, "challengeEndDay");
    auto* note = child<QLabel>(dialog, "challengeNote");

    // Words by the end of the month, from today
    WritingChallenge challenge = dialog.challenge();
    CHECK(challenge.kind == ChallengeKind::Words);
    CHECK(challenge.target == 5000);
    CHECK(challenge.start == TODAY);
    CHECK(challenge.end == QDate(2026, 11, 30));
    // The field shows the last day of the choice, closed
    CHECK(day->date() == QDate(2026, 11, 30));
    CHECK_FALSE(day->isEnabled());
    CHECK(dialog.acceptButton()->isEnabled());

    // Days in a row by Sunday: four days, too few for seven
    kind->setCurrentIndex(1);
    CHECK(target->value() == 7);
    end->setCurrentIndex(0);
    CHECK(dialog.endDay() == QDate(2026, 11, 15));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK(note->text() == QStringLiteral("There are only 4 days until then."));
    target->setValue(4);
    CHECK(dialog.acceptButton()->isEnabled());

    // A day of one's choice, from the day shown before
    end->setCurrentIndex(2);
    CHECK(day->isEnabled());
    CHECK(day->date() == QDate(2026, 11, 15));
    day->setDate(QDate(2026, 12, 31));
    challenge = dialog.challenge();
    CHECK(challenge.kind == ChallengeKind::DaysInRow);
    CHECK(challenge.target == 4);
    CHECK(challenge.end == QDate(2026, 12, 31));
}

// =============================================================================
// The view Achievements of the Statistics panel
// =============================================================================

TEST_CASE("Writing awards: the view Achievements shows the badge won last",
          "[gui][statistics][awards]")
{
    Journal journal;
    WritingAwardsTracker tracker(&journal.model);
    WritingStatsPanel panel;
    panel.setModel(&journal.model);
    panel.setAwardsTracker(&tracker);
    panel.resize(240, 700);
    panel.show();
    REQUIRE(panel.listedViews().contains(WritingStatsPanel::View::Awards));

    // Choosing the view opens nothing
    int opened = 0;
    int added = 0;
    QObject::connect(&panel, &WritingStatsPanel::awardsRequested, [&opened]() { ++opened; });
    QObject::connect(&panel, &WritingStatsPanel::addChallengeRequested, [&added]() { ++added; });
    panel.setView(WritingStatsPanel::View::Awards);
    CHECK(panel.titleText() == QStringLiteral("Achievements"));
    CHECK(opened == 0);
    QAbstractButton* badge = panel.lastBadgeButton();
    REQUIRE(badge != nullptr);
    CHECK(badge->isVisible());
    CHECK(badge->accessibleName().startsWith(QStringLiteral("No badges yet")));

    tracker.evaluate();
    const int won = int(tracker.shownBadges().size());
    CHECK(badge->accessibleName().contains(QStringLiteral("won on")));
    CHECK(badge->accessibleName().endsWith(QStringLiteral("%1 badges").arg(won)));
    CHECK(panel.listTexts().last() == QStringLiteral("Achievements: %1 badges").arg(won));

    // A click on the badge opens the tab; Add Challenge is on top
    badge->click();
    CHECK(opened == 1);
    child<QPushButton>(panel, "statsAddChallenge")->click();
    CHECK(added == 1);

    // Other views have neither
    panel.setView(WritingStatsPanel::View::Today);
    CHECK_FALSE(badge->isVisible());
    CHECK_FALSE(child<QPushButton>(panel, "statsAddChallenge")->isVisible());
}

// =============================================================================
// The window: the commands, the messages at the bottom and the status bar
// =============================================================================

TEST_CASE("Writing awards: the commands, the tab and the messages at the bottom",
          "[gui][statistics][awards]")
{
    DefaultSettings defaults;
    QMainWindow window;
    gui::registerAllCommands(gui::CommandCallbacks{});
    auto* docks = new gui::DockCoordinator(&window, &window);
    docks->createDocks();
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    gui::WritingStatsCoordinator coordinator(docks, nullptr, [](const QString&) {}, nullptr,
                                             dir.filePath(QStringLiteral("journal.db")));
    coordinator.connectCommands();
    window.show();

    // Tools > Challenges and Badges: one tab in the middle
    QTabWidget* tabs = docks->centralTabs();
    const int before = tabs->count();
    auto& registry = gui::CommandRegistry::getInstance();
    REQUIRE(registry.executeCommand("tools.challenges") == gui::CommandExecutionResult::Success);
    REQUIRE(tabs->count() == before + 1);
    auto* tab = qobject_cast<WritingAwardsTab*>(tabs->currentWidget());
    REQUIRE(tab != nullptr);
    CHECK(tabs->tabText(tabs->currentIndex()) == QStringLiteral("Challenges and Badges"));
    CHECK(tab->property("tabIconId").toString() == QStringLiteral("tools.challenges"));
    tabs->setCurrentIndex(0);
    REQUIRE(registry.executeCommand("tools.challenges") == gui::CommandExecutionResult::Success);
    CHECK(tabs->count() == before + 1);
    CHECK(tabs->currentWidget() == tab);

    // Tools > Add Challenge: the window, then the challenge on the tab
    whenShown([](QDialog& dialog) {
        dialog.findChild<QSpinBox*>(QStringLiteral("challengeTarget"))->setValue(20000);
        dialog.accept();
    });
    REQUIRE(registry.executeCommand("tools.addChallenge") == gui::CommandExecutionResult::Success);
    REQUIRE(coordinator.tracker()->challenges().size() == 1);
    CHECK(coordinator.tracker()->challenges().first().target == 20000);
    CHECK(tabs->currentWidget() == tab);
    CHECK(child<QListWidget>(*tab, "challenges")->count() == 1);

    // The messages of one moment together at the bottom, with Copy
    gui::InfoBar* bar = coordinator.messageBar();
    REQUIRE(bar != nullptr);
    CHECK(bar->isHidden());
    // The signal alone: the journal has nothing this week, so the whole weekly goal is left
    emit coordinator.tracker()->dailyGoalReached(1000, 1000);
    emit coordinator.tracker()->challengeDone(coordinator.tracker()->challenges().first(), 20000);
    QCoreApplication::processEvents();
    CHECK(bar->isVisible());
    const QString expected =
        QStringLiteral("Daily goal reached: %1 of %1 words. Left to the weekly goal: %2 words. "
                       "Challenge done: %3 words (until 30 November).")
            .arg(number(1000), number(5000), number(20000));
    CHECK(bar->message() == expected);
    const QList<QPushButton*> buttons = bar->findChildren<QPushButton*>();
    REQUIRE(buttons.size() == 1);
    CHECK(buttons.first()->text() == QStringLiteral("Copy"));
    buttons.first()->click();
    CHECK(QApplication::clipboard()->text() == expected);

    // A new badge, and the settings that silence the goals
    QList<EarnedBadge> badges{{BadgeKind::Streak, 0, TODAY, QString()}};
    emit coordinator.tracker()->badgesWon(badges);
    QCoreApplication::processEvents();
    CHECK(bar->message() == QStringLiteral("New badge: Streak: 7 days in a row (bronze)."));
    core::SettingsManager::getInstance().set<bool>("stats.notify.weeklyGoal", false);
    emit coordinator.tracker()->weeklyGoalReached(5000, 5000);
    emit coordinator.tracker()->deadlineComing(QStringLiteral("For the editor"), TODAY.addDays(1),
                                               1, 3000, 4000);
    QCoreApplication::processEvents();
    CHECK(bar->message() == QStringLiteral("The deadline “For the editor” is tomorrow (13 November). "
                                           "There are %1 of %2 words.")
                                .arg(number(3000), number(4000)));
}

TEST_CASE("Writing awards: the time of writing and the days in a row in the status bar",
          "[gui][statistics][awards]")
{
    Journal journal;
    QStatusBar statusBar;
    gui::StatusBarWriting labels(&statusBar, &journal.model);
    statusBar.show();
    CHECK(labels.timeLabel()->isVisible());
    CHECK(labels.timeLabel()->text() == QStringLiteral("0:05"));
    CHECK(labels.streakLabel()->text() == QStringLiteral("9 days"));
    CHECK(labels.streakLabel()->toolTip() == QStringLiteral("Days in a row: 9"));

    // The setting reaches the labels through their slot, queued by the settings
    core::SettingsManager::getInstance().set<bool>(gui::StatusBarWriting::SETTING, false);
    QApplication::processEvents();
    CHECK_FALSE(labels.timeLabel()->isVisible());
    CHECK_FALSE(labels.streakLabel()->isVisible());
}

TEST_CASE("Writing awards: the summary of the session", "[gui][statistics][awards]")
{
    Journal journal;
    using gui::dialogs::SessionSummary;
    const SessionSummary summary =
        SessionSummary::of(journal.model, NOW.addSecs(-3 * 3600), QStringLiteral("Caravan"));
    CHECK(summary.writtenSince);
    CHECK(summary.words == 1200);
    CHECK(summary.sessions == 1);
    CHECK(summary.sessionFrom == QTime(21, 0));
    CHECK(summary.sessionTo == QTime(21, 5));
    CHECK(summary.sessionWords == 1200);
    CHECK(summary.streak == 9);
    CHECK(summary.heading() == QStringLiteral("Today: %1 words").arg(number(1200)));
    CHECK(summary.subheading().startsWith(QStringLiteral("Book “Caravan”, ")));
    CHECK(summary.subheading().endsWith(QStringLiteral(", 1 session")));
    CHECK(summary.text().contains(QStringLiteral("Daily goal: 120%")));
    CHECK_FALSE(summary.pieces.isEmpty());

    // Nothing written since the session started: no summary
    CHECK_FALSE(SessionSummary::of(journal.model, NOW.addSecs(-60), QString()).writtenSince);

    gui::dialogs::SessionSummaryDialog dialog(summary);
    child<QPushButton>(dialog, "summaryCopy")->click();
    CHECK(QApplication::clipboard()->text() == summary.text());
    auto* again = dialog.showAgainBox();
    CHECK(again->isChecked());
    again->setChecked(false);
    CHECK_FALSE(core::SettingsManager::getInstance().get<bool>("stats.sessionSummary", true));
}
