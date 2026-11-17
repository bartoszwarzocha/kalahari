// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_view_input.cpp
/// @brief The keys, the mouse and the menu of the map of a mind map

#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_menus.h>
#include <kalahari/mindmap/mind_map_view.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>

#include <algorithm>
#include <cmath>

namespace kalahari::mindmap {

namespace {

/// How far Ctrl and an arrow move a node that stands where the writer put it
constexpr double KEY_STEP = 14.0;
/// The parts of a node at its edges that put a dragged node before or after it
constexpr double EDGE = 0.28;
/// A new node in the free layout: this far beside its parent, or under the node before it
constexpr double FREE_BESIDE = 110.0;
constexpr double FREE_UNDER = 38.0;
/// A node taken out of its branch goes this far from where it was
constexpr double LOOSE_SHIFT = 40.0;
/// A shortcut shown beside an action of the menu; the keys themselves are the map's
void showKeys(QAction* action, const QKeySequence& keys) {
    action->setShortcut(keys);
    action->setShortcutContext(Qt::WidgetShortcut);
    action->setShortcutVisibleInContextMenu(true);
}

}  // namespace

// =============================================================================
// Changes
// =============================================================================

void MindMapView::addChild() {
    finishEditing(true);
    const QString parent =
        m_document.selected().isEmpty() ? m_document.map().root() : m_document.selected();
    editNew(m_document.addChild(parent));
}

void MindMapView::addSibling(bool before) {
    finishEditing(true);
    const QString id =
        m_document.selected().isEmpty() ? m_document.map().root() : m_document.selected();
    editNew(m_document.addSibling(id, before));
}

void MindMapView::removeSelected() {
    QString message;
    const bool removed = MindMapMenus::remove(m_document, m_document.selected(), &message);
    if (!message.isEmpty()) {
        emit told(message, removed);
    }
}

void MindMapView::toggleSelected() {
    const QString id = m_document.selected();
    const MindMapNode* node = m_document.map().node(id);
    if (node == nullptr) {
        return;
    }
    if (node->children.isEmpty()) {
        //: In Polish: „%1” nie ma węzłów podrzędnych, więc nie ma czego zwinąć.
        emit told(tr("“%1” has no nodes inside it, so there is nothing to collapse.")
                      .arg(m_document.shownText(id)),
                  false);
        return;
    }
    m_document.toggleCollapsed(id);
}

void MindMapView::moveSelected(int dx, int dy) {
    const QString id = m_document.selected();
    const MindMap& map = m_document.map();
    const MindMapNode* node = map.node(id);
    const NodeBox* box = m_scene ? m_scene->box(id) : nullptr;
    if (node == nullptr || box == nullptr) {
        return;
    }
    const QPointF step(dx * KEY_STEP, dy * KEY_STEP);
    if (map.layout() == MindMapLayout::Free) {
        // The node with its branch, a little further
        QHash<QString, QPointF> places;
        places.insert(id, node->freePlace.value_or(box->rect.center()) + step);
        for (const QString& inside : map.descendantsOf(id)) {
            const MindMapNode* each = map.node(inside);
            const NodeBox* shown = m_scene->box(inside);
            if (each != nullptr && (each->freePlace || shown != nullptr)) {
                places.insert(inside, each->freePlace.value_or(
                                          shown != nullptr ? shown->rect.center() : QPointF()) +
                                          step);
            }
        }
        m_document.movePlaces(places, true);
        return;
    }
    if (id == map.root()) {
        return;
    }
    if (map.looseNodes().contains(id)) {
        m_document.movePlaces({{id, node->loosePlace.value_or(box->rect.center()) + step}},
                              false);
        return;
    }
    // In a tree: along the nodes beside it, or a level up or down
    const QString text = m_document.shownText(id);
    int along = 0;
    bool up = false;
    if (map.layout() == MindMapLayout::Down) {
        along = dx;
        up = dy < 0;
    } else {
        along = dy;
        const int side = box->side != 0 ? box->side : 1;
        up = dx != 0 && (dx < 0) == (side > 0);
    }
    if (along != 0) {
        if (!m_document.moveAmongSiblings(id, along)) {
            emit told(along < 0
                          //: In Polish: To już pierwszy węzeł w tej gałęzi.
                          ? tr("It is the first node of this branch already.")
                          //: In Polish: To już ostatni węzeł w tej gałęzi.
                          : tr("It is the last node of this branch already."),
                      false);
        }
    } else if (up) {
        if (map.parentOf(id) == map.root() || !m_document.outdent(id)) {
            //: In Polish: „%1” to już gałąź główna.
            emit told(tr("“%1” is a main branch already.").arg(text), false);
        }
    } else if (!m_document.indent(id)) {
        //: In Polish: Przed „%1” nie ma węzła, do którego mógłby wejść.
        emit told(tr("There is no node before “%1” that it could go into.").arg(text), false);
    }
    ensureShown(id);
}

void MindMapView::openLink() {
    const MindMapNode* node = m_document.map().node(m_document.selected());
    if (node == nullptr) {
        return;
    }
    if (node->link.isEmpty()) {
        //: In Polish: Ten węzeł nie jest o żadnym elemencie książki. Wskaż go poleceniem
        //: „Element książki…” z menu węzła.
        emit told(tr("This node is not about an element of the book. Choose one with “Element "
                     "of the book…” in the menu of the node."),
                  false);
        return;
    }
    emit openElementRequested(node->link);
}

void MindMapView::copyAsText() {
    const QString message = MindMapMenus::copy(m_document, m_document.selected());
    if (!message.isEmpty()) {
        emit told(message, false);
    }
}

void MindMapView::paste() {
    const QString parent = m_document.map().node(m_document.selected()) != nullptr
                               ? m_document.selected()
                               : m_document.map().root();
    QString message;
    const QStringList added = MindMapMenus::paste(m_document, parent, &message);
    for (const QString& id : added) {
        placeNewInFree(id);
    }
    if (!message.isEmpty()) {
        emit told(message, true);
    }
}

void MindMapView::placeNewInFree(const QString& id) {
    // In the free layout a new node goes beside its parent, or under the node before it, not
    // where the layout on both sides would put it
    const MindMap& map = m_document.map();
    const MindMapNode* node = map.node(id);
    if (map.layout() != MindMapLayout::Free || node == nullptr || !m_scene) {
        return;
    }
    const NodeBox* box = m_scene->box(id);
    const QString parent = map.parentOf(id);
    const NodeBox* parentBox = m_scene->box(parent);
    if (box == nullptr || parentBox == nullptr) {
        return;
    }
    const QStringList& siblings = map.node(parent)->children;
    const qsizetype index = siblings.indexOf(id);
    QPointF place;
    if (const NodeBox* before = index > 0 ? m_scene->box(siblings.at(index - 1)) : nullptr;
        before != nullptr) {
        place = QPointF(before->rect.left() + box->rect.width() / 2.0,
                        before->rect.center().y() + FREE_UNDER);
    } else if (const NodeBox* after =
                   index + 1 < siblings.size() ? m_scene->box(siblings.at(index + 1)) : nullptr;
               after != nullptr) {
        place = QPointF(after->rect.left() + box->rect.width() / 2.0,
                        after->rect.center().y() - FREE_UNDER);
    } else {
        const QPointF root = m_scene->box(map.root()) != nullptr
                                 ? m_scene->box(map.root())->rect.center()
                                 : QPointF();
        const double side = parentBox->rect.center().x() < root.x() ? -1.0 : 1.0;
        place = QPointF(parentBox->rect.center().x() +
                            side * (parentBox->rect.width() / 2.0 + FREE_BESIDE +
                                    box->rect.width() / 2.0),
                        parentBox->rect.center().y());
    }
    const QPointF shift = place - box->rect.center();
    QHash<QString, QPointF> places;
    places.insert(id, place);
    for (const QString& inside : map.descendantsOf(id)) {
        if (const NodeBox* shown = m_scene->box(inside); shown != nullptr) {
            places.insert(inside, shown->rect.center() + shift);
        }
    }
    // A part of adding the node: not a step of Undo of its own
    m_document.changeView([&places](MindMap& changed) {
        for (auto it = places.cbegin(); it != places.cend(); ++it) {
            if (const MindMapNode* each = changed.node(it.key()); each != nullptr) {
                MindMapNode moved = *each;
                moved.freePlace = it.value();
                changed.setNode(moved);
            }
        }
    });
    refresh();
}

// =============================================================================
// Connections
// =============================================================================

void MindMapView::startConnecting() {
    const QString from = m_document.selected();
    if (m_document.map().node(from) == nullptr) {
        return;
    }
    m_connectFrom = from;
    //: In Polish: Wskaż węzeł, z którym połączyć „%1” – kliknięciem albo strzałkami i Enter.
    //: Esc odwołuje.
    emit told(tr("Choose the node to connect “%1” with: click it, or go to it with the arrows "
                 "and press Enter. Esc cancels.")
                  .arg(m_document.shownText(from)),
              false);
    viewport()->update();
}

void MindMapView::finishConnecting(const QString& to) {
    const QString from = m_connectFrom;
    m_connectFrom.clear();
    viewport()->update();
    if (to.isEmpty() || to == from) {
        //: In Polish: Połączenie odwołane.
        emit told(tr("The connection is cancelled."), false);
        return;
    }
    if (!m_document.connectNodes(from, to)) {
        //: In Polish: Te węzły są już połączone.
        emit told(tr("These nodes are connected already."), false);
        return;
    }
    //: In Polish: Połączono „%1” z „%2”. Wpisz podpis i naciśnij Enter.
    emit told(tr("Connected “%1” with “%2”. Type a label and press Enter.")
                  .arg(m_document.shownText(from), m_document.shownText(to)),
              true);
    startEditingLabel(m_document.map().connections().size() - 1);
}

// =============================================================================
// The keys
// =============================================================================

void MindMapView::keyPressEvent(QKeyEvent* event) {
    const MindMap& map = m_document.map();
    if (map.node(m_document.selected()) == nullptr) {
        m_document.select(map.root());
    }
    const QString id = m_document.selected();
    const int key = event->key();
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool control = (modifiers & Qt::ControlModifier) != 0;
    const bool shift = (modifiers & Qt::ShiftModifier) != 0;
    const bool alt = (modifiers & Qt::AltModifier) != 0;
    const bool enter = key == Qt::Key_Return || key == Qt::Key_Enter;

    if ((key == Qt::Key_F10 && shift) || key == Qt::Key_Menu) {
        const NodeBox* box = m_scene ? m_scene->box(id) : nullptr;
        showMenuAt(box != nullptr ? toViewport(box->rect.center()).toPoint()
                                  : viewport()->rect().center());
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
    if (control && (key == Qt::Key_Y || (key == Qt::Key_Z && shift))) {
        const QString what = m_document.redoText();
        m_document.redo();
        if (!what.isEmpty()) {
            //: In Polish: Ponowiono: %1.
            emit told(tr("Done again: %1.").arg(what), false);
        }
        return;
    }
    if (control && key == Qt::Key_Z) {
        const QString what = m_document.undoText();
        m_document.undo();
        if (!what.isEmpty()) {
            //: In Polish: Cofnięto: %1.
            emit told(tr("Undone: %1.").arg(what), false);
        }
        return;
    }

    if (!m_connectFrom.isEmpty()) {
        if (key == Qt::Key_Escape) {
            finishConnecting(QString());
            return;
        }
        if (enter) {
            finishConnecting(id);
            return;
        }
    }

    // A connection selected: its keys
    const qsizetype connection = m_document.selectedConnection();
    if (connection >= 0) {
        if (key == Qt::Key_Delete) {
            m_document.removeConnection(connection);
            m_document.select(id);
            //: In Polish: Usunięto połączenie.
            emit told(tr("The connection is deleted."), true);
            return;
        }
        if (key == Qt::Key_F2 || enter) {
            startEditingLabel(connection);
            return;
        }
        m_document.select(id);
        if (key == Qt::Key_Escape) {
            return;
        }
    }

    const bool arrow = key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up ||
                       key == Qt::Key_Down;
    if (arrow) {
        const int dx = key == Qt::Key_Left ? -1 : key == Qt::Key_Right ? 1 : 0;
        const int dy = key == Qt::Key_Up ? -1 : key == Qt::Key_Down ? 1 : 0;
        if (control) {
            moveSelected(dx, dy);
        } else if (m_scene) {
            const QString next = m_scene->nodeToward(id, dx, dy);
            if (!next.isEmpty()) {
                m_document.select(next);
            }
        }
        return;
    }
    if (key == Qt::Key_Home) {
        m_document.select(map.root());
        ensureShown(map.root());
        return;
    }
    if (key == Qt::Key_End) {
        // The last node of the branch, as deep as it shows
        QString last = id;
        for (const MindMapNode* node = map.node(last);
             node != nullptr && !node->collapsed && !node->children.isEmpty();
             node = map.node(last)) {
            last = node->children.constLast();
        }
        m_document.select(last);
        return;
    }
    if (key == Qt::Key_Backtab) {
        const QString parent = map.parentOf(id);
        if (!parent.isEmpty()) {
            m_document.select(parent);
        }
        return;
    }
    if ((key == Qt::Key_Tab && !shift) || key == Qt::Key_Insert) {
        addChild();
        return;
    }
    if (enter) {
        if (control) {
            openLink();
        } else if (alt) {
            emit noteRequested(id);
        } else {
            addSibling(shift);
        }
        return;
    }
    if (key == Qt::Key_F2) {
        startEditing(id);
        return;
    }
    if (key == Qt::Key_Space && !control && !alt) {
        toggleSelected();
        return;
    }
    if (key == Qt::Key_Delete) {
        removeSelected();
        return;
    }
    if (key == Qt::Key_Escape) {
        return;
    }
    if (control && key == Qt::Key_C) {
        copyAsText();
        return;
    }
    if (control && key == Qt::Key_V) {
        paste();
        return;
    }
    const QString text = event->text();
    if (!control && !alt && !text.isEmpty() && text.at(0).isPrint() &&
        !text.trimmed().isEmpty()) {
        // Typing changes the text of the node, as in a table
        startEditing(id, text);
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

// =============================================================================
// The mouse
// =============================================================================

MindMapView::Drop MindMapView::dropAt(const QPointF& mapPoint) const {
    const MindMap& map = m_document.map();
    Drop drop;
    QString target;
    const QStringList inside = map.descendantsOf(m_dragNode);
    for (auto it = m_scene->nodes().crbegin(); it != m_scene->nodes().crend(); ++it) {
        if (it->id != m_dragNode && it->rect.contains(mapPoint) && !inside.contains(it->id)) {
            target = it->id;
            break;
        }
    }
    if (target.isEmpty()) {
        // Over the node itself or its branch it goes nowhere, but where the writer puts it
        const bool over = !m_scene->nodeAt(mapPoint).isEmpty();
        drop.where = !over || map.layout() == MindMapLayout::Free ? Drop::Where::Place
                                                                   : Drop::Where::None;
        return drop;
    }
    drop.target = target;
    drop.where = Drop::Where::Into;
    const NodeBox* box = m_scene->box(target);
    if (map.layout() != MindMapLayout::Free && box != nullptr && !map.parentOf(target).isEmpty()) {
        const double along = map.layout() == MindMapLayout::Down
                                 ? (mapPoint.x() - box->rect.left()) / box->rect.width()
                                 : (mapPoint.y() - box->rect.top()) / box->rect.height();
        if (along < EDGE) {
            drop.where = Drop::Where::Before;
        } else if (along > 1.0 - EDGE) {
            drop.where = Drop::Where::After;
        }
    }
    return drop;
}

void MindMapView::finishDrag(const QPointF& mapPoint) {
    const MindMap& map = m_document.map();
    const QString id = m_dragNode;
    const NodeBox* box = m_scene->box(id);
    const MindMapNode* node = map.node(id);
    if (box == nullptr || node == nullptr) {
        return;
    }
    const Drop drop = dropAt(mapPoint);
    const QString text = m_document.shownText(id);
    const QPointF centre = mapPoint - m_dragGrab;
    const QPointF shift = centre - box->rect.center();
    const bool free = map.layout() == MindMapLayout::Free;

    // The places of the node and its branch, moved with it, in the free layout
    QHash<QString, QPointF> places;
    places.insert(id, centre);
    for (const QString& inside : map.descendantsOf(id)) {
        const MindMapNode* each = map.node(inside);
        const NodeBox* shown = m_scene->box(inside);
        if (each != nullptr && (each->freePlace || shown != nullptr)) {
            places.insert(inside, each->freePlace.value_or(
                                      shown != nullptr ? shown->rect.center() : QPointF()) +
                                      shift);
        }
    }

    switch (drop.where) {
    case Drop::Where::Into: {
        if (map.parentOf(id) == drop.target && !free) {
            //: In Polish: „%1” już jest w „%2”.
            emit told(tr("“%1” is inside “%2” already.")
                          .arg(text, m_document.shownText(drop.target)),
                      false);
            return;
        }
        const QString target = drop.target;
        //: What Undo undoes in a mind map. In Polish: przeniesienie węzła
        const bool moved =
            m_document.change(tr("moving a node"), [&id, &target, &places, free](MindMap& changed) {
                if (!changed.move(id, target, -1)) {
                    return false;
                }
                for (auto it = places.cbegin(); free && it != places.cend(); ++it) {
                    if (const MindMapNode* each = changed.node(it.key()); each != nullptr) {
                        MindMapNode placed = *each;
                        placed.freePlace = it.value();
                        changed.setNode(placed);
                    }
                }
                return true;
            });
        if (moved) {
            //: In Polish: Przeniesiono „%1” do „%2”.
            emit told(tr("Moved “%1” into “%2”.").arg(text, m_document.shownText(target)), true);
        }
        return;
    }
    case Drop::Where::Before:
    case Drop::Where::After: {
        const QString parent = map.parentOf(drop.target);
        const qsizetype index = map.node(parent)->children.indexOf(drop.target) +
                                (drop.where == Drop::Where::After ? 1 : 0);
        if (m_document.moveNode(id, parent, index)) {
            emit told(drop.where == Drop::Where::Before
                          //: In Polish: „%1” jest teraz przed „%2”.
                          ? tr("“%1” is now before “%2”.")
                                .arg(text, m_document.shownText(drop.target))
                          //: In Polish: „%1” jest teraz za „%2”.
                          : tr("“%1” is now after “%2”.")
                                .arg(text, m_document.shownText(drop.target)),
                      true);
        }
        return;
    }
    case Drop::Where::Place:
        if (free) {
            m_document.movePlaces(places, true);
        } else if (map.looseNodes().contains(id)) {
            m_document.movePlaces({{id, centre}}, false);
        } else if (m_document.makeLoose(id, centre)) {
            //: In Polish: „%1” jest teraz luźnym węzłem. Przeciągnij go na węzeł, żeby wrócił do
            //: gałęzi.
            emit told(tr("“%1” is a loose node now. Drag it onto a node to bring it back into a "
                         "branch.")
                          .arg(text),
                      true);
        }
        return;
    case Drop::Where::None:
        //: In Polish: Tu nie można upuścić węzła.
        emit told(tr("A node cannot be dropped here."), false);
        return;
    }
}

void MindMapView::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    m_pressAt = event->position().toPoint();
    if (event->button() == Qt::MiddleButton) {
        m_drag = Drag::Pan;
        viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton || !m_scene) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    const QPointF point = toMap(event->position());
    if (const QString badge = m_scene->badgeAt(point); !badge.isEmpty()) {
        m_document.select(badge);
        m_document.toggleCollapsed(badge);
        return;
    }
    const QString id = m_scene->nodeAt(point);
    if (!m_connectFrom.isEmpty()) {
        finishConnecting(id);
        return;
    }
    if (!id.isEmpty()) {
        const NodeBox* box = m_scene->box(id);
        m_document.select(id);
        if (box->link && box->linkRect().contains(point)) {
            openLink();
            return;
        }
        if (box->note && box->noteRect().contains(point)) {
            emit noteRequested(id);
            return;
        }
        if (id == m_document.map().root() && m_document.map().layout() != MindMapLayout::Free) {
            return;  // The main topic stays in the middle of the tree
        }
        m_drag = Drag::Pending;
        m_dragNode = id;
        m_dragGrab = point - box->rect.center();
        m_dragAt = point;
        return;
    }
    if (const qsizetype connection = m_scene->connectionAt(point); connection >= 0) {
        m_document.selectConnection(connection);
        return;
    }
    if (m_document.selectedConnection() >= 0) {
        m_document.select(m_document.selected());
    }
    m_drag = Drag::Pan;
}

void MindMapView::mouseMoveEvent(QMouseEvent* event) {
    const QPoint at = event->position().toPoint();
    switch (m_drag) {
    case Drag::Pan: {
        const QPoint moved = at - m_pressAt;
        m_pressAt = at;
        viewport()->setCursor(Qt::ClosedHandCursor);
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - moved.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - moved.y());
        return;
    }
    case Drag::Pending:
        if ((at - m_pressAt).manhattanLength() < QApplication::startDragDistance()) {
            return;
        }
        m_drag = Drag::Node;
        viewport()->setCursor(Qt::ClosedHandCursor);
        [[fallthrough]];
    case Drag::Node:
        m_dragAt = toMap(event->position());
        m_drop = dropAt(m_dragAt);
        viewport()->update();
        return;
    case Drag::None:
        break;
    }
    // Over a link, a note or a badge: they are clicked
    if (m_scene) {
        const QPointF point = toMap(event->position());
        const NodeBox* box = m_scene->box(m_scene->nodeAt(point));
        const bool clickable =
            !m_scene->badgeAt(point).isEmpty() ||
            (box != nullptr && ((box->link && box->linkRect().contains(point)) ||
                                (box->note && box->noteRect().contains(point))));
        if (clickable) {
            viewport()->setCursor(Qt::PointingHandCursor);
        } else {
            viewport()->unsetCursor();
        }
    }
}

void MindMapView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_drag == Drag::Node) {
        finishDrag(toMap(event->position()));
    }
    m_drag = Drag::None;
    m_dragNode.clear();
    m_drop = Drop();
    viewport()->unsetCursor();
    viewport()->update();
}

void MindMapView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !m_scene) {
        QAbstractScrollArea::mouseDoubleClickEvent(event);
        return;
    }
    m_drag = Drag::None;
    const QPointF point = toMap(event->position());
    if (!m_scene->badgeAt(point).isEmpty()) {
        return;
    }
    if (const QString id = m_scene->nodeAt(point); !id.isEmpty()) {
        const NodeBox* box = m_scene->box(id);
        if ((box->link && box->linkRect().contains(point)) ||
            (box->note && box->noteRect().contains(point))) {
            return;
        }
        startEditing(id);
        return;
    }
    if (const qsizetype connection = m_scene->connectionAt(point); connection >= 0) {
        startEditingLabel(connection);
        return;
    }
    // On an empty place: a loose node there
    const QString added = m_document.addLoose(point);
    if (!added.isEmpty()) {
        if (m_document.map().layout() == MindMapLayout::Free) {
            m_document.changeView([&added, &point](MindMap& map) {
                MindMapNode node = *map.node(added);
                node.freePlace = point;
                map.setNode(node);
            });
            refresh();
        }
        startEditing(added);
        m_editingNew = true;
    }
}

// =============================================================================
// The menu
// =============================================================================

void MindMapView::contextMenuEvent(QContextMenuEvent* event) {
    if (isEditing() || !m_scene) {
        return;
    }
    if (event->reason() != QContextMenuEvent::Mouse) {
        const NodeBox* box = m_scene->box(m_document.selected());
        showMenuAt(box != nullptr ? toViewport(box->rect.center()).toPoint()
                                  : viewport()->rect().center());
        return;
    }
    const QPointF point = toMap(event->pos());
    if (const QString id = m_scene->nodeAt(point); !id.isEmpty()) {
        m_document.select(id);
        showMenuAt(event->pos());
        return;
    }
    if (const qsizetype connection = m_scene->connectionAt(point); connection >= 0) {
        m_document.selectConnection(connection);
        showMenuAt(event->pos());
        return;
    }
    // On an empty place
    QMenu menu(this);
    //: In the menu of an empty place of a mind map. In Polish: Nowy luźny węzeł tutaj
    connect(menu.addAction(tr("New loose node here")), &QAction::triggered, this,
            [this, point]() {
                const QString added = m_document.addLoose(point);
                if (added.isEmpty()) {
                    return;
                }
                if (m_document.map().layout() == MindMapLayout::Free) {
                    m_document.changeView([&added, &point](MindMap& map) {
                        MindMapNode node = *map.node(added);
                        node.freePlace = point;
                        map.setNode(node);
                    });
                    refresh();
                }
                startEditing(added);
                m_editingNew = true;
            });
    QAction* paste = menu.addAction(tr("Paste"));
    showKeys(paste, QKeySequence(Qt::CTRL | Qt::Key_V));
    paste->setEnabled(MindMapMenus::canPaste());
    connect(paste, &QAction::triggered, this, &MindMapView::paste);
    menu.addSeparator();
    //: In Polish: Powiększenie 100%
    QAction* zoom = menu.addAction(tr("Zoom 100%"));
    showKeys(zoom, QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(zoom, &QAction::triggered, this, &MindMapView::resetZoom);
    menu.exec(event->globalPos());
}

void MindMapView::showMenuAt(const QPoint& viewportPoint) {
    QMenu menu(this);
    fillMenu(menu);
    if (!menu.isEmpty()) {
        menu.exec(viewport()->mapToGlobal(viewportPoint));
    }
}

void MindMapView::fillMenu(QMenu& menu) {
    const MindMap& map = m_document.map();
    const qsizetype connection = m_document.selectedConnection();
    if (connection >= 0 && connection < map.connections().size()) {
        //: In the menu of a connection of a mind map. In Polish: Podpis…
        QAction* label = menu.addAction(tr("Label…"));
        showKeys(label, QKeySequence(Qt::Key_F2));
        connect(label, &QAction::triggered, this,
                [this, connection]() { startEditingLabel(connection); });
        //: In Polish: Odwróć strzałkę
        connect(menu.addAction(tr("Reverse the arrow")), &QAction::triggered, this,
                [this, connection]() { m_document.reverseConnection(connection); });
        menu.addSeparator();
        //: In Polish: Usuń połączenie
        QAction* remove = menu.addAction(tr("Delete the connection"));
        showKeys(remove, QKeySequence(Qt::Key_Delete));
        connect(remove, &QAction::triggered, this, [this, connection]() {
            m_document.removeConnection(connection);
            m_document.select(m_document.selected());
            //: In Polish: Usunięto połączenie.
            emit told(tr("The connection is deleted."), true);
        });
        return;
    }
    const QString id = m_document.selected();
    const MindMapNode* node = map.node(id);
    if (node == nullptr) {
        return;
    }
    const bool root = id == map.root();
    const bool looseTop = map.looseNodes().contains(id);

    //: In the menu of a node of a mind map. In Polish: Nowy węzeł w środku
    QAction* child = menu.addAction(tr("New node inside"));
    showKeys(child, QKeySequence(Qt::Key_Tab));
    connect(child, &QAction::triggered, this, &MindMapView::addChild);
    if (!root) {
        //: In Polish: Nowy węzeł za nim
        QAction* after = menu.addAction(tr("New node after it"));
        showKeys(after, QKeySequence(Qt::Key_Return));
        connect(after, &QAction::triggered, this, [this]() { addSibling(false); });
        //: In Polish: Nowy węzeł przed nim
        QAction* before = menu.addAction(tr("New node before it"));
        showKeys(before, QKeySequence(Qt::SHIFT | Qt::Key_Return));
        connect(before, &QAction::triggered, this, [this]() { addSibling(true); });
    }
    menu.addSeparator();
    //: In Polish: Zmień tekst
    QAction* edit = menu.addAction(tr("Change the text"));
    showKeys(edit, QKeySequence(Qt::Key_F2));
    connect(edit, &QAction::triggered, this, [this, id]() { startEditing(id); });
    //: In Polish: Notatka…
    QAction* note = menu.addAction(tr("Note…"));
    showKeys(note, QKeySequence(Qt::ALT | Qt::Key_Return));
    connect(note, &QAction::triggered, this, [this, id]() { emit noteRequested(id); });
    //: In Polish: Element książki…
    connect(menu.addAction(tr("Element of the book…")), &QAction::triggered, this,
            [this, id]() { emit linkRequested(id); });
    if (!node->link.isEmpty()) {
        //: In Polish: Otwórz element
        QAction* open = menu.addAction(tr("Open the element"));
        showKeys(open, QKeySequence(Qt::CTRL | Qt::Key_Return));
        connect(open, &QAction::triggered, this, &MindMapView::openLink);
        //: In Polish: Odłącz element
        connect(menu.addAction(tr("Unlink the element")), &QAction::triggered, this,
                [this, id]() { m_document.setLink(id, QString()); });
    }
    MindMapMenus::addMarkers(menu, m_document, m_look, id);
    if (!root) {
        MindMapMenus::addColors(menu, m_document, m_look, id);
    }
    menu.addSeparator();
    //: In Polish: Połącz z…
    connect(menu.addAction(tr("Connect to…")), &QAction::triggered, this,
            &MindMapView::startConnecting);
    if (!node->children.isEmpty()) {
        QAction* collapse = menu.addAction(node->collapsed
                                               //: In Polish: Rozwiń gałąź
                                               ? tr("Expand the branch")
                                               //: In Polish: Zwiń gałąź
                                               : tr("Collapse the branch"));
        showKeys(collapse, QKeySequence(Qt::Key_Space));
        connect(collapse, &QAction::triggered, this, &MindMapView::toggleSelected);
    }
    if (!root && !looseTop) {
        //: In Polish: Odłącz od gałęzi
        connect(menu.addAction(tr("Detach from the branch")), &QAction::triggered, this,
                [this, id]() {
                    const NodeBox* box = m_scene ? m_scene->box(id) : nullptr;
                    const QPointF place = box != nullptr ? box->rect.center() +
                                                               QPointF(LOOSE_SHIFT, LOOSE_SHIFT)
                                                         : QPointF();
                    m_document.makeLoose(id, place);
                });
    }
    menu.addSeparator();
    //: In Polish: Kopiuj jako tekst
    QAction* copy = menu.addAction(tr("Copy as text"));
    showKeys(copy, QKeySequence(Qt::CTRL | Qt::Key_C));
    connect(copy, &QAction::triggered, this, &MindMapView::copyAsText);
    QAction* paste = menu.addAction(tr("Paste"));
    showKeys(paste, QKeySequence(Qt::CTRL | Qt::Key_V));
    paste->setEnabled(MindMapMenus::canPaste());
    connect(paste, &QAction::triggered, this, &MindMapView::paste);
    menu.addSeparator();
    //: In Polish: Usuń
    QAction* remove = menu.addAction(tr("Delete"));
    showKeys(remove, QKeySequence(Qt::Key_Delete));
    remove->setEnabled(!root);
    connect(remove, &QAction::triggered, this, &MindMapView::removeSelected);
}

}  // namespace kalahari::mindmap
