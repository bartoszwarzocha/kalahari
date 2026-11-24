// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_coordinator_texts.cpp
/// @brief What the plot board asks for of whole texts: a new text, a text in the plan without a
/// file, its file created and taken back, and a scene written added to a text

#include "kalahari/gui/plot_board_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/file_trash.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"

#include <QFile>
#include <QList>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <memory>
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

/// @brief Whether @p path can leave for good: it is not there, or the Trash takes it, or the
/// text has no words (@p empty)
bool canLeave(const QString& path, bool empty) {
    return !QFile::exists(path) || core::canMoveToTrash(path) || empty;
}

/// @brief Delete @p path: to the Trash, else for good
bool deleteFile(const QString& path) {
    if (!QFile::exists(path)) {
        return true;
    }
    return core::canMoveToTrash(path) ? core::moveToTrash(path) : QFile::remove(path);
}

}  // namespace

void PlotBoardCoordinator::addText(const QString& afterId, bool planned) {
    if (m_book.project() == nullptr || m_book.isReadOnly()) {
        return;
    }
    const NewText text = newTextAfter(m_book, afterId);
    if (!text.kind || !core::ProjectManager::canAdd(text.kind)) {
        return;
    }
    const QString title = m_book.defaultTitle(text.kind);
    ++m_ownChanges;
    const QString added =
        planned ? m_book.addPlannedText(text.kind, title, text.place.place, text.place.groupId,
                                        text.place.index)
                : m_book.addElement(text.kind, title, text.place.place, text.place.groupId,
                                    text.place.index);
    --m_ownChanges;
    const core::ProjectElement* element = m_book.findElement(added);
    const std::optional<core::ElementPlace> place =
        element != nullptr ? m_book.project()->placeOf(added) : std::nullopt;
    if (element == nullptr || !place) {
        core::Logger::getInstance().error("PlotBoardCoordinator: Failed to add '{}'",
                                          title.toStdString());
        notSaved();
        return;
    }
    // Ctrl+Z takes it out; Ctrl+Y puts it back with what its file had then
    auto kml = std::make_shared<std::optional<QString>>();
    pushChange(
        planned
            //: What Undo undoes on the plot board, after "Undone:"; %1 the title of a text in
            //: the plan. In Polish: dodanie „%1” do planu
            ? tr("adding \"%1\" to the plan").arg(title)
            //: What Undo undoes on the plot board, after "Undone:"; %1 the title of a new text.
            //: In Polish: dodanie „%1”
            : tr("adding \"%1\"").arg(title),
        [this, added, kml]() { return takeOutText(added, *kml); },
        [this, copy = *element, at = *place, kml]() { return putBackText(copy, at, *kml); },
        [this, added]() { return openChangesOf(added); });
    m_navigator->refreshNavigator();
    refreshNow();
    if (m_panel == nullptr) {
        return;
    }
    m_panel->setCurrentCard(added);
    m_panel->editCard(added);
    QString message;
    if (planned) {
        message = !text.afterTitle.isEmpty()
                      //: Under the plot board after Ctrl+Shift+Insert; %1 the title of the new
                      //: text in the plan, %2 the card it is after. In Polish: Dodano „%1” do
                      //: planu za „%2”, jeszcze bez pliku.
                      ? tr(R"(Added "%1" to the plan after "%2", without a file yet.)")
                            .arg(title, text.afterTitle)
                      //: Under the plot board after Ctrl+Shift+Insert with no card; %1 the title
                      //: of the new text in the plan. In Polish: Dodano „%1” do planu na końcu
                      //: książki, jeszcze bez pliku.
                      : tr("Added \"%1\" to the plan at the end of the book, without a file yet.")
                            .arg(title);
    } else {
        message = !text.afterTitle.isEmpty()
                      //: Under the plot board after Insert; %1 the title of the new text, %2 the
                      //: card it is after. In Polish: Dodano „%1” za „%2”.
                      ? tr(R"(Added "%1" after "%2".)").arg(title, text.afterTitle)
                      //: Under the plot board after Insert with no card; %1 the title of the
                      //: new text. In Polish: Dodano „%1” na końcu książki.
                      : tr("Added \"%1\" at the end of the book.").arg(title);
    }
    say(message, true);
}

void PlotBoardCoordinator::createFile(const QString& textId) {
    const core::ProjectElement* element = m_book.findElement(textId);
    if (element == nullptr || !m_book.isPlannedText(*element) || m_book.isReadOnly()) {
        return;
    }
    const QString title = element->title;
    ++m_ownChanges;
    const QString path = m_book.createTextFile(textId);
    --m_ownChanges;
    if (path.isEmpty()) {
        notSaved();
        return;
    }
    // Ctrl+Z puts the text back into the plan, and Ctrl+Y gives it the file it had then
    auto kml = std::make_shared<std::optional<QString>>();
    pushChange(
        //: What Undo undoes on the plot board, after "Undone:"; %1 the title of the text. In
        //: Polish: utworzenie pliku „%1”
        tr("creating the file of \"%1\"").arg(title),
        [this, textId, kml]() {
            const core::ProjectElement* text = m_book.findElement(textId);
            if (text == nullptr || text->file.isEmpty()) {
                return false;
            }
            const QString file = m_book.filePathOf(*text);
            if (!canLeave(file, m_book.wordCount(textId) == 0)) {
                return false;
            }
            *kml = m_book.chapterText(textId);
            m_navigator->closeTabsOf(*text);
            ++m_ownChanges;
            const bool back = m_book.returnToPlan(textId, core::canMoveToTrash(file));
            --m_ownChanges;
            m_navigator->refreshNavigator();
            refreshNow();
            if (back && m_panel != nullptr) {
                m_panel->setCurrentCard(textId);
            }
            return back;
        },
        [this, textId, kml]() {
            ++m_ownChanges;
            const bool created = !m_book.createTextFile(textId, *kml).isEmpty();
            --m_ownChanges;
            m_navigator->refreshNavigator();
            refreshNow();
            if (created && m_panel != nullptr) {
                m_panel->setCurrentCard(textId);
            }
            return created;
        },
        [this, textId]() { return openChangesOf(textId); });
    m_navigator->refreshNavigator();
    refreshNow();
    // The new text opens in the editor; the board keeps its card chosen
    m_navigator->onElementSelected(textId, title);
    if (m_panel != nullptr) {
        m_panel->setCurrentCard(textId);
    }
    //: Under the plot board after "Create the Chapter File"; %1 the title of the text. In
    //: Polish: Utworzono plik rozdziału „%1” i otwarto go w edytorze. Sceny z planu czekają na
    //: „Napisz scenę”.
    say(tr("Created the chapter file of \"%1\" and opened it in the editor. The scenes of the "
           "plan wait for Write the Scene.")
            .arg(title),
        true);
}

void PlotBoardCoordinator::removePlannedText(const QString& textId) {
    const core::ProjectElement* element = m_book.findElement(textId);
    const std::optional<core::ElementPlace> place =
        element != nullptr ? m_book.project()->placeOf(textId) : std::nullopt;
    if (element == nullptr || !place || !m_book.isPlannedText(*element)) {
        return;
    }
    const core::ProjectElement copy = *element;
    // The card beside it is chosen then
    const QList<PlotCard> cards = PlotBoard::cardsOf(m_panel != nullptr ? m_panel->bands()
                                                                         : QList<PlotBand>());
    QString next;
    for (qsizetype i = 0; i < cards.size(); ++i) {
        if (cards.at(i).id == textId) {
            next = i + 1 < cards.size() ? cards.at(i + 1).id
                   : i > 0              ? cards.at(i - 1).id
                                        : QString();
        }
    }
    std::optional<QString> none;
    if (!takeOutText(textId, none)) {
        notSaved();
        return;
    }
    pushChange(
        //: What Undo undoes on the plot board, after "Undone:"; %1 the title of a text in the
        //: plan. In Polish: usunięcie „%1” z planu
        tr("deleting \"%1\" from the plan").arg(copy.title),
        [this, copy, at = *place]() { return putBackText(copy, at, std::nullopt); },
        [this, textId]() {
            std::optional<QString> nothing;
            return takeOutText(textId, nothing);
        });
    if (m_panel != nullptr && !next.isEmpty()) {
        m_panel->setCurrentCard(next);
    }
    //: Under the plot board after Delete on a text in the plan; %1 its title. In Polish:
    //: Usunięto „%1” z planu.
    say(tr("Deleted \"%1\" from the plan.").arg(copy.title), true);
}

void PlotBoardCoordinator::newScene(const QString& cardId) {
    const QString textId = PlotBoard::textOf(cardId);
    if (m_book.isReadOnly() || m_book.findElement(textId) == nullptr ||
        m_book.isPlannedText(textId)) {
        return;
    }
    // After a scene written; before a scene in the plan, after the scenes written before it;
    // else at the end of the text
    int at = sceneCount(textId);
    if (const auto scene = PlotBoard::sceneOf(cardId)) {
        at = scene->second + 1;
    } else if (PlotBoard::plannedOf(cardId)) {
        if (const std::optional<ScenesState> state = scenesStateOf(textId)) {
            const QList<PlotBoard::SceneSlot> order = orderOf(*state);
            const qsizetype slot = order.indexOf(PlotBoard::slotOf(cardId));
            if (slot >= 0) {
                at = static_cast<int>(std::count_if(
                    order.cbegin(), order.cbegin() + slot,
                    [](const PlotBoard::SceneSlot& each) { return !each.planned; }));
            }
        }
    }
    // A text without breaks gets its second scene
    const int number = std::max(at, 1) + 1;
    //: Under the plot board after "New Scene"; %1 its number in the text, %2 the title of the
    //: text. In Polish: Dodano scenę %1 do „%2”: w tekście jest nowy podział sceny.
    addSceneAt(textId, at,
               tr("Added scene %1 to \"%2\": the text has a new scene break.")
                   .arg(number)
                   .arg(titleOf(textId)));
}

bool PlotBoardCoordinator::takeOutText(const QString& elementId, std::optional<QString>& kml) {
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (element == nullptr) {
        return false;
    }
    const QStringList files = m_book.filesOf(*element);
    const bool empty = m_book.wordCount(elementId) == 0;
    for (const QString& file : files) {
        if (!canLeave(file, empty)) {
            return false;
        }
    }
    kml = element->file.isEmpty() ? std::nullopt : std::optional(m_book.chapterText(elementId));
    m_navigator->closeTabsOf(*element);
    ++m_ownChanges;
    const bool removed = m_book.removeElement(elementId).has_value();
    --m_ownChanges;
    if (removed) {
        for (const QString& file : files) {
            if (!deleteFile(file)) {
                core::Logger::getInstance().warn("PlotBoardCoordinator: {} stays",
                                                 file.toStdString());
            }
        }
    }
    m_navigator->refreshNavigator();
    refreshNow();
    return removed;
}

bool PlotBoardCoordinator::putBackText(const core::ProjectElement& element,
                                       const core::ElementPlace& place,
                                       const std::optional<QString>& kml) {
    ++m_ownChanges;
    const bool back = m_book.insertElement(element, place, kml);
    --m_ownChanges;
    m_navigator->refreshNavigator();
    refreshNow();
    if (back && m_panel != nullptr) {
        m_panel->setCurrentCard(element.id);
    }
    return back;
}

QString PlotBoardCoordinator::openChangesOf(const QString& elementId) const {
    if (!m_navigator->isChapterDirty(elementId)) {
        return {};
    }
    //: Under the plot board after Ctrl+Z on a new text with changes not saved in its tab; %1
    //: its title. In Polish: „%1” ma niezapisane zmiany: najpierw trzeba je zapisać albo
    //: porzucić.
    return tr("\"%1\" has changes not saved: save or discard them first.")
        .arg(textTitleOf(elementId));
}

}  // namespace kalahari::gui
