// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_settings_page.h
/// @brief Settings > Timelines: new timelines, the cards of the events, how the times are
/// written and the colors

#ifndef KALAHARI_TIMELINE_TIMELINE_SETTINGS_PAGE_H
#define KALAHARI_TIMELINE_TIMELINE_SETTINGS_PAGE_H

#include <kalahari/gui/settings/settings_page.h>

namespace kalahari::timeline {

/// @brief Settings > Timelines
///
/// The open timelines follow the settings at once (TimelineEditor subscribes to them), the
/// colors through the theme. The colors of the threads are shared with the mind maps; the
/// colors of the characters and the places come from their cards.
class TimelineSettingsPage : public gui::SettingsPage {
    Q_OBJECT

public:
    explicit TimelineSettingsPage(QWidget* parent = nullptr);
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_SETTINGS_PAGE_H
