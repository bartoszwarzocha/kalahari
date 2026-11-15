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
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/plot_board_panel.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>

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
    if (!m_marked.isEmpty() && m_book.findElement(m_marked) == nullptr) {
        m_marked.clear();
    }
    m_panel->setReadOnly(m_book.isReadOnly());
    m_panel->setSuggestions(PlotBoard::povSuggestions(m_book),
                            PlotBoard::placeSuggestions(m_book));
    m_panel->setBands(PlotBoard::bandsOf(m_book));
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
    connect(panel, &PlotBoardPanel::currentCardChanged, this, [this](const QString& elementId) {
        // The text chosen shows in the Properties panel, as one chosen in the Navigator
        if (!elementId.isEmpty() && m_book.findElement(elementId) != nullptr) {
            m_navigator->onRequestProperties(elementId);
        }
    });
    connect(panel, &PlotBoardPanel::openRequested, this, &PlotBoardCoordinator::open);
    connect(panel, &PlotBoardPanel::editFinished, this, &PlotBoardCoordinator::edit);
    connect(panel, &PlotBoardPanel::statusRequested, this, &PlotBoardCoordinator::setStatus);
    connect(panel, &PlotBoardPanel::colorRequested, this, &PlotBoardCoordinator::setColor);
    connect(panel, &PlotBoardPanel::moveRequested, this, &PlotBoardCoordinator::move);
    connect(panel, &PlotBoardPanel::dropBesideRequested, this,
            &PlotBoardCoordinator::dropBeside);
    connect(panel, &PlotBoardPanel::dropInPartRequested, this,
            &PlotBoardCoordinator::dropInPart);
    connect(panel, &PlotBoardPanel::addRequested, this, &PlotBoardCoordinator::add);
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
    if (const core::ProjectElement* element = m_book.findElement(elementId)) {
        m_navigator->onElementSelected(elementId, element->title);
    }
}

void PlotBoardCoordinator::edit(const QString& elementId, const PlotCardEdit& edit) {
    if (m_book.isReadOnly() || m_book.findElement(elementId) == nullptr) {
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
    if (m_book.isReadOnly() || m_book.findElement(elementId) == nullptr) {
        return;
    }
    CardText text = cardTextOf(elementId);
    text.plot.color = color;
    const QString message =
        color.isEmpty()
            //: Under the plot board; %1 the title. In Polish: Karta „%1” nie ma już koloru.
            ? tr("The card \"%1\" has no color now.").arg(text.title)
            //: Under the plot board; %1 the title, %2 the color ("Blue"). In Polish: Karta
            //: „%1” ma teraz kolor: %2.
            : tr("The card \"%1\" is now: %2.").arg(text.title, PlotColors::ownTitle(color));
    //: What Undo undoes on the plot board, after "Undone:"; %1 the title. In Polish: kolor
    //: karty „%1”
    changeWithUndo(elementId, text, message, tr("the color of the card \"%1\"").arg(text.title));
}

void PlotBoardCoordinator::move(const QString& elementId, int steps) {
    if (m_book.isReadOnly() || steps == 0) {
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

void PlotBoardCoordinator::dropBeside(const QString& elementId, const QString& anchorId,
                                      bool after) {
    if (m_book.isReadOnly()) {
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

void PlotBoardCoordinator::remove(const QString& elementId) {
    if (m_book.isReadOnly() || m_book.findElement(elementId) == nullptr) {
        return;
    }
    // The Navigator asks first and says where the files go
    m_navigator->onRequestDelete(elementId);
    if (m_panel != nullptr) {
        m_panel->focusView();
    }
}

void PlotBoardCoordinator::cut(const QString& elementId) {
    if (m_book.isReadOnly() || m_book.findElement(elementId) == nullptr || m_panel == nullptr) {
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
    if (afterId.isEmpty() || afterId == marked || m_book.findElement(marked) == nullptr) {
        if (m_panel != nullptr) {
            m_panel->showMessage(QString(), false);
        }
        return;
    }
    dropBeside(marked, afterId, true);
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
    m_undo->undo();
    if (!change->done()) {
        m_undo->clear();
        notSaved();
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
    m_undo->redo();
    if (!change->done()) {
        m_undo->clear();
        notSaved();
        return;
    }
    //: Under the plot board after Ctrl+Y; %1 what was done again. In Polish: Przywrócono: %1.
    say(tr("Redone: %1.").arg(text), true);
}

void PlotBoardCoordinator::showProperties(const QString& elementId, bool keys) {
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
    const core::ProjectElement* element = m_book.findElement(elementId);
    return element != nullptr ? element->title : QString();
}

void PlotBoardCoordinator::say(const QString& message, bool canUndo) {
    if (m_panel != nullptr) {
        m_panel->showMessage(message, canUndo);
    }
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
