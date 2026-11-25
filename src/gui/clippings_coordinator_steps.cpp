// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file clippings_coordinator_steps.cpp
/// @brief Clippings: the cuts and insertions followed in the undo of their texts, and the
///        tabs, the keys and the status bar of the coordinator

#include "kalahari/gui/clippings_coordinator.h"

#include "kalahari/core/clipping.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QFile>
#include <QKeySequence>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextDocument>

#include <algorithm>
#include <list>
#include <optional>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief How long a message stays in the status bar
constexpr int MESSAGE_MS = 4000;

}  // namespace

// =============================================================================
// The undo of the texts
// =============================================================================

void ClippingsCoordinator::addStep(Step step) {
    editor::BookEditor* text = step.text;
    if (text == nullptr) {
        return;
    }
    step.id = m_nextStepId++;
    step.done = true;
    m_lastStepId = step.id;
    m_steps.push_back(std::move(step));
    follow(text);
}

void ClippingsCoordinator::follow(editor::BookEditor* text) {
    if (text == nullptr || text->textDocument() == nullptr ||
        std::ranges::find(m_followed, text, &Followed::text) != m_followed.cend()) {
        return;
    }
    Followed followed{.text = text, .object = text, .connections = {}};
    followed.connections << connect(text, &editor::BookEditor::contentChanged, this,
                                    [this, text]() { textChanged(text); });
    followed.connections << connect(text, &editor::BookEditor::documentChanged, this,
                                    [this, text]() { unfollow(text); });
    // Its steps go with it at once
    followed.connections << connect(text, &QObject::destroyed, this,
                                    [this](const QObject* gone) { forget(gone); });
    // After the editor counts the new level (it connected first)
    followed.connections << connect(text->textDocument(), &QTextDocument::undoCommandAdded,
                                    this, [this, text]() { undoAdded(text); });
    m_followed.append(followed);
}

void ClippingsCoordinator::unfollow(const editor::BookEditor* text) {
    for (qsizetype i = m_followed.size() - 1; i >= 0; --i) {
        if (m_followed.at(i).text == text) {
            for (const QMetaObject::Connection& connection :
                 std::as_const(m_followed.at(i).connections)) {
                disconnect(connection);
            }
            m_followed.removeAt(i);
        }
    }
    std::erase_if(m_steps, [text](const Step& step) { return step.text == text; });
}

void ClippingsCoordinator::forget(const QObject* gone) {
    // Only the pointers are compared: what it points to is no longer a BookEditor
    if (const auto followed = std::ranges::find(m_followed, gone, &Followed::object);
        followed != m_followed.cend()) {
        unfollow(followed->text);
    }
}

void ClippingsCoordinator::undoAdded(const editor::BookEditor* text) {
    if (m_changing) {
        return;
    }
    // The steps undone are gone from the redo of the text
    const int level = text->undoLevel();
    std::erase_if(m_steps, [text, level](const Step& step) {
        return step.text == text && !step.done && step.level >= level;
    });
}

void ClippingsCoordinator::textChanged(editor::BookEditor* text) {
    if (m_changing) {
        return;
    }
    if (text->textDocument() == nullptr) {
        unfollow(text);
        return;
    }
    // One level is undone or redone at a time: at most one step changes, but a step forgotten
    // on the way changes the list, so it is looked for again
    while (applyNextStep(text)) {
    }
}

bool ClippingsCoordinator::applyNextStep(editor::BookEditor* text) {
    const auto step = std::ranges::find_if(m_steps, [text](const Step& each) {
        return each.text == text && each.document == text->textDocument() &&
               (text->undoLevel() >= each.level) != each.done;
    });
    if (step == m_steps.end()) {
        return false;
    }
    const bool done = text->undoLevel() >= step->level;
    // A copy: the Workshop changes, and its tabs with it
    Step changing = *step;
    const bool applied = applyState(changing, done);
    if (Step* kept = stepWithId(changing.id); kept != nullptr && applied) {
        *kept = changing;
        kept->done = done;
    } else {
        forgetStep(changing.id);
    }
    refreshAfter(text);
    return true;
}

bool ClippingsCoordinator::applyState(Step& step, bool done) {
    const bool exists = done ? step.existsDone : step.existsUndone;
    const core::ClippingData& data = done ? step.dataDone : step.dataUndone;
    const core::ProjectElement* clipping = m_book.findElement(step.clippingId);
    const bool isThere = clipping != nullptr && core::ProjectManager::isClipping(*clipping);
    if (exists && isThere) {
        // It stays: only what it knows changes (marked as inserted, or not)
        if (clipping->clipping != data && !m_book.setClipping(step.clippingId, data)) {
            core::Logger::getInstance().warn("ClippingsCoordinator: '{}' keeps its data",
                                             step.title.toStdString());
        }
        return true;
    }
    if (exists) {
        return bringBack(step, data);
    }
    return !isThere || takeAway(step, *clipping);  // not there: gone already
}

bool ClippingsCoordinator::bringBack(Step& step, const core::ClippingData& data) {
    const QString oldId = step.clippingId;
    const QString newId = addAgain(step, data);
    if (newId.isEmpty()) {
        //: In Polish: Nie udało się przywrócić wycinka „%1” w Warsztacie.
        say(tr("The clipping \"%1\" could not be put back in the Workshop.").arg(step.title));
        return false;
    }
    step.clippingId = newId;
    step.fileKml = m_book.chapterText(newId);
    for (Step& each : m_steps) {
        if (each.clippingId == oldId) {
            each.clippingId = newId;
            each.fileKml = step.fileKml;
        }
    }
    say(step.isCut
            //: Ctrl+Y cut the fragment again. In Polish: Fragment znów jest w wycinkach
            //: jako „%1”.
            ? tr("The fragment is in the clippings again as \"%1\".").arg(step.title)
            //: Ctrl+Z took an insertion back. In Polish: Wycinek „%1” znów jest w
            //: Warsztacie.
            : tr("The clipping \"%1\" is in the Workshop again.").arg(step.title));
    return true;
}

bool ClippingsCoordinator::takeAway(const Step& step, const core::ProjectElement& clipping) {
    // It goes from the Workshop, unless the writer changed it since
    if (!isAsLeft(step)) {
        //: Ctrl+Z or Ctrl+Y changed the text, and the clipping stays. In Polish: Wycinek „%1”
        //: zmienił się od tamtej pory, dlatego zostaje w Warsztacie.
        say(tr("The clipping \"%1\" changed since, so it stays in the Workshop.")
                .arg(clipping.title));
        return false;
    }
    if (!removeClipping(step.clippingId)) {
        return false;
    }
    say(step.isCut
            //: Ctrl+Z took a cut back. In Polish: Fragment wrócił do tekstu, a wycinek „%1”
            //: zniknął z Warsztatu.
            ? tr("The fragment is back in the text, and the clipping \"%1\" went from the "
                 "Workshop.")
                  .arg(step.title)
            //: Ctrl+Y inserted the clipping again. In Polish: Wycinek „%1” znów jest w
            //: tekście i zniknął z Warsztatu.
            : tr("The clipping \"%1\" is in the text again and went from the Workshop.")
                  .arg(step.title));
    return true;
}

bool ClippingsCoordinator::isAsLeft(const Step& step) const {
    const core::ProjectElement* clipping = m_book.findElement(step.clippingId);
    if (clipping == nullptr || clipping->title != step.title ||
        m_book.notes(step.clippingId).trimmed() != step.note.trimmed()) {
        return false;
    }
    if (const editor::BookEditor* open = editorOf(step.clippingId);
        open != nullptr && open->isModified()) {
        return false;
    }
    return m_book.chapterText(step.clippingId) == step.fileKml;
}

bool ClippingsCoordinator::removeClipping(const QString& clippingId) {
    const core::ProjectElement* clipping = m_book.findElement(clippingId);
    if (clipping == nullptr) {
        return false;
    }
    const QStringList files = m_book.filesOf(*clipping);
    const std::optional<core::ProjectElement> removed = m_book.removeElement(clippingId);
    if (!removed) {
        //: In Polish: Nie udało się zapisać zmian.
        say(tr("Failed to save changes."));
        return false;
    }
    if (m_navigator != nullptr) {
        m_navigator->closeTabsOf(*removed);
    }
    for (const QString& file : files) {
        if (!QFile::remove(file)) {
            core::Logger::getInstance().warn("ClippingsCoordinator: '{}' stays on the disk",
                                             file.toStdString());
        }
    }
    return true;
}

QString ClippingsCoordinator::addAgain(const Step& step, const core::ClippingData& data) {
    QString clippingId = m_book.addClipping(step.title, step.kml, data);
    if (clippingId.isEmpty()) {
        return {};
    }
    if (!step.note.trimmed().isEmpty() && !m_book.setNotes(clippingId, step.note)) {
        core::Logger::getInstance().warn("ClippingsCoordinator: the note of '{}' was not saved",
                                         step.title.toStdString());
    }
    return clippingId;
}

ClippingsCoordinator::Step* ClippingsCoordinator::stepWithId(int stepId) {
    const auto step = std::ranges::find(m_steps, stepId, &Step::id);
    return step != m_steps.end() ? &*step : nullptr;
}

void ClippingsCoordinator::forgetStep(int stepId) {
    std::erase_if(m_steps, [stepId](const Step& step) { return step.id == stepId; });
}

void ClippingsCoordinator::forgetSteps() {
    for (const Followed& followed : std::as_const(m_followed)) {
        for (const QMetaObject::Connection& connection : followed.connections) {
            disconnect(connection);
        }
    }
    m_followed.clear();
    m_steps.clear();
    m_lastStepId = 0;
}

void ClippingsCoordinator::saveText(const editor::BookEditor* text) {
    const core::ProjectElement* element = elementOf(text);
    if (element != nullptr && m_navigator != nullptr &&
        !m_navigator->saveTextsOf(*element).isEmpty()) {
        core::Logger::getInstance().warn("ClippingsCoordinator: '{}' keeps its change unsaved",
                                         element->title.toStdString());
    }
}

// =============================================================================
// Helpers
// =============================================================================

const core::ProjectElement* ClippingsCoordinator::elementOf(const editor::BookEditor* text) const {
    const EditorPanel* panel = panelOf(text);
    const QString elementId =
        panel != nullptr ? panel->property("elementId").toString() : QString();
    if (elementId.isEmpty() || m_book.isBookFile(elementId)) {
        return nullptr;
    }
    return m_book.findElement(elementId);
}

EditorPanel* ClippingsCoordinator::panelOf(const editor::BookEditor* text) const {
    if (m_centralTabs == nullptr || text == nullptr) {
        return nullptr;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        if (auto* panel = qobject_cast<EditorPanel*>(page);
            panel != nullptr && panel->getBookEditor() == text) {
            return panel;
        }
    }
    return nullptr;
}

editor::BookEditor* ClippingsCoordinator::editorOf(const QString& elementId) const {
    if (m_centralTabs == nullptr || elementId.isEmpty()) {
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

editor::BookEditor* ClippingsCoordinator::open(const QString& elementId) {
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (element == nullptr) {
        return nullptr;
    }
    if (editorOf(elementId) == nullptr && m_navigator != nullptr) {
        m_navigator->onElementSelected(elementId, element->title);
    }
    EditorPanel* panel = panelOf(editorOf(elementId));
    if (panel == nullptr) {
        return nullptr;
    }
    if (m_centralTabs != nullptr) {
        DocumentWindows::of(*m_centralTabs).showPage(panel);
    }
    return panel->getBookEditor();
}

QString ClippingsCoordinator::undoKeys() const {
    const Command* command = m_commands.getCommand("edit.undo");
    if (command != nullptr && !command->shortcut.isEmpty()) {
        return command->shortcut.toQKeySequence().toString(QKeySequence::NativeText);
    }
    //: Undo without keys. In Polish: Edycja > Cofnij
    return tr("Edit > Undo");
}

void ClippingsCoordinator::say(const QString& message) const {
    if (m_statusBar != nullptr && !message.isEmpty()) {
        m_statusBar->showMessage(message, MESSAGE_MS);
    }
}

}  // namespace kalahari::gui
