// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_module.cpp
/// @brief The module of the mind maps: what the program knows of it

#include <kalahari/mindmap/mind_map_module.h>

#include <kalahari/core/icon_registry.h>
#include <kalahari/core/settings_schema.h>
#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_editor.h>
#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_relation_map.h>
#include <kalahari/mindmap/mind_map_settings_page.h>

#include <QCoreApplication>
#include <QJsonDocument>

#include <array>
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

QWidget* MindMapModule::createEditor(const core::WindowEditorContext& context, QWidget* parent) {
    return new MindMapEditor(context, parent);
}

QString MindMapModule::settingsTitle() const {
    //: The page of Settings of the mind maps. In Polish: Mapy myśli
    return QCoreApplication::translate("kalahari::mindmap::MindMapModule", "Mind Maps");
}

QWidget* MindMapModule::createSettingsPage() {
    return new MindMapSettingsPage();
}

std::vector<core::WindowEditorCommand> MindMapModule::commands() const {
    core::WindowEditorCommand command;
    command.id = NEW_RELATION_MAP;
    //: The command of the Book menu and of the Navigator that makes a map of the relations of
    //: the cards. In Polish: Nowa mapa &relacji...
    command.menuText = QCoreApplication::translate("kalahari::mindmap::MindMapModule",
                                                   "New &Relation Map...");
    command.menuOrder = 65;  // After New Mind Map... (60)
    command.run = [](const core::WindowEditorContext& context, QWidget* parent) {
        askNewRelationMap(context, {}, parent);
    };
    return {command};
}

QWidget* MindMapModule::relationAdded(const core::WindowEditorContext& context,
                                      const QString& cardId, const QString& otherId,
                                      const QString& relationId, QWidget* parent) {
    return offerRelationMap(context, cardId, otherId, relationId, parent);
}

namespace {

/// The icons of the mind maps: the kinds of the nodes and the commands
void registerIcons() {
    struct Icon {
        const char* id;
        const char* file;
        const char* label;
    };
    static constexpr std::array<Icon, 26> ICONS{{
        {MindMapIcons::KIND_TOPIC, "hub", "Mind Map: Main Topic"},
        {MindMapIcons::KIND_IDEA, "lightbulb", "Mind Map: Idea"},
        {MindMapIcons::KIND_QUESTION, "help_outline", "Mind Map: Question"},
        {MindMapIcons::KIND_NOTE, "sticky_note_2", "Mind Map: Note"},
        {MindMapIcons::KIND_SCENE, "theaters", "Mind Map: Scene"},
        {MindMapIcons::KIND_THREAD, "timeline", "Mind Map: Thread"},
        {MindMapIcons::KIND_ELEMENT, "description", "Mind Map: Element of the Book"},
        {MindMapIcons::ADD, "add", "Mind Map: Add Node"},
        {MindMapIcons::BRANCH, "account_tree", "Mind Map: Add Branch"},
        {MindMapIcons::BELOW, "view_agenda", "Mind Map: Add Below"},
        {MindMapIcons::RENAME, "edit", "Mind Map: Edit Name"},
        {MindMapIcons::FOLD, "unfold_less", "Mind Map: Fold"},
        {MindMapIcons::UNFOLD, "unfold_more", "Mind Map: Unfold"},
        {MindMapIcons::FRAME, "select_all", "Mind Map: Frame"},
        {MindMapIcons::ARRANGE, "view_quilt", "Mind Map: Arrange"},
        {MindMapIcons::REMOVE, "delete", "Mind Map: Delete"},
        {MindMapIcons::CONNECT, "link", "Mind Map: Connect To"},
        {MindMapIcons::PROPERTIES, "tune", "Mind Map: Node Properties"},
        {MindMapIcons::OPEN, "open_in_new", "Mind Map: Open the Element"},
        {MindMapIcons::COPY, "content_copy", "Mind Map: Copy"},
        {MindMapIcons::PASTE, "content_paste", "Mind Map: Paste"},
        {MindMapIcons::REVERSE, "swap_horiz", "Mind Map: Reverse the Direction"},
        {MindMapIcons::LABEL, "label", "Mind Map: Label"},
        {MindMapIcons::ZOOM_IN, "zoom_in", "Mind Map: Zoom In"},
        {MindMapIcons::ZOOM_OUT, "zoom_out", "Mind Map: Zoom Out"},
        {MindMapIcons::FIT, "fit_screen", "Mind Map: Whole Map"},
    }};
    core::IconRegistry& registry = core::IconRegistry::getInstance();
    for (const Icon& icon : ICONS) {
        registry.registerIcon(
            QString::fromLatin1(icon.id),
            QStringLiteral("resources/icons/twotone/%1.svg").arg(QString::fromLatin1(icon.file)),
            QString::fromLatin1(icon.label));
    }
    for (const char* id : {MindMapIcons::RELATION_MAP, MindMapModule::NEW_RELATION_MAP}) {
        registry.registerIcon(QString::fromLatin1(id),
                              QStringLiteral("resources/icons/twotone/hub.svg"),
                              QStringLiteral("Mind Map: New Relation Map"));
    }
}

}  // namespace

void registerMindMapModule() {
    core::settings_schema::addDefaults(MindMapSettings::defaults());
    registerIcons();
    core::ExtensionPointRegistry::getInstance().registerPlugin(std::make_shared<MindMapModule>());
}

}  // namespace kalahari::mindmap
