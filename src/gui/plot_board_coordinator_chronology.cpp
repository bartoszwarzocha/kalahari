// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_coordinator_chronology.cpp
/// @brief The chronology of the Book Plan and its file: the events outside the text, day one
/// and the periods of the writer, kept beside the texts of the book, with Undo

#include "kalahari/gui/plot_board_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plan_chronology.h"
#include "kalahari/gui/plot_board.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief How the chronology shows the plan: kept in its file, but no step of Undo
constexpr const char* VIEW_KEY = "view";

/// @brief @p plan with the view of @p current: Undo gives back the events, not how they show
QJsonObject withViewOf(QJsonObject plan, const QJsonObject& current) {
    if (current.contains(QLatin1String(VIEW_KEY))) {
        plan.insert(QLatin1String(VIEW_KEY), current.value(QLatin1String(VIEW_KEY)));
    }
    return plan;
}

}  // namespace

QString PlotBoardCoordinator::planPath() const {
    const core::ProjectBook* book = m_book.book();
    if (!m_book.isProjectOpen() || book == nullptr) {
        return {};
    }
    const QDir project(m_book.getProjectPath());
    const QString folder = book->folder.isEmpty() ? project.path() : project.filePath(book->folder);
    return QDir(folder).filePath(QString::fromLatin1(PLAN_FILE));
}

void PlotBoardCoordinator::showPlan() {
    PlanChronology* chronology = m_panel != nullptr ? m_panel->chronology() : nullptr;
    if (chronology == nullptr) {
        return;
    }
    const QString path = planPath();
    if (path.isEmpty()) {
        return;
    }
    if (path != m_planPath) {
        m_planPath = path;
        m_plan = readPlan(path);
    }
    chronology->setPlan(m_plan);
    if (m_panel->view() == QLatin1String(PlotBoard::VIEW_CHRONOLOGY)) {
        // Once the chronology is on the screen
        QTimer::singleShot(0, this, &PlotBoardCoordinator::offerTimelineTimes);
    }
}

QJsonObject PlotBoardCoordinator::readPlan(const QString& path) {
    const std::shared_ptr<IPlanChronologyProvider> provider = planChronologyProvider();
    const QJsonObject fresh = provider ? provider->newPlan() : QJsonObject();
    QFile file(path);
    if (!file.exists()) {
        return fresh;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        core::Logger::getInstance().warn("Book plan: cannot read {}: {}", path.toStdString(),
                                         file.errorString().toStdString());
        return fresh;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (!document.isObject()) {
        core::Logger::getInstance().warn("Book plan: {} is not a plan: {}", path.toStdString(),
                                         error.errorString().toStdString());
        return fresh;
    }
    return document.object();
}

bool PlotBoardCoordinator::writePlan(const QString& path, const QJsonObject& plan) {
    if (path.isEmpty() || m_book.isReadOnly()) {
        return false;
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        core::Logger::getInstance().warn("Book plan: cannot write {}: {}", path.toStdString(),
                                         file.errorString().toStdString());
        return false;
    }
    file.write(QJsonDocument(plan).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        core::Logger::getInstance().warn("Book plan: cannot write {}: {}", path.toStdString(),
                                         file.errorString().toStdString());
        return false;
    }
    if (path == m_planPath) {
        m_plan = plan;
    }
    return true;
}

bool PlotBoardCoordinator::changePlan(const QString& path, const QJsonObject& plan) {
    // How the chronology shows the plan stays as it is now
    const QJsonObject kept = withViewOf(plan, path == m_planPath ? m_plan : plan);
    if (!writePlan(path, kept)) {
        return false;
    }
    showPlan();
    return true;
}

void PlotBoardCoordinator::connectChronology(PlotBoardPanel* panel) {
    PlanChronology* chronology = panel->chronology();
    if (chronology == nullptr) {
        return;
    }
    connect(chronology, &PlanChronology::planEdited, this,
            [this](const QJsonObject& plan, const QString& undoText, const QString& message) {
                // Undo changes the plan of this book, even after another volume is chosen
                const QString path = planPath();
                const QJsonObject before = m_plan;
                if (!writePlan(path, plan)) {
                    // The chronology shows the plan the file keeps
                    showPlan();
                    notSaved();
                    return;
                }
                pushChange(
                    undoText, [this, path, before]() { return changePlan(path, before); },
                    [this, path, plan]() { return changePlan(path, plan); });
                say(message, true);
            });
    connect(panel, &PlotBoardPanel::viewShown, this, [this](const QString& view) {
        if (view == QLatin1String(PlotBoard::VIEW_CHRONOLOGY)) {
            offerTimelineTimes();
        }
    });
    // The scale and the bands are kept as they are chosen
    connect(chronology, &PlanChronology::planViewChanged, this,
            [this](const QJsonObject& plan) {
                // A book open to read shows it so until it is closed
                if (!writePlan(planPath(), plan)) {
                    m_plan = plan;
                }
            });
}

void PlotBoardCoordinator::offerTimelineTimes() {
    if (m_panel == nullptr || m_book.isReadOnly() ||
        m_panel->view() != QLatin1String(PlotBoard::VIEW_CHRONOLOGY)) {
        return;
    }
    // Once for each book, before its plan has a file
    const QString path = planPath();
    if (path.isEmpty() || m_timesOffered.contains(path) || QFileInfo::exists(path)) {
        return;
    }
    m_timesOffered.insert(path);
    const std::shared_ptr<IPlanChronologyProvider> provider = planChronologyProvider();
    if (!provider) {
        return;
    }
    const PlanTimelineTimes found = provider->timelineTimes(m_book);
    QList<std::pair<QString, CardText>> before;
    QList<std::pair<QString, CardText>> after;
    QStringList timelines;
    for (const PlanTextTime& each : found.texts) {
        const core::ProjectElement* element = m_book.findElement(each.elementId);
        if (element == nullptr || m_book.bookOf(each.elementId) != m_book.book() ||
            !element->plot.time.trimmed().isEmpty()) {
            continue;
        }
        CardText was = cardTextOf(each.elementId);
        CardText now = was;
        now.plot.time = each.time;
        if (now.plot.pov.trimmed().isEmpty()) {
            now.plot.pov = each.pov;
        }
        if (now.plot.place.trimmed().isEmpty()) {
            now.plot.place = each.place;
        }
        before.append({each.elementId, was});
        after.append({each.elementId, now});
        if (!timelines.contains(each.timeline)) {
            timelines << each.timeline;
        }
    }
    if (after.isEmpty()) {
        return;
    }
    const auto count = static_cast<int>(after.size());
    const dialogs::MessageDialog::Answer answer = dialogs::MessageDialog::ask(
        window(),
        //: The title of the question of the chronology of the Book Plan. In Polish: Czasy z osi
        //: czasu
        tr("Times from the Timelines"),
        //: %n how many texts, %1 the titles of the timelines ("Wyprawa, Rodzina"). In Polish:
        //: Osie czasu w Warsztacie (%1) mówią, kiedy dzieje się %n tekst(ów) książki („W
        //: książce”). Przenieść ich czas, punkt widzenia i miejsce na karty bez czasu akcji?
        //: Osie zostaną bez zmian, a Ctrl+Z cofnie przeniesienie.
        tr("The timelines of the Workshop (%1) say when %n text(s) of the book happen (“In "
           "the book”). Copy their times, points of view and places to the cards without a "
           "time of the action? The timelines stay as they are, and Ctrl+Z takes it back.",
           nullptr, count)
            .arg(timelines.join(QStringLiteral(", "))),
        //: In Polish: &Przenieś czasy
        tr("&Copy the Times"),
        //: In Polish: &Nie przenoś
        tr("&Don't Copy"));
    if (answer == dialogs::MessageDialog::Answer::Cancel) {
        return;
    }
    // Either way the plan gets its file, and the question is not asked again; day one is the
    // timeline's, so that its days of the story stay the same days
    if (!writePlan(path, found.plan.isEmpty() ? m_plan : found.plan)) {
        notSaved();
        return;
    }
    showPlan();
    if (answer != dialogs::MessageDialog::Answer::Accept) {
        return;
    }
    const auto apply = [this](const QList<std::pair<QString, CardText>>& cards) {
        bool saved = true;
        for (const auto& [elementId, text] : cards) {
            saved = changeCard(elementId, text) && saved;
        }
        return saved;
    };
    if (!apply(after)) {
        apply(before);
        notSaved();
        return;
    }
    pushChange(
        //: What Undo undoes in the Book Plan, after "Undone:". In Polish: przeniesienie czasów
        //: z osi czasu
        tr("the times from the timelines"), [apply, before]() { return apply(before); },
        [apply, after]() { return apply(after); });
    //: Under the chronology of the Book Plan. In Polish: Przeniesiono czas akcji na %n
    //: kart(y) z osi czasu; osie zostały bez zmian.
    say(tr("%n card(s) got the time of the action from the timelines; the timelines stay as "
           "they were.",
           nullptr, count),
        true);
}

}  // namespace kalahari::gui
