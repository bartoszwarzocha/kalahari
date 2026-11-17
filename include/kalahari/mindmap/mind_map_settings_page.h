// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_settings_page.h
/// @brief Settings > Mind Maps: new maps, the look of the maps, pasting and the colors

#ifndef KALAHARI_MINDMAP_MIND_MAP_SETTINGS_PAGE_H
#define KALAHARI_MINDMAP_MIND_MAP_SETTINGS_PAGE_H

#include <kalahari/gui/settings/settings_page.h>

namespace kalahari::mindmap {

/// @brief Settings > Mind Maps
///
/// The open maps follow the settings at once (MindMapEditor subscribes to them), the colors
/// through the theme. The colors of the branches are shared with the timelines.
class MindMapSettingsPage : public gui::SettingsPage {
    Q_OBJECT

public:
    explicit MindMapSettingsPage(QWidget* parent = nullptr);
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_SETTINGS_PAGE_H
