// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_module.cpp
/// @brief The module of the timelines: what the program knows of it

#include <kalahari/timeline/timeline_module.h>

#include <kalahari/core/book_project.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_schema.h>
#include <kalahari/timeline/timeline.h>
#include <kalahari/timeline/timeline_index.h>
#include <kalahari/timeline/timeline_editor.h>
#include <kalahari/timeline/plan_chronology_view.h>
#include <kalahari/timeline/timeline_look.h>
#include <kalahari/timeline/timeline_settings_page.h>

#include <QCoreApplication>
#include <QDate>
#include <QHash>
#include <QJsonDocument>

#include <memory>
#include <optional>
#include <utility>

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

std::string PlanChronologyModule::getPluginId() const {
    return "kalahari.timelines.plan";
}

std::string PlanChronologyModule::getVersion() const {
    return "1.0.0";
}

void PlanChronologyModule::onInit() {
    // Nothing to prepare: the chronology comes with the tab of the Book Plan
}

void PlanChronologyModule::onActivate() {
    // Always active while the program has the module
}

gui::PlanChronology* PlanChronologyModule::createChronology(QWidget* parent) {
    return new PlanChronologyView(parent);
}

QJsonObject PlanChronologyModule::newPlan() const {
    Timeline plan;
    TimelineView view;
    view.scale = TimelineView::Scale::Days;
    view.lanes = TimelineView::Lanes::Characters;
    plan.setView(view);
    plan.setDayOne(QDate::currentDate().toJulianDay());
    return plan.toJson();
}

namespace {

/// @brief A time a timeline gives a text, and when it starts to compare it with the others
struct FoundTime {
    gui::PlanTextTime time;
    std::optional<double> start;  ///< None: a time the program does not read
};

/// @brief The timelines of the Workshop: its elements with a file of a timeline, the path of
/// the file and the title of each
void timelinesIn(const QList<core::ProjectElement>& elements,
                 QList<std::pair<QString, QString>>& files, const core::ProjectManager& book) {
    for (const core::ProjectElement& element : elements) {
        if (element.file.endsWith(QLatin1Char('.') + QLatin1String(TimelineModule::FILE_SUFFIX),
                                  Qt::CaseInsensitive)) {
            files.append({book.filePathOf(element), element.title});
        }
        timelinesIn(element.elements, files, book);
    }
}

/// @brief The time of @p event as the plan writes it: in days of the story where the
/// timeline counts them, else as a date
QString timeTextOf(const TimelineEvent& event, const Timeline& timeline,
                   const StoryTimeStyle& style) {
    if (!event.time) {
        return event.timeText;
    }
    if (timeline.view().scale == TimelineView::Scale::Days &&
        event.time->precision == StoryTime::Precision::Day) {
        return event.time->dayText(timeline.dayOne(), style);
    }
    return event.time->dateText(style);
}

}  // namespace

gui::PlanTimelineTimes PlanChronologyModule::timelineTimes(
    const core::ProjectManager& book) const {
    gui::PlanTimelineTimes times;
    const core::BookProject* project = book.project();
    if (project == nullptr) {
        return times;
    }
    QList<std::pair<QString, QString>> files;
    timelinesIn(project->workshop.elements, files, book);
    const TimelineIndex index = TimelineIndex::of(book);
    const StoryTimeStyle style = TimelineLook::current().style;
    QHash<QString, FoundTime> found;
    for (const auto& [file, title] : files) {
        Timeline timeline;
        if (!timeline.load(file)) {
            continue;
        }
        for (const TimelineEvent* event : timeline.eventsInOrder()) {
            const QString text = timeTextOf(*event, timeline, style);
            if (text.isEmpty()) {
                continue;
            }
            const std::optional<double> start =
                event->time ? std::optional<double>(event->time->start()) : std::nullopt;
            for (const QString& chapter : event->chapters) {
                // The earliest event of a text; one with a time before one without
                if (const auto known = found.constFind(chapter);
                    known != found.cend() &&
                    (!start || (known->start && *known->start <= *start))) {
                    continue;
                }
                if (times.plan.isEmpty()) {
                    Timeline plan;
                    plan.setView(timeline.view());
                    plan.setDayOne(timeline.dayOne());
                    times.plan = plan.toJson();
                }
                found.insert(chapter,
                             FoundTime{gui::PlanTextTime{
                                           .elementId = chapter,
                                           .time = text,
                                           .pov = event->characters.isEmpty()
                                                      ? QString()
                                                      : index.nameOf(event->characters.first()),
                                           .place = index.nameOf(event->place),
                                           .timeline = title},
                                       start});
            }
        }
    }
    // In the order of the book
    for (const TimelineIndex::Entry& chapter : index.chapters) {
        if (const auto known = found.constFind(chapter.id); known != found.cend()) {
            times.texts << known->time;
        }
    }
    return times;
}

void registerTimelineModule() {
    core::settings_schema::addDefaults(TimelineSettings::defaults());
    core::ExtensionPointRegistry::getInstance().registerPlugin(std::make_shared<TimelineModule>());
    core::ExtensionPointRegistry::getInstance().registerPlugin(
        std::make_shared<PlanChronologyModule>());
}

}  // namespace kalahari::timeline
