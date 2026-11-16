// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_axis_view_input.cpp
/// @brief The axis of a timeline: the keys, the mouse and the menus

#include <kalahari/timeline/timeline_axis_view.h>

#include <kalahari/timeline/timeline_document.h>
#include <kalahari/timeline/timeline_edits.h>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace kalahari::timeline {

namespace {

constexpr int WHEEL_STEP = 120;

/// A shortcut shown beside an action of the menu; the keys themselves are the axis's
void showKeys(QAction* action, const QKeySequence& keys) {
    action->setShortcut(keys);
    action->setShortcutContext(Qt::WidgetShortcut);
    action->setShortcutVisibleInContextMenu(true);
}

}  // namespace

// =============================================================================
// Changes
// =============================================================================

void TimelineAxisView::tellResult(const QString& message, bool done) {
    if (!message.isEmpty()) {
        emit told(message, done);
    }
}

void TimelineAxisView::addAfterSelected() {
    QString message;
    const QString lane = m_document.selectedLane();
    const QString added = TimelineEdits::addAfter(
        m_document, m_document.selected(),
        lane.isEmpty() ? std::optional<QString>() : std::optional<QString>(lane), &message);
    if (added.isEmpty()) {
        return;
    }
    startEditing(added);
    m_editingNew = isEditing();
    tellResult(message, true);
}

void TimelineAxisView::removeSelected() {
    QString message;
    const bool done = TimelineEdits::remove(m_document, m_document.selected(), &message);
    tellResult(message, done);
}

void TimelineAxisView::shiftSelected(int direction) {
    const QString id = m_document.selected();
    if (id.isEmpty()) {
        return;
    }
    QString message;
    const bool done = TimelineEdits::shift(m_document, id, direction, m_scene->pixelsPerDay(),
                                           m_look, &message);
    tellResult(message, done);
    ensureShown(id);
}

void TimelineAxisView::moveSelectedToLane(int direction) {
    const QString id = m_document.selected();
    const TimelineEvent* event = m_document.timeline().event(id);
    if (event == nullptr) {
        return;
    }
    const QStringList keys = m_scene->laneKeysOf(*event);
    const QString from = keys.contains(m_document.selectedLane()) ? m_document.selectedLane()
                                                                   : keys.value(0);
    const QList<TimelineLane>& lanes = m_scene->lanes();
    qsizetype at = -1;
    for (qsizetype i = 0; i < lanes.size(); ++i) {
        if (lanes.at(i).key == from) {
            at = i;
        }
    }
    QString message;
    if (m_document.timeline().view().lanes == TimelineView::Lanes::One) {
        TimelineEdits::moveToLane(m_document, id, from, QStringLiteral("-"), QString(), &message);
        tellResult(message, false);
        return;
    }
    const qsizetype to = at + direction;
    if (at < 0 || to < 0 || to >= lanes.size()) {
        //: In Polish: To już skrajne pasmo.
        tellResult(tr("This is the outermost lane."), false);
        return;
    }
    const TimelineLane& lane = lanes.at(to);
    const bool done =
        TimelineEdits::moveToLane(m_document, id, from, lane.key, lane.name, &message);
    tellResult(message, done);
    ensureShown(id);
}

// =============================================================================
// Moving the selection
// =============================================================================

void TimelineAxisView::selectNeighbour(int direction) {
    QStringList sequence;
    for (const TimelineEvent* event : m_scene->onAxis()) {
        sequence.append(event->id);
    }
    for (const TimelineItem& item : m_scene->trayItems()) {
        sequence.append(item.id);
    }
    if (sequence.isEmpty()) {
        return;
    }
    const qsizetype at = sequence.indexOf(m_document.selected());
    const qsizetype to = at < 0 ? 0 : std::clamp<qsizetype>(at + direction, 0, sequence.size() - 1);
    const QString& id = sequence.at(to);
    const TimelineEvent* event = m_document.timeline().event(id);
    const QString lane = event != nullptr && m_scene->laneKeysOf(*event).contains(
                                                 m_document.selectedLane())
                             ? m_document.selectedLane()
                             : QString();
    m_document.select(id, lane);
}

void TimelineAxisView::selectInLane(int direction) {
    const TimelineItem* current = m_scene->itemOf(m_document.selected(), m_document.selectedLane());
    if (current == nullptr || m_scene->itemsOf(current->id).isEmpty()) {
        selectEdge(false);
        return;
    }
    const QList<TimelineLane>& lanes = m_scene->lanes();
    qsizetype at = 0;
    for (qsizetype i = 0; i < lanes.size(); ++i) {
        if (lanes.at(i).key == current->lane) {
            at = i;
        }
    }
    for (qsizetype j = at + direction; j >= 0 && j < lanes.size(); j += direction) {
        const TimelineItem* nearest = nullptr;
        double distance = std::numeric_limits<double>::max();
        for (const TimelineItem& item : m_scene->items()) {
            if (item.lane == lanes.at(j).key &&
                std::abs(item.rect.x() - current->rect.x()) < distance) {
                distance = std::abs(item.rect.x() - current->rect.x());
                nearest = &item;
            }
        }
        if (nearest != nullptr) {
            m_document.select(nearest->id, nearest->lane);
            return;
        }
    }
}

void TimelineAxisView::selectEdge(bool last) {
    const QList<const TimelineEvent*>& events = m_scene->onAxis();
    if (!events.isEmpty()) {
        m_document.select(last ? events.constLast()->id : events.constFirst()->id);
    }
}

// =============================================================================
// The keys
// =============================================================================

void TimelineAxisView::keyPressEvent(QKeyEvent* event) {
    if (isEditing()) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool control = (modifiers & Qt::ControlModifier) != 0;
    const bool shift = (modifiers & Qt::ShiftModifier) != 0;
    const int key = event->key();
    const QString selected = m_document.selected();
    event->accept();

    if ((key == Qt::Key_F10 && shift) || key == Qt::Key_Menu) {
        const TimelineItem* item = m_scene->itemOf(selected, m_document.selectedLane());
        const bool tray = item != nullptr && m_scene->itemsOf(item->id).isEmpty();
        const QPoint at = item != nullptr ? viewRectOf(*item, tray).center().toPoint()
                                          : QPoint(static_cast<int>(TimelineScene::HEAD_WIDTH) + 40,
                                                   static_cast<int>(m_scene->lanesTop()) + 40);
        showMenuAt(at, item, tray);
        return;
    }
    if (control && (key == Qt::Key_Plus || key == Qt::Key_Equal)) {
        zoomBy(1);
        return;
    }
    if (control && key == Qt::Key_Minus) {
        zoomBy(-1);
        return;
    }
    if (control && key == Qt::Key_0) {
        resetZoom();
        return;
    }
    if (control && key == Qt::Key_Z && !shift) {
        const QString what = m_document.undoText();
        //: In Polish: Nie ma już czego cofnąć. / Cofnięto: %1.
        emit told(what.isEmpty() ? tr("There is nothing more to undo.")
                                 : tr("Undone: %1.").arg(what),
                  false);
        m_document.undo();
        return;
    }
    if (control && (key == Qt::Key_Y || (key == Qt::Key_Z && shift))) {
        const QString what = m_document.redoText();
        //: In Polish: Nie ma czego ponowić. / Ponowiono: %1.
        emit told(what.isEmpty() ? tr("There is nothing to redo.") : tr("Redone: %1.").arg(what),
                  false);
        m_document.redo();
        return;
    }
    switch (key) {
        case Qt::Key_Left:
        case Qt::Key_Right: {
            const int direction = key == Qt::Key_Left ? -1 : 1;
            if (control) {
                shiftSelected(direction);
            } else {
                selectNeighbour(direction);
            }
            return;
        }
        case Qt::Key_Up:
        case Qt::Key_Down: {
            const int direction = key == Qt::Key_Up ? -1 : 1;
            if (control) {
                moveSelectedToLane(direction);
            } else {
                selectInLane(direction);
            }
            return;
        }
        case Qt::Key_Home:
        case Qt::Key_End:
            selectEdge(key == Qt::Key_End);
            return;
        case Qt::Key_Insert:
            addAfterSelected();
            return;
        case Qt::Key_Escape:
            m_document.select(QString());
            return;
        default:
            break;
    }
    if (selected.isEmpty()) {
        event->ignore();
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (control) {
            emit chapterRequested(selected);
        } else {
            emit eventRequested(selected);
        }
        return;
    }
    if (key == Qt::Key_Delete) {
        removeSelected();
        return;
    }
    if (key == Qt::Key_F2) {
        startEditing(selected);
        return;
    }
    if (typed(*event)) {
        startEditing(selected, event->text());
        return;
    }
    event->ignore();
    QAbstractScrollArea::keyPressEvent(event);
}

// =============================================================================
// The mouse
// =============================================================================

void TimelineAxisView::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (isEditing()) {
        finishEditing(true);
    }
    const QPointF at = event->position();
    bool tray = false;
    const TimelineItem* item = itemAt(at, &tray);
    if (item != nullptr) {
        m_document.select(item->id, tray ? QString() : item->lane);
        if (event->button() == Qt::LeftButton) {
            Drag drag;
            drag.id = item->id;
            drag.lane = tray ? QString() : item->lane;
            drag.fromTray = tray;
            drag.pressedAt = event->position().toPoint();
            const QRectF rect = viewRectOf(*item, tray);
            drag.grip = at - rect.topLeft();
            drag.size = rect.size();
            drag.at = drag.pressedAt;
            m_drag = drag;
        }
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_document.select(QString());
        m_panning = true;
        m_panFrom = event->position().toPoint();
        m_scrollFrom = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
        viewport()->setCursor(Qt::ClosedHandCursor);
    }
}

void TimelineAxisView::mouseMoveEvent(QMouseEvent* event) {
    const QPoint at = event->position().toPoint();
    if (m_panning) {
        const QPoint moved = at - m_panFrom;
        horizontalScrollBar()->setValue(m_scrollFrom.x() - moved.x());
        verticalScrollBar()->setValue(m_scrollFrom.y() - moved.y());
        return;
    }
    if (m_drag && (event->buttons() & Qt::LeftButton)) {
        if (!m_drag->started &&
            (at - m_drag->pressedAt).manhattanLength() < QApplication::startDragDistance()) {
            return;
        }
        m_drag->started = true;
        updateDrag(at);
        return;
    }
    bool tray = false;
    viewport()->setCursor(itemAt(event->position(), &tray) != nullptr ? Qt::OpenHandCursor
                                                                      : Qt::ArrowCursor);
}

void TimelineAxisView::mouseReleaseEvent(QMouseEvent* /*event*/) {
    if (m_panning) {
        m_panning = false;
        viewport()->unsetCursor();
    }
    if (m_drag) {
        if (m_drag->started) {
            finishDrag();
        }
        m_drag.reset();
        viewport()->update();
    }
}

void TimelineAxisView::mouseDoubleClickEvent(QMouseEvent* event) {
    const QPointF at = event->position();
    bool tray = false;
    if (const TimelineItem* item = itemAt(at, &tray); item != nullptr) {
        m_drag.reset();
        emit eventRequested(item->id);
        return;
    }
    addAt(at);
}

void TimelineAxisView::addAt(const QPointF& at) {
    const TimelineLane* lane = inLanes(at) ? m_scene->laneAt(toLanes(at).y()) : nullptr;
    if (lane == nullptr) {
        addAfterSelected();
        return;
    }
    const QPointF point = toLanes(at);
    int slot = 0;
    if (m_scene->ordered()) {
        const double step = m_scene->xOfSlot(1) - m_scene->xOfSlot(0);
        const auto after = static_cast<int>(std::floor((point.x() - TimelineScene::ORIGIN) / step));
        slot = std::clamp(after + 1, 0, static_cast<int>(m_scene->onAxis().size()));
    }
    QString message;
    const QString key = lane->key == QLatin1String("*") ? QString() : lane->key;
    const QString added =
        TimelineEdits::addAt(m_document, m_scene->dayAt(point.x()), slot, key, m_look, &message);
    if (!added.isEmpty()) {
        startEditing(added);
        m_editingNew = isEditing();
        tellResult(message, true);
    }
}

void TimelineAxisView::wheelEvent(QWheelEvent* event) {
    if (!(event->modifiers() & Qt::ControlModifier)) {
        QAbstractScrollArea::wheelEvent(event);
        return;
    }
    // Ctrl and the wheel zoom about the mouse, a step for each step of the wheel
    m_wheel += event->angleDelta().y();
    const int steps = m_wheel / WHEEL_STEP;
    if (steps != 0) {
        m_wheel -= steps * WHEEL_STEP;
        zoomTo(m_document.timeline().view().zoom + steps, event->position().x());
    }
    event->accept();
}

// =============================================================================
// Dragging an event
// =============================================================================

void TimelineAxisView::updateDrag(const QPoint& at) {
    if (!m_drag) {
        return;
    }
    Drag& drag = *m_drag;
    drag.at = at;
    drag.valid = false;
    drag.label.clear();
    const TimelineEvent* event = m_document.timeline().event(drag.id);
    if (event == nullptr || !inLanes(at)) {
        viewport()->update();
        return;
    }
    const TimelineLane* lane = m_scene->laneAt(toLanes(at).y());
    if (lane == nullptr) {
        viewport()->update();
        return;
    }
    const double left = toLanes(QPointF(at) - drag.grip).x();
    drag.valid = true;
    drag.toLane = lane->key;
    const Timeline& timeline = m_document.timeline();
    if (m_scene->ordered()) {
        drag.slot = m_scene->slotAt(left);
        drag.dropX = m_scene->xOfSlot(drag.slot);
        //: Where a dragged event goes in the order of the story. In Polish: %1. w kolejności
        drag.label = tr("%1. in the order").arg(drag.slot + 1);
    } else {
        drag.day = m_scene->dayAt(left);
        TimelineEvent moved = *event;
        moved.time = TimelineEdits::movedTo(*event, drag.day, m_scene->pixelsPerDay(),
                                            timeline.view().scale);
        drag.dropX = m_scene->xOfDay(moved.time->start());
        drag.label = TimelineWords::when(timeline, moved, m_look);
    }
    if (timeline.view().lanes != TimelineView::Lanes::One && drag.toLane != drag.lane) {
        //: Where a dragged event goes: another lane. In Polish: pasmo „%1”
        drag.label += QStringLiteral(" · ") + tr("lane %1").arg(TimelineEdits::quoted(lane->name));
    }
    viewport()->update();
}

void TimelineAxisView::finishDrag() {
    if (!m_drag) {
        return;
    }
    const Drag drag = *m_drag;
    const Timeline& timeline = m_document.timeline();
    const TimelineEvent* event = timeline.event(drag.id);
    if (event == nullptr || !drag.valid) {
        return;
    }
    const QString title = event->title;
    TimelineEvent changed = *event;
    const bool ordered = m_scene->ordered();
    if (!ordered) {
        changed.time = TimelineEdits::movedTo(*event, drag.day, m_scene->pixelsPerDay(),
                                              timeline.view().scale);
        changed.timeText.clear();
    }
    QString laneNote;
    const TimelineView::Lanes lanes = timeline.view().lanes;
    QString lane = drag.lane;
    if (lanes != TimelineView::Lanes::One && drag.toLane != drag.lane) {
        const TimelineLane* to = m_scene->lane(drag.toLane);
        const QString name = to != nullptr ? to->name : drag.toLane;
        if (TimelineEdits::setLane(changed, lanes, drag.fromTray ? QString() : drag.lane,
                                   drag.toLane)) {
            lane = drag.toLane;
            //: In Polish: pasmo „%1”
            laneNote = QStringLiteral(" · ") + tr("lane %1").arg(TimelineEdits::quoted(name));
        } else {
            //: In Polish: %1 już jest w tym wydarzeniu
            laneNote = QStringLiteral(" (") + tr("%1 is already in this event").arg(name) +
                       QLatin1Char(')');
        }
    }
    const QString id = drag.id;
    const int slot = drag.slot;
    const bool done = m_document.change(
        //: What Undo undoes in a timeline. In Polish: przesunięcie „%1”
        tr("moving %1").arg(TimelineEdits::quoted(title)),
        [&changed, ordered, &id, slot](Timeline& timelineOf) {
            const bool set = timelineOf.setEvent(changed);
            return ordered ? set && timelineOf.moveInOrder(id, slot + 1) : set;
        });
    if (!done) {
        return;
    }
    m_document.select(id, lane);
    const TimelineEvent* now = m_document.timeline().event(id);
    QString when;
    if (ordered) {
        when = tr("%1. in the order").arg(now != nullptr ? now->order : 0);
    } else if (now != nullptr) {
        when = TimelineWords::when(m_document.timeline(), *now, m_look);
    }
    tellResult(TimelineEdits::quoted(title) + QStringLiteral(": ") + when + laneNote +
                   QLatin1Char('.'),
               true);
}

// =============================================================================
// The menus
// =============================================================================

void TimelineAxisView::contextMenuEvent(QContextMenuEvent* event) {
    if (isEditing()) {
        return;
    }
    const QPoint at = event->pos();
    bool tray = false;
    const TimelineItem* item = itemAt(at, &tray);
    showMenuAt(at, item, tray);
}

void TimelineAxisView::showMenuAt(const QPoint& at, const TimelineItem* item, bool tray) {
    QMenu menu(this);
    if (item != nullptr) {
        m_document.select(item->id, tray ? QString() : item->lane);
        fillEventMenu(menu, item->id);
    } else {
        //: In the menu of the axis of a timeline. In Polish: Nowe wydarzenie tutaj
        QAction* add = menu.addAction(tr("New event here"));
        connect(add, &QAction::triggered, this, [this, at]() { addAt(QPointF(at)); });
        //: In the menu of the axis of a timeline. In Polish: Dzień 1 i wątki…
        QAction* timeline = menu.addAction(tr("Day One and Threads…"));
        connect(timeline, &QAction::triggered, this, &TimelineAxisView::timelineRequested);
        if (!m_scene->ordered()) {
            menu.addSeparator();
            //: In Polish: Powiększenie 100%
            QAction* zoom = menu.addAction(tr("Zoom 100%"));
            showKeys(zoom, QKeySequence(Qt::CTRL | Qt::Key_0));
            connect(zoom, &QAction::triggered, this, &TimelineAxisView::resetZoom);
        }
    }
    menu.exec(viewport()->mapToGlobal(at));
}

void TimelineAxisView::fillEventMenu(QMenu& menu, const QString& id) {
    const TimelineEvent* event = m_document.timeline().event(id);
    if (event == nullptr) {
        return;
    }
    const bool ordered = m_scene->ordered();
    //: In the menu of an event of a timeline. In Polish: Okno „Wydarzenie”…
    QAction* open = menu.addAction(tr("Event…"));
    showKeys(open, QKeySequence(Qt::Key_Return));
    connect(open, &QAction::triggered, this, [this, id]() { emit eventRequested(id); });
    //: In Polish: Zmień tytuł
    QAction* rename = menu.addAction(tr("Change the Title"));
    showKeys(rename, QKeySequence(Qt::Key_F2));
    connect(rename, &QAction::triggered, this, [this, id]() { startEditing(id); });
    //: In Polish: Otwórz rozdział
    QAction* chapter = menu.addAction(tr("Open the Chapter"));
    showKeys(chapter, QKeySequence(Qt::CTRL | Qt::Key_Return));
    chapter->setEnabled(!event->chapters.isEmpty());
    connect(chapter, &QAction::triggered, this, [this, id]() { emit chapterRequested(id); });
    menu.addSeparator();
    //: In Polish: Nowe wydarzenie po nim
    QAction* add = menu.addAction(tr("New Event after It"));
    showKeys(add, QKeySequence(Qt::Key_Insert));
    connect(add, &QAction::triggered, this, &TimelineAxisView::addAfterSelected);
    //: In Polish: Wcześniej w kolejności / Wcześniej
    QAction* earlier = menu.addAction(ordered ? tr("Earlier in the Order") : tr("Earlier"));
    showKeys(earlier, QKeySequence(Qt::CTRL | Qt::Key_Left));
    earlier->setEnabled(ordered || event->time.has_value());
    connect(earlier, &QAction::triggered, this, [this]() { shiftSelected(-1); });
    //: In Polish: Później w kolejności / Później
    QAction* later = menu.addAction(ordered ? tr("Later in the Order") : tr("Later"));
    showKeys(later, QKeySequence(Qt::CTRL | Qt::Key_Right));
    later->setEnabled(ordered || event->time.has_value());
    connect(later, &QAction::triggered, this, [this]() { shiftSelected(1); });
    const bool one = m_document.timeline().view().lanes == TimelineView::Lanes::One;
    //: In Polish: Do pasma wyżej
    QAction* up = menu.addAction(tr("To the Lane Above"));
    showKeys(up, QKeySequence(Qt::CTRL | Qt::Key_Up));
    up->setEnabled(!one);
    connect(up, &QAction::triggered, this, [this]() { moveSelectedToLane(-1); });
    //: In Polish: Do pasma niżej
    QAction* down = menu.addAction(tr("To the Lane Below"));
    showKeys(down, QKeySequence(Qt::CTRL | Qt::Key_Down));
    down->setEnabled(!one);
    connect(down, &QAction::triggered, this, [this]() { moveSelectedToLane(1); });
    TimelineEdits::addColors(menu, m_document, m_look, id);
    menu.addSeparator();
    //: In Polish: Usuń wydarzenie
    QAction* remove = menu.addAction(tr("Delete the Event"));
    showKeys(remove, QKeySequence(Qt::Key_Delete));
    connect(remove, &QAction::triggered, this, &TimelineAxisView::removeSelected);
}

}  // namespace kalahari::timeline
