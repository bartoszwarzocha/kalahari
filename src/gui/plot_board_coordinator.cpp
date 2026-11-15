// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_coordinator.cpp
/// @brief The plot board of the book on its tab in the middle of the window, and the changes
/// of the book it asks for

#include "kalahari/gui/plot_board_coordinator.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
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

#include <QLatin1String>
#include <QObject>
#include <QStatusBar>
#include <QString>
#include <QTabWidget>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>
#include <QWidget>

#include <functional>
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
      m_showProperties(std::move(showProperties)),
      m_undo(new QUndoStack(this)),
      m_refreshTimer(new QTimer(this)) {
    // Many changes at once (a text moved: the book, the Navigator) fill the board once
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

void PlotBoardCoordinator::showOnBoard(const QString& elementId) {
    PlotBoardPanel* panel = openBoard(QString(), true);
    if (panel == nullptr || elementId.isEmpty() || panel->setCurrentCard(elementId)) {
        return;
    }
    // A part: the card of its first text
    for (const PlotBand& band : panel->bands()) {
        if (band.partId == elementId && !band.cards.isEmpty()) {
            panel->setCurrentCard(band.cards.constFirst().id);
            return;
        }
    }
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
    if (!m_marked.isEmpty() && m_book.findElement(PlotBoard::textOf(m_marked)) == nullptr) {
        m_marked.clear();
    }
    m_panel->setReadOnly(m_book.isReadOnly());
    m_panel->setSuggestions(PlotBoard::povSuggestions(m_book),
                            PlotBoard::placeSuggestions(m_book));
    m_panel->setBands(PlotBoard::bandsOf(m_book, unsavedTexts()));
    m_panel->setSections(PlotBoard::sectionBand(m_book, core::BookPlace::Front),
                         PlotBoard::sectionBand(m_book, core::BookPlace::Back));
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
    // Edit > Undo and Redo follow what can be undone
    const auto undoState = [this, panel]() {
        panel->setUndoState(m_undo->canUndo(), m_undo->canRedo());
    };
    connect(m_undo, &QUndoStack::canUndoChanged, panel, undoState);
    connect(m_undo, &QUndoStack::canRedoChanged, panel, undoState);
    undoState();
    connect(panel, &PlotBoardPanel::currentCardChanged, this, [this](const QString& cardId) {
        // The text chosen shows in the Properties panel, as one chosen in the Navigator; for a
        // scene, its text
        const QString elementId = PlotBoard::textOf(cardId);
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
    connect(panel, &PlotBoardPanel::renumberRequested, this,
            &PlotBoardCoordinator::renumberTitles);
    connect(panel, &PlotBoardPanel::propertiesRequested, this,
            [this](const QString& elementId) { showProperties(elementId, true); });
    // Another variant of the scenes shows other cards
    connect(panel, &PlotBoardPanel::scenesVariantChanged, this, &PlotBoardCoordinator::refresh);
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
    // A scene in the plan beside the scenes of its text is written there
    if (const auto planned = PlotBoard::plannedOf(elementId);
        planned && !m_book.isReadOnly() &&
        PlotBoard::scenesVariant() == QLatin1String(PlotBoard::SCENES_BOTH)) {
        writePlanned(planned->first, planned->second);
        return;
    }
    // A scene: its text, at the scene; a scene in the plan only: its text
    const auto scene = PlotBoard::sceneOf(elementId);
    const QString textId = PlotBoard::textOf(elementId);
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
    if (const auto planned = PlotBoard::plannedOf(elementId)) {
        editPlanned(planned->first, planned->second, edit);
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
    if (PlotBoard::plannedOf(elementId)) {
        //: Under the plot board after a state was given to a scene in the plan; %1 the scene.
        //: In Polish: „%1” jest w planie: stan dostanie po napisaniu.
        say(tr("\"%1\" is in the plan: it gets a state once it is written.")
                .arg(titleOf(elementId)));
        return;
    }
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
    pushChange(
        //: What Undo undoes on the plot board, after "Undone:"; %1 the title. In Polish: stan
        //: „%1”
        tr("the state of \"%1\"").arg(title),
        [this, elementId, before]() { return changeStatus(elementId, before); },
        [this, elementId, status]() { return changeStatus(elementId, status); });
    //: Under the plot board after the state of a text was changed; %1 its title, %2 the state
    //: ("Draft"). In Polish: „%1” ma teraz stan: %2.
    say(tr("\"%1\" is now in the state: %2.").arg(title, PlotBoard::statusName(status)), true);
}

void PlotBoardCoordinator::setColor(const QString& elementId, const QString& color) {
    const auto scene = PlotBoard::sceneOf(elementId);
    if (m_book.isReadOnly() || m_book.findElement(PlotBoard::textOf(elementId)) == nullptr) {
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
    if (const auto planned = PlotBoard::plannedOf(elementId)) {
        colorPlanned(planned->first, planned->second, color, message, undoText);
        return;
    }
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
    const QString elementId = PlotBoard::textOf(cardId);
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
    // The numbers in the titles that no longer follow the order (PlotBoard::RENUMBER_SETTING):
    // renumbered at once in the same step, or offered on the bar
    const QList<PlotBoard::TitleChange> titles =
        PlotBoard::renumbering() == QLatin1String(PlotBoard::RENUMBER_NEVER)
            ? QList<PlotBoard::TitleChange>()
            : PlotBoard::renumbered(m_book);
    QList<PlotBoard::TitleChange> former;
    const QList<PlotBoard::TitleChange> renamed = renumberAtOnce(titles, former);
    pushChange(
        //: What Undo undoes on the plot board, after "Undone:"; %1 the title. In Polish:
        //: przeniesienie „%1”
        tr("moving \"%1\"").arg(title),
        [this, elementId, from, former]() { return retitle(former) && moveTo(elementId, *from); },
        [this, elementId, place, renamed]() {
            return moveTo(elementId, place) && retitle(renamed);
        });
    if (m_panel != nullptr) {
        m_panel->setCurrentCard(elementId);
    }
    //: Under the plot board after a card was moved; %1 its title. In Polish: Przeniesiono
    //: „%1”.
    sayMoved(tr("\"%1\" moved.").arg(title), !titles.isEmpty(), !renamed.isEmpty());
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
    pushChange(
        undoText, [this, elementId, before]() { return changeCard(elementId, before); },
        [this, elementId, text]() { return changeCard(elementId, text); });
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

void PlotBoardCoordinator::pushChange(const QString& undoText, std::function<bool()> undoIt,
                                      std::function<bool()> redoIt) {
    m_undo->push(new BoardChange(undoText, std::move(undoIt), std::move(redoIt)));
}

// =============================================================================
// Helpers
// =============================================================================

PlotBoardCoordinator::CardText PlotBoardCoordinator::cardTextOf(const QString& elementId) const {
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (element == nullptr) {
        return {};
    }
    return CardText{.title = element->title, .description = element->description,
                    .plot = element->plot};
}

QString PlotBoardCoordinator::titleOf(const QString& elementId) const {
    if (const auto planned = PlotBoard::plannedOf(elementId)) {
        const core::ProjectElement* element = m_book.findElement(planned->first);
        if (element != nullptr && planned->second < element->plot.planned.size() &&
            !element->plot.planned.at(planned->second).plot.title.isEmpty()) {
            return element->plot.planned.at(planned->second).plot.title;
        }
        //: A scene in the plan without a title in what is said under the plot board, after
        //: the title of its text ("The Storm, a scene in the plan"). In Polish: scena w planie
        const QString inPlan = tr("a scene in the plan");
        return tr("%1, %2").arg(textTitleOf(planned->first), inPlan);
    }
    if (const auto scene = PlotBoard::sceneOf(elementId)) {
        const std::optional<core::ScenePlot> plot = scenePlotOf(scene->first, scene->second);
        if (plot && !plot->title.isEmpty()) {
            return plot->title;
        }
        //: A scene without a title in what is said under the plot board; %1 the title of its
        //: text, %2 "Scene 2". In Polish: %1, %2
        return tr("%1, %2").arg(textTitleOf(scene->first), PlotBoard::sceneName(scene->second));
    }
    return textTitleOf(elementId);
}

QString PlotBoardCoordinator::textTitleOf(const QString& textId) const {
    const core::ProjectElement* element = m_book.findElement(textId);
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
