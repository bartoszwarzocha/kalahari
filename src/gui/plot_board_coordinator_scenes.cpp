// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file plot_board_coordinator_scenes.cpp
/// @brief The scenes of the texts changed from the plot board: what they say, added, deleted
/// and replaced, in the editor of an open text and in the file of a closed one

#include "kalahari/gui/plot_board_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"

#include <QHash>
#include <QLatin1String>
#include <QList>
#include <QString>
#include <QWidget>
#include <QtGlobal>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief Ask over @p window whether to delete scene @p name (the @p first one of its text)
/// with its text or its break only
dialogs::MessageDialog::Answer askDeleteScene(QWidget* window, const QString& name, bool first) {
    return dialogs::MessageDialog::ask(
        window,
        //: The title of the question. In Polish: Usuń scenę
        PlotBoardCoordinator::tr("Delete Scene"),
        (first
             //: %1 the scene. In Polish: Usunąć scenę „%1” razem z jej tekstem? Ctrl+Z na
             //: tablicy ją przywróci. (new paragraph) Zachowaj tekst usuwa tylko podział
             //: sceny: następna scena dołączy do tej.
             ? PlotBoardCoordinator::tr(
                   "Delete the scene \"%1\" with its text? Ctrl+Z on the board brings it "
                   "back.\n\nKeep the Text removes only the scene break: the next scene joins "
                   "this one.")
             //: %1 the scene. In Polish: Usunąć scenę „%1” razem z jej tekstem? Ctrl+Z na
             //: tablicy ją przywróci. (new paragraph) Zachowaj tekst usuwa tylko podział
             //: sceny: jej tekst dołączy do poprzedniej sceny.
             : PlotBoardCoordinator::tr(
                   "Delete the scene \"%1\" with its text? Ctrl+Z on the board brings it "
                   "back.\n\nKeep the Text removes only the scene break: its text joins the "
                   "scene before it."))
            .arg(name),
        //: In Polish: &Usuń z tekstem
        PlotBoardCoordinator::tr("&Delete with Text"),
        //: In Polish: &Zachowaj tekst
        PlotBoardCoordinator::tr("&Keep the Text"));
}

}  // namespace

void PlotBoardCoordinator::changeSceneWithUndo(const QString& cardId,
                                               const core::ScenePlot& plot,
                                               const QString& message,
                                               const QString& undoText) {
    const auto scene = PlotBoard::sceneOf(cardId);
    if (!scene) {
        return;
    }
    const QString textId = scene->first;
    const int number = scene->second;
    const std::optional<core::ScenePlot> before = scenePlotOf(textId, number);
    if (!before || *before == plot) {
        return;
    }
    if (!changeScene(textId, number, *before, plot)) {
        notSaved();
        return;
    }
    pushChange(
        undoText,
        [this, textId, number, before, plot]() {
            return changeScene(textId, number, plot, *before);
        },
        [this, textId, number, before, plot]() {
            return changeScene(textId, number, *before, plot);
        });
    if (m_panel != nullptr) {
        m_panel->setCurrentCard(cardId);
    }
    say(message, true);
}

bool PlotBoardCoordinator::changeScene(const QString& textId, int scene,
                                       const core::ScenePlot& before,
                                       const core::ScenePlot& plot) {
    const std::optional<core::ScenePlot> now = scenePlotOf(textId, scene);
    if (!now || *now != before) {
        // The text was changed in the editor since: the scene may be another one now
        m_changedElsewhere = true;
        return false;
    }
    bool saved = false;
    ++m_ownChanges;
    editor::BookEditor* text = scene > 0 ? editorOf(textId) : nullptr;
    if (text != nullptr) {
        // The open text keeps it with its other changes, until it is saved
        saved = text->setSceneAttributes(scene, plot.toAttributes());
    } else {
        saved = m_book.setScenePlot(textId, scene, plot);
    }
    --m_ownChanges;
    refreshNow();
    if (m_panel != nullptr) {
        m_panel->setCurrentCard(PlotBoard::sceneId(textId, scene));
    }
    return saved;
}

std::optional<core::ScenePlot> PlotBoardCoordinator::scenePlotOf(const QString& textId,
                                                                 int scene) const {
    const editor::BookEditor* text = editorOf(textId);
    const QList<core::KmlScene> scenes = m_book.scenesOf(
        textId, text != nullptr ? std::optional<QString>(text->toKml()) : std::nullopt);
    if (scene < 0 || scene >= scenes.size()) {
        return std::nullopt;
    }
    return scenes.at(scene).plot;
}

void PlotBoardCoordinator::addSceneAt(const QString& textId, int at, const QString& message) {
    const std::optional<core::SceneText> text = sceneTextOf(textId);
    if (!text) {
        return;
    }
    // A text without breaks gets its second scene: the first one is the text it had
    const int place = std::max(at, 1);
    const std::optional<core::SceneText> added =
        core::ChapterDocument::withSceneAdded(*text, place);
    const std::optional<ScenesState> before = scenesStateOf(textId);
    if (!added || !before) {
        notSaved();
        return;
    }
    // The scenes in the plan after it keep their places among the scenes of the text
    ScenesState wanted{.text = *added, .plan = before->plan};
    for (core::PlannedScene& planned : wanted.plan) {
        if (planned.after >= place) {
            ++planned.after;
        }
    }
    const QString cardId = PlotBoard::sceneId(textId, place);
    changeScenes(textId, wanted, cardId, message,
                 //: What Undo undoes on the plot board, after "Undone:"; %1 the title of the
                 //: text. In Polish: dodanie sceny do „%1”
                 tr("adding a scene to \"%1\"").arg(titleOf(textId)));
    // The scenes shown, its card is edited at once
    if (m_panel != nullptr && m_panel->setCurrentCard(cardId)) {
        m_panel->editCard(cardId);
    }
}

void PlotBoardCoordinator::removeScene(const QString& textId, int scene) {
    const std::optional<core::SceneText> text = sceneTextOf(textId);
    const int count = sceneCount(textId);
    // A text with one scene has no scene break to delete
    if (!text || scene < 0 || scene >= count || count < 2) {
        return;
    }
    using dialogs::MessageDialog;
    const MessageDialog::Answer answer =
        askDeleteScene(window(), titleOf(PlotBoard::sceneId(textId, scene)), scene == 0);
    if (answer != MessageDialog::Answer::Cancel) {
        deleteScene(textId, *text, scene, answer == MessageDialog::Answer::Alternative);
    }
    // The keys go back to the board after the question
    if (m_panel != nullptr) {
        m_panel->focusView();
    }
}

void PlotBoardCoordinator::deleteScene(const QString& textId, const core::SceneText& text,
                                       int scene, bool keepText) {
    const std::optional<core::SceneText> changed =
        core::ChapterDocument::withoutScene(text, scene, keepText);
    const std::optional<ScenesState> before = scenesStateOf(textId);
    if (!changed || !before) {
        notSaved();
        return;
    }
    // The scenes in the plan after the scene that goes keep their places among the others
    ScenesState wanted{.text = *changed, .plan = before->plan};
    const int gone = keepText && scene == 0 ? 1 : scene;
    for (core::PlannedScene& planned : wanted.plan) {
        if (planned.after > gone) {
            --planned.after;
        }
    }
    const QString name = titleOf(PlotBoard::sceneId(textId, scene));
    const QString cardId = cardAfterRemoving(textId, scene, sceneCount(textId), keepText);
    if (keepText) {
        changeScenes(textId, wanted, cardId,
                     //: Under the plot board; %1 the scene. In Polish: „%1” połączono ze
                     //: sceną obok.
                     tr("\"%1\" is joined with the scene beside it.").arg(name),
                     //: What Undo undoes on the plot board, after "Undone:". In Polish:
                     //: połączenie „%1”
                     tr("joining \"%1\"").arg(name));
        return;
    }
    changeScenes(textId, wanted, cardId,
                 //: Under the plot board; %1 the scene. In Polish: Usunięto „%1”.
                 tr("Deleted \"%1\".").arg(name),
                 //: In Polish: usunięcie „%1”
                 tr("deleting \"%1\"").arg(name));
}

QString PlotBoardCoordinator::cardAfterRemoving(const QString& textId, int scene, int count,
                                                bool keepText) {
    // The scene beside is chosen; the text when it has one scene now
    const int left = count - 1;
    if (left < 2) {
        return textId;
    }
    return PlotBoard::sceneId(textId,
                              keepText ? std::max(scene - 1, 0) : std::min(scene, left - 1));
}

bool PlotBoardCoordinator::changeScenes(const QString& textId, const ScenesState& wanted,
                                        const QString& cardId, const QString& message,
                                        const QString& undoText) {
    const std::optional<ScenesState> before = scenesStateOf(textId);
    if (!before) {
        return false;
    }
    return changeScenes({ScenesChange{.textId = textId, .before = *before, .wanted = wanted}},
                        cardId, message, undoText);
}

bool PlotBoardCoordinator::changeScenes(QList<ScenesChange> changes, const QString& cardId,
                                        const QString& message, const QString& undoText) {
    changes.removeIf([](const ScenesChange& change) { return change.before == change.wanted; });
    if (changes.isEmpty()) {
        return false;
    }
    // What the texts are after each step, as their editors or the book have them: the next
    // step goes only from there
    QList<ScenesState> before;
    for (const ScenesChange& change : changes) {
        before << change.before;
    }
    auto shown = std::make_shared<QList<ScenesState>>(before);
    auto undone = std::make_shared<QList<ScenesState>>(before);
    if (!replaceAll(changes, before, false, *shown)) {
        notSaved();
        return false;
    }
    pushChange(
        undoText,
        [this, changes, shown, undone]() { return replaceAll(changes, *shown, true, *undone); },
        [this, changes, shown, undone]() { return replaceAll(changes, *undone, false, *shown); });
    forgetMarkedOf(changes);
    if (m_panel != nullptr && !m_panel->setCurrentCard(cardId)) {
        m_panel->setCurrentCard(PlotBoard::textOf(cardId));
    }
    say(message, true);
    return true;
}

bool PlotBoardCoordinator::replaceAll(const QList<ScenesChange>& changes,
                                      const QList<ScenesState>& from, bool back,
                                      QList<ScenesState>& now) {
    for (qsizetype step = 0; step < changes.size(); ++step) {
        // Undone, the last change first
        const qsizetype i = back ? changes.size() - 1 - step : step;
        const ScenesChange& change = changes.at(i);
        if (!replaceScenes(change.textId, from.at(i), back ? change.before : change.wanted,
                           now[i])) {
            return false;
        }
    }
    return true;
}

void PlotBoardCoordinator::forgetMarkedOf(const QList<ScenesChange>& changes) {
    // The scenes of the texts may be other ones now: a scene marked to move is forgotten
    const QString marked = PlotBoard::textOf(m_marked);
    const auto ofMarked = [&marked](const ScenesChange& change) {
        return change.textId == marked;
    };
    if (marked != m_marked && std::ranges::any_of(changes, ofMarked)) {
        cancelCut();
    }
}

bool PlotBoardCoordinator::replaceScenes(const QString& textId, const ScenesState& expected,
                                         const ScenesState& wanted, ScenesState& now) {
    const std::optional<ScenesState> current = scenesStateOf(textId);
    if (!current || !isAsExpected(*current, expected, wanted)) {
        m_changedElsewhere = true;
        return false;
    }
    const bool newText = expected.text != wanted.text;
    const bool newPlan = expected.plan != wanted.plan;
    ++m_ownChanges;
    bool saved = !newText || writeSceneText(textId, current->text, wanted.text);
    if (saved && newPlan) {
        saved = writePlan(textId, wanted.plan);
    }
    --m_ownChanges;
    if (const std::optional<ScenesState> after = scenesStateOf(textId)) {
        // What did not change stays as it was asked for
        now.text = newText ? after->text : wanted.text;
        now.plan = newPlan ? after->plan : wanted.plan;
    }
    refreshNow();
    return saved;
}

bool PlotBoardCoordinator::isAsExpected(const ScenesState& current, const ScenesState& expected,
                                        const ScenesState& wanted) {
    return (expected.text == wanted.text || current.text == expected.text) &&
           (expected.plan == wanted.plan || current.plan == expected.plan);
}

bool PlotBoardCoordinator::writeSceneText(const QString& textId, const core::SceneText& current,
                                          const core::SceneText& wanted) {
    editor::BookEditor* text = editorOf(textId);
    if (text == nullptr) {
        return m_book.setSceneText(textId, wanted);
    }
    // The open text keeps it with its other changes, until it is saved: one step of its Ctrl+Z
    if (current.kml != wanted.kml) {
        text->replaceWithKml(wanted.kml);
    }
    return current.opening == wanted.opening || m_book.setScenePlot(textId, 0, wanted.opening);
}

bool PlotBoardCoordinator::writePlan(const QString& textId,
                                     const QList<core::PlannedScene>& plan) {
    const core::ProjectElement* element = m_book.findElement(textId);
    if (element == nullptr) {
        return false;
    }
    core::ElementPlot plot = element->plot;
    plot.planned = plan;
    return m_book.setPlot(textId, element->description, plot);
}

std::optional<core::SceneText> PlotBoardCoordinator::sceneTextOf(const QString& textId) const {
    const core::ProjectElement* element = m_book.findElement(textId);
    if (element == nullptr) {
        return std::nullopt;
    }
    const editor::BookEditor* text = editorOf(textId);
    return core::SceneText{
        .kml = text != nullptr ? text->toKml() : m_book.chapterText(textId),
        .opening = element->plot.opening};
}

std::optional<PlotBoardCoordinator::ScenesState> PlotBoardCoordinator::scenesStateOf(
    const QString& textId) const {
    const std::optional<core::SceneText> text = sceneTextOf(textId);
    const core::ProjectElement* element = m_book.findElement(textId);
    if (!text || element == nullptr) {
        return std::nullopt;
    }
    return ScenesState{.text = *text, .plan = element->plot.planned};
}

QList<PlotBoard::SceneSlot> PlotBoardCoordinator::orderOf(const ScenesState& state) {
    const QString variant = PlotBoard::scenesVariant();
    if (variant == QLatin1String(PlotBoard::SCENES_PLAN)) {
        return PlotBoard::sceneOrder(0, state.plan);
    }
    const auto written = static_cast<int>(core::ChapterDocument::kmlScenes(state.text.kml).size());
    return PlotBoard::sceneOrder(written, variant == QLatin1String(PlotBoard::SCENES_TEXT)
                                              ? QList<core::PlannedScene>()
                                              : state.plan);
}

int PlotBoardCoordinator::sceneCount(const QString& textId) const {
    const std::optional<core::SceneText> text = sceneTextOf(textId);
    return text ? static_cast<int>(core::ChapterDocument::kmlScenes(text->kml).size()) : 0;
}

QHash<QString, QString> PlotBoardCoordinator::unsavedTexts() const {
    QHash<QString, QString> texts;
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        const auto* panel = qobject_cast<EditorPanel*>(page);
        const QString elementId = page->property("elementId").toString();
        if (panel != nullptr && !elementId.isEmpty() && panel->getBookEditor() != nullptr &&
            panel->getBookEditor()->isModified()) {
            texts.insert(elementId, panel->getBookEditor()->toKml());
        }
    }
    return texts;
}

editor::BookEditor* PlotBoardCoordinator::editorOf(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return nullptr;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        if (auto* panel = qobject_cast<EditorPanel*>(page);
            panel != nullptr && page->property("elementId").toString() == elementId) {
            return panel->getBookEditor();
        }
    }
    return nullptr;
}

}  // namespace kalahari::gui
