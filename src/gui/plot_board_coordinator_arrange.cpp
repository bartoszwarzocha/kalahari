// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file plot_board_coordinator_arrange.cpp
/// @brief What the plot board asks for to arrange its cards: texts and scenes moved, dropped,
/// added, deleted, cut and pasted

#include "kalahari/gui/plot_board_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"

#include <QLatin1String>
#include <QList>
#include <QString>
#include <QtGlobal>

#include <optional>
#include <utility>

namespace kalahari::gui {

void PlotBoardCoordinator::move(const QString& elementId, int steps) {
    if (m_book.isReadOnly() || steps == 0) {
        return;
    }
    if (const QString textId = PlotBoard::textOf(elementId); textId != elementId) {
        // A scene among the scenes the board shows
        const PlotBoard::SceneSlot slot = PlotBoard::slotOf(elementId);
        const std::optional<ScenesState> state = scenesStateOf(textId);
        const qsizetype at = state ? orderOf(*state).indexOf(slot) : -1;
        if (at >= 0) {
            placeScene(textId, slot, static_cast<int>(at) + steps);
        }
        return;
    }
    const std::optional<core::ElementPlace> place =
        PlotBoard::stepPlace(m_book, elementId, steps);
    if (!place) {
        //: Under the plot board: a card at the start or the end of the book; %1 its title. In
        //: Polish: „%1” nie przesunie się dalej.
        say(tr("\"%1\" cannot move further.").arg(titleOf(elementId)));
        return;
    }
    moveWithUndo(elementId, *place);
}

void PlotBoardCoordinator::moveToText(const QString& cardId, int direction) {
    if (m_book.isReadOnly()) {
        return;
    }
    if (const PlotBoard::SceneSlot slot = PlotBoard::slotOf(cardId); slot.index >= 0) {
        moveToTextBeside(PlotBoard::textOf(cardId), slot, direction);
    }
}

void PlotBoardCoordinator::dropBeside(const QString& elementId, const QString& anchorId,
                                      bool after) {
    if (m_book.isReadOnly()) {
        return;
    }
    if (PlotBoard::textOf(elementId) != elementId || PlotBoard::textOf(anchorId) != anchorId) {
        dropSceneBeside(elementId, anchorId, after);
        return;
    }
    const std::optional<core::ElementPlace> place =
        PlotBoard::placeBeside(m_book, elementId, anchorId, after);
    if (!place) {
        //: Under the plot board: a card dropped where its kind cannot be; %1 its title. In
        //: Polish: „%1” nie może się tam znaleźć.
        say(tr("\"%1\" cannot be there.").arg(titleOf(elementId)));
        return;
    }
    moveWithUndo(elementId, *place);
}

void PlotBoardCoordinator::dropSceneBeside(const QString& cardId, const QString& anchorId,
                                           bool after) {
    const QString textId = PlotBoard::textOf(cardId);
    const PlotBoard::SceneSlot slot = PlotBoard::slotOf(cardId);
    const QString anchorText = PlotBoard::textOf(anchorId);
    const std::optional<ScenesState> there = scenesStateOf(anchorText);
    if (slot.index < 0 || !there) {
        return;
    }
    const QList<PlotBoard::SceneSlot> order = orderOf(*there);
    // Beside the card of a text: first among its scenes
    const qsizetype anchor = order.indexOf(PlotBoard::slotOf(anchorId));
    const qsizetype at = anchor < 0 ? 0 : anchor + (after ? 1 : 0);
    if (anchorText != textId) {
        moveSlotToText(textId, slot, anchorText, static_cast<int>(at));
        return;
    }
    // Among the scenes of its text: the place in the list without it
    const qsizetype from = order.indexOf(slot);
    placeScene(textId, slot, static_cast<int>(at > from ? at - 1 : at));
}

void PlotBoardCoordinator::dropInPart(const QString& elementId, const QString& partId) {
    if (m_book.isReadOnly()) {
        return;
    }
    // A scene goes beside a scene or a text, not in a part
    const std::optional<core::ElementPlace> place =
        PlotBoard::textOf(elementId) == elementId
            ? PlotBoard::placeInPart(m_book, elementId, partId, false)
            : std::nullopt;
    if (!place) {
        say(tr("\"%1\" cannot be there.").arg(titleOf(elementId)));
        return;
    }
    moveWithUndo(elementId, *place);
}

void PlotBoardCoordinator::add(const QString& afterId) {
    if (PlotBoard::textOf(afterId) != afterId) {
        addAfterScene(afterId);
        return;
    }
    addText(afterId, false);
}

void PlotBoardCoordinator::addAfterScene(const QString& cardId) {
    const QString textId = PlotBoard::textOf(cardId);
    if (PlotBoard::scenesVariant() == QLatin1String(PlotBoard::SCENES_TEXT)) {
        if (const auto scene = PlotBoard::sceneOf(cardId)) {
            addSceneAt(textId, scene->second + 1,
                       //: Under the plot board after Insert on a scene; %1 the scene. In Polish:
                       //: Dodano scenę za „%1”.
                       tr("Added a scene after \"%1\".").arg(titleOf(cardId)));
        }
        return;
    }
    // With the plans: a scene in the plan
    const std::optional<ScenesState> state = scenesStateOf(textId);
    const qsizetype at = state ? orderOf(*state).indexOf(PlotBoard::slotOf(cardId)) : -1;
    if (at >= 0) {
        //: Under the plot board after Insert on a scene; %1 the scene. In Polish: Dodano scenę
        //: w planie za „%1”.
        addPlanned(textId, static_cast<int>(at) + 1,
                   tr("Added a scene in the plan after \"%1\".").arg(titleOf(cardId)));
    }
}

void PlotBoardCoordinator::addScene(const QString& cardId) {
    const QString textId = PlotBoard::textOf(cardId);
    if (m_book.isReadOnly() || m_book.findElement(textId) == nullptr) {
        return;
    }
    if (PlotBoard::scenesVariant() != QLatin1String(PlotBoard::SCENES_TEXT)) {
        const std::optional<ScenesState> state = scenesStateOf(textId);
        if (state) {
            //: Under the plot board after Alt+Insert; %1 the title of the text. In Polish:
            //: Dodano scenę w planie na końcu „%1”.
            addPlanned(textId, static_cast<int>(orderOf(*state).size()),
                       tr("Added a scene in the plan at the end of \"%1\".")
                           .arg(titleOf(textId)));
        }
        return;
    }
    addSceneAt(textId, sceneCount(textId),
               //: Under the plot board after Alt+Insert; %1 the title of the text. In Polish:
               //: Dodano scenę na końcu „%1”.
               tr("Added a scene at the end of \"%1\".").arg(titleOf(textId)));
}

void PlotBoardCoordinator::remove(const QString& elementId) {
    if (m_book.isReadOnly()) {
        return;
    }
    if (const auto planned = PlotBoard::plannedOf(elementId)) {
        removePlanned(planned->first, planned->second);
        return;
    }
    if (const auto scene = PlotBoard::sceneOf(elementId)) {
        removeScene(scene->first, scene->second);
        return;
    }
    if (m_book.findElement(elementId) == nullptr) {
        return;
    }
    // A text in the plan has nothing but its card
    if (m_book.isPlannedText(elementId)) {
        removePlannedText(elementId);
        return;
    }
    // The Navigator asks first and says where the files go
    m_navigator->onRequestDelete(elementId);
    if (m_panel != nullptr) {
        m_panel->focusView();
    }
}

void PlotBoardCoordinator::cut(const QString& elementId) {
    if (m_book.isReadOnly() || m_panel == nullptr ||
        m_book.findElement(PlotBoard::textOf(elementId)) == nullptr) {
        return;
    }
    m_marked = elementId;
    m_panel->setMarkedCard(elementId);
    //: Under the plot board after Ctrl+X; %1 the title. In Polish: „%1” czeka na przeniesienie:
    //: wybierz kartę i naciśnij Ctrl+V, by wstawić ją za nią; Esc rezygnuje.
    say(tr("\"%1\" waits to move: choose a card and press Ctrl+V to put it after that card; Esc "
           "leaves it.")
            .arg(titleOf(elementId)));
}

void PlotBoardCoordinator::paste(const QString& afterId) {
    if (m_marked.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const QString marked = std::exchange(m_marked, QString());
    if (m_panel != nullptr) {
        m_panel->setMarkedCard(QString());
    }
    if (PlotBoard::slotOf(marked).index >= 0) {
        pasteScene(marked, afterId);
        return;
    }
    // After a scene: after its text
    const QString textId = PlotBoard::textOf(afterId);
    if (textId.isEmpty() || textId == marked || m_book.findElement(marked) == nullptr) {
        if (m_panel != nullptr) {
            m_panel->showMessage(QString(), false);
        }
        return;
    }
    dropBeside(marked, textId, true);
}

void PlotBoardCoordinator::pasteScene(const QString& marked, const QString& afterId) {
    const QString textId = PlotBoard::textOf(marked);
    const PlotBoard::SceneSlot slot = PlotBoard::slotOf(marked);
    const QString afterText = PlotBoard::textOf(afterId);
    const std::optional<ScenesState> there = scenesStateOf(afterText);
    if (afterId == marked || !there) {
        say(QString());
        return;
    }
    // After a scene, or first after the card of a text
    const QList<PlotBoard::SceneSlot> order = orderOf(*there);
    const qsizetype anchor = order.indexOf(PlotBoard::slotOf(afterId));
    if (afterText != textId) {
        moveSlotToText(textId, slot, afterText, static_cast<int>(anchor + 1));
        return;
    }
    const qsizetype from = order.indexOf(slot);
    placeScene(afterText, slot, static_cast<int>(anchor >= from ? anchor : anchor + 1));
}

// =============================================================================
// The numbers in the titles
// =============================================================================

void PlotBoardCoordinator::renumberTitles() {
    if (m_book.isReadOnly()) {
        return;
    }
    const QList<PlotBoard::TitleChange> titles = PlotBoard::renumbered(m_book);
    if (titles.isEmpty()) {
        //: Under the plot board after Renumber, when nothing had to change. In Polish: Numery w
        //: tytułach zgadzają się z kolejnością.
        say(tr("The numbers in the titles follow the order."));
        return;
    }
    const QList<PlotBoard::TitleChange> former = formerTitles(titles);
    if (!retitle(titles)) {
        retitle(former);
        notSaved();
        return;
    }
    pushChange(
        //: What Undo undoes on the plot board, after "Undone:". In Polish: ponumerowanie
        //: tytułów od nowa
        tr("renumbering the titles"), [this, former]() { return retitle(former); },
        [this, titles]() { return retitle(titles); });
    const int count = static_cast<int>(titles.size());
    //: Under the plot board after Renumber; %n the titles changed. In Polish: Ponumerowano od
    //: nowa %n tytuł / tytuły / tytułów; zmieniły się tylko numery.
    QString message = tr("%n titles renumbered; only the numbers changed.", nullptr, count);
    if (count == 1) {
        //: Under the plot board after Renumber. In Polish: Ponumerowano od nowa 1 tytuł;
        //: zmienił się tylko numer.
        message = tr("1 title renumbered; only its number changed.");
    }
    say(message, true);
}

QList<PlotBoard::TitleChange> PlotBoardCoordinator::renumberAtOnce(
    const QList<PlotBoard::TitleChange>& titles, QList<PlotBoard::TitleChange>& former) {
    if (titles.isEmpty() ||
        PlotBoard::renumbering() != QLatin1String(PlotBoard::RENUMBER_AUTO)) {
        return {};
    }
    former = formerTitles(titles);
    if (!retitle(titles)) {
        retitle(former);
        former.clear();
        notSaved();
        return {};
    }
    return titles;
}

void PlotBoardCoordinator::sayMoved(const QString& moved, bool outOfOrder, bool renumbered) {
    if (renumbered) {
        //: Added under the plot board after a move that renumbered the titles at once
        //: ("Chapter 3"). In Polish: Tytuły ponumerowano od nowa.
        say(tr("%1 The titles were renumbered.").arg(moved), true);
        return;
    }
    if (outOfOrder && m_panel != nullptr &&
        PlotBoard::renumbering() == QLatin1String(PlotBoard::RENUMBER_ASK)) {
        //: Added under the plot board after a move, with the button Renumber. In Polish:
        //: Numery w tytułach nie zgadzają się już z kolejnością.
        m_panel->offerRenumbering(
            tr("%1 The numbers in the titles no longer follow the order.").arg(moved));
        return;
    }
    say(moved, true);
}

bool PlotBoardCoordinator::retitle(const QList<PlotBoard::TitleChange>& titles) {
    if (titles.isEmpty()) {
        return true;
    }
    bool saved = true;
    ++m_ownChanges;
    for (const PlotBoard::TitleChange& change : titles) {
        if (m_book.findElement(change.id) == nullptr ||
            !m_book.renameElement(change.id, change.title)) {
            saved = false;
        }
    }
    --m_ownChanges;
    m_navigator->refreshNavigator();
    for (const PlotBoard::TitleChange& change : titles) {
        m_navigator->refreshTabTitle(change.id);
    }
    refreshNow();
    return saved;
}

QList<PlotBoard::TitleChange> PlotBoardCoordinator::formerTitles(
    const QList<PlotBoard::TitleChange>& titles) const {
    QList<PlotBoard::TitleChange> former;
    for (const PlotBoard::TitleChange& change : titles) {
        former << PlotBoard::TitleChange{.id = change.id, .title = textTitleOf(change.id)};
    }
    return former;
}

}  // namespace kalahari::gui
