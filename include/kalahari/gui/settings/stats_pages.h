// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file stats_pages.h
/// @brief Settings > Statistics and Goals: Bar and Panel, Counting, Notifications and Colors
///
/// Four short pages instead of one long one. Each has a few groups with headings; an option is
/// one row with a short note under it in plain words; the settings that are seldom changed are
/// under the button "Advanced Settings", as on the Grammar page. The goals and the deadlines
/// themselves are not here: they are on the tab Writing Goals and Deadlines.

#ifndef KALAHARI_GUI_SETTINGS_STATS_PAGES_H
#define KALAHARI_GUI_SETTINGS_STATS_PAGES_H

#include "kalahari/gui/settings/settings_page.h"

#include <string>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLayout;
class QToolButton;

namespace kalahari::gui {

/// @brief What the pages of the statistics share: options with notes and the advanced settings
class StatsSettingsPage : public SettingsPage {
    Q_OBJECT

public:
    explicit StatsSettingsPage(QWidget* parent = nullptr);

    /// @brief The button that shows the advanced settings; nullptr on a page without them
    [[nodiscard]] QToolButton* advancedToggle() const { return advancedSettingsButton(); }

    /// @brief The group of the advanced settings; nullptr on a page without them
    [[nodiscard]] QGroupBox* advancedGroup() const { return m_advancedGroup; }

protected:
    /// @brief A check box bound to @p key in @p layout, with a note under it in line with its
    /// text
    QCheckBox* addOption(QLayout* layout, const QString& text, const std::string& key,
                         const QString& note = QString());

    /// @brief A short text on top of the page: what it is about
    QLabel* addIntro(const QString& text);

    /// @brief The button "Advanced Settings" and the group it shows, hidden at first
    ///        (addAdvancedGroup())
    /// @param title The heading of the group
    QFormLayout* addAdvanced(const QString& title);

private:
    QGroupBox* m_advancedGroup{nullptr};
};

/// @brief Settings > Statistics and Goals > Bar and Panel
class StatsBarPage : public StatsSettingsPage {
    Q_OBJECT
public:
    explicit StatsBarPage(QWidget* parent = nullptr);
};

/// @brief Settings > Statistics and Goals > Counting
class StatsCountingPage : public StatsSettingsPage {
    Q_OBJECT
public:
    explicit StatsCountingPage(QWidget* parent = nullptr);

private:
    /// @brief "Cutting to the clippings is not deleting" (the words of the day stay as they
    ///        were) in @p counting, under @p goal, the list of what counts toward the goal: only
    ///        for the words added
    void addClippingsOption(QFormLayout* counting, const QComboBox* goal);
};

/// @brief Settings > Statistics and Goals > Notifications
class StatsNotificationsPage : public StatsSettingsPage {
    Q_OBJECT
public:
    explicit StatsNotificationsPage(QWidget* parent = nullptr);
};

/// @brief Settings > Statistics and Goals > Colors: the colors of the graphs, of the bar of the
/// daily goal and of the pace of work, of each theme of the program, with a preview
class StatsColorsPage : public StatsSettingsPage {
    Q_OBJECT
public:
    explicit StatsColorsPage(QWidget* parent = nullptr);

private:
    /// @brief The group of the colors of one theme
    void addTheme(const QString& themeName);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_SETTINGS_STATS_PAGES_H
