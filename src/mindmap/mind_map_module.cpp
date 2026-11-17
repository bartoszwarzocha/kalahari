// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_module.cpp
/// @brief The module of the mind maps: what the program knows of it

#include <kalahari/mindmap/mind_map_module.h>

#include <kalahari/core/settings_schema.h>
#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_editor.h>
#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_settings_page.h>

#include <QCoreApplication>
#include <QJsonDocument>

#include <memory>

namespace kalahari::mindmap {

std::string MindMapModule::getPluginId() const {
    return "kalahari.mindmaps";
}

std::string MindMapModule::getVersion() const {
    return "1.0.0";
}

void MindMapModule::onInit() {
    // Nothing to prepare: the editors come with their tabs
}

void MindMapModule::onActivate() {
    // Always active while the program has the module
}

QString MindMapModule::editorId() const {
    return QString::fromLatin1(EDITOR_ID);
}

QString MindMapModule::fileSuffix() const {
    return QString::fromLatin1(FILE_SUFFIX);
}

QByteArray MindMapModule::newFile() const {
    MindMap map;
    map.setLayout(MindMapLook::newLayout());
    return QJsonDocument(map.toJson()).toJson(QJsonDocument::Indented);
}

QWidget* MindMapModule::createEditor(const core::WindowEditorContext& context,
                                     QWidget* parent) {
    return new MindMapEditor(context, parent);
}

QString MindMapModule::settingsTitle() const {
    //: The page of Settings of the mind maps. In Polish: Mapy myśli
    return QCoreApplication::translate("kalahari::mindmap::MindMapModule", "Mind Maps");
}

QWidget* MindMapModule::createSettingsPage() {
    return new MindMapSettingsPage();
}

void registerMindMapModule() {
    core::settings_schema::addDefaults(MindMapSettings::defaults());
    core::ExtensionPointRegistry::getInstance().registerPlugin(std::make_shared<MindMapModule>());
}

}  // namespace kalahari::mindmap
