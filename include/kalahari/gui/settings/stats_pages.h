// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file stats_pages.h
/// @brief Settings > Statistics and Goals: Bar and Panel, Counting, Notifications and Colors
///
/// Four short pages instead of one long one. Each has a few groups with headings; an option is
/// one row with a short note under it in plain words; the settings that are seldom changed are
/// under the button "Advanced Settings", as on the Grammar page. The goals and the deadlines
/// themselves are not here: they are on the tab Writing Goals and Deadlines.

#pragma once

#include "kalahari/gui/settings/settings_page.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

class QCheckBox;
class QFormLayout;
class QRadioButton;
class QToolButton;
class QGroupBox;

namespace kalahari::gui {

/// @brief What the pages of the statistics share: choices with notes and the advanced settings
class StatsSettingsPage : public SettingsPage {
    Q_OBJECT

public:
    explicit StatsSettingsPage(QWidget* parent = nullptr);

    /// @brief The button that shows the advanced settings; nullptr on a page without them
    QToolButton* advancedToggle() const { return m_advancedToggle; }

    /// @brief The group of the advanced settings; nullptr on a page without them
    QGroupBox* advancedGroup() const { return m_advancedGroup; }

protected:
    /// @brief One choice of a setting
    struct Choice {
        QString text;          ///< What the radio button says
        nlohmann::json value;  ///< The value of the setting it stands for
        QString note;          ///< A note under it; empty: none
    };

    /// @brief Radio buttons of a setting, under one label of the form, bound to the setting
    /// @param inRow The buttons side by side (without notes)
    /// @return The buttons, in the order of @p choices
    std::vector<QRadioButton*> addChoices(QFormLayout* form, const QString& label,
                                          const std::string& key, std::vector<Choice> choices,
                                          bool inRow = false);

    /// @brief A check box bound to @p key in @p layout, with a note under it in line with its
    /// text
    QCheckBox* addOption(QLayout* layout, const QString& text, const std::string& key,
                         const QString& note = QString());

    /// @brief A note under the control of the last row of @p form, in its column
    QLabel* addFieldNote(QFormLayout* form, const QString& text);

    /// @brief A short text on top of the page: what it is about
    QLabel* addIntro(const QString& text);

    /// @brief The button "Advanced Settings" and the group it shows, hidden at first
    /// @param title The heading of the group
    QFormLayout* addAdvanced(const QString& title);

private:
    QToolButton* m_advancedToggle{nullptr};
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
