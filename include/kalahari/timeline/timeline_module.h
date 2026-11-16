// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_module.h
/// @brief The module of the timelines: what the program knows of it

#ifndef KALAHARI_TIMELINE_TIMELINE_MODULE_H
#define KALAHARI_TIMELINE_TIMELINE_MODULE_H

#include <kalahari/core/window_editor_provider.h>

namespace kalahari::timeline {

/// @brief The editor "timeline" of the window kinds: the files of new timelines, their tabs
/// and Settings > Timelines
class TimelineModule : public core::IWindowEditorProvider {
public:
    static constexpr const char* EDITOR_ID = "timeline";
    static constexpr const char* FILE_SUFFIX = "ktl";

    [[nodiscard]] std::string getPluginId() const override;
    [[nodiscard]] std::string getVersion() const override;
    void onInit() override;
    void onActivate() override;

    [[nodiscard]] QString editorId() const override;
    [[nodiscard]] QString fileSuffix() const override;
    [[nodiscard]] QByteArray newFile() const override;
    [[nodiscard]] QWidget* createEditor(const core::WindowEditorContext& context,
                                        QWidget* parent) override;
    [[nodiscard]] QString settingsTitle() const override;
    [[nodiscard]] QWidget* createSettingsPage() override;
};

/// @brief Register the module of the timelines: its provider and the defaults of its settings
///
/// The program calls it once, before its first window; nothing else of the program names the
/// classes of the module.
void registerTimelineModule();

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_MODULE_H
