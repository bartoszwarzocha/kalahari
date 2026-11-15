// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_coordinator.cpp
/// @brief The plot board of the book on its tab in the middle of the window, and the changes
/// of the book it asks for

#include "kalahari/gui/plot_board_coordinator.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief How long the status bar shows what the board says
constexpr int MESSAGE_TIMEOUT = 3000;

/// @brief The id of the icon of the tab of the board (also Book > Plot Board)
constexpr const char* BOARD_ICON = "view.plotBoard";

/// @brief A change of the book made on the board, that Ctrl+Z undoes and Ctrl+Y does again
///
/// The board makes the change before it remembers it: the first redo(), which QUndoStack::
/// push() calls, does nothing.
class BoardChange : public QUndoCommand {
public:
    BoardChange(const QString& text, std::function<bool()> undoIt, std::function<bool()> redoIt)
        : QUndoCommand(text), m_undo(std::move(undoIt)), m_redo(std::move(redoIt)) {}

    void undo() override { m_done = m_undo(); }

    void redo() override {
        if (m_first) {
            m_first = false;
            return;
        }
        m_done = m_redo();
    }

    /// @brief Whether the book took the change when it was undone or done again last
    [[nodiscard]] bool done() const { return m_done; }

private:
    std::function<bool()> m_undo;
    std::function<bool()> m_redo;
    bool m_first = true;
    bool m_done = true;
};

}  // namespace

PlotBoardCoordinator::PlotBoardCoordinator(core::ProjectManager& book, QTabWidget* centralTabs,
                                           NavigatorCoordinator* navigator,
                                           QStatusBar* statusBar,
                                           PropertiesShower showProperties, QObject* parent)
    : QObject(parent),
      m_book(book),
      m_centralTabs(centralTabs),
      m_navigator(navigator),
      m_statusBar(statusBar),
      m_showProperties(std::move(showProperties)) {
    m_undo = new QUndoStack(this);

    // Many changes at once (a text moved: the book, the Navigator) fill the board once
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0);
    connect(m_refreshTimer, &QTimer::timeout, this, &PlotBoardCoordinator::refreshNow);

    connect(&m_book, &core::ProjectManager::projectChanged, this,
            &PlotBoardCoordinator::onProjectChanged);
    connect(&m_book, &core::ProjectManager::projectOpened, this, &PlotBoardCoordinator::forget);
    connect(&m_book, &core::ProjectManager::projectClosed, this, &PlotBoardCoordinator::forget);
    connect(m_navigator, &NavigatorCoordinator::refreshNavigatorRequested, this,
            &PlotBoardCoordinator::refresh);
    // A text saved has the words of its file
    connect(m_navigator, &NavigatorCoordinator::chapterDirtyStateChanged, this,
            &PlotBoardCoordinator::refresh);
    connect(m_centralTabs, &QTabWidget::currentChanged, this,
            &PlotBoardCoordinator::onCurrentTabChanged);
}

PlotBoardCoordinator::~PlotBoardCoordinator() = default;

PlotBoardPanel* PlotBoardCoordinator::openBoard(const QString& view, bool keys) {
    if (!m_book.isProjectOpen()) {
        //: The status bar after Book > Plot Board without a book. In Polish: Tablica fabuły
        //: pokazuje teksty otwartej książki.
        m_statusBar->showMessage(tr("The plot board shows the texts of an open book."),
                                 MESSAGE_TIMEOUT);
        return nullptr;
    }
    // A tab closed is deleted a moment later
    if (m_panel == nullptr || m_centralTabs->indexOf(m_panel) < 0) {
        m_panel = new PlotBoardPanel(m_centralTabs);
        connect(m_panel, &QObject::destroyed, this, [this](const QObject* gone) {
            if (gone == m_panel) {
                m_panel = nullptr;
            }
        });
        m_marked.clear();
        connectBoard(m_panel);
        m_stale = true;
        m_centralTabs->addTab(m_panel, QString());
    }
    if (!view.isEmpty()) {
        core::SettingsManager::getInstance().set<std::string>(PlotBoard::VIEW_SETTING,
                                                              view.toStdString());
        m_panel->showView(view);
    }
    DocumentWindows::of(*m_centralTabs).showPage(m_panel);
    if (m_stale) {
        refreshNow();
    }
    updateTab();
    if (keys) {
        m_panel->focusView();
    }
    return m_panel;
}

PlotBoardPanel* PlotBoardCoordinator::board() const {
    return m_panel;
}

void PlotBoardCoordinator::refresh() {
    m_refreshTimer->start();
}

void PlotBoardCoordinator::refreshNow() {
    m_refreshTimer->stop();
    if (m_panel == nullptr || !m_book.isProjectOpen()) {
        return;
    }
    if (m_centralTabs->currentWidget() != m_panel) {
        m_stale = true;
        return;
    }
    m_stale = false;
    const auto markedScene = PlotBoard::sceneOf(m_marked);
    if (!m_marked.isEmpty() &&
        m_book.findElement(markedScene ? markedScene->first : m_marked) == nullptr) {
        m_marked.clear();
    }
    m_panel->setReadOnly(m_book.isReadOnly());
    m_panel->setSuggestions(PlotBoard::povSuggestions(m_book),
                            PlotBoard::placeSuggestions(m_book));
    m_panel->setBands(PlotBoard::bandsOf(m_book, unsavedTexts()));
    m_panel->setMarkedCard(m_marked);
    updateTab();
}

void PlotBoardCoordinator::onCurrentTabChanged() {
    // The words of the texts change in their tabs: the board shows them when it comes back
    if (m_panel != nullptr && m_centralTabs->currentWidget() == m_panel) {
        refreshNow();
    }
}

void PlotBoardCoordinator::onProjectChanged() {
    if (m_ownChanges == 0) {
        m_undo->clear();
    }
    refresh();
}

void PlotBoardCoordinator::forget() {
    m_undo->clear();
    m_marked.clear();
    if (m_panel != nullptr) {
        m_panel->setMarkedCard(QString());
        m_panel->showMessage(QString(), false);
    }
    refresh();
}

void PlotBoardCoordinator::connectBoard(PlotBoardPanel* panel) {
    connect(panel, &PlotBoardPanel::currentCardChanged, this, [this](const QString& cardId) {
        // The text chosen shows in the Properties panel, as one chosen in the Navigator; for a
        // scene, its text
        const auto scene = PlotBoard::sceneOf(cardId);
        const QString elementId = scene ? scene->first : cardId;
        if (!elementId.isEmpty() && m_book.findElement(elementId) != nullptr) {
            m_navigator->onRequestProperties(elementId);
        }
    });
    connect(panel, &PlotBoardPanel::openRequested, this, &PlotBoardCoordinator::open);
    connect(panel, &PlotBoardPanel::editFinished, this, &PlotBoardCoordinator::edit);
    connect(panel, &PlotBoardPanel::statusRequested, this, &PlotBoardCoordinator::setStatus);
    connect(panel, &PlotBoardPanel::colorRequested, this, &PlotBoardCoordinator::setColor);
    connect(panel, &PlotBoardPanel::moveRequested, this, &PlotBoardCoordinator::move);
    connect(panel, &PlotBoardPanel::moveToTextRequested, this, &PlotBoardCoordinator::moveToText);
    connect(panel, &PlotBoardPanel::dropBesideRequested, this,
            &PlotBoardCoordinator::dropBeside);
    connect(panel, &PlotBoardPanel::dropInPartRequested, this,
            &PlotBoardCoordinator::dropInPart);
    connect(panel, &PlotBoardPanel::addRequested, this, &PlotBoardCoordinator::add);
    connect(panel, &PlotBoardPanel::addSceneRequested, this, &PlotBoardCoordinator::addScene);
    connect(panel, &PlotBoardPanel::deleteRequested, this, &PlotBoardCoordinator::remove);
    connect(panel, &PlotBoardPanel::cutRequested, this, &PlotBoardCoordinator::cut);
    connect(panel, &PlotBoardPanel::pasteRequested, this, &PlotBoardCoordinator::paste);
    connect(panel, &PlotBoardPanel::cutCancelled, this, &PlotBoardCoordinator::cancelCut);
    connect(panel, &PlotBoardPanel::undoRequested, this, &PlotBoardCoordinator::undo);
    connect(panel, &PlotBoardPanel::redoRequested, this, &PlotBoardCoordinator::redo);
    connect(panel, &PlotBoardPanel::propertiesRequested, this,
            [this](const QString& elementId) { showProperties(elementId, true); });
}

void PlotBoardCoordinator::updateTab() {
    const int index = m_panel != nullptr ? m_centralTabs->indexOf(m_panel) : -1;
    if (index < 0) {
        return;
    }
    // DockCoordinator::refreshTabIcons() gives the tab the icon of another theme
    m_panel->setProperty("tabIconId", QString::fromLatin1(BOARD_ICON));
    m_centralTabs->setTabIcon(index, core::ArtProvider::getInstance().getIcon(BOARD_ICON));
    //: The tab of the cards of the texts of the book. In Polish: Tablica fabuły
    m_centralTabs->setTabText(index, tr("Plot Board"));
    m_centralTabs->setTabToolTip(index, tr("Plot Board"));
}

// =============================================================================
// What the board asks for
// =============================================================================

void PlotBoardCoordinator::open(const QString& elementId) {
    // A scene: its text, at the scene
    const auto scene = PlotBoard::sceneOf(elementId);
    const QString textId = scene ? scene->first : elementId;
    const core::ProjectElement* element = m_book.findElement(textId);
    if (element == nullptr) {
        return;
    }
    m_navigator->onElementSelected(textId, element->title);
    if (scene) {
        if (editor::BookEditor* text = editorOf(textId)) {
            text->goToScene(scene->second);
        }
    }
}

void PlotBoardCoordinator::edit(const QString& elementId, const PlotCardEdit& edit) {
    if (m_book.isReadOnly()) {
        return;
    }
    if (const auto scene = PlotBoard::sceneOf(elementId)) {
        std::optional<core::ScenePlot> plot = scenePlotOf(scene->first, scene->second);
        if (!plot) {
            return;
        }
        // A scene may have no title
        plot->title = edit.title.trimmed();
        plot->description = edit.description;
        plot->pov = edit.plot.pov;
        plot->place = edit.plot.place;
        plot->time = edit.plot.time;
        const QString name = titleOf(PlotBoard::sceneId(scene->first, scene->second));
        changeSceneWithUndo(elementId, *plot, tr("The card \"%1\" changed.").arg(name),
                            tr("change of the card \"%1\"").arg(name));
        return;
    }
    if (m_book.findElement(elementId) == nullptr) {
        return;
    }
    CardText text = cardTextOf(elementId);
    // A text keeps its title
    if (!edit.title.trimmed().isEmpty()) {
        text.title = edit.title.trimmed();
    }
    text.description = edit.description;
    text.plot = edit.plot;
    //: Under the plot board after a card was edited; %1 the title. In Polish: Zmieniono kartę
    //: „%1”.
    const QString message = tr("The card \"%1\" changed.").arg(text.title);
    //: What Undo undoes on the plot board, after "Undone:"; %1 the title. In Polish: zmiana
    //: karty „%1”
    const QString undoText = tr("change of the card \"%1\"").arg(text.title);
    changeWithUndo(elementId, text, message, undoText);
}

void PlotBoardCoordinator::setStatus(const QString& elementId, const QString& status) {
    if (const auto scene = PlotBoard::sceneOf(elementId)) {
        std::optional<core::ScenePlot> plot = scenePlotOf(scene->first, scene->second);
        if (m_book.isReadOnly() || !plot) {
            return;
        }
        plot->status = status;
        const QString name = titleOf(elementId);
        changeSceneWithUndo(
            elementId, *plot,
            tr("\"%1\" is now in the state: %2.").arg(name, PlotBoard::statusName(status)),
            tr("the state of \"%1\"").arg(name));
        return;
    }
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (m_book.isReadOnly() || element == nullptr) {
        return;
    }
    const QString before = core::ProjectManager::statusOf(*element);
    if (before == status) {
        return;
    }
    const QString title = element->title;
    if (!changeStatus(elementId, status)) {
        notSaved();
        return;
    }
    m_undo->push(new BoardChange(
        //: What Undo undoes on the plot board, after "Undone:"; %1 the title. In Polish: stan
        //: „%1”
        tr("the state of \"%1\"").arg(title),
        [this, elementId, before]() { return changeStatus(elementId, before); },
        [this, elementId, status]() { return changeStatus(elementId, status); }));
    //: Under the plot board after the state of a text was changed; %1 its title, %2 the state
    //: ("Draft"). In Polish: „%1” ma teraz stan: %2.
    say(tr("\"%1\" is now in the state: %2.").arg(title, PlotBoard::statusName(status)), true);
}

void PlotBoardCoordinator::setColor(const QString& elementId, const QString& color) {
    const auto scene = PlotBoard::sceneOf(elementId);
    if (m_book.isReadOnly() ||
        m_book.findElement(scene ? scene->first : elementId) == nullptr) {
        return;
    }
    const QString name = titleOf(elementId);
    const QString message =
        color.isEmpty()
            //: Under the plot board; %1 the title. In Polish: Karta „%1” nie ma już koloru.
            ? tr("The card \"%1\" has no color now.").arg(name)
            //: Under the plot board; %1 the title, %2 the color ("Blue"). In Polish: Karta
            //: „%1” ma teraz kolor: %2.
            : tr("The card \"%1\" is now: %2.").arg(name, PlotColors::ownTitle(color));
    //: What Undo undoes on the plot board, after "Undone:"; %1 the title. In Polish: kolor
    //: karty „%1”
    const QString undoText = tr("the color of the card \"%1\"").arg(name);
    if (scene) {
        std::optional<core::ScenePlot> plot = scenePlotOf(scene->first, scene->second);
        if (plot) {
            plot->color = color;
            changeSceneWithUndo(elementId, *plot, message, undoText);
        }
        return;
    }
    CardText text = cardTextOf(elementId);
    text.plot.color = color;
    changeWithUndo(elementId, text, message, undoText);
}

void PlotBoardCoordinator::move(const QString& elementId, int steps) {
    if (m_book.isReadOnly() || steps == 0) {
        return;
    }
    if (const auto scene = PlotBoard::sceneOf(elementId)) {
        moveScene(scene->first, scene->second, scene->second + steps);
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

void PlotBoardCoordinator::moveToText(const QString& cardId, int /*direction*/) {
    if (m_book.isReadOnly() || !PlotBoard::sceneOf(cardId)) {
        return;
    }
    sceneInEditor();
}

void PlotBoardCoordinator::dropBeside(const QString& elementId, const QString& anchorId,
                                      bool after) {
    if (m_book.isReadOnly()) {
        return;
    }
    const auto scene = PlotBoard::sceneOf(elementId);
    const auto anchor = PlotBoard::sceneOf(anchorId);
    if (scene && anchor && scene->first == anchor->first) {
        // Among the scenes of its text: the place of the anchor in the list without it
        const int at = anchor->second > scene->second ? anchor->second - 1 : anchor->second;
        moveScene(scene->first, scene->second, after ? at + 1 : at);
        return;
    }
    if (scene || anchor) {
        sceneInEditor();
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

void PlotBoardCoordinator::dropInPart(const QString& elementId, const QString& partId) {
    if (m_book.isReadOnly()) {
        return;
    }
    if (PlotBoard::sceneOf(elementId)) {
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
    core::BookProject* project = m_book.project();
    if (project == nullptr || m_book.isReadOnly()) {
        return;
    }
    if (const auto scene = PlotBoard::sceneOf(afterId)) {
        addSceneAt(scene->first, scene->second + 1,
                   //: Under the plot board after Insert on a scene; %1 the scene. In Polish:
                   //: Dodano scenę za „%1”.
                   tr("Added a scene after \"%1\".").arg(titleOf(afterId)));
        return;
    }
    // A text of the kind of the card, right after it; without a card, a text of the main kind
    // of the book at the end of its main section
    const core::ProjectElement* after = m_book.findElement(afterId);
    const std::optional<core::ElementPlace> afterPlace =
        after != nullptr ? project->placeOf(afterId) : std::nullopt;
    // The element is no more where it was once the book has another one: its title is kept
    QString afterTitle;
    core::KindRef kind;
    core::ElementPlace place;
    if (after != nullptr && afterPlace && afterPlace->place == core::BookPlace::Main) {
        afterTitle = after->title;
        kind = m_book.kindOf(*after);
        place = *afterPlace;
        ++place.index;
    } else {
        kind = m_book.mainTextKind();
        place.index = -1;
    }
    if (!kind || !core::ProjectManager::canAdd(kind)) {
        return;
    }
    const QString title = m_book.defaultTitle(kind);
    // The places the board remembers move: what can be undone is forgotten (onProjectChanged)
    const QString added = m_book.addElement(kind, title, place.place, place.groupId, place.index);
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
    say(!afterTitle.isEmpty()
            //: Under the plot board after Insert; %1 the title of the new text, %2 the card it
            //: is after. In Polish: Dodano „%1” za „%2”.
            ? tr("Added \"%1\" after \"%2\".").arg(title, afterTitle)
            //: Under the plot board after Insert with no card; %1 the title of the new text.
            //: In Polish: Dodano „%1” na końcu książki.
            : tr("Added \"%1\" at the end of the book.").arg(title));
}

void PlotBoardCoordinator::addScene(const QString& cardId) {
    const auto scene = PlotBoard::sceneOf(cardId);
    const QString textId = scene ? scene->first : cardId;
    if (m_book.isReadOnly() || m_book.findElement(textId) == nullptr) {
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
    const auto scene = PlotBoard::sceneOf(elementId);
    if (m_book.isReadOnly() || m_panel == nullptr ||
        m_book.findElement(scene ? scene->first : elementId) == nullptr) {
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
    const auto scene = PlotBoard::sceneOf(afterId);
    if (const auto markedScene = PlotBoard::sceneOf(marked)) {
        // A scene: after a scene of its text, or first after the card of its text
        if (afterId == markedScene->first) {
            moveScene(markedScene->first, markedScene->second, 0);
        } else if (scene && scene->first == markedScene->first && afterId != marked) {
            dropBeside(marked, afterId, true);
        } else if (afterId == marked) {
            say(QString());
        } else {
            sceneInEditor();
        }
        return;
    }
    // After a scene: after its text
    const QString textId = scene ? scene->first : afterId;
    if (textId.isEmpty() || textId == marked || m_book.findElement(marked) == nullptr) {
        if (m_panel != nullptr) {
            m_panel->showMessage(QString(), false);
        }
        return;
    }
    dropBeside(marked, textId, true);
}

void PlotBoardCoordinator::cancelCut() {
    m_marked.clear();
    if (m_panel != nullptr) {
        m_panel->setMarkedCard(QString());
        m_panel->showMessage(QString(), false);
    }
}

void PlotBoardCoordinator::undo() {
    if (!m_undo->canUndo()) {
        //: Under the plot board after Ctrl+Z with nothing to undo. In Polish: Na tablicy nie ma
        //: nic do cofnięcia.
        say(tr("Nothing to undo on the board."));
        return;
    }
    const QString text = m_undo->undoText();
    const auto* change = static_cast<const BoardChange*>(m_undo->command(m_undo->index() - 1));
    m_changedElsewhere = false;
    m_undo->undo();
    if (!change->done()) {
        m_undo->clear();
        changedOrNotSaved();
        return;
    }
    //: Under the plot board after Ctrl+Z; %1 what was undone ("moving \"Chapter 3\""). In
    //: Polish: Cofnięto: %1.
    say(tr("Undone: %1.").arg(text), m_undo->canUndo());
}

void PlotBoardCoordinator::redo() {
    if (!m_undo->canRedo()) {
        //: Under the plot board after Ctrl+Y with nothing to do again. In Polish: Na tablicy nie
        //: ma nic do przywrócenia.
        say(tr("Nothing to redo on the board."));
        return;
    }
    const QString text = m_undo->redoText();
    const auto* change = static_cast<const BoardChange*>(m_undo->command(m_undo->index()));
    m_changedElsewhere = false;
    m_undo->redo();
    if (!change->done()) {
        m_undo->clear();
        changedOrNotSaved();
        return;
    }
    //: Under the plot board after Ctrl+Y; %1 what was done again. In Polish: Przywrócono: %1.
    say(tr("Redone: %1.").arg(text), true);
}

void PlotBoardCoordinator::showProperties(const QString& cardId, bool keys) {
    // A scene: its text
    const auto scene = PlotBoard::sceneOf(cardId);
    const QString elementId = scene ? scene->first : cardId;
    if (m_book.findElement(elementId) == nullptr) {
        return;
    }
    m_navigator->onRequestProperties(elementId);
    if (m_showProperties) {
        m_showProperties(keys);
    }
}

// =============================================================================
// The changes, done and undone
// =============================================================================

void PlotBoardCoordinator::moveWithUndo(const QString& elementId,
                                        const core::ElementPlace& place) {
    const core::BookProject* project = m_book.project();
    const std::optional<core::ElementPlace> from =
        project != nullptr ? project->placeOf(elementId) : std::nullopt;
    if (!from) {
        return;
    }
    // Its place in its list is also its place in the list without it
    if (*from == place) {
        return;
    }
    const QString title = titleOf(elementId);
    if (!moveTo(elementId, place)) {
        notSaved();
        return;
    }
    m_undo->push(new BoardChange(
        //: What Undo undoes on the plot board, after "Undone:"; %1 the title. In Polish:
        //: przeniesienie „%1”
        tr("moving \"%1\"").arg(title),
        [this, elementId, from]() { return moveTo(elementId, *from); },
        [this, elementId, place]() { return moveTo(elementId, place); }));
    if (m_panel != nullptr) {
        m_panel->setCurrentCard(elementId);
    }
    //: Under the plot board after a card was moved; %1 its title. In Polish: Przeniesiono
    //: „%1”.
    say(tr("\"%1\" moved.").arg(title), true);
}

void PlotBoardCoordinator::changeWithUndo(const QString& elementId, const CardText& text,
                                          const QString& message, const QString& undoText) {
    const CardText before = cardTextOf(elementId);
    if (before == text) {
        return;
    }
    if (!changeCard(elementId, text)) {
        notSaved();
        return;
    }
    m_undo->push(new BoardChange(
        undoText, [this, elementId, before]() { return changeCard(elementId, before); },
        [this, elementId, text]() { return changeCard(elementId, text); }));
    say(message, true);
}

bool PlotBoardCoordinator::moveTo(const QString& elementId, const core::ElementPlace& place) {
    if (m_book.findElement(elementId) == nullptr) {
        return false;
    }
    ++m_ownChanges;
    const bool moved = m_book.moveElementTo(elementId, place);
    --m_ownChanges;
    m_navigator->refreshNavigator();
    refreshNow();
    if (moved && m_panel != nullptr) {
        m_panel->setCurrentCard(elementId);
    }
    return moved;
}

bool PlotBoardCoordinator::changeCard(const QString& elementId, const CardText& text) {
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (element == nullptr) {
        return false;
    }
    const bool renamed = element->title != text.title;
    const bool plotChanged =
        element->description != text.description || element->plot != text.plot;
    bool saved = true;
    ++m_ownChanges;
    if (renamed) {
        saved = m_book.renameElement(elementId, text.title);
    }
    if (saved && plotChanged) {
        saved = m_book.setPlot(elementId, text.description, text.plot);
    }
    --m_ownChanges;
    if (renamed) {
        m_navigator->refreshNavigator();
        m_navigator->refreshTabTitle(elementId);
    }
    if (m_panel != nullptr && m_panel->currentCard() == elementId) {
        m_navigator->onRequestProperties(elementId);
    }
    refreshNow();
    return saved;
}

bool PlotBoardCoordinator::changeStatus(const QString& elementId, const QString& status) {
    if (m_book.findElement(elementId) == nullptr) {
        return false;
    }
    ++m_ownChanges;
    const bool saved = m_book.setStatus(elementId, status);
    --m_ownChanges;
    m_navigator->refreshNavigator();
    if (m_panel != nullptr && m_panel->currentCard() == elementId) {
        m_navigator->onRequestProperties(elementId);
    }
    refreshNow();
    return saved;
}

// =============================================================================
// The scenes of the texts
// =============================================================================

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
    m_undo->push(new BoardChange(
        undoText,
        [this, textId, number, before, plot]() {
            return changeScene(textId, number, plot, *before);
        },
        [this, textId, number, before, plot]() {
            return changeScene(textId, number, *before, plot);
        }));
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

void PlotBoardCoordinator::sceneInEditor() {
    //: Under the plot board after a scene was to be moved to another text. In Polish: Scenę
    //: do innego tekstu przenosi się w edytorze: wytnij ją tam i wklej w tamtym tekście.
    say(tr("A scene moves to another text in the editor: cut it there and paste it in the other "
           "text."));
}

void PlotBoardCoordinator::moveScene(const QString& textId, int from, int to) {
    const std::optional<core::SceneText> text = sceneTextOf(textId);
    if (!text || from == to) {
        return;
    }
    const QString name = titleOf(PlotBoard::sceneId(textId, from));
    const int count = sceneCount(textId);
    if (to < 0 || to >= count) {
        say(to < 0
                //: Under the plot board: a scene at the start of its text moved back; %1 the
                //: scene. In Polish: „%1” to pierwsza scena tekstu.
                ? tr("\"%1\" is the first scene of its text.").arg(name)
                //: In Polish: „%1” to ostatnia scena tekstu.
                : tr("\"%1\" is the last scene of its text.").arg(name));
        return;
    }
    const std::optional<core::SceneText> moved =
        core::ChapterDocument::withSceneMoved(*text, from, to);
    if (!moved) {
        notSaved();
        return;
    }
    changeScenes(textId, *moved, PlotBoard::sceneId(textId, to),
                 tr("\"%1\" moved.").arg(name), tr("moving \"%1\"").arg(name));
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
    if (!added) {
        notSaved();
        return;
    }
    const QString cardId = PlotBoard::sceneId(textId, place);
    changeScenes(textId, *added, cardId, message,
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
    if (!text || scene < 0 || scene >= count || count < 2) {
        return;
    }
    const QString name = titleOf(PlotBoard::sceneId(textId, scene));
    using dialogs::MessageDialog;
    const MessageDialog::Answer answer = MessageDialog::ask(
        window(),
        //: The title of the question. In Polish: Usuń scenę
        tr("Delete Scene"),
        (scene == 0
             //: %1 the scene. In Polish: Usunąć scenę „%1” razem z jej tekstem? Ctrl+Z na
             //: tablicy ją przywróci. (new paragraph) Zachowaj tekst usuwa tylko podział
             //: sceny: następna scena dołączy do tej.
             ? tr("Delete the scene \"%1\" with its text? Ctrl+Z on the board brings it "
                  "back.\n\nKeep the Text removes only the scene break: the next scene joins "
                  "this one.")
             //: %1 the scene. In Polish: Usunąć scenę „%1” razem z jej tekstem? Ctrl+Z na
             //: tablicy ją przywróci. (new paragraph) Zachowaj tekst usuwa tylko podział
             //: sceny: jej tekst dołączy do poprzedniej sceny.
             : tr("Delete the scene \"%1\" with its text? Ctrl+Z on the board brings it "
                  "back.\n\nKeep the Text removes only the scene break: its text joins the scene "
                  "before it."))
            .arg(name),
        //: In Polish: &Usuń z tekstem
        tr("&Delete with Text"),
        //: In Polish: &Zachowaj tekst
        tr("&Keep the Text"));
    if (answer == MessageDialog::Answer::Cancel) {
        if (m_panel != nullptr) {
            m_panel->focusView();
        }
        return;
    }
    const bool keepText = answer == MessageDialog::Answer::Alternative;
    const std::optional<core::SceneText> changed =
        core::ChapterDocument::withoutScene(*text, scene, keepText);
    if (!changed) {
        notSaved();
        return;
    }
    // The scene beside is chosen; the text when it has one scene now
    const int left = count - 1;
    const int chosen = keepText ? std::max(scene - 1, 0) : std::min(scene, left - 1);
    const QString cardId = left > 1 ? PlotBoard::sceneId(textId, chosen) : textId;
    changeScenes(textId, *changed, cardId,
                 keepText
                     //: Under the plot board; %1 the scene. In Polish: „%1” połączono ze
                     //: sceną obok.
                     ? tr("\"%1\" is joined with the scene beside it.").arg(name)
                     //: Under the plot board; %1 the scene. In Polish: Usunięto „%1”.
                     : tr("Deleted \"%1\".").arg(name),
                 keepText
                     //: What Undo undoes on the plot board, after "Undone:". In Polish:
                     //: połączenie „%1”
                     ? tr("joining \"%1\"").arg(name)
                     //: In Polish: usunięcie „%1”
                     : tr("deleting \"%1\"").arg(name));
    if (m_panel != nullptr) {
        m_panel->focusView();
    }
}

void PlotBoardCoordinator::changeScenes(const QString& textId, const core::SceneText& wanted,
                                        const QString& cardId, const QString& message,
                                        const QString& undoText) {
    const std::optional<core::SceneText> before = sceneTextOf(textId);
    if (!before || *before == wanted) {
        return;
    }
    core::SceneText done;
    if (!replaceSceneText(textId, *before, wanted, &done)) {
        notSaved();
        return;
    }
    // What the text is after each step, as its editor or its file has it: the next step goes
    // only from there
    auto shown = std::make_shared<core::SceneText>(done);
    auto undone = std::make_shared<core::SceneText>(*before);
    m_undo->push(new BoardChange(
        undoText,
        [this, textId, shown, undone, original = *before]() {
            core::SceneText now;
            if (!replaceSceneText(textId, *shown, original, &now)) {
                return false;
            }
            *undone = now;
            return true;
        },
        [this, textId, shown, undone, wanted]() {
            core::SceneText now;
            if (!replaceSceneText(textId, *undone, wanted, &now)) {
                return false;
            }
            *shown = now;
            return true;
        }));
    if (m_panel != nullptr) {
        m_panel->setCurrentCard(cardId);
    }
    say(message, true);
}

bool PlotBoardCoordinator::replaceSceneText(const QString& textId,
                                            const core::SceneText& expected,
                                            const core::SceneText& wanted,
                                            core::SceneText* now) {
    const std::optional<core::SceneText> current = sceneTextOf(textId);
    if (!current || *current != expected) {
        // The text was changed in the editor since
        m_changedElsewhere = true;
        return false;
    }
    bool saved = true;
    ++m_ownChanges;
    if (editor::BookEditor* text = editorOf(textId)) {
        // The open text keeps it with its other changes, until it is saved: one step of its
        // Ctrl+Z
        if (current->kml != wanted.kml) {
            text->replaceWithKml(wanted.kml);
        }
        if (current->opening != wanted.opening) {
            saved = m_book.setScenePlot(textId, 0, wanted.opening);
        }
    } else {
        saved = m_book.setSceneText(textId, wanted);
    }
    --m_ownChanges;
    if (const std::optional<core::SceneText> after = sceneTextOf(textId); after && now) {
        *now = *after;
    }
    refreshNow();
    return saved;
}

std::optional<core::SceneText> PlotBoardCoordinator::sceneTextOf(const QString& textId) const {
    const core::ProjectElement* element = m_book.findElement(textId);
    if (element == nullptr) {
        return std::nullopt;
    }
    const editor::BookEditor* text = editorOf(textId);
    return core::SceneText{text != nullptr ? text->toKml() : m_book.chapterText(textId),
                           element->plot.opening};
}

int PlotBoardCoordinator::sceneCount(const QString& textId) const {
    const std::optional<core::SceneText> text = sceneTextOf(textId);
    return text ? int(core::ChapterDocument::kmlScenes(text->kml).size()) : 0;
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

// =============================================================================
// Helpers
// =============================================================================

PlotBoardCoordinator::CardText PlotBoardCoordinator::cardTextOf(const QString& elementId) const {
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (element == nullptr) {
        return {};
    }
    return CardText{element->title, element->description, element->plot};
}

QString PlotBoardCoordinator::titleOf(const QString& elementId) const {
    if (const auto scene = PlotBoard::sceneOf(elementId)) {
        const std::optional<core::ScenePlot> plot = scenePlotOf(scene->first, scene->second);
        if (plot && !plot->title.isEmpty()) {
            return plot->title;
        }
        //: A scene without a title in what is said under the plot board; %1 the title of its
        //: text, %2 "Scene 2". In Polish: %1, %2
        return tr("%1, %2").arg(titleOf(scene->first), PlotBoard::sceneName(scene->second));
    }
    const core::ProjectElement* element = m_book.findElement(elementId);
    return element != nullptr ? element->title : QString();
}

void PlotBoardCoordinator::say(const QString& message, bool canUndo) {
    if (m_panel != nullptr) {
        m_panel->showMessage(message, canUndo);
    }
}

void PlotBoardCoordinator::changedOrNotSaved() {
    if (m_changedElsewhere) {
        m_changedElsewhere = false;
        //: Under the plot board after Ctrl+Z or Ctrl+Y on a scene changed in the editor since.
        //: In Polish: Scena zmieniła się w edytorze: tablica tego nie cofnie.
        say(tr("The scene changed in the editor: the board cannot undo that."));
        refresh();
        return;
    }
    notSaved();
}

void PlotBoardCoordinator::notSaved() {
    //: In Polish: Nie zapisano zmiany
    dialogs::MessageDialog::warning(window(), tr("Change Not Saved"),
                                    //: In Polish: Nie udało się zapisać zmian.
                                    tr("Failed to save changes."));
    refresh();
}

QWidget* PlotBoardCoordinator::window() const {
    return m_centralTabs->window();
}

}  // namespace kalahari::gui
