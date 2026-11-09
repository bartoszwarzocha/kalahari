/// @file test_stats_pages.cpp
/// @brief Settings > Statistics and Goals: Bar and Panel, Counting, Notifications and Colors

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/writing_statistics.h"
#include "kalahari/gui/settings/stats_pages.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/color_config_widget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>

using namespace kalahari;
using gui::SettingsDialog;

namespace {

template <typename T>
T* withText(QWidget& page, const QString& text)
{
    const QList<T*> all = page.findChildren<T*>();
    const auto found = std::find_if(all.begin(), all.end(), [&](T* widget) {
        QString shown = widget->text();
        shown.remove(QLatin1Char('&'));
        return shown == text;
    });
    return found != all.end() ? *found : nullptr;
}

std::vector<std::string> sorted(std::vector<std::string> keys)
{
    std::sort(keys.begin(), keys.end());
    return keys;
}

}  // namespace

TEST_CASE("Statistics settings: four pages under Statistics and Goals", "[gui][settings][statistics]")
{
    SettingsDialog dialog(nullptr);
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    const QList<QTreeWidgetItem*> found =
        tree->findItems(QStringLiteral("Statistics and Goals"), Qt::MatchExactly);
    REQUIRE(found.size() == 1);
    QTreeWidgetItem* category = found.first();
    REQUIRE(category->childCount() == 4);
    CHECK(category->child(0)->text(0) == QStringLiteral("Bar and Panel"));
    CHECK(category->child(1)->text(0) == QStringLiteral("Counting"));
    CHECK(category->child(2)->text(0) == QStringLiteral("Notifications"));
    CHECK(category->child(3)->text(0) == QStringLiteral("Colors"));
    for (const char* page : {SettingsDialog::STATS_BAR_PAGE, SettingsDialog::STATS_COUNTING_PAGE,
                             SettingsDialog::STATS_NOTIFICATIONS_PAGE,
                             SettingsDialog::STATS_COLORS_PAGE}) {
        INFO(page);
        CHECK(dialog.openPage(QString::fromLatin1(page)));
    }
}

TEST_CASE("Statistics settings: Bar and Panel writes only what was changed",
          "[gui][settings][statistics]")
{
    gui::StatsBarPage page;
    page.load();
    CHECK_FALSE(page.isChanged());
    auto* line = withText<QRadioButton>(page, QStringLiteral("One line"));
    REQUIRE(line != nullptr);
    CHECK_FALSE(line->isChecked());
    line->setChecked(true);
    auto* streak = withText<QCheckBox>(page, QStringLiteral("Days in a row"));
    REQUIRE(streak != nullptr);
    streak->setChecked(false);
    CHECK(page.isChanged());
    CHECK(sorted(page.apply()) ==
          std::vector<std::string>{"stats.bar.mode", "stats.bar.showStreak"});
    auto& settings = core::SettingsManager::getInstance();
    CHECK(settings.get<std::string>("stats.bar.mode") == "line");
    CHECK_FALSE(settings.get<bool>("stats.bar.showStreak"));

    // The graph is never lower than 83 pixels
    auto* height = page.findChild<QSpinBox*>();
    REQUIRE(height != nullptr);
    CHECK(height->minimum() == 83);

    // The details of the graph are hidden until asked for
    REQUIRE(page.advancedToggle() != nullptr);
    CHECK(page.advancedGroup()->isHidden());
    page.advancedToggle()->click();
    CHECK_FALSE(page.advancedGroup()->isHidden());
}

TEST_CASE("Statistics settings: Counting", "[gui][settings][statistics]")
{
    gui::StatsCountingPage page;
    page.load();
    auto* written = withText<QRadioButton>(page, QStringLiteral("All the words written"));
    auto* openBook = withText<QRadioButton>(page, QStringLiteral("Words of the open book only"));
    auto* goal = withText<QRadioButton>(page, QStringLiteral("reach the daily goal"));
    REQUIRE(written != nullptr);
    REQUIRE(openBook != nullptr);
    REQUIRE(goal != nullptr);
    CHECK_FALSE(written->isChecked());
    CHECK_FALSE(openBook->isChecked());
    written->setChecked(true);
    openBook->setChecked(true);
    goal->setChecked(true);

    // The text pasted is not writing until the writer says so
    auto* pasted = withText<QCheckBox>(page, QStringLiteral("Count pasted text in the statistics"));
    REQUIRE(pasted != nullptr);
    CHECK_FALSE(pasted->isChecked());
    pasted->setChecked(true);

    // The end of the writing day, as a time of the clock
    auto* dayEnd = page.findChild<QTimeEdit*>();
    REQUIRE(dayEnd != nullptr);
    CHECK(dayEnd->time() == QTime(4, 0));
    dayEnd->setTime(QTime(3, 30));

    CHECK(sorted(page.apply()) == std::vector<std::string>{"stats.countPasted",
                                                           "stats.dailyGoalAllBooks",
                                                           "stats.dayStart",
                                                           "stats.goalCounting",
                                                           "stats.streakRule"});
    const core::WritingRules rules = core::WritingRules::fromSettings();
    CHECK(rules.countPasted);
    CHECK(rules.counting == core::GoalCounting::Written);
    CHECK_FALSE(rules.allBooks);
    CHECK(rules.streakRule == core::StreakRule::DailyGoal);
    CHECK(rules.dayStartMinutes == 210);
}

TEST_CASE("Statistics settings: Notifications", "[gui][settings][statistics]")
{
    gui::StatsNotificationsPage page;
    page.load();
    auto* deadline = withText<QCheckBox>(page, QStringLiteral("A deadline is coming,"));
    auto* challenges = withText<QCheckBox>(page, QStringLiteral("Challenges"));
    REQUIRE(deadline != nullptr);
    REQUIRE(challenges != nullptr);
    CHECK(deadline->isChecked());
    CHECK_FALSE(challenges->isChecked());
    CHECK(page.advancedToggle() == nullptr);
    deadline->setChecked(false);
    challenges->setChecked(true);
    CHECK(sorted(page.apply()) ==
          std::vector<std::string>{"stats.challenges", "stats.notify.deadline"});
}

TEST_CASE("Statistics settings: the colors of each theme, kept only when changed",
          "[gui][settings][statistics]")
{
    gui::StatsColorsPage page;
    page.load();
    QGroupBox* dark = nullptr;
    for (QGroupBox* group : page.findChildren<QGroupBox*>()) {
        if (group->title() == QStringLiteral("Colors of the Dark Theme")) {
            dark = group;
        }
    }
    REQUIRE(dark != nullptr);
    const QList<gui::ColorConfigWidget*> colors = dark->findChildren<gui::ColorConfigWidget*>();
    REQUIRE(colors.size() == 13);
    // The colors of the theme file
    CHECK(colors[3]->color() == QColor(QStringLiteral("#4CB0E8")));
    colors[3]->setColor(QColor(QStringLiteral("#123456")));
    auto* fill = dark->findChild<QSpinBox*>();
    REQUIRE(fill != nullptr);
    CHECK(fill->value() == 22);
    CHECK(page.apply() == std::vector<std::string>{"themes.Dark.editor.statsLine"});
    auto& settings = core::SettingsManager::getInstance();
    CHECK(settings.get<std::string>("themes.Dark.editor.statsLine", "") == "#123456");

    // Back to the theme's own color: nothing is kept
    page.load();
    colors[3]->setColor(QColor(QStringLiteral("#4CB0E8")));
    page.apply();
    CHECK_FALSE(settings.hasKey("themes.Dark.editor.statsLine"));
}
