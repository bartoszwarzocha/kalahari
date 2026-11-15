// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_coordinator_arrange.cpp
/// @brief What the plot board asks for to arrange its cards: texts and scenes moved, dropped,
/// added, deleted, cut and pasted

#include "kalahari/gui/plot_board_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/logger.h"
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

namespace {

/// @brief Where a text added after a card goes, of what kind
struct NewText {
    core::KindRef kind;        ///< Of the card after which it goes, else the main one
    core::ElementPlace place;  ///< Right after the card, else at the end (index -1)
    QString afterTitle;        ///< The title of the card; empty: at the end of the book
};

/// @brief Where a text added after card @p afterId of @p book goes: a text of the kind of the
/// card right after it; without a card, a text of the main kind of the book at the end of its
/// main section
NewText newTextAfter(const core::ProjectManager& book, const QString& afterId) {
    NewText text;
    const core::ProjectElement* after = book.findElement(afterId);
    const std::optional<core::ElementPlace> afterPlace =
        after != nullptr && book.project() != nullptr ? book.project()->placeOf(afterId)
                                                      : std::nullopt;
    if (after == nullptr || !afterPlace || afterPlace->place != core::BookPlace::Main) {
        text.kind = book.mainTextKind();
        text.place.index = -1;
        return text;
    }
    // The element is no more where it was once the book has another one: its title is kept
    text.afterTitle = after->title;
    text.kind = book.kindOf(*after);
    text.place = *afterPlace;
    ++text.place.index;
    return text;
}

}  // namespace

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
    if (const auto planned = PlotBoard::plannedOf(cardId)) {
        movePlannedToText(planned->first, planned->second, direction);
    } else if (PlotBoard::sceneOf(cardId)) {
        sceneInEditor();
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
        // A scene in the plan goes to the plan of the other text; a scene of a text, in the
        // editor
        if (slot.planned) {
            movePlanned(textId, slot.index, anchorText, static_cast<int>(at));
        } else {
            sceneInEditor();
        }
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
    if (PlotBoard::textOf(elementId) != elementId) {
        sceneInEditor();
        return;
    }
    const std::optional<core::ElementPlace> place =
        PlotBoard::placeInPart(m_book, elementId, partId, false);
    if (!place) {
        say(tr("\"%1\" cannot be there.").arg(titleOf(elementId)));
        return;
    }
    moveWithUndo(elementId, *place);
}

void PlotBoardCoordinator::add(const QString& afterId) {
    if (m_book.project() == nullptr || m_book.isReadOnly()) {
        return;
    }
    if (PlotBoard::textOf(afterId) != afterId) {
        addAfterScene(afterId);
        return;
    }
    const NewText text = newTextAfter(m_book, afterId);
    if (!text.kind || !core::ProjectManager::canAdd(text.kind)) {
        return;
    }
    const QString title = m_book.defaultTitle(text.kind);
    // The places the board remembers move: what can be undone is forgotten (onProjectChanged)
    const QString added =
        m_book.addElement(text.kind, title, text.place.place, text.place.groupId, text.place.index);
    if (added.isEmpty()) {
        core::Logger::getInstance().error("PlotBoardCoordinator: Failed to add '{}'",
                                          title.toStdString());
        notSaved();
        return;
    }
    m_navigator->refreshNavigator();
    refreshNow();
    if (m_panel == nullptr) {
        return;
    }
    m_panel->setCurrentCard(added);
    m_panel->editCard(added);
    say(!text.afterTitle.isEmpty()
            //: Under the plot board after Insert; %1 the title of the new text, %2 the card it
            //: is after. In Polish: Dodano „%1” za „%2”.
            ? tr(R"(Added "%1" after "%2".)").arg(title, text.afterTitle)
            //: Under the plot board after Insert with no card; %1 the title of the new text.
            //: In Polish: Dodano „%1” na końcu książki.
            : tr("Added \"%1\" at the end of the book.").arg(title));
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
    if (PlotBoard::plannedOf(marked)) {
        pastePlanned(marked, afterId);
        return;
    }
    if (const auto markedScene = PlotBoard::sceneOf(marked)) {
        pasteScene(marked, markedScene->first, markedScene->second, afterId);
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

void PlotBoardCoordinator::pasteScene(const QString& marked, const QString& textId, int scene,
                                      const QString& afterId) {
    // A scene: after a scene of its text, or first after the card of its text
    if (afterId == textId) {
        placeScene(textId, PlotBoard::SceneSlot{.planned = false, .index = scene}, 0);
    } else if (afterId == marked) {
        say(QString());
    } else if (PlotBoard::textOf(afterId) == textId) {
        dropBeside(marked, afterId, true);
    } else {
        sceneInEditor();
    }
}

}  // namespace kalahari::gui
