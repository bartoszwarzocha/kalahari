// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_view_input.cpp
/// @brief The map of nodes and the keys, the mouse, the menu and the cards dropped on it

#include <kalahari/core/project_manager.h>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/mindmap/mind_map_content.h>
#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_view.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>

#include <algorithm>
#include <limits>
#include <utility>

namespace kalahari::mindmap {

namespace {

/// How near a socket the mouse catches it, on the screen
constexpr double SOCKET_REACH = 9.0;
/// How near a socket a dragged link snaps to it
constexpr double TARGET_REACH = 16.0;
/// How near a line the mouse catches it
constexpr double LINK_REACH = 6.0;
/// The arrow of a header catches the mouse this much around it
constexpr double FOLD_REACH = 3.0;

/// The name of the sockets of @p kind, in a message
QString socketName(SocketKind kind) {
    switch (kind) {
    case SocketKind::Branch:
        //: The sockets of a node of a mind map, in a message. In Polish: gałąź
        return QCoreApplication::translate("kalahari::mindmap::MindMapView", "branch");
    case SocketKind::Family:
        //: In Polish: rodzina
        return QCoreApplication::translate("kalahari::mindmap::MindMapView", "family");
    case SocketKind::Relations:
        //: In Polish: relacje
        return QCoreApplication::translate("kalahari::mindmap::MindMapView", "relations");
    case SocketKind::Places:
        //: In Polish: miejsca
        return QCoreApplication::translate("kalahari::mindmap::MindMapView", "places");
    case SocketKind::Items:
        //: In Polish: przedmioty
        return QCoreApplication::translate("kalahari::mindmap::MindMapView", "items");
    }
    return QString();
}

/// The elements of the book among the links dragged from the Navigator
QStringList elementsOf(const core::ProjectManager* book, const QMimeData* data) {
    QStringList elements;
    if (book == nullptr || data == nullptr) {
        return elements;
    }
    for (const QString& link : gui::NavigatorPanel::linksOf(data)) {
        if (book->findElement(link) != nullptr && !elements.contains(link)) {
            elements.append(link);
        }
    }
    return elements;
}

}  // namespace

// =============================================================================
// The keys
// =============================================================================

bool MindMapView::ownKey(const QKeyEvent& key) const {
    const Qt::KeyboardModifiers modifiers = key.modifiers() & ~Qt::KeypadModifier;
    const int code = key.key();
    const bool plain = modifiers == Qt::NoModifier;
    const bool shift = modifiers == Qt::ShiftModifier;
    const bool control = modifiers & Qt::ControlModifier;
    const bool alt = modifiers == Qt::AltModifier;
    const bool enter = code == Qt::Key_Return || code == Qt::Key_Enter;
    const bool arrow = code == Qt::Key_Left || code == Qt::Key_Right || code == Qt::Key_Up ||
                       code == Qt::Key_Down;
    if (plain) {
        return enter || arrow || code == Qt::Key_Delete || code == Qt::Key_Backspace ||
               code == Qt::Key_F2 || code == Qt::Key_Insert || code == Qt::Key_Home ||
               code == Qt::Key_Escape || code == Qt::Key_Tab || code == Qt::Key_H;
    }
    if (shift) {
        return enter || code == Qt::Key_A || code == Qt::Key_Backtab;
    }
    if (control) {
        return enter || arrow || code == Qt::Key_Z || code == Qt::Key_Y || code == Qt::Key_C ||
               code == Qt::Key_V || code == Qt::Key_A || code == Qt::Key_J ||
               code == Qt::Key_Plus || code == Qt::Key_Equal || code == Qt::Key_Minus ||
               code == Qt::Key_0;
    }
    return alt && enter;
}

bool MindMapView::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride && !isEditing() &&
        ownKey(*static_cast<QKeyEvent*>(event))) {
        // The keys of the map go to it before the commands of the window: Delete, Ctrl+Z...
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

void MindMapView::keyPressEvent(QKeyEvent* event) {
    if (isEditing() || !ownKey(*event)) {
        QWidget::keyPressEvent(event);
        return;
    }
    finishAnimations();
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const int code = event->key();
    const bool enter = code == Qt::Key_Return || code == Qt::Key_Enter;
    const QString selected = m_document.selected();
    const auto toward = [this, &selected](int dx, int dy) {
        if (selected.isEmpty()) {
            // The main topic, or the first node a map of relations draws
            if (m_document.map().showsRoot()) {
                m_document.select(m_document.map().root());
            } else if (m_scene && !m_scene->nodes().isEmpty()) {
                m_document.select(m_scene->nodes().constFirst().id);
            }
            return;
        }
        if (const QString next = m_scene ? m_scene->nodeToward(selected, dx, dy) : QString();
            !next.isEmpty()) {
            m_document.select(next);
        }
    };
    if (modifiers & Qt::ControlModifier) {
        const double step = NodeMetrics::GRID;
        switch (code) {
        case Qt::Key_Z:
            tellUndone(modifiers & Qt::ShiftModifier);
            break;
        case Qt::Key_Y:
            tellUndone(true);
            break;
        case Qt::Key_C:
            copySelected();
            break;
        case Qt::Key_V:
            paste();
            break;
        case Qt::Key_A:
            m_link.reset();
            m_frame.clear();
            m_document.setSelection(m_document.map().nodeIds());
            break;
        case Qt::Key_J:
            frameSelected();
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomBy(1);
            break;
        case Qt::Key_Minus:
            zoomBy(-1);
            break;
        case Qt::Key_0:
            resetZoom();
            break;
        case Qt::Key_Left:
            moveSelectedBy(QPointF(-step, 0.0));
            break;
        case Qt::Key_Right:
            moveSelectedBy(QPointF(step, 0.0));
            break;
        case Qt::Key_Up:
            moveSelectedBy(QPointF(0.0, -step));
            break;
        case Qt::Key_Down:
            moveSelectedBy(QPointF(0.0, step));
            break;
        default:
            if (enter) {
                openSelected();
            }
            break;
        }
        event->accept();
        return;
    }
    if (modifiers == Qt::AltModifier) {
        if (!selected.isEmpty()) {
            emit nodeWindowRequested(selected);
        }
        event->accept();
        return;
    }
    if (modifiers == Qt::ShiftModifier) {
        if (enter) {
            addBelow(true);
        } else if (code == Qt::Key_A) {
            openAddNode();
        } else if (code == Qt::Key_Backtab && !selected.isEmpty()) {
            if (const QString parent = m_document.map().parentOf(selected); !parent.isEmpty()) {
                m_document.select(parent);
            }
        }
        event->accept();
        return;
    }
    switch (code) {
    case Qt::Key_Tab:
    case Qt::Key_Insert:
        addBranch();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        addBelow(false);
        break;
    case Qt::Key_F2:
        if (m_link && m_link->type == LinkLine::Type::Connection) {
            startEditingLabel(m_link->connection);
        } else if (!m_frame.isEmpty()) {
            startEditingFrame(m_frame);
        } else if (!selected.isEmpty()) {
            startEditing(selected);
        }
        break;
    case Qt::Key_H:
        foldSelected();
        break;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        removeSelected();
        break;
    case Qt::Key_Home:
        showAll();
        break;
    case Qt::Key_Escape:
        if (m_drag != Drag::None) {
            m_drag = Drag::None;
            m_linkStart.reset();
            m_linkTarget.reset();
            refresh();
        } else {
            clearSelection();
        }
        break;
    case Qt::Key_Left:
        toward(-1, 0);
        break;
    case Qt::Key_Right:
        toward(1, 0);
        break;
    case Qt::Key_Up:
        toward(0, -1);
        break;
    case Qt::Key_Down:
        toward(0, 1);
        break;
    default:
        break;
    }
    event->accept();
}

// =============================================================================
// The mouse
// =============================================================================

void MindMapView::mousePressEvent(QMouseEvent* event) {
    finishAnimations();
    if (isEditing()) {
        finishEditing(true);
    }
    setFocus(Qt::MouseFocusReason);
    const QPointF point = toMap(event->position());
    m_pressAt = event->position().toPoint();
    m_pressMap = point;
    m_dragMap = point;
    m_dragDone = QPointF();
    m_pressNode.clear();
    m_dragNodes.clear();
    m_linkStart.reset();
    m_linkTarget.reset();
    m_panOffset = m_offset;
    if (event->button() == Qt::MiddleButton) {
        m_drag = Drag::Pan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton || !m_scene) {
        QWidget::mousePressEvent(event);
        return;
    }
    const bool adding = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    m_drag = Drag::Pending;
    if (std::optional<SocketBox> socket = m_scene->socketAt(point, reach(SOCKET_REACH))) {
        m_linkStart = socket;
        m_pending = Drag::Link;
        return;
    }
    if (const QString id = m_scene->nodeAt(point); !id.isEmpty()) {
        const NodeBox* box = m_scene->box(id);
        if (!adding &&
            box->fold.adjusted(-FOLD_REACH, -FOLD_REACH, FOLD_REACH, FOLD_REACH).contains(point)) {
            m_drag = Drag::None;
            m_document.select(id);
            m_document.toggleFolded({id});
            return;
        }
        m_link.reset();
        m_frame.clear();
        m_pressNode = id;
        if (adding) {
            m_document.toggleSelection(id);
        } else if (!m_document.selection().contains(id)) {
            m_document.select(id);
        }
        m_dragNodes = m_document.selection();
        m_pending = m_dragNodes.contains(id) ? Drag::Nodes : Drag::None;
        update();
        return;
    }
    if (const QString frame = m_scene->frameAt(point); !frame.isEmpty()) {
        selectFrame(frame);
        if (const MindMapFrame* own = m_document.map().frame(frame)) {
            m_dragNodes = own->nodes;
        }
        m_pending = Drag::Frame;
        return;
    }
    if (const qsizetype link = m_scene->linkAt(point, reach(LINK_REACH)); link >= 0) {
        selectLink(linkRefOf(link));
        m_drag = Drag::None;
        return;
    }
    m_pending = adding ? Drag::Band : Drag::Pan;
}

std::optional<SocketBox> MindMapView::targetOf(const QPointF& mapPoint) const {
    if (!m_scene || !m_linkStart) {
        return std::nullopt;
    }
    if (std::optional<SocketBox> socket = m_scene->socketAt(mapPoint, reach(TARGET_REACH));
        socket && MindMapScene::joinable(*m_linkStart, *socket)) {
        return socket;
    }
    // Over a node: its socket that joins, the nearest
    const NodeBox* box = m_scene->box(m_scene->nodeAt(mapPoint));
    if (box == nullptr) {
        return std::nullopt;
    }
    std::optional<SocketBox> best;
    double nearest = std::numeric_limits<double>::max();
    for (const SocketBox& socket : box->sockets) {
        if (!MindMapScene::joinable(*m_linkStart, socket)) {
            continue;
        }
        const QPointF way = socket.centre - mapPoint;
        const double distance = way.x() * way.x() + way.y() * way.y();
        if (distance < nearest) {
            nearest = distance;
            best = socket;
        }
    }
    return best;
}

void MindMapView::mouseMoveEvent(QMouseEvent* event) {
    const QPointF point = toMap(event->position());
    if (m_drag == Drag::Pending) {
        if ((event->position().toPoint() - m_pressAt).manhattanLength() <
            QApplication::startDragDistance()) {
            return;
        }
        m_drag = m_pending;
        if (m_drag == Drag::Pan) {
            setCursor(Qt::ClosedHandCursor);
        }
    }
    switch (m_drag) {
    case Drag::Nodes:
    case Drag::Frame: {
        const QPointF delta = point - m_pressMap - m_dragDone;
        if (m_scene) {
            m_scene->moveNodes(m_dragNodes, delta);
        }
        m_dragDone += delta;
        update();
        break;
    }
    case Drag::Pan:
        m_offset = m_panOffset + (event->position() - QPointF(m_pressAt));
        viewMoved();
        break;
    case Drag::Link:
        m_dragMap = point;
        m_linkTarget = targetOf(point);
        update();
        break;
    case Drag::Band:
        m_dragMap = point;
        update();
        break;
    case Drag::None:
    case Drag::Pending: {
        const std::optional<SocketBox> socket = m_scene
                                                    ? m_scene->socketAt(point, reach(SOCKET_REACH))
                                                    : std::nullopt;
        if (socket != m_hoverSocket) {
            m_hoverSocket = socket;
            update();
        }
        if (socket) {
            setCursor(Qt::CrossCursor);
        } else {
            unsetCursor();
        }
        break;
    }
    }
}

void MindMapView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_drag == Drag::Pending) {
        // A click: what it was on is selected
        m_drag = Drag::None;
        const bool adding = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
        if (m_pending == Drag::Pan) {
            clearSelection();
        } else if (m_pending == Drag::Nodes && !adding && m_document.selection().size() > 1) {
            m_document.select(m_pressNode);
        } else if (m_pending == Drag::Link && m_linkStart) {
            m_link.reset();
            m_frame.clear();
            m_document.select(m_linkStart->node);
        }
        m_linkStart.reset();
        update();
        return;
    }
    if (m_drag == Drag::None) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    finishDrag(toMap(event->position()), event->globalPosition().toPoint(), event->modifiers());
}

void MindMapView::finishDrag(const QPointF& mapPoint, const QPoint& globalPoint,
                             Qt::KeyboardModifiers modifiers) {
    const Drag drag = std::exchange(m_drag, Drag::None);
    unsetCursor();
    switch (drag) {
    case Drag::Nodes:
    case Drag::Frame: {
        const QPointF done = std::exchange(m_dragDone, QPointF());
        if (done.isNull() || !m_document.moveNodesBy(m_dragNodes, done)) {
            refresh();
        }
        break;
    }
    case Drag::Link:
        finishLink(mapPoint, globalPoint);
        break;
    case Drag::Band: {
        const QRectF band = QRectF(m_pressMap, mapPoint).normalized();
        QStringList ids = modifiers & (Qt::ControlModifier | Qt::ShiftModifier)
                              ? m_document.selection()
                              : QStringList();
        if (m_scene) {
            for (const NodeBox& box : m_scene->nodes()) {
                if (band.intersects(box.rect) && !ids.contains(box.id)) {
                    ids.append(box.id);
                }
            }
        }
        if (ids.isEmpty()) {
            clearSelection();
        } else {
            m_link.reset();
            m_frame.clear();
            m_document.setSelection(ids);
        }
        break;
    }
    case Drag::Pan:
    case Drag::Pending:
    case Drag::None:
        break;
    }
    m_linkStart.reset();
    m_linkTarget.reset();
    m_dragNodes.clear();
    update();
}

void MindMapView::finishLink(const QPointF& mapPoint, const QPoint& globalPoint) {
    if (!m_linkStart || !m_scene) {
        return;
    }
    const SocketBox start = *m_linkStart;
    if (const std::optional<SocketBox> target = targetOf(mapPoint)) {
        m_linkStart.reset();
        m_linkTarget.reset();
        joinSockets(start, *target, globalPoint);
        return;
    }
    if (const std::optional<SocketBox> other = m_scene->socketAt(mapPoint, reach(TARGET_REACH));
        other && other->node != start.node) {
        //: In the bar under a mind map, when a link is dropped on a socket of another color.
        //: In Polish: Te gniazda się nie łączą: %1 i %2. Łączą się gniazda tego samego koloru.
        emit told(tr("These sockets do not join: %1 and %2. Sockets of the same color join.")
                      .arg(socketName(start.kind), socketName(other->kind)),
                  false);
        return;
    }
    if (const QString over = m_scene->nodeAt(mapPoint); !over.isEmpty()) {
        if (over != start.node) {
            //: In the bar under a mind map. In Polish: Ten węzeł nie ma gniazda, które łączy się
            //: z tym połączeniem.
            emit told(tr("This node has no socket that joins with this link."), false);
        }
        return;
    }
    if (start.kind == SocketKind::Branch && start.right) {
        // From the output to an empty place: a new node there, joined
        openAddNode(mapPoint - QPointF(0.0, NodeMetrics::HEADER / 2.0), start.node, globalPoint);
        return;
    }
    //: In the bar under a mind map, when a link is dropped on an empty place. In Polish:
    //: Upuść połączenie na gnieździe tego samego koloru. Z prawego gniazda gałęzi w puste
    //: miejsce dodasz połączony węzeł.
    emit told(tr("Drop the link on a socket of the same color. From the socket of a branch on "
                 "the right to an empty place, you add a joined node."),
              false);
}

void MindMapView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !m_scene) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    m_drag = Drag::None;
    const QPointF point = toMap(event->position());
    if (m_scene->socketAt(point, reach(SOCKET_REACH))) {
        return;
    }
    if (const QString id = m_scene->nodeAt(point); !id.isEmpty()) {
        const NodeBox* box = m_scene->box(id);
        if (box->header.contains(point)) {
            if (!box->fold.adjusted(-FOLD_REACH, -FOLD_REACH, FOLD_REACH, FOLD_REACH)
                     .contains(point)) {
                startEditing(id);
            }
        } else {
            emit nodeWindowRequested(id);
        }
        return;
    }
    if (const QString frame = m_scene->frameAt(point); !frame.isEmpty()) {
        startEditingFrame(frame);
        return;
    }
    if (const qsizetype index = m_scene->linkAt(point, reach(LINK_REACH)); index >= 0) {
        const std::optional<LinkRef> link = linkRefOf(index);
        if (link && link->type == LinkLine::Type::Connection) {
            startEditingLabel(link->connection);
        } else if (link && link->type == LinkLine::Type::Relation) {
            changeRelation(link->relation);
        }
        return;
    }
    // On an empty place: Add Node, the new node there
    openAddNode(point - QPointF(NodeMetrics::WIDTH / 2.0, NodeMetrics::HEADER / 2.0), QString(),
                event->globalPosition().toPoint());
}

// =============================================================================
// The menu
// =============================================================================

void MindMapView::contextMenuEvent(QContextMenuEvent* event) {
    if (isEditing() || !m_scene) {
        return;
    }
    finishAnimations();
    QPoint at = event->pos();
    m_menuPoint.reset();
    if (event->reason() == QContextMenuEvent::Mouse) {
        const QPointF point = toMap(event->pos());
        if (const QString id = m_scene->nodeAt(point); !id.isEmpty()) {
            m_link.reset();
            m_frame.clear();
            if (!m_document.selection().contains(id)) {
                m_document.select(id);
            }
        } else if (const qsizetype link = m_scene->linkAt(point, reach(LINK_REACH)); link >= 0) {
            selectLink(linkRefOf(link));
        } else if (const QString frame = m_scene->frameAt(point); !frame.isEmpty()) {
            selectFrame(frame);
        } else {
            clearSelection();
            m_menuPoint = point;
        }
    } else if (const NodeBox* box = m_scene->box(m_document.selected())) {
        at = toView(box->rect.center()).toPoint();
    } else {
        at = rect().center();
    }
    QMenu menu(this);
    fillMenu(menu);
    if (!menu.isEmpty()) {
        menu.exec(mapToGlobal(at));
    }
    m_menuPoint.reset();
}

// =============================================================================
// The cards dragged from the Navigator
// =============================================================================

void MindMapView::dragEnterEvent(QDragEnterEvent* event) {
    if (elementsOf(m_book, event->mimeData()).isEmpty()) {
        event->ignore();
        return;
    }
    m_dropping = true;
    m_dropAt = toMap(event->position()) -
               QPointF(NodeMetrics::WIDTH / 2.0, NodeMetrics::HEADER / 2.0);
    event->acceptProposedAction();
    update();
}

void MindMapView::dragMoveEvent(QDragMoveEvent* event) {
    if (!m_dropping) {
        event->ignore();
        return;
    }
    m_dropAt = toMap(event->position()) -
               QPointF(NodeMetrics::WIDTH / 2.0, NodeMetrics::HEADER / 2.0);
    event->acceptProposedAction();
    update();
}

void MindMapView::dragLeaveEvent(QDragLeaveEvent* event) {
    m_dropping = false;
    update();
    QWidget::dragLeaveEvent(event);
}

void MindMapView::dropEvent(QDropEvent* event) {
    const QStringList elements = elementsOf(m_book, event->mimeData());
    m_dropping = false;
    if (elements.isEmpty()) {
        event->ignore();
        update();
        return;
    }
    event->acceptProposedAction();
    const QPointF place = toMap(event->position()) -
                          QPointF(NodeMetrics::WIDTH / 2.0, NodeMetrics::HEADER / 2.0);
    setFocus(Qt::OtherFocusReason);
    addCards(elements, place);
    update();
}

QString MindMapView::hint() const {
    const MindMap& map = m_document.map();
    if (m_link && m_scene != nullptr) {
        const qsizetype index = linkIndexOf(*m_link);
        if (index >= 0) {
            const LinkLine& line = m_scene->links().at(index);
            QString what;
            switch (line.type) {
            case LinkLine::Type::Branch:
                //: In the line under a mind map, when a branch is selected. In Polish: Gałąź.
                //: Delete ją odłącza; węzeł zostaje na mapie.
                return tr("A branch. Delete detaches it; the node stays on the map.");
            case LinkLine::Type::Connection:
                //: In Polish: połączenie
                what = tr("a connection");
                break;
            case LinkLine::Type::Relation:
                what = socketName(line.kind);
                break;
            }
            return line.label.isEmpty()
                       //: In the line under a mind map; %1: "relacje", "rodzina", "połączenie".
                       //: In Polish: Połączenie: %1. Delete je usuwa.
                       ? tr("Link: %1. Delete removes it.").arg(what)
                       //: %2: its label. In Polish: Połączenie: %1, „%2”. Delete je usuwa.
                       : tr("Link: %1, “%2”. Delete removes it.").arg(what, line.label);
        }
    }
    if (!m_frame.isEmpty()) {
        //: In the line under a mind map, when a frame is selected. In Polish: Ramka. Jej tytuł
        //: przeciąga ją razem z węzłami; Delete usuwa ramkę, węzły zostają.
        return tr("A frame. Its title drags it with its nodes; Delete removes the frame, the "
                  "nodes stay.");
    }
    const QString id = m_document.selected();
    const NodeBox* box = m_scene != nullptr && !id.isEmpty() ? m_scene->box(id) : nullptr;
    if (box != nullptr && m_document.selection().size() == 1) {
        const QString kind = box->content.kindName.toLower();
        if (!box->content.cardKind.isEmpty()) {
            //: In the line under a mind map; %1: the name of a card, %2: its kind ("postać").
            //: In Polish: %1: węzeł karty Warsztatu (%2). Gniazda w kolorach rodzajów łączą
            //: się z gniazdami tego samego koloru.
            return tr("%1: a node of a Workshop card (%2). Sockets in the colors of the kinds "
                      "join with the sockets of the same color.")
                .arg(box->content.title, kind);
        }
        if (id == map.root()) {
            //: In Polish: %1: główny temat. Tab dodaje gałąź po prawej, Shift+A – węzeł
            //: dowolnego rodzaju.
            return tr("%1: the main topic. Tab adds a branch on the right, Shift+A – a node of "
                      "any kind.")
                .arg(box->content.title);
        }
        //: %2: its kind ("pomysł"). In Polish: %1: %2. Tab dodaje gałąź po prawej, Enter –
        //: węzeł poniżej.
        return tr("%1: %2. Tab adds a branch on the right, Enter – a node below.")
            .arg(box->content.title, kind);
    }
    if (m_document.selection().size() > 1) {
        //: In the line under a mind map. In Polish: Wybrane węzły: %n. Przeciągnięcie przesuwa
        //: je razem; Ctrl+J otacza je ramką.
        return tr("Selected nodes: %n. Dragging moves them together; Ctrl+J puts a frame around "
                  "them.",
                  nullptr, static_cast<int>(m_document.selection().size()));
    }
    //: In the line under a mind map, when nothing is selected. In Polish: Przeciągnij od gniazda
    //: do gniazda, aby połączyć. Kółko myszy powiększa, przeciągnięcie tła przesuwa mapę.
    return tr("Drag from socket to socket to connect. The mouse wheel zooms, dragging the "
              "background moves the map.");
}

}  // namespace kalahari::mindmap
