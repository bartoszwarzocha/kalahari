// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_module.h
/// @brief The module of the mind maps: what the program knows of it

#ifndef KALAHARI_MINDMAP_MIND_MAP_MODULE_H
#define KALAHARI_MINDMAP_MIND_MAP_MODULE_H

#include <kalahari/core/window_editor_provider.h>

namespace kalahari::mindmap {

/// @brief The editor "mindmap" of the window kinds: the files of new maps, their tabs,
/// Settings > Mind Maps, New Relation Map... and the offer of a map of relations after the
/// first relation of a card
class MindMapModule : public core::IWindowEditorProvider {
public:
    static constexpr const char* EDITOR_ID = "mindmap";
    static constexpr const char* FILE_SUFFIX = "kmap";

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
    /// @brief New Relation Map... (NEW_RELATION_MAP)
    [[nodiscard]] std::vector<core::WindowEditorCommand> commands() const override;
    /// @brief After the first relation of a card: the offer of a map of relations
    [[nodiscard]] QWidget* relationAdded(const core::WindowEditorContext& context,
                                         const QString& cardId, const QString& otherId,
                                         const QString& relationId, QWidget* parent) override;

    /// The id of New Relation Map...
    static constexpr const char* NEW_RELATION_MAP = "book.newRelationMap";
};

/// @brief Register the module of the mind maps: its provider and the defaults of its settings
///
/// The program calls it once, before its first window; nothing else of the program names the
/// classes of the module.
void registerMindMapModule();

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_MODULE_H
