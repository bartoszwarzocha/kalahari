// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_coordinator_plan.cpp
/// @brief The scenes the plot board shows moved among the scenes of their text, and the scenes
/// in the plans of the texts: written, edited, added, moved and deleted

#include "kalahari/core/book_project.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/plot_board_coordinator.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QList>
#include <QString>

#include <algorithm>
#include <optional>

namespace kalahari::gui {

namespace {

using SceneSlot = PlotBoard::SceneSlot;

/// @brief The state of a scene in the plan once it is written in its text
constexpr const char* WRITTEN_STATUS = "draft";

/// @brief How many scenes of the text come before place @p at of @p order
int writtenBefore(const QList<SceneSlot>& order, qsizetype at) {
    return static_cast<int>(std::count_if(order.cbegin(), order.cbegin() + at,
                                          [](const SceneSlot& slot) { return !slot.planned; }));
}

/// @brief How many scenes in the plan come before place @p at of @p order
int plannedBefore(const QList<SceneSlot>& order, qsizetype at) {
    return static_cast<int>(std::count_if(order.cbegin(), order.cbegin() + at,
                                          [](const SceneSlot& slot) { return slot.planned; }));
}

/// @brief Plan @p plan with scene @p scene at place @p at of @p order (the scenes the board
/// shows of its text, without it); @p index gets its place in the plan
QList<core::PlannedScene> planWith(QList<SceneSlot> order, QList<core::PlannedScene> plan,
                                   core::PlannedScene scene, qsizetype at, int* index) {
    at = std::clamp<qsizetype>(at, 0, order.size());
    // Among the scenes in the plan only, it comes after the scenes of the text the one before
    // it comes after
    const SceneSlot* previous = at > 0 ? &order.at(at - 1) : nullptr;
    scene.after = previous != nullptr && previous->planned ? plan.at(previous->index).after
                                                           : writtenBefore(order, at);
    plan << scene;
    order.insert(at, SceneSlot{.planned = true, .index = static_cast<int>(plan.size() - 1)});
    *index = plannedBefore(order, at);
    return PlotBoard::planInOrder(order, plan);
}

/// @brief Whether text @p text is empty: one scene without words and with nothing said of it
bool isEmptyText(const core::SceneText& text) {
    const QList<core::KmlScene> scenes = core::ChapterDocument::kmlScenes(text.kml);
    return scenes.size() == 1 && scenes.constFirst().words == core::WordTally{} &&
           text.opening == core::ScenePlot{};
}

/// @brief Text @p text with its scene @p from moved to place @p to; none: the book could not
/// move it
std::optional<core::SceneText> withSceneAt(const core::SceneText& text, int from, int to) {
    if (from == to) {
        return text;
    }
    return core::ChapterDocument::withSceneMoved(text, from, to);
}

/// @brief What the board says when scene @p name, the @p first or the last of its text, cannot
/// move further
QString endOfTextMessage(const QString& name, bool first) {
    return first
               //: Under the plot board: a scene at the start of its text moved back; %1 the
               //: scene. In Polish: „%1” to pierwsza scena tekstu.
               ? PlotBoardCoordinator::tr("\"%1\" is the first scene of its text.").arg(name)
               //: In Polish: „%1” to ostatnia scena tekstu.
               : PlotBoardCoordinator::tr("\"%1\" is the last scene of its text.").arg(name);
}

}  // namespace

// =============================================================================
// The scenes the board shows, moved among the scenes of their text
// =============================================================================

void PlotBoardCoordinator::placeScene(const QString& textId, const SceneSlot& slot, int to) {
    const std::optional<ScenesState> before = scenesStateOf(textId);
    if (!before) {
        return;
    }
    QList<SceneSlot> order = orderOf(*before);
    const qsizetype from = order.indexOf(slot);
    if (from < 0 || from == to) {
        return;
    }
    const QString name = titleOf(PlotBoard::slotId(textId, slot));
    if (to < 0 || to >= order.size()) {
        say(endOfTextMessage(name, to < 0));
        return;
    }
    order.move(from, to);
    ScenesState wanted = *before;
    // The scenes in the plan the board shows keep their places among the others
    if (std::ranges::any_of(order, &SceneSlot::planned)) {
        wanted.plan = PlotBoard::planInOrder(order, before->plan);
    }
    QString cardId = PlotBoard::plannedId(textId, plannedBefore(order, to));
    if (!slot.planned) {
        const int at = writtenBefore(order, to);
        const std::optional<core::SceneText> moved = withSceneAt(before->text, slot.index, at);
        if (!moved) {
            notSaved();
            return;
        }
        wanted.text = *moved;
        cardId = PlotBoard::sceneId(textId, at);
    }
    changeScenes(textId, wanted, cardId, tr("\"%1\" moved.").arg(name),
                 tr("moving \"%1\"").arg(name));
}

// =============================================================================
// The scenes in the plans of the texts
// =============================================================================

void PlotBoardCoordinator::writePlanned(const QString& textId, int index) {
    const std::optional<ScenesState> before = scenesStateOf(textId);
    if (!before || index < 0 || index >= before->plan.size()) {
        return;
    }
    QList<SceneSlot> order = orderOf(*before);
    const qsizetype position = order.indexOf(SceneSlot{.planned = true, .index = index});
    if (position < 0) {
        return;
    }
    core::ScenePlot plot = before->plan.at(index).plot;
    plot.status = QString::fromLatin1(WRITTEN_STATUS);
    ScenesState wanted = *before;
    int at = writtenBefore(order, position);
    if (isEmptyText(before->text) && plannedBefore(order, position) == 0) {
        // The first scene of a text not written yet: the text is this scene
        at = 0;
        wanted.text.opening = plot;
        order.removeAt(order.indexOf(SceneSlot{.planned = false, .index = 0}));
        order.replace(order.indexOf(SceneSlot{.planned = true, .index = index}),
                      SceneSlot{.planned = false, .index = 0});
    } else {
        const std::optional<core::SceneText> added =
            core::ChapterDocument::withSceneAdded(before->text, at, plot);
        if (!added) {
            notSaved();
            return;
        }
        wanted.text = *added;
        // Its place is a scene of the text now
        order.replace(position, SceneSlot{.planned = false, .index = -1});
    }
    wanted.plan = PlotBoard::planInOrder(order, before->plan);
    const QString name = titleOf(PlotBoard::plannedId(textId, index));
    const QString cardId = PlotBoard::sceneId(textId, at);
    //: Under the plot board after Enter on a scene in the plan; %1 the scene. In Polish:
    //: „%1” jest teraz sceną tekstu.
    if (changeScenes(textId, wanted, cardId, tr("\"%1\" is a scene of the text now.").arg(name),
                     //: What Undo undoes on the plot board, after "Undone:"; %1 the scene. In
                     //: Polish: napisanie „%1”
                     tr("writing \"%1\"").arg(name))) {
        open(cardId);
    }
}

void PlotBoardCoordinator::editPlanned(const QString& textId, int index,
                                       const PlotCardEdit& edit) {
    const std::optional<ScenesState> before = scenesStateOf(textId);
    if (!before || index < 0 || index >= before->plan.size()) {
        return;
    }
    const QString cardId = PlotBoard::plannedId(textId, index);
    ScenesState wanted = *before;
    core::ScenePlot& plot = wanted.plan[index].plot;
    // A scene may have no title
    plot.title = edit.title.trimmed();
    plot.description = edit.description;
    plot.pov = edit.plot.pov;
    plot.place = edit.plot.place;
    plot.time = edit.plot.time;
    const QString name = plot.title.isEmpty() ? titleOf(cardId) : plot.title;
    changeScenes(textId, wanted, cardId, tr("The card \"%1\" changed.").arg(name),
                 tr("change of the card \"%1\"").arg(name));
}

void PlotBoardCoordinator::colorPlanned(const QString& textId, int index, const QString& color,
                                        const QString& message, const QString& undoText) {
    const std::optional<ScenesState> before = scenesStateOf(textId);
    if (!before || index < 0 || index >= before->plan.size()) {
        return;
    }
    ScenesState wanted = *before;
    wanted.plan[index].plot.color = color;
    changeScenes(textId, wanted, PlotBoard::plannedId(textId, index), message, undoText);
}

void PlotBoardCoordinator::addPlanned(const QString& textId, int at, const QString& message) {
    const std::optional<ScenesState> before = scenesStateOf(textId);
    if (!before) {
        return;
    }
    int index = 0;
    ScenesState wanted = *before;
    wanted.plan = planWith(orderOf(*before), before->plan, core::PlannedScene{}, at, &index);
    const QString cardId = PlotBoard::plannedId(textId, index);
    //: What Undo undoes on the plot board, after "Undone:"; %1 the title of the text. In
    //: Polish: dodanie sceny do planu „%1”
    if (changeScenes(textId, wanted, cardId, message,
                     tr("adding a scene to the plan of \"%1\"").arg(textTitleOf(textId))) &&
        m_panel != nullptr && m_panel->setCurrentCard(cardId)) {
        // The scenes shown, its card is edited at once
        m_panel->editCard(cardId);
    }
}

void PlotBoardCoordinator::removePlanned(const QString& textId, int index) {
    const std::optional<ScenesState> before = scenesStateOf(textId);
    if (!before || index < 0 || index >= before->plan.size()) {
        return;
    }
    const qsizetype position =
        orderOf(*before).indexOf(SceneSlot{.planned = true, .index = index});
    const QString name = titleOf(PlotBoard::plannedId(textId, index));
    ScenesState wanted = *before;
    wanted.plan.removeAt(index);
    // The card beside it is chosen: the one after it, else the one before it, else the text
    const QList<SceneSlot> left = orderOf(wanted);
    const QString cardId =
        left.isEmpty()
            ? textId
            : PlotBoard::slotId(textId, left.at(std::clamp<qsizetype>(position, 0,
                                                                      left.size() - 1)));
    //: Under the plot board after Delete on a scene in the plan; %1 the scene. In Polish:
    //: Usunięto „%1” z planu.
    changeScenes(textId, wanted, cardId, tr("Deleted \"%1\" from the plan.").arg(name),
                 tr("deleting \"%1\"").arg(name));
}

void PlotBoardCoordinator::movePlanned(const QString& textId, int index,
                                       const QString& targetId, int at) {
    const std::optional<ScenesState> from = scenesStateOf(textId);
    const std::optional<ScenesState> to = scenesStateOf(targetId);
    if (!from || !to || targetId == textId || index < 0 || index >= from->plan.size()) {
        return;
    }
    const QString name = titleOf(PlotBoard::plannedId(textId, index));
    ScenesState fromWanted = *from;
    const core::PlannedScene scene = fromWanted.plan.takeAt(index);
    int placed = 0;
    ScenesState toWanted = *to;
    toWanted.plan = planWith(orderOf(*to), to->plan, scene, at, &placed);
    changeScenes({ScenesChange{.textId = textId, .before = *from, .wanted = fromWanted},
                  ScenesChange{.textId = targetId, .before = *to, .wanted = toWanted}},
                 PlotBoard::plannedId(targetId, placed),
                 //: Under the plot board after a scene in the plan moved to another text; %1
                 //: the scene, %2 the title of the text. In Polish: „%1” przeniesiono do
                 //: „%2”.
                 tr(R"("%1" moved to "%2".)").arg(name, textTitleOf(targetId)),
                 tr("moving \"%1\"").arg(name));
}

void PlotBoardCoordinator::movePlannedToText(const QString& textId, int index, int direction) {
    const QString target = textBeside(textId, direction);
    const std::optional<ScenesState> there = scenesStateOf(target);
    if (!there) {
        const QString name = titleOf(PlotBoard::plannedId(textId, index));
        say(direction < 0
                //: Under the plot board: a scene in the plan of the first text moved back; %1
                //: the scene. In Polish: „%1” jest w pierwszym tekście tablicy.
                ? tr("\"%1\" is in the first text of the board.").arg(name)
                //: In Polish: „%1” jest w ostatnim tekście tablicy.
                : tr("\"%1\" is in the last text of the board.").arg(name));
        return;
    }
    // To the end of the text before it, to the start of the text after it
    movePlanned(textId, index, target,
                direction < 0 ? static_cast<int>(orderOf(*there).size()) : 0);
}

void PlotBoardCoordinator::pastePlanned(const QString& marked, const QString& afterId) {
    const auto planned = PlotBoard::plannedOf(marked);
    const QString afterText = PlotBoard::textOf(afterId);
    const std::optional<ScenesState> there = scenesStateOf(afterText);
    if (!planned || afterId == marked || !there) {
        say(QString());
        return;
    }
    // After a scene, or first after the card of a text
    const SceneSlot slot{.planned = true, .index = planned->second};
    const QList<SceneSlot> order = orderOf(*there);
    const qsizetype anchor = order.indexOf(PlotBoard::slotOf(afterId));
    if (afterText != planned->first) {
        movePlanned(planned->first, planned->second, afterText, static_cast<int>(anchor + 1));
        return;
    }
    const qsizetype from = order.indexOf(slot);
    placeScene(afterText, slot, static_cast<int>(anchor >= from ? anchor : anchor + 1));
}

QString PlotBoardCoordinator::textBeside(const QString& textId, int direction) const {
    if (m_panel == nullptr) {
        return {};
    }
    const QList<PlotCard> texts = PlotBoard::cardsOf(m_panel->bands());
    const auto found = std::ranges::find(texts, textId, &PlotCard::id);
    if (found == texts.cend()) {
        return {};
    }
    const qsizetype at = (found - texts.cbegin()) + (direction < 0 ? -1 : 1);
    return at >= 0 && at < texts.size() ? texts.at(at).id : QString();
}

}  // namespace kalahari::gui
