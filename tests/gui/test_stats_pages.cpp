// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_stats_pages.cpp
/// @brief Settings > Statistics and Goals: Bar and Panel, Counting, Notifications and Colors

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/writing_statistics.h"
#include "kalahari/gui/settings/stats_pages.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/gui/widgets/stats_painter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QSpinBox>
#include <QTimeEdit>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include "test_support/window_parts.h"

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

/// Choose the item of @p list with value @p value, as the writer does
void choose(QComboBox* list, const QVariant& value)
{
    const int index = list->findData(value);
    REQUIRE(index >= 0);
    list->setCurrentIndex(index);
}

std::vector<std::string> sorted(std::vector<std::string> keys)
{
    std::sort(keys.begin(), keys.end());
    return keys;
}

}  // namespace

TEST_CASE("Statistics settings: four pages under Statistics and Goals", "[gui][settings][statistics]")
{
    SettingsDialog dialog(kalahari::test::book(), kalahari::test::commands(), nullptr);
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
    auto* look = page.findChild<QComboBox*>(QStringLiteral("statsBarLook"));
    REQUIRE(look != nullptr);
    CHECK(look->currentData().toString() != QStringLiteral("line"));
    choose(look, QStringLiteral("line"));
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
    REQUIRE(page.advancedGroup() != nullptr);
    CHECK_FALSE(page.advancedGroup()->isVisibleTo(&page));
    page.advancedToggle()->click();
    CHECK(page.advancedGroup()->isVisibleTo(&page));
}

TEST_CASE("Statistics settings: Counting", "[gui][settings][statistics]")
{
    gui::StatsCountingPage page;
    page.load();
    auto* counted = page.findChild<QComboBox*>(QStringLiteral("statsGoalCounting"));
    auto* books = page.findChild<QComboBox*>(QStringLiteral("statsDailyGoalBooks"));
    auto* rule = page.findChild<QComboBox*>(QStringLiteral("statsStreakRule"));
    auto* minimum = page.findChild<QSpinBox*>(QStringLiteral("statsStreakMinimum"));
    REQUIRE(counted != nullptr);
    REQUIRE(books != nullptr);
    REQUIRE(rule != nullptr);
    REQUIRE(minimum != nullptr);
    CHECK(counted->currentData().toString() != QStringLiteral("written"));
    CHECK(books->currentData().toBool());
    CHECK(minimum->isEnabled() ==
          (rule->currentData().toString() == QStringLiteral("minimumWords")));
    choose(counted, QStringLiteral("written"));
    choose(books, false);
    choose(rule, QStringLiteral("dailyGoal"));
    // The words a day wait while the daily goal decides
    CHECK_FALSE(minimum->isEnabled());

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
    auto* deadline = withText<QCheckBox>(page, QStringLiteral("A deadline is coming"));
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
    auto& settings = core::SettingsManager::getInstance();
    const std::string iconColor = "icons.themes.Dark.colorPrimary";
    settings.removeKey(iconColor);
    const auto darkColors = [](gui::StatsColorsPage& page) {
        QGroupBox* dark = nullptr;
        for (QGroupBox* group : page.findChildren<QGroupBox*>()) {
            if (group->title() == QStringLiteral("Colors of the Dark Theme")) {
                dark = group;
            }
        }
        REQUIRE(dark != nullptr);
        return dark;
    };
    gui::StatsColorsPage page;
    page.load();
    QGroupBox* dark = darkColors(page);
    const QList<gui::ColorConfigWidget*> colors = dark->findChildren<gui::ColorConfigWidget*>();
    REQUIRE(colors.size() == 17);
    // The line and the times of writing take the theme's color (#FFAA00 of the Dark theme),
    // the grid and the frame a share of it over the background; the others are the theme file's
    const QColor orange(QStringLiteral("#FFAA00"));
    const QColor background(QStringLiteral("#1A1A1A"));
    CHECK(colors[0]->color() == background);
    CHECK(colors[3]->color() == orange);
    CHECK(colors[11]->color() == orange);
    CHECK(colors[1]->color() == gui::StatsColors::themeDefault("statsGrid", orange, background));
    CHECK(colors[1]->color() != orange);
    CHECK(colors[1]->color() != background);
    CHECK(colors[4]->color() == QColor(QStringLiteral("#E3B341")));
    CHECK(gui::StatsColors::followsTheme("statsFrame"));
    CHECK_FALSE(gui::StatsColors::followsTheme("statsGoal"));
    CHECK_FALSE(gui::StatsColors::themeDefault("statsGoal", orange, background).isValid());

    colors[3]->setColor(QColor(QStringLiteral("#123456")));
    auto* fill = dark->findChild<QSpinBox*>();
    REQUIRE(fill != nullptr);
    CHECK(fill->value() == 22);
    CHECK(page.apply() == std::vector<std::string>{"themes.Dark.editor.statsLine"});
    CHECK(settings.get<std::string>("themes.Dark.editor.statsLine", "") == "#123456");

    // Back to the theme's own color: nothing is kept
    page.load();
    colors[3]->setColor(orange);
    page.apply();
    CHECK_FALSE(settings.hasKey("themes.Dark.editor.statsLine"));

    // The writer's color of the icons is the theme's color of the graphs
    settings.setIconColorPrimaryForTheme("Dark", "#00aaff");
    gui::StatsColorsPage another;
    another.load();
    const QList<gui::ColorConfigWidget*> followed =
        darkColors(another)->findChildren<gui::ColorConfigWidget*>();
    REQUIRE(followed.size() == 17);
    CHECK(followed[3]->color() == QColor(QStringLiteral("#00AAFF")));
    CHECK(followed[2]->color() == gui::StatsColors::themeDefault(
                                      "statsFrame", QColor(QStringLiteral("#00AAFF")), background));
    CHECK(another.apply().empty());
    settings.removeKey(iconColor);
}

TEST_CASE("Statistics colors: the grid, the frame and the lines follow the theme's color",
          "[gui][settings][statistics]")
{
    auto& themes = core::ThemeManager::getInstance();
    auto& settings = core::SettingsManager::getInstance();
    const core::Theme& theme = themes.getCurrentTheme();
    const std::string line = core::ThemeManager::editorColorSetting(theme.name, "statsLine");
    settings.removeKey(line);
    QCoreApplication::processEvents();

    gui::StatsColors colors = gui::StatsColors::current();
    CHECK(colors.line == theme.colors.primary);
    CHECK(colors.hoursHigh == theme.colors.primary);
    CHECK(colors.grid ==
          gui::StatsColors::themeDefault("statsGrid", theme.colors.primary, colors.background));
    CHECK(colors.frame ==
          gui::StatsColors::themeDefault("statsFrame", theme.colors.primary, colors.background));

    // The writer's own color of the line, shown at once where the statistics are
    int colorChanges = 0;
    QObject guard;
    QObject::connect(&themes, &core::ThemeManager::editorColorsChanged, &guard,
                     [&colorChanges]() { ++colorChanges; });
    settings.set<std::string>(line, "#123456");
    QCoreApplication::processEvents();
    CHECK(colorChanges == 1);
    colors = gui::StatsColors::current();
    CHECK(colors.line == QColor(QStringLiteral("#123456")));
    CHECK(colors.grid ==
          gui::StatsColors::themeDefault("statsGrid", theme.colors.primary, colors.background));

    // A color of another theme changes nothing here
    const std::string other = theme.name == "Dark" ? "Light" : "Dark";
    settings.set<std::string>(core::ThemeManager::editorColorSetting(other, "statsLine"),
                              "#654321");
    QCoreApplication::processEvents();
    CHECK(colorChanges == 1);
    settings.removeKey(core::ThemeManager::editorColorSetting(other, "statsLine"));

    settings.removeKey(line);
    QCoreApplication::processEvents();
    CHECK(colorChanges == 2);
    CHECK(gui::StatsColors::current().line == theme.colors.primary);
}
