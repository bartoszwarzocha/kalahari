// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_view_input.cpp
/// @brief PlotBoardView: its keys, the mouse, and a card dragged to another place

#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view.h"
#include "kalahari/gui/widgets/plot_board_view_internal.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QByteArray>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPoint>
#include <QRect>
#include <QScrollBar>
#include <QString>
#include <QWidget>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace kalahari::gui {

using board_view::GAP;
using board_view::HALF_GAP;
using board_view::isScene;
using board_view::MARKER_GAP;
using board_view::MARKER_WIDTH;
using board_view::SCROLL_EDGE;
using board_view::SCROLL_STEP;
using board_view::textOfCard;

namespace {

using Key = PlotBoardView::Key;

/// @brief A key of the board and what it does
struct KeyBinding {
    constexpr KeyBinding(int keyCode, Qt::KeyboardModifiers keyModifiers, Key keyAction)
        : key(keyCode), modifiers(keyModifiers), action(keyAction) {}

    int key;                          ///< Its Qt::Key
    Qt::KeyboardModifiers modifiers;  ///< Its modifiers, without the keypad
    Key action;                       ///< What it does
};

/// @brief The keys of the board
constexpr std::array<KeyBinding, 29> KEYS{{
    {Qt::Key_Left, Qt::NoModifier, Key::Left},
    {Qt::Key_Right, Qt::NoModifier, Key::Right},
    {Qt::Key_Up, Qt::NoModifier, Key::Up},
    {Qt::Key_Down, Qt::NoModifier, Key::Down},
    {Qt::Key_Home, Qt::NoModifier, Key::First},
    {Qt::Key_End, Qt::NoModifier, Key::Last},
    {Qt::Key_Left, Qt::ControlModifier, Key::MoveBack},
    {Qt::Key_Right, Qt::ControlModifier, Key::MoveForward},
    {Qt::Key_Up, Qt::ControlModifier, Key::MoveUp},
    {Qt::Key_Down, Qt::ControlModifier, Key::MoveDown},
    {Qt::Key_Return, Qt::NoModifier, Key::Open},
    {Qt::Key_Enter, Qt::NoModifier, Key::Open},
    {Qt::Key_Return, Qt::AltModifier, Key::Properties},
    {Qt::Key_Enter, Qt::AltModifier, Key::Properties},
    {Qt::Key_F2, Qt::NoModifier, Key::Edit},
    {Qt::Key_Insert, Qt::NoModifier, Key::Add},
    {Qt::Key_Insert, Qt::ControlModifier | Qt::ShiftModifier, Key::AddPlanned},
    {Qt::Key_Insert, Qt::AltModifier, Key::AddScene},
    {Qt::Key_Insert, Qt::AltModifier | Qt::ShiftModifier, Key::NewScene},
    {Qt::Key_Delete, Qt::NoModifier, Key::Delete},
    {Qt::Key_X, Qt::ControlModifier, Key::Cut},
    {Qt::Key_V, Qt::ControlModifier, Key::Paste},
    {Qt::Key_Escape, Qt::NoModifier, Key::CancelCut},
    {Qt::Key_Z, Qt::ControlModifier, Key::Undo},
    {Qt::Key_Y, Qt::ControlModifier, Key::Redo},
    {Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier, Key::Redo},
    {Qt::Key_Menu, Qt::NoModifier, Key::Menu},
    {Qt::Key_F10, Qt::ShiftModifier, Key::Menu},
    {Qt::Key_Space, Qt::NoModifier, Key::Scenes},
}};

/// @brief What a key needs to ask for its request
enum class Need : std::uint8_t {
    Card,      ///< A card chosen
    Change,    ///< A card chosen, in a book that can be changed
    Writable,  ///< A book that can be changed
    Marked     ///< A card marked to move, in a book that can be changed
};

/// @brief A key that asks for something with the card chosen
struct CardRequest {
    constexpr CardRequest(Key requestKey, Need requestNeed,
                          void (PlotBoardView::*requestSignal)(const QString&))
        : key(requestKey), need(requestNeed), signal(requestSignal) {}

    Key key;                                       ///< The key
    Need need;                                     ///< What it needs
    void (PlotBoardView::*signal)(const QString&);  ///< What it asks for, with the card
};

/// @brief The keys that ask for something with the card chosen
constexpr std::array<CardRequest, 10> CARD_REQUESTS{{
    {Key::Open, Need::Card, &PlotBoardView::openRequested},
    {Key::Edit, Need::Change, &PlotBoardView::editRequested},
    {Key::Add, Need::Writable, &PlotBoardView::addRequested},
    {Key::AddPlanned, Need::Writable, &PlotBoardView::addPlannedRequested},
    {Key::AddScene, Need::Change, &PlotBoardView::addSceneRequested},
    {Key::NewScene, Need::Change, &PlotBoardView::newSceneRequested},
    {Key::Delete, Need::Change, &PlotBoardView::deleteRequested},
    {Key::Cut, Need::Change, &PlotBoardView::cutRequested},
    {Key::Paste, Need::Marked, &PlotBoardView::pasteRequested},
    {Key::Properties, Need::Card, &PlotBoardView::propertiesRequested},
}};

/// @brief The request of @p key; nullptr: none
const CardRequest* requestOf(Key key) {
    for (const CardRequest& request : CARD_REQUESTS) {
        if (request.key == key) {
            return &request;
        }
    }
    return nullptr;
}

/// @brief Whether a request that needs @p need can be made for card @p cardId, with card
/// @p marked marked to move
bool allowed(Need need, bool readOnly, const QString& cardId, const QString& marked) {
    switch (need) {
    case Need::Card:
        return !cardId.isEmpty();
    case Need::Change:
        return !readOnly && !cardId.isEmpty();
    case Need::Writable:
        return !readOnly;
    case Need::Marked:
        return !readOnly && !marked.isEmpty();
    }
    return false;
}

/// @brief Whether @p key goes to another card: an arrow, Home, End
constexpr bool isStep(Key key) {
    return key >= Key::Left && key <= Key::Last;
}

/// @brief Whether @p key moves the card: Ctrl with an arrow
constexpr bool isMove(Key key) {
    return key >= Key::MoveBack && key <= Key::MoveDown;
}

/// @brief The direction of a move: back (-1) with Ctrl+Left and Ctrl+Up, else forward (1)
constexpr int directionOf(Key key) {
    return key == Key::MoveBack || key == Key::MoveUp ? -1 : 1;
}

/// @brief Where a card dragged near @p rect goes beside it: @p rect and half the gap around it
QRect nearArea(const QRect& rect) {
    return rect.adjusted(-HALF_GAP - 1, -HALF_GAP, HALF_GAP + 1, HALF_GAP);
}

/// @brief How far the board scrolls while a card is dragged at @p at of the @p size of the
/// board: back near its start, forward near its end
int scrollStep(int at, int size) {
    if (at < SCROLL_EDGE) {
        return -SCROLL_STEP;
    }
    return at > size - SCROLL_EDGE ? SCROLL_STEP : 0;
}

/// @brief The id of the card dragged in @p mime
QString draggedCard(const QMimeData* mime) {
    return QString::fromUtf8(mime->data(QString::fromLatin1(PlotBoardView::CARD_MIME_TYPE)));
}

}  // namespace

// =============================================================================
// The keys
// =============================================================================

PlotBoardView::Key PlotBoardView::boardKey(const QKeyEvent* event) {
    Qt::KeyboardModifiers modifiers = event->modifiers();
    modifiers.setFlag(Qt::KeypadModifier, false);
    for (const KeyBinding& binding : KEYS) {
        if (binding.key == event->key() && binding.modifiers == modifiers) {
            return binding.action;
        }
    }
    return Key::None;
}

bool PlotBoardView::keyAllowed(Key key, bool readOnly, const QString& cardId,
                               const QString& marked) {
    const CardRequest* request = requestOf(key);
    return request == nullptr || allowed(request->need, readOnly, cardId, marked);
}

PlotBoardView::Key PlotBoardView::keyOf(const QKeyEvent* event) const {
    const Key key = boardKey(event);
    // Esc forgets a card marked, and Space opens a drawer of a grid: else they are not the
    // board's
    if ((key == Key::CancelCut && m_marked.isEmpty()) ||
        (key == Key::Scenes && m_arrangement != Arrangement::Grid)) {
        return Key::None;
    }
    return key;
}

void PlotBoardView::keyPressEvent(QKeyEvent* event) {
    const Key key = keyOf(event);
    if (key == Key::None) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    event->accept();
    doKey(key);
}

void PlotBoardView::doKey(Key key) {
    // The card the key is for: whoever takes the request may choose another one
    const QString current = m_current;
    if (isStep(key)) {
        if (const QString target = neighbour(key); !target.isEmpty()) {
            choose(target);
        }
        return;
    }
    if (isMove(key)) {
        moveByKey(key, current);
        return;
    }
    if (const CardRequest* request = requestOf(key)) {
        if (keyAllowed(key, m_readOnly, current, m_marked)) {
            emit(this->*request->signal)(current);
        }
        return;
    }
    doBoardKey(key, current);
}

void PlotBoardView::moveByKey(Key key, const QString& cardId) {
    if (m_readOnly || cardId.isEmpty()) {
        return;
    }
    const int direction = directionOf(key);
    // Along the board: Ctrl+Left and Ctrl+Right
    const bool along = key == Key::MoveBack || key == Key::MoveForward;
    const bool columns = m_arrangement == Arrangement::Columns;
    if (!isScene(cardId)) {
        // A text along the board; in a grid by a row too (in columns its scenes are under it)
        if (along) {
            emit moveRequested(cardId, direction);
        } else if (!columns) {
            emit moveRequested(cardId, direction * cardsPerRow());
        }
        return;
    }
    // A scene among the scenes of its text along them (down a column, along a drawer), else to
    // the text beside its text
    if (along != columns) {
        emit moveRequested(cardId, direction);
    } else {
        emit moveToTextRequested(cardId, direction);
    }
}

void PlotBoardView::doBoardKey(Key key, const QString& cardId) {
    switch (key) {
    case Key::Scenes:
        // The drawer of the text of the card, or of the scene
        toggleScenes(textOfCard(cardId));
        break;
    case Key::Menu: {
        const QRect rect = cardRect(cardId);
        const QPoint at = rect.isValid() ? rect.center() : viewport()->rect().center();
        emit menuRequested(cardId, viewport()->mapToGlobal(at));
        break;
    }
    case Key::CancelCut:
        emit cutCancelled();
        break;
    case Key::Undo:
        if (!m_readOnly) {
            emit undoRequested();
        }
        break;
    case Key::Redo:
        if (!m_readOnly) {
            emit redoRequested();
        }
        break;
    default:
        break;
    }
}

QString PlotBoardView::neighbour(Key key) const {
    if (m_cards.isEmpty()) {
        return {};
    }
    const CardPlace* current = placeOf(m_current);
    if (current == nullptr || key == Key::First) {
        return m_cards.constFirst().id;
    }
    switch (key) {
    case Key::Last:
        return m_cards.constLast().id;
    case Key::Left:
        return sideNeighbour(*current, -1);
    case Key::Right:
        return sideNeighbour(*current, 1);
    case Key::Up:
        return rowNeighbour(*current, -1);
    case Key::Down:
        return rowNeighbour(*current, 1);
    default:
        break;
    }
    return {};
}

QString PlotBoardView::sideNeighbour(const CardPlace& current, int step) const {
    if (m_arrangement != Arrangement::Columns) {
        // The card before or after it
        const qsizetype at = (&current - m_cards.constData()) + step;
        return at >= 0 && at < m_cards.size() ? m_cards.at(at).id : QString();
    }
    // The column beside, the card nearest in height (a scene beside a scene)
    const auto distance = [&current](const CardPlace& place) {
        return std::abs(place.rect.top() - current.rect.top());
    };
    const CardPlace* best = nullptr;
    for (const CardPlace& place : m_cards) {
        if (place.column == current.column + step &&
            (best == nullptr || distance(place) < distance(*best))) {
            best = &place;
        }
    }
    return best != nullptr ? best->id : QString();
}

QString PlotBoardView::rowNeighbour(const CardPlace& current, int step) const {
    // The nearest row above or below, the card nearest to the middle of this one
    const QRect from = current.rect;
    const CardPlace* best = nullptr;
    int bestRow = std::numeric_limits<int>::max();
    int bestDistance = std::numeric_limits<int>::max();
    for (const CardPlace& place : m_cards) {
        // How far its row is in the direction of the step; 0 or less: not in that direction
        const int row = step < 0 ? from.top() - place.rect.bottom() : place.rect.top() - from.bottom();
        const int distance = std::abs(place.rect.center().x() - from.center().x());
        if (row > 0 && (row < bestRow || (row == bestRow && distance < bestDistance))) {
            best = &place;
            bestRow = row;
            bestDistance = distance;
        }
    }
    return best != nullptr ? best->id : QString();
}

// =============================================================================
// The mouse
// =============================================================================

void PlotBoardView::mousePressEvent(QMouseEvent* event) {
    if (m_editor != nullptr) {
        // A click beside the card edited keeps what was written
        closeEditor(true);
    }
    setFocus(Qt::MouseFocusReason);
    event->accept();
    const QPoint pos = contentsPos(event->position().toPoint());
    const CardPlace* place = cardAt(pos);
    if (place == nullptr) {
        return;
    }
    const QString id = place->id;
    const bool left = event->button() == Qt::LeftButton;
    if (left && scenesNoteRect(*place).contains(pos)) {
        // "3 scenes" opens and closes the drawer
        choose(id);
        toggleScenes(id);
        return;
    }
    // The pencil edits the card, the button at its bottom does what Enter does
    if (left && pencilRectOf(*place).contains(pos)) {
        choose(id);
        emit editRequested(id);
        return;
    }
    if (left && planButtonRectOf(*place).contains(pos)) {
        choose(id);
        emit openRequested(id);
        return;
    }
    choose(id);
    if (left) {
        m_pressed = id;
        m_pressPos = event->position().toPoint();
    }
}

void PlotBoardView::mouseMoveEvent(QMouseEvent* event) {
    if (!event->buttons().testFlag(Qt::LeftButton) || m_pressed.isEmpty() || m_readOnly) {
        return;
    }
    if ((event->position().toPoint() - m_pressPos).manhattanLength() <
        QApplication::startDragDistance()) {
        return;
    }
    startDrag();
}

void PlotBoardView::mouseReleaseEvent(QMouseEvent* event) {
    m_pressed.clear();
    event->accept();
}

void PlotBoardView::mouseDoubleClickEvent(QMouseEvent* event) {
    const QPoint pos = contentsPos(event->position().toPoint());
    const CardPlace* place = cardAt(pos);
    // The second click on the pencil or the button is theirs, as the first one
    if (place != nullptr && event->button() == Qt::LeftButton &&
        !pencilRectOf(*place).contains(pos) && !planButtonRectOf(*place).contains(pos)) {
        // The cards are made again when the text opens
        const QString id = place->id;
        emit openRequested(id);
    }
    event->accept();
}

void PlotBoardView::contextMenuEvent(QContextMenuEvent* event) {
    event->accept();
    if (event->reason() == QContextMenuEvent::Keyboard) {
        doKey(Key::Menu);
        return;
    }
    const CardPlace* place = cardAt(contentsPos(event->pos()));
    const QString id = place != nullptr ? place->id : QString();
    choose(id);
    emit menuRequested(id, event->globalPos());
}

// =============================================================================
// A card dragged
// =============================================================================

void PlotBoardView::startDrag() {
    const QString dragged = m_pressed;
    m_pressed.clear();
    const QRect rect = cardRect(dragged);
    auto* mime = new QMimeData;
    mime->setData(QString::fromLatin1(CARD_MIME_TYPE), dragged.toUtf8());
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    if (rect.isValid()) {
        drag->setPixmap(viewport()->grab(rect));
        drag->setHotSpot(m_pressPos - rect.topLeft());
    }
    drag->exec(Qt::MoveAction);
    m_drop = {};
    viewport()->update();
}

const PlotBoardView::CardPlace* PlotBoardView::cardNear(const QPoint& pos, bool scene) const {
    for (const CardPlace& place : m_cards) {
        if ((place.scene >= 0) == scene && nearArea(place.rect).contains(pos)) {
            return &place;
        }
    }
    return nullptr;
}

PlotBoardView::DropTarget PlotBoardView::besideCard(const CardPlace& place, bool after,
                                                    bool vertical) {
    DropTarget target;
    target.anchorId = place.id;
    target.after = after;
    const QRect& rect = place.rect;
    if (vertical) {
        const int y = after ? rect.bottom() + HALF_GAP : rect.top() - HALF_GAP;
        target.marker = QRect(rect.left(), y - 1, rect.width(), MARKER_WIDTH);
    } else {
        const int x = after ? rect.right() + HALF_GAP : rect.left() - HALF_GAP;
        target.marker = QRect(x - 1, rect.top(), MARKER_WIDTH, rect.height());
    }
    return target;
}

PlotBoardView::DropTarget PlotBoardView::dropTargetAt(const QPoint& pos,
                                                      const QString& dragged) const {
    if (isScene(dragged)) {
        return sceneDropTargetAt(pos, dragged);
    }
    // Before or after the card under the mouse, or the nearest card of its row
    if (const CardPlace* place = cardNear(pos, false)) {
        if (place->id == dragged) {
            return {};
        }
        return besideCard(*place, pos.x() > place->rect.center().x(), false);
    }
    return bandDropTargetAt(pos, dragged);
}

PlotBoardView::DropTarget PlotBoardView::sceneDropTargetAt(const QPoint& pos,
                                                           const QString& dragged) const {
    // Before or after a scene, of its text or another: in columns above or below it, in a
    // drawer on its left or right
    if (const CardPlace* place = cardNear(pos, true)) {
        if (place->id == dragged) {
            return {};
        }
        if (m_arrangement == Arrangement::Columns) {
            return besideCard(*place, pos.y() > place->rect.center().y(), true);
        }
        return besideCard(*place, pos.x() > place->rect.center().x(), false);
    }
    // On the card of a text: first among its scenes
    if (const CardPlace* text = cardNear(pos, false)) {
        return besideCard(*text, true, true);
    }
    return {};
}

bool PlotBoardView::dropsInPart(const BandPlace& band, const QPoint& pos) const {
    // The title of a part, or an empty part
    const PlotBand& shown = m_bands.at(band.band);
    return !shown.partId.isEmpty() &&
           (band.header.contains(pos) || (shown.cards.isEmpty() && band.area.contains(pos)));
}

PlotBoardView::DropTarget PlotBoardView::bandDropTargetAt(const QPoint& pos,
                                                          const QString& dragged) const {
    for (const BandPlace& band : m_bandPlaces) {
        const PlotBand& shown = m_bands.at(band.band);
        if (dropsInPart(band, pos)) {
            // At the start of the part
            DropTarget target;
            target.partId = shown.partId;
            target.marker = QRect(band.area.left(), band.area.top() - MARKER_GAP,
                                  band.area.width(), MARKER_WIDTH);
            return target;
        }
        if (!shown.cards.isEmpty() && band.area.adjusted(0, 0, GAP, 0).contains(pos)) {
            // Beyond the last card of a band: after it
            const CardPlace* last = placeOf(shown.cards.constLast().id);
            if (last == nullptr || last->id == dragged) {
                return {};
            }
            return besideCard(*last, true, false);
        }
    }
    return {};
}

void PlotBoardView::scrollNear(const QPoint& pos) {
    // Near the edges the board scrolls
    QScrollBar* across = horizontalScrollBar();
    QScrollBar* down = verticalScrollBar();
    across->setValue(across->value() + scrollStep(pos.x(), viewport()->width()));
    down->setValue(down->value() + scrollStep(pos.y(), viewport()->height()));
}

void PlotBoardView::dragEnterEvent(QDragEnterEvent* event) {
    if (!m_readOnly && event->source() == this &&
        event->mimeData()->hasFormat(QString::fromLatin1(CARD_MIME_TYPE))) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void PlotBoardView::dragMoveEvent(QDragMoveEvent* event) {
    const QPoint pos = event->position().toPoint();
    m_drop = dropTargetAt(contentsPos(pos), draggedCard(event->mimeData()));
    scrollNear(pos);
    if (m_drop.isValid()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
    viewport()->update();
}

void PlotBoardView::dragLeaveEvent(QDragLeaveEvent* event) {
    m_drop = {};
    viewport()->update();
    event->accept();
}

void PlotBoardView::dropEvent(QDropEvent* event) {
    const QString dragged = draggedCard(event->mimeData());
    const DropTarget target = dropTargetAt(contentsPos(event->position().toPoint()), dragged);
    m_drop = {};
    viewport()->update();
    if (!target.isValid() || dragged.isEmpty()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    if (!target.anchorId.isEmpty()) {
        emit dropBesideRequested(dragged, target.anchorId, target.after);
    } else {
        emit dropInPartRequested(dragged, target.partId);
    }
}

}  // namespace kalahari::gui
