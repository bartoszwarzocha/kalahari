// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_panel.cpp
/// @brief The tab of the plot board in the middle of the window: the board of the cards or the
/// outline, and what was just done; the switches over them and the look from the settings are
/// in plot_board_panel_header.cpp

#include "kalahari/gui/panels/plot_board_panel.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/edit_target.h"
#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/plot_outline_model.h"
#include "kalahari/gui/widgets/info_bar.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QAbstractItemView>
#include <QActionGroup>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

namespace kalahari::gui {

namespace {

constexpr int SWATCH_SIZE = 12;  ///< Of a color in the menu Card Color

// The widths of the columns of the outline in average characters
constexpr int NUMBER_CHARACTERS = 5;   ///< The number of a text
constexpr int TITLE_CHARACTERS = 24;   ///< Its title
constexpr int SHORT_CHARACTERS = 14;   ///< Its point of view, time and state
constexpr int PLACE_CHARACTERS = 18;   ///< Its place
constexpr int WORDS_CHARACTERS = 9;    ///< Its words
constexpr int OTHER_CHARACTERS = 40;   ///< The others

/// @brief The width of a column of the outline in average characters
int charactersOf(int column) {
    switch (column) {
    case PlotOutlineModel::NUMBER_COLUMN:
        return NUMBER_CHARACTERS;
    case PlotOutlineModel::TITLE_COLUMN:
        return TITLE_CHARACTERS;
    case PlotOutlineModel::POV_COLUMN:
    case PlotOutlineModel::TIME_COLUMN:
    case PlotOutlineModel::STATUS_COLUMN:
        return SHORT_CHARACTERS;
    case PlotOutlineModel::PLACE_COLUMN:
        return PLACE_CHARACTERS;
    case PlotOutlineModel::WORDS_COLUMN:
        return WORDS_CHARACTERS;
    default:
        break;
    }
    return OTHER_CHARACTERS;
}

using Key = PlotBoardView::Key;

/// @brief A key of the outline that asks for something with the card chosen
struct CardRequest {
    constexpr CardRequest(Key requestKey, void (PlotBoardPanel::*requestSignal)(const QString&))
        : key(requestKey), signal(requestSignal) {}

    Key key;                                        ///< The key
    void (PlotBoardPanel::*signal)(const QString&);  ///< What it asks for, with the card
};

/// @brief A key of the outline that asks for something with the whole board
struct BoardRequest {
    constexpr BoardRequest(Key requestKey, void (PlotBoardPanel::*requestSignal)())
        : key(requestKey), signal(requestSignal) {}

    Key key;                          ///< The key
    void (PlotBoardPanel::*signal)();  ///< What it asks for
};

/// @brief The keys of the board the outline takes for the card chosen; Enter on a cell edited
/// is the cell's
constexpr std::array<CardRequest, 7> CARD_REQUESTS{{
    {Key::Open, &PlotBoardPanel::openRequested},
    {Key::Properties, &PlotBoardPanel::propertiesRequested},
    {Key::Add, &PlotBoardPanel::addRequested},
    {Key::AddScene, &PlotBoardPanel::addSceneRequested},
    {Key::Delete, &PlotBoardPanel::deleteRequested},
    {Key::Cut, &PlotBoardPanel::cutRequested},
    {Key::Paste, &PlotBoardPanel::pasteRequested},
}};

/// @brief The keys of the board the outline takes for the whole board
constexpr std::array<BoardRequest, 3> BOARD_REQUESTS{{
    {Key::Undo, &PlotBoardPanel::undoRequested},
    {Key::Redo, &PlotBoardPanel::redoRequested},
    {Key::CancelCut, &PlotBoardPanel::cutCancelled},
}};

/// @brief The request of @p key in @p requests; nullptr: none
template <typename Request, std::size_t COUNT>
const Request* requestOf(const std::array<Request, COUNT>& requests, Key key) {
    for (const Request& request : requests) {
        if (request.key == key) {
            return &request;
        }
    }
    return nullptr;
}

/// @brief What the items of the menu of a card say, by what the card is
struct CardMenuTexts {
    QString open;         ///< Return
    bool writes = false;  ///< Return writes a scene in the plan in its text
    QString add;          ///< Insert
    QString addAtEnd;     ///< Alt+Insert
    QString remove;       ///< Delete
};

/// @brief What Insert and Alt+Insert on card @p card say
void addTextsOf(const std::optional<PlotCard>& card, CardMenuTexts& texts) {
    if (PlotBoard::scenesVariant() == QLatin1String(PlotBoard::SCENES_TEXT)) {
        texts.add = card && card->isScene()
                        //: A new scene right after the card of a scene. In Polish: &Nowa scena
                        //: za nią
                        ? PlotBoardPanel::tr("&New Scene After It")
                        //: A new text of the same kind right after the card. In Polish: &Nowa
                        //: karta za nią
                        : PlotBoardPanel::tr("&New Card After It");
        //: A new scene at the end of the text of the card. In Polish: Nowa scena n&a końcu
        //: tekstu
        texts.addAtEnd = PlotBoardPanel::tr("New Scene &at the End of the Text");
        return;
    }
    texts.add = card && card->isScene()
                    //: A new scene in the plan of the text, right after the card of a scene.
                    //: In Polish: &Nowa scena w planie za nią
                    ? PlotBoardPanel::tr("&New Scene in the Plan After It")
                    : PlotBoardPanel::tr("&New Card After It");
    //: A new scene at the end of the plan of the text of the card. In Polish: Nowa scena w
    //: planie n&a końcu tekstu
    texts.addAtEnd = PlotBoardPanel::tr("New Scene in the Plan &at the End of the Text");
}

/// @brief What the items of the menu of card @p card say
CardMenuTexts menuTextsOf(const std::optional<PlotCard>& card) {
    CardMenuTexts texts;
    const bool planned = card && card->planned;
    texts.writes =
        planned && PlotBoard::scenesVariant() == QLatin1String(PlotBoard::SCENES_BOTH);
    texts.open = texts.writes
                     //: The menu of a card of a scene in the plan: a scene break with what it
                     //: says at its place in the text. In Polish: Na&pisz scenę
                     ? PlotBoardPanel::tr("&Write the Scene")
                     //: The menu of a card of the plot board. In Polish: &Otwórz
                     : PlotBoardPanel::tr("&Open");
    addTextsOf(card, texts);
    if (planned) {
        //: The menu of the card of a scene in the plan. In Polish: &Usuń z planu
        texts.remove = PlotBoardPanel::tr("&Delete from the Plan");
    } else {
        texts.remove = card && card->isScene()
                           //: The menu of the card of a scene. In Polish: &Usuń scenę…
                           ? PlotBoardPanel::tr("&Delete Scene...")
                           //: In Polish: &Usuń z książki…
                           : PlotBoardPanel::tr("&Delete from Book...");
    }
    return texts;
}

}  // namespace

PlotBoardPanel::PlotBoardPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("plotBoardPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("plotBoardHeader"));
    buildHeader(header);
    layout->addWidget(header);
    buildBody(layout);
    connectBoard();
    connectOutline();
    followSettings();

    applySettings();
    for (int column = 0; column < PlotOutlineModel::COLUMN_COUNT; ++column) {
        if (column != PlotOutlineModel::DESCRIPTION_COLUMN) {
            m_outline->setColumnWidth(column,
                                      m_outline->fontMetrics().averageCharWidth() *
                                          charactersOf(column));
        }
    }
    showView(PlotBoard::startView());
}

void PlotBoardPanel::buildBody(QVBoxLayout* layout) {
    m_stack = new QStackedWidget(this);
    m_board = new PlotBoardView(m_stack);
    m_outline = new QTableView(m_stack);
    m_outline->setObjectName(QStringLiteral("plotOutline"));
    //: The outline of the book, for the screen reader. In Polish: Konspekt
    m_outline->setAccessibleName(tr("Outline"));
    m_model = new PlotOutlineModel(this);
    m_delegate = new PlotOutlineDelegate(this);
    m_outline->setModel(m_model);
    m_outline->setItemDelegate(m_delegate);
    m_outline->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_outline->setSelectionMode(QAbstractItemView::SingleSelection);
    m_outline->setEditTriggers(QAbstractItemView::DoubleClicked |
                               QAbstractItemView::EditKeyPressed |
                               QAbstractItemView::AnyKeyPressed);
    m_outline->setTabKeyNavigation(true);
    m_outline->setWordWrap(false);
    m_outline->verticalHeader()->hide();
    m_outline->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_outline->horizontalHeader()->setStretchLastSection(false);
    m_outline->horizontalHeader()->setSectionResizeMode(PlotOutlineModel::DESCRIPTION_COLUMN,
                                                        QHeaderView::Stretch);
    m_outline->setContextMenuPolicy(Qt::CustomContextMenu);
    m_outline->installEventFilter(this);
    m_stack->addWidget(m_board);
    m_stack->addWidget(m_outline);
    layout->addWidget(m_stack, 1);

    m_infoBar = new InfoBar(this);
    m_infoBar->setObjectName(QStringLiteral("plotBoardInfoBar"));
    m_infoBar->hide();
    layout->addWidget(m_infoBar);
    connect(m_infoBar, &InfoBar::actionClicked, this, [this]() {
        if (m_offersRenumbering) {
            emit renumberRequested();
        } else {
            emit undoRequested();
        }
    });
    connect(m_infoBar, &InfoBar::dismissed, m_infoBar, &QWidget::hide);

    m_menu = new QMenu(this);
}

void PlotBoardPanel::connectBoard() {
    connect(m_board, &PlotBoardView::currentCardChanged, this, [this](const QString& id) {
        const int row = m_model->rowOf(id);
        if (row >= 0 && m_stack->currentWidget() == m_board) {
            m_outline->setCurrentIndex(m_model->index(row, PlotOutlineModel::TITLE_COLUMN));
        }
        emit currentCardChanged(id);
        emit editStatesChanged();
    });
    connect(m_board, &PlotBoardView::openRequested, this, &PlotBoardPanel::openRequested);
    connect(m_board, &PlotBoardView::editRequested, this, &PlotBoardPanel::editCard);
    connect(m_board, &PlotBoardView::editFinished, this, &PlotBoardPanel::editFinished);
    connect(m_board, &PlotBoardView::moveRequested, this, &PlotBoardPanel::moveRequested);
    connect(m_board, &PlotBoardView::moveToTextRequested, this,
            &PlotBoardPanel::moveToTextRequested);
    connect(m_board, &PlotBoardView::dropBesideRequested, this,
            &PlotBoardPanel::dropBesideRequested);
    connect(m_board, &PlotBoardView::dropInPartRequested, this,
            &PlotBoardPanel::dropInPartRequested);
    connect(m_board, &PlotBoardView::addRequested, this, &PlotBoardPanel::addRequested);
    connect(m_board, &PlotBoardView::addSceneRequested, this, &PlotBoardPanel::addSceneRequested);
    connect(m_board, &PlotBoardView::deleteRequested, this, &PlotBoardPanel::deleteRequested);
    connect(m_board, &PlotBoardView::cutRequested, this, &PlotBoardPanel::cutRequested);
    connect(m_board, &PlotBoardView::pasteRequested, this, &PlotBoardPanel::pasteRequested);
    connect(m_board, &PlotBoardView::cutCancelled, this, &PlotBoardPanel::cutCancelled);
    connect(m_board, &PlotBoardView::undoRequested, this, &PlotBoardPanel::undoRequested);
    connect(m_board, &PlotBoardView::redoRequested, this, &PlotBoardPanel::redoRequested);
    connect(m_board, &PlotBoardView::propertiesRequested, this,
            &PlotBoardPanel::propertiesRequested);
    connect(m_board, &PlotBoardView::menuRequested, this, &PlotBoardPanel::showMenu);
}

void PlotBoardPanel::connectOutline() {
    connect(m_model, &PlotOutlineModel::cellEdited, this, &PlotBoardPanel::onOutlineEdited);
    connect(m_outline->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current) {
                const QString id = m_model->cardIdAt(current.row());
                if (!id.isEmpty() && m_stack->currentWidget() == m_outline) {
                    m_board->setCurrentCard(id);
                }
            });
    connect(m_outline, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        // A double-click edits the cells that can be edited, and opens the text on the others
        const QString id = m_model->cardIdAt(index.row());
        if (!m_model->flags(index).testFlag(Qt::ItemIsEditable) && !id.isEmpty()) {
            emit openRequested(id);
        }
    });
    connect(m_outline, &QTableView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = m_outline->indexAt(pos);
        if (index.isValid()) {
            m_outline->setCurrentIndex(index);
        }
        showMenu(m_model->cardIdAt(index.row()), m_outline->viewport()->mapToGlobal(pos));
    });
}

PlotBoardPanel::~PlotBoardPanel() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

void PlotBoardPanel::setSections(const PlotBand& front, const PlotBand& back) {
    m_board->setSections(front, back);
}

void PlotBoardPanel::setBands(const QList<PlotBand>& bands) {
    m_bands = bands;
    const QString current = currentCard();
    m_board->setBands(bands);
    m_model->setBands(bands);
    const int row = m_model->rowOf(m_board->currentCard().isEmpty() ? current
                                                                    : m_board->currentCard());
    if (row >= 0 && !m_outline->currentIndex().isValid()) {
        m_outline->setCurrentIndex(m_model->index(row, PlotOutlineModel::TITLE_COLUMN));
    }
    updateTotals();
    emit editStatesChanged();
}

void PlotBoardPanel::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    m_board->setReadOnly(readOnly);
    m_model->setReadOnly(readOnly);
    emit editStatesChanged();
}

void PlotBoardPanel::setSuggestions(const QStringList& povs, const QStringList& places) {
    m_povs = povs;
    m_places = places;
    m_delegate->setSuggestions(povs, places);
}

void PlotBoardPanel::showView(const QString& view) {
    const bool outline = view == QLatin1String(PlotBoard::VIEW_OUTLINE);
    const QString current = currentCard();
    m_stack->setCurrentWidget(outline ? static_cast<QWidget*>(m_outline)
                                      : static_cast<QWidget*>(m_board));
    if (!current.isEmpty()) {
        setCurrentCard(current);
    }
    updateSwitches();
    emit editStatesChanged();
}

QString PlotBoardPanel::view() const {
    return QString::fromLatin1(m_stack->currentWidget() == m_outline ? PlotBoard::VIEW_OUTLINE
                                                                     : PlotBoard::VIEW_BOARD);
}

void PlotBoardPanel::focusView() {
    m_stack->currentWidget()->setFocus(Qt::OtherFocusReason);
}

QString PlotBoardPanel::currentCard() const {
    if (m_stack != nullptr && m_stack->currentWidget() == m_outline) {
        const QString id = m_model->cardIdAt(m_outline->currentIndex().row());
        if (!id.isEmpty()) {
            return id;
        }
    }
    return m_board != nullptr ? m_board->currentCard() : QString();
}

bool PlotBoardPanel::setCurrentCard(const QString& elementId) {
    const bool found = m_board->setCurrentCard(elementId);
    const int row = m_model->rowOf(elementId);
    if (row >= 0) {
        const int column = m_outline->currentIndex().isValid()
                               ? m_outline->currentIndex().column()
                               : PlotOutlineModel::TITLE_COLUMN;
        m_outline->setCurrentIndex(m_model->index(row, column));
    }
    return found;
}

void PlotBoardPanel::setMarkedCard(const QString& elementId) {
    m_board->setMarkedCard(elementId);
    emit editStatesChanged();
}

void PlotBoardPanel::setUndoState(bool canUndo, bool canRedo) {
    m_canUndo = canUndo;
    m_canRedo = canRedo;
    emit editStatesChanged();
}

bool PlotBoardPanel::takesEdit(EditCommand command) const {
    // Copy and Select All stay the outline's own (the text of a cell), and do nothing on the
    // board
    return command != EditCommand::Copy && command != EditCommand::SelectAll;
}

bool PlotBoardPanel::canEditHere(EditCommand command) const {
    const QString current = currentCard();
    const bool changing = card(current).has_value() && !m_readOnly;
    switch (command) {
    case EditCommand::Undo:
        return m_canUndo;
    case EditCommand::Redo:
        return m_canRedo;
    case EditCommand::Cut:
    case EditCommand::Delete:
        return changing;
    case EditCommand::Paste:
        return changing && canPasteAfter(current);
    default:
        return false;
    }
}

void PlotBoardPanel::editHere(EditCommand command) {
    const QString current = currentCard();
    switch (command) {
    case EditCommand::Undo:
        emit undoRequested();
        break;
    case EditCommand::Redo:
        emit redoRequested();
        break;
    case EditCommand::Cut:
        emit cutRequested(current);
        break;
    case EditCommand::Paste:
        emit pasteRequested(current);
        break;
    case EditCommand::Delete:
        emit deleteRequested(current);
        break;
    default:
        break;
    }
}

QMetaObject::Connection PlotBoardPanel::followEditStates(QObject* receiver,
                                                         std::function<void()> changed) {
    return connect(this, &PlotBoardPanel::editStatesChanged, receiver, std::move(changed));
}

bool PlotBoardPanel::editCard(const QString& elementId) {
    if (m_stack->currentWidget() == m_board) {
        return m_board->editCard(elementId, m_povs, m_places);
    }
    const int row = m_model->rowOf(elementId);
    const QModelIndex title = m_model->index(row, PlotOutlineModel::TITLE_COLUMN);
    if (row < 0 || !(m_model->flags(title) & Qt::ItemIsEditable)) {
        return false;
    }
    m_outline->setCurrentIndex(title);
    m_outline->edit(title);
    return m_outline->indexWidget(title) != nullptr;
}

void PlotBoardPanel::offerRenumbering(const QString& message) {
    m_offersRenumbering = true;
    m_infoBar->setMessage(message);
    //: The button of the bar under the plot board that renumbers the titles of the texts after
    //: a move ("Chapter 3"). In Polish: Ponume&ruj od nowa
    m_infoBar->setActionText(tr("&Renumber"));
    m_infoBar->setActionToolTip(QString());
    m_infoBar->setVisible(true);
}

void PlotBoardPanel::showMessage(const QString& message, bool canUndo) {
    m_offersRenumbering = false;
    m_infoBar->setMessage(message);
    //: The button of the bar under the plot board that undoes what it says. In Polish: &Cofnij
    m_infoBar->setActionText(canUndo ? tr("&Undo") : QString());
    m_infoBar->setActionToolTip(canUndo ? QStringLiteral("Ctrl+Z") : QString());
    m_infoBar->setVisible(!message.isEmpty());
}

QString PlotBoardPanel::message() const {
    return m_infoBar->isHidden() ? QString() : m_infoBar->message();
}

std::optional<PlotCard> PlotBoardPanel::card(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return std::nullopt;
    }
    for (const PlotCard& each : PlotBoard::allCardsOf(m_bands)) {
        if (each.id == elementId) {
            return each;
        }
    }
    return std::nullopt;
}

void PlotBoardPanel::onOutlineEdited(const QString& elementId, int column,
                                     const QString& value) {
    const std::optional<PlotCard> found = card(elementId);
    if (!found) {
        return;
    }
    if (column == PlotOutlineModel::STATUS_COLUMN) {
        emit statusRequested(elementId, value);
        return;
    }
    PlotCardEdit edit{.title = found->title, .description = found->description,
                      .plot = found->plot};
    switch (column) {
    case PlotOutlineModel::TITLE_COLUMN:
        edit.title = value;
        break;
    case PlotOutlineModel::DESCRIPTION_COLUMN:
        edit.description = value;
        break;
    case PlotOutlineModel::POV_COLUMN:
        edit.plot.pov = value;
        break;
    case PlotOutlineModel::PLACE_COLUMN:
        edit.plot.place = value;
        break;
    case PlotOutlineModel::TIME_COLUMN:
        edit.plot.time = value;
        break;
    default:
        return;
    }
    emit editFinished(elementId, edit);
}

QAction* PlotBoardPanel::addMenuAction(const QString& text, const QString& keys, bool enabled,
                                       const std::function<void()>& run) {
    QAction* action = m_menu->addAction(text);
    if (!keys.isEmpty()) {
        action->setShortcut(QKeySequence(keys));
        action->setShortcutVisibleInContextMenu(true);
    }
    action->setEnabled(enabled);
    connect(action, &QAction::triggered, this, run);
    return action;
}

bool PlotBoardPanel::canPasteAfter(const QString& elementId) const {
    const QString marked = m_board->markedCard();
    return !m_readOnly && !marked.isEmpty() && marked != elementId;
}

QMenu* PlotBoardPanel::cardMenu(const QString& elementId) {
    m_menu->clear();
    const std::optional<PlotCard> found = card(elementId);
    const bool shown = found.has_value();
    const bool changing = shown && !m_readOnly;
    const CardMenuTexts texts = menuTextsOf(found);
    addMenuAction(texts.open, QStringLiteral("Return"), shown && (!texts.writes || !m_readOnly),
                  [this, elementId]() { emit openRequested(elementId); });
    //: In Polish: &Edytuj kartę
    addMenuAction(tr("&Edit Card"), QStringLiteral("F2"), changing,
                  [this, elementId]() { editCard(elementId); });
    addMenuAction(texts.add, QStringLiteral("Ins"), !m_readOnly,
                  [this, elementId]() { emit addRequested(elementId); });
    addMenuAction(texts.addAtEnd, QStringLiteral("Alt+Ins"), changing,
                  [this, elementId]() { emit addSceneRequested(elementId); });
    m_menu->addSeparator();
    //: In Polish: Wy&tnij
    addMenuAction(tr("Cu&t"), QStringLiteral("Ctrl+X"), changing,
                  [this, elementId]() { emit cutRequested(elementId); });
    //: In Polish: Wklej &za nią
    addMenuAction(tr("&Paste After It"), QStringLiteral("Ctrl+V"),
                  shown && canPasteAfter(elementId),
                  [this, elementId]() { emit pasteRequested(elementId); });
    m_menu->addSeparator();
    addStateMenu(elementId, found);
    addColorMenu(elementId, found);
    m_menu->addSeparator();
    addMenuAction(texts.remove, QStringLiteral("Del"), changing,
                  [this, elementId]() { emit deleteRequested(elementId); });
    //: In Polish: &Właściwości
    addMenuAction(tr("P&roperties"), QStringLiteral("Alt+Return"), shown,
                  [this, elementId]() { emit propertiesRequested(elementId); });
    return m_menu;
}

void PlotBoardPanel::addStateMenu(const QString& elementId, const std::optional<PlotCard>& found) {
    //: In Polish: &Stan
    QMenu* states = m_menu->addMenu(tr("&State"));
    // A scene in the plan gets a state once it is written
    states->setEnabled(found.has_value() && !m_readOnly && !found->planned);
    const QString current = found.has_value() ? found->status : QString();
    auto* group = new QActionGroup(states);
    group->setExclusive(true);
    for (const char* status : {"draft", "revision", "final"}) {
        const QString code = QString::fromLatin1(status);
        QAction* action = states->addAction(PlotBoard::statusName(code));
        action->setCheckable(true);
        action->setChecked(found.has_value() && current == code);
        group->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, elementId, code]() { emit statusRequested(elementId, code); });
    }
}

void PlotBoardPanel::addColorMenu(const QString& elementId, const std::optional<PlotCard>& found) {
    //: In Polish: &Kolor karty
    QMenu* colors = m_menu->addMenu(tr("Card &Color"));
    colors->setEnabled(found.has_value() && !m_readOnly);
    const QString current = found.has_value() ? found->plot.color : QString();
    auto* group = new QActionGroup(colors);
    group->setExclusive(true);
    QStringList names{QString()};
    names << PlotColors::ownNames();
    const PlotColors palette = PlotColors::current();
    for (const QString& name : names) {
        //: A card without an own color. In Polish: Bez koloru
        QAction* action = colors->addAction(name.isEmpty() ? tr("No Color")
                                                           : PlotColors::ownTitle(name));
        if (!name.isEmpty()) {
            QPixmap swatch(SWATCH_SIZE, SWATCH_SIZE);
            swatch.fill(palette.ofOwn(name));
            action->setIcon(QIcon(swatch));
        }
        action->setCheckable(true);
        action->setChecked(found.has_value() && current == name);
        group->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, elementId, name]() { emit colorRequested(elementId, name); });
    }
}

void PlotBoardPanel::showMenu(const QString& elementId, const QPoint& globalPos) {
    cardMenu(elementId)->popup(globalPos);
}

bool PlotBoardPanel::eventFilter(QObject* watched, QEvent* event) {
    // While a cell is edited, a key its editor does not take (Ctrl+Up) does nothing
    if (watched == m_outline &&
        (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) &&
        m_outline->indexWidget(m_outline->currentIndex()) == nullptr) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (event->type() == QEvent::ShortcutOverride) {
            // The keys of the outline go to it, not to the commands of the window
            if (outlineKey(key, false)) {
                key->accept();
                return true;
            }
        } else if (outlineKey(key, true)) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool PlotBoardPanel::outlineKey(const QKeyEvent* key, bool run) {
    const Key action = PlotBoardView::boardKey(key);
    if (!takesKey(action)) {
        return false;
    }
    if (run) {
        doOutlineKey(action);
    }
    return true;
}

bool PlotBoardPanel::takesKey(PlotBoardView::Key action) const {
    // Esc forgets a card marked; else it is the table's, as the arrows, F2 and Space are
    if (action == Key::CancelCut) {
        return !m_board->markedCard().isEmpty();
    }
    return action == Key::MoveUp || action == Key::MoveDown || action == Key::Menu ||
           requestOf(CARD_REQUESTS, action) != nullptr ||
           requestOf(BOARD_REQUESTS, action) != nullptr;
}

void PlotBoardPanel::doOutlineKey(PlotBoardView::Key action) {
    const QString id = m_model->cardIdAt(m_outline->currentIndex().row());
    if (const CardRequest* cardRequest = requestOf(CARD_REQUESTS, action)) {
        if (PlotBoardView::keyAllowed(action, m_readOnly, id, m_board->markedCard())) {
            emit(this->*cardRequest->signal)(id);
        }
        return;
    }
    if (const BoardRequest* boardRequest = requestOf(BOARD_REQUESTS, action)) {
        emit(this->*boardRequest->signal)();
        return;
    }
    if (action == Key::Menu) {
        const QRect cell = m_outline->visualRect(m_outline->currentIndex());
        showMenu(id, m_outline->viewport()->mapToGlobal(cell.isValid() ? cell.center() : QPoint()));
        return;
    }
    // Ctrl+Up and Ctrl+Down move the row
    if (!m_readOnly && !id.isEmpty()) {
        emit moveRequested(id, action == Key::MoveUp ? -1 : 1);
    }
}

void PlotBoardPanel::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        updateTotals();
    }
}

}  // namespace kalahari::gui
