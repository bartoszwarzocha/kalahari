// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_module.cpp
/// @brief The module of the timelines: what the program knows of it

#include <kalahari/timeline/timeline_module.h>

#include <kalahari/core/settings_schema.h>
#include <kalahari/timeline/timeline.h>
#include <kalahari/timeline/timeline_editor.h>
#include <kalahari/timeline/timeline_look.h>
#include <kalahari/timeline/timeline_settings_page.h>

#include <QCoreApplication>
#include <QDate>
#include <QJsonDocument>

#include <memory>

namespace kalahari::timeline {

std::string TimelineModule::getPluginId() const {
    return "kalahari.timelines";
}

std::string TimelineModule::getVersion() const {
    return "1.0.0";
}

void TimelineModule::onInit() {
    // Nothing to prepare: the editors come with their tabs
}

void TimelineModule::onActivate() {
    // Always active while the program has the module
}

QString TimelineModule::editorId() const {
    return QString::fromLatin1(EDITOR_ID);
}

QString TimelineModule::fileSuffix() const {
    return QString::fromLatin1(FILE_SUFFIX);
}

QByteArray TimelineModule::newFile() const {
    Timeline timeline;
    timeline.setView(TimelineLook::newView());
    timeline.setDayOne(QDate::currentDate().toJulianDay());
    timeline.addThread(
        //: The thread of a new timeline. In Polish: Główny wątek
        QCoreApplication::translate("kalahari::timeline::TimelineModule", "Main plot"),
        QStringLiteral("1"));
    return QJsonDocument(timeline.toJson()).toJson(QJsonDocument::Indented);
}

QWidget* TimelineModule::createEditor(const core::WindowEditorContext& context,
                                      QWidget* parent) {
    return new TimelineEditor(context, parent);
}

QString TimelineModule::settingsTitle() const {
    //: The page of Settings of the timelines. In Polish: Osie czasu
    return QCoreApplication::translate("kalahari::timeline::TimelineModule", "Timelines");
}

QWidget* TimelineModule::createSettingsPage() {
    return new TimelineSettingsPage();
}

void registerTimelineModule() {
    core::settings_schema::addDefaults(TimelineSettings::defaults());
    core::ExtensionPointRegistry::getInstance().registerPlugin(std::make_shared<TimelineModule>());
}

}  // namespace kalahari::timeline
