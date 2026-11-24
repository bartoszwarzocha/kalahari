// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_scene.cpp
/// @brief The drawing of a mind map as a map of nodes: the boxes of the nodes with their rows
/// and sockets, the links between the sockets and the frames

#include <kalahari/mindmap/mind_map_scene.h>

#include <QFontMetricsF>
#include <QPainterPathStroker>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace kalahari::mindmap {

namespace {

/// The space of the arrow and of the icon in a header
constexpr double FOLD_SIDE = 14.0;
constexpr double ICON_SIDE = 14.0;
constexpr double HEADER_GAP = 5.0;
/// The space between the lines of a text across a node
constexpr double LINE_GAP = 2.0;
/// A wide row has this much room above and below its lines
constexpr double WIDE_ROOM = 4.0;
/// A curve leaves its socket at least this far before it bends
constexpr double LEAST_BEND = 44.0;
constexpr double BEND = 0.45;
/// A line with right angles turns this far from a socket
constexpr double ELBOW = 28.0;
constexpr double LEAST_ELBOW = 24.0;
/// Two nodes this close across stand side by side: a link of both sides goes out on the right
constexpr double SIDE_BY_SIDE = NodeMetrics::WIDTH * 0.6;
/// The room around the text of the label of a link
constexpr double LABEL_ROOM_X = 6.0;
constexpr double LABEL_ROOM_Y = 2.0;
/// A frame's title strip
constexpr double FRAME_TITLE = 24.0;
constexpr double FRAME_TITLE_ROOM = 4.0;

/// The side of a socket that faces point @p other from node @p box
bool facesRight(const QRectF& box, const QPointF& other) {
    return other.x() >= box.center().x();
}

}  // namespace

const RowBox* NodeBox::row(const QString& rowId) const {
    const auto it = std::find_if(rows.cbegin(), rows.cend(),
                                 [&rowId](const RowBox& each) { return each.row.id == rowId; });
    return it != rows.cend() ? &*it : nullptr;
}

MindMapScene::MindMapScene(const MindMap& map, const MindMapLook& look, const QFont& font,
                           const Content& content, QList<RelationLink> relations)
    : m_map(map),
      m_look(look),
      m_titleFont(font),
      m_textFont(font),
      m_labelFont(font),
      m_content(content),
      m_relations(std::move(relations)) {
    m_titleFont.setPixelSize(
        static_cast<int>(std::lround(NodeMetrics::TITLE_SIZE * look.textScale)));
    m_titleFont.setBold(true);
    m_textFont.setPixelSize(static_cast<int>(std::lround(NodeMetrics::TEXT_SIZE * look.textScale)));
    m_labelFont.setPixelSize(
        static_cast<int>(std::lround(NodeMetrics::LABEL_SIZE * look.textScale)));
    placeNodes();
    addLinks();
    addFrames();
    updateBounds();
}

const NodeBox* MindMapScene::box(const QString& id) const {
    const auto it = m_index.constFind(id);
    return it != m_index.cend() ? &m_nodes.at(*it) : nullptr;
}

QSizeF MindMapScene::sizeOf(const QString& id) const {
    const NodeBox* found = box(id);
    return found != nullptr ? found->rect.size() : QSizeF(NodeMetrics::WIDTH, NodeMetrics::HEADER);
}

// =============================================================================
// The nodes
// =============================================================================

QStringList MindMapScene::wrap(const QString& text, const QFont& font, double width,
                               int most) const {
    const QFontMetricsF metrics(font);
    QStringList lines;
    for (const QString& paragraph : text.split(QLatin1Char('\n'))) {
        QString line;
        for (const QString& word : paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            const QString longer = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
            if (metrics.horizontalAdvance(longer) <= width || line.isEmpty()) {
                line = longer;
            } else {
                lines.append(line);
                line = word;
            }
        }
        lines.append(line);
        if (lines.size() >= most) {
            break;
        }
    }
    if (lines.size() > most || (lines.size() == most && text.count(QLatin1Char('\n')) >= most)) {
        lines = lines.mid(0, most);
    }
    for (QString& line : lines) {
        line = metrics.elidedText(line, Qt::ElideRight, width);
    }
    if (lines.size() == most) {
        // Cut short: the last line says so
        const QString joined = lines.join(QLatin1Char(' '));
        if (joined.size() < text.simplified().size()) {
            lines.last() = metrics.elidedText(lines.last() + QStringLiteral(" …"), Qt::ElideRight,
                                              width);
        }
    }
    return lines;
}

NodeBox MindMapScene::measure(const MindMapNode& node) const {
    NodeBox box;
    box.id = node.id;
    box.root = node.id == m_map.root();
    box.folded = node.folded;
    box.content = m_content(node);
    const QPointF place = node.place.value_or(QPointF());
    const double width = NodeMetrics::WIDTH;
    box.header = QRectF(place, QSizeF(width, NodeMetrics::HEADER));
    box.fold = QRectF(place.x() + HEADER_GAP, place.y() + (NodeMetrics::HEADER - FOLD_SIDE) / 2.0,
                      FOLD_SIDE, FOLD_SIDE);
    box.icon = QRectF(box.fold.right() + 2.0, place.y() + (NodeMetrics::HEADER - ICON_SIDE) / 2.0,
                      ICON_SIDE, ICON_SIDE);
    box.title = QRectF(box.icon.right() + HEADER_GAP, place.y(),
                       place.x() + width - HEADER_GAP - (box.icon.right() + HEADER_GAP),
                       NodeMetrics::HEADER);
    box.elidedTitle =
        QFontMetricsF(m_titleFont).elidedText(box.content.title, Qt::ElideRight, box.title.width());

    double y = place.y() + NodeMetrics::HEADER;
    const bool hasBody = !node.folded &&
                         (!box.content.rows.isEmpty() || !box.content.image.isNull());
    if (hasBody) {
        y += NodeMetrics::PADDING;
        const double inner = width - 2.0 * NodeMetrics::PADDING;
        if (!box.content.image.isNull()) {
            box.image = QRectF(place.x() + NodeMetrics::PADDING, y, inner, NodeMetrics::IMAGE);
            y += NodeMetrics::IMAGE + NodeMetrics::PADDING;
        }
        const double lineHeight = QFontMetricsF(m_textFont).height() + LINE_GAP;
        for (const NodeRow& row : std::as_const(box.content.rows)) {
            RowBox drawn;
            drawn.row = row;
            double height = NodeMetrics::ROW;
            if (!row.markers.isEmpty()) {
                height = NodeMetrics::MARKERS;
            } else if (row.wide) {
                drawn.lines = wrap(row.value, m_textFont, inner, NodeMetrics::WIDE_LINES);
                height = std::max(NodeMetrics::ROW,
                                  static_cast<double>(drawn.lines.size()) * lineHeight +
                                      2.0 * WIDE_ROOM);
            }
            drawn.rect = QRectF(place.x(), y, width, height);
            y += height;
            box.rows.append(drawn);
        }
        y += NodeMetrics::PADDING;
    }
    box.rect = QRectF(place, QPointF(place.x() + width, y));
    box.branch = branchColorOf(m_map, m_look, node.id);

    // The sockets: the branch at the header, the relations at their rows
    const double headerY = place.y() + NodeMetrics::HEADER / 2.0;
    if (!box.root) {
        box.sockets.append(SocketBox{node.id, QString(), SocketKind::Branch, false,
                                     QPointF(place.x(), headerY), QString(), QString(), QString()});
    }
    box.sockets.append(SocketBox{node.id, QString(), SocketKind::Branch, true,
                                 QPointF(place.x() + width, headerY), QString(), QString(),
                                 QString()});
    for (const RowBox& row : std::as_const(box.rows)) {
        if (!row.row.socket) {
            continue;
        }
        const double rowY = row.rect.center().y();
        for (const bool right : {false, true}) {
            if ((right && row.row.right) || (!right && row.row.left)) {
                box.sockets.append(SocketBox{node.id, row.row.id, *row.row.socket, right,
                                             QPointF(right ? place.x() + width : place.x(), rowY),
                                             row.row.otherKind, row.row.group,
                                             box.content.cardKind});
            }
        }
    }
    return box;
}

void MindMapScene::placeNodes() {
    const bool root = m_map.showsRoot();
    for (const QString& id : m_map.nodeIds()) {
        const MindMapNode* node = m_map.node(id);
        if (node == nullptr || (!root && id == m_map.root())) {
            continue;
        }
        m_index.insert(id, m_nodes.size());
        m_nodes.append(measure(*node));
    }
}

// =============================================================================
// The links
// =============================================================================

QColor MindMapScene::branchColorOf(const MindMap& map, const MindMapLook& look, const QString& id) {
    const QColor plain = look.socketColor(SocketKind::Branch);
    if (look.colors == BranchColors::None || id == map.root()) {
        return plain;
    }
    if (look.colors == BranchColors::One) {
        return look.palette.front();
    }
    QString at = id;
    while (!at.isEmpty() && at != map.root()) {
        const MindMapNode* node = map.node(at);
        if (node == nullptr) {
            break;
        }
        if (const QColor own = look.paletteColor(node->color); own.isValid()) {
            return own;
        }
        const QString parent = map.parentOf(at);
        if (parent.isEmpty()) {
            return plain;  // the top of a loose branch
        }
        if (parent == map.root()) {
            const qsizetype index = map.node(parent)->children.indexOf(at);
            return look.palette.at(static_cast<std::size_t>(index) % look.palette.size());
        }
        at = parent;
    }
    return plain;
}

QPainterPath MindMapScene::pathBetween(const QPointF& start, bool startRight, const QPointF& end,
                                       bool endRight, LineStyle style) {
    QPainterPath path(start);
    const double out = startRight ? 1.0 : -1.0;
    const double in = endRight ? 1.0 : -1.0;
    if (style == LineStyle::Straight) {
        path.lineTo(end);
        return path;
    }
    if (style == LineStyle::Elbow) {
        double middle = (start.x() + end.x()) / 2.0;
        if (startRight == endRight) {
            middle = startRight ? std::max(start.x(), end.x()) + ELBOW
                                : std::min(start.x(), end.x()) - ELBOW;
        } else if ((end.x() - start.x()) * out < LEAST_ELBOW) {
            middle = start.x() + out * LEAST_ELBOW;
        }
        path.lineTo(middle, start.y());
        path.lineTo(middle, end.y());
        path.lineTo(end);
        return path;
    }
    const double bend = std::max(LEAST_BEND, std::abs(end.x() - start.x()) * BEND);
    path.cubicTo(start + QPointF(out * bend, 0.0), end + QPointF(in * bend, 0.0), end);
    return path;
}

QPointF MindMapScene::socketPoint(const NodeBox& box, const QString& row, bool right) const {
    const double x = right ? box.rect.right() : box.rect.left();
    if (!row.isEmpty()) {
        if (const RowBox* shown = box.row(row)) {
            return QPointF(x, shown->rect.center().y());
        }
    }
    return QPointF(x, box.header.center().y());
}

std::optional<SocketKind> MindMapScene::joinable(const SocketBox& a, const SocketBox& b) {
    if (a.node == b.node) {
        return std::nullopt;
    }
    if (a.kind == SocketKind::Branch || b.kind == SocketKind::Branch) {
        if (a.kind == SocketKind::Branch && b.kind == SocketKind::Branch && a.right != b.right) {
            return SocketKind::Branch;
        }
        return std::nullopt;
    }
    // The relations of two cards: the row of each names the kind of the other
    if (a.kind != b.kind || a.group != b.group || a.otherKind != b.cardKind ||
        b.otherKind != a.cardKind) {
        return std::nullopt;
    }
    return a.kind;
}

void MindMapScene::addLinks() {
    m_links.clear();
    const auto branchSides = [](const NodeBox& from, const NodeBox& to) {
        // The output on the right goes to the input on the left; a node on the left of the one
        // it leaves is joined the other way round, as a mind map draws its left side
        const bool mirrored = to.rect.right() < from.rect.left();
        return std::pair<bool, bool>{!mirrored, mirrored};
    };
    // The branches
    for (const NodeBox& child : std::as_const(m_nodes)) {
        const QString parent = m_map.parentOf(child.id);
        const NodeBox* from = parent.isEmpty() ? nullptr : box(parent);
        if (from == nullptr) {
            continue;
        }
        LinkLine link;
        link.type = LinkLine::Type::Branch;
        link.from = parent;
        link.to = child.id;
        link.color = child.branch;
        const auto [startRight, endRight] = branchSides(*from, child);
        link.startRight = startRight;
        link.endRight = endRight;
        link.start = socketPoint(*from, QString(), startRight);
        link.end = socketPoint(child, QString(), endRight);
        link.path = pathBetween(link.start, startRight, link.end, endRight, m_look.lines);
        m_links.append(link);
    }
    // The connections
    const QList<MindMapConnection>& connections = m_map.connections();
    for (qsizetype i = 0; i < connections.size(); ++i) {
        const NodeBox* from = box(connections.at(i).from);
        const NodeBox* to = box(connections.at(i).to);
        if (from == nullptr || to == nullptr) {
            continue;
        }
        LinkLine link;
        link.type = LinkLine::Type::Connection;
        link.from = from->id;
        link.to = to->id;
        link.index = i;
        link.color = m_look.connection;
        const auto [startRight, endRight] = branchSides(*from, *to);
        link.startRight = startRight;
        link.endRight = endRight;
        link.start = socketPoint(*from, QString(), startRight);
        link.end = socketPoint(*to, QString(), endRight);
        link.path = pathBetween(link.start, startRight, link.end, endRight, m_look.lines);
        link.label = connections.at(i).label;
        m_links.append(link);
    }
    // The relations of the cards
    for (qsizetype i = 0; i < m_relations.size(); ++i) {
        const RelationLink& relation = m_relations.at(i);
        const NodeBox* from = box(relation.from);
        const NodeBox* to = box(relation.to);
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const RowBox* fromRow = from->folded ? nullptr : from->row(relation.fromRow);
        const RowBox* toRow = to->folded ? nullptr : to->row(relation.toRow);
        // The sides of the sockets: those of the rows; a row with both, or none shown, faces
        // the other node
        const auto sideOf = [](const RowBox* row, const NodeBox& own, const NodeBox& other) {
            if (row != nullptr && row->row.left != row->row.right) {
                return row->row.right;
            }
            return facesRight(own.rect, other.rect.center());
        };
        bool startRight = sideOf(fromRow, *from, *to);
        bool endRight = sideOf(toRow, *to, *from);
        const bool bothBoth = fromRow != nullptr && toRow != nullptr && fromRow->row.left &&
                              fromRow->row.right && toRow->row.left && toRow->row.right;
        if (bothBoth) {
            const double across = to->rect.center().x() - from->rect.center().x();
            if (std::abs(across) < SIDE_BY_SIDE) {
                startRight = true;
                endRight = true;
            } else {
                startRight = across > 0;
                endRight = !startRight;
            }
        }
        LinkLine link;
        link.type = LinkLine::Type::Relation;
        link.from = from->id;
        link.to = to->id;
        link.index = i;
        link.kind = relation.kind;
        link.color = m_look.socketColor(relation.kind);
        link.startRight = startRight;
        link.endRight = endRight;
        link.start = socketPoint(*from, fromRow != nullptr ? relation.fromRow : QString(),
                                 startRight);
        link.end = socketPoint(*to, toRow != nullptr ? relation.toRow : QString(), endRight);
        link.path = pathBetween(link.start, startRight, link.end, endRight, m_look.lines);
        link.label = relation.label;
        m_links.append(link);
    }
    placeLabels();
}

void MindMapScene::placeLabels() {
    // The middle of each line, else the first place along it where the label covers neither
    // another label nor a node
    static constexpr std::array<double, 9> ALONG{0.5, 0.4, 0.6, 0.3, 0.7, 0.22, 0.78, 0.14, 0.86};
    QList<QRectF> taken;
    for (qsizetype i = 0; i < m_links.size(); ++i) {
        LinkLine& link = m_links[i];
        link.labelCentre = link.path.pointAtPercent(ALONG.front());
        if (link.label.isEmpty()) {
            continue;
        }
        for (const double along : ALONG) {
            link.labelCentre = link.path.pointAtPercent(along);
            const QRectF rect = labelRect(i);
            const bool free =
                std::none_of(taken.cbegin(), taken.cend(),
                             [&rect](const QRectF& other) { return other.intersects(rect); }) &&
                std::none_of(m_nodes.cbegin(), m_nodes.cend(),
                             [&rect](const NodeBox& node) { return node.rect.intersects(rect); });
            if (free) {
                break;
            }
            link.labelCentre = link.path.pointAtPercent(ALONG.front());
        }
        taken.append(labelRect(i));
    }
}

QRectF MindMapScene::labelRect(qsizetype index) const {
    if (index < 0 || index >= m_links.size() || m_links.at(index).label.isEmpty()) {
        return QRectF();
    }
    const LinkLine& link = m_links.at(index);
    const QFontMetricsF metrics(m_labelFont);
    const double width = metrics.horizontalAdvance(link.label) + 2.0 * LABEL_ROOM_X;
    const double height = metrics.height() + 2.0 * LABEL_ROOM_Y;
    return QRectF(link.labelCentre.x() - width / 2.0, link.labelCentre.y() - height / 2.0, width,
                  height);
}

qsizetype MindMapScene::linkOfConnection(qsizetype index) const {
    for (qsizetype i = 0; i < m_links.size(); ++i) {
        if (m_links.at(i).type == LinkLine::Type::Connection && m_links.at(i).index == index) {
            return i;
        }
    }
    return -1;
}

// =============================================================================
// The frames
// =============================================================================

void MindMapScene::addFrames() {
    m_frames.clear();
    for (const MindMapFrame& frame : m_map.frames()) {
        QRectF around;
        for (const QString& id : frame.nodes) {
            if (const NodeBox* node = box(id)) {
                around = around.isNull() ? node->rect : around.united(node->rect);
            }
        }
        if (around.isNull()) {
            continue;
        }
        FrameBox drawn;
        drawn.id = frame.id;
        drawn.text = frame.title;
        drawn.rect = around.adjusted(-NodeMetrics::FRAME_SIDE, -NodeMetrics::FRAME_TOP,
                                     NodeMetrics::FRAME_SIDE, NodeMetrics::FRAME_SIDE);
        drawn.title = QRectF(drawn.rect.left(), drawn.rect.top() + FRAME_TITLE_ROOM,
                             drawn.rect.width(), FRAME_TITLE);
        const QColor own = m_look.paletteColor(frame.color);
        drawn.color = own.isValid() ? own : m_look.frameLine;
        m_frames.append(drawn);
    }
}

void MindMapScene::updateBounds() {
    m_bounds = QRectF();
    const auto add = [this](const QRectF& rect) {
        if (!rect.isNull()) {
            m_bounds = m_bounds.isNull() ? rect : m_bounds.united(rect);
        }
    };
    for (const NodeBox& node : std::as_const(m_nodes)) {
        add(node.rect.adjusted(-NodeMetrics::SOCKET, 0.0, NodeMetrics::SOCKET, 0.0));
    }
    for (const FrameBox& frame : std::as_const(m_frames)) {
        add(frame.rect);
    }
    for (qsizetype i = 0; i < m_links.size(); ++i) {
        add(m_links.at(i).path.boundingRect());
        add(labelRect(i));
    }
}

void MindMapScene::translate(NodeBox& node, const QPointF& delta) {
    node.rect.translate(delta);
    node.header.translate(delta);
    node.fold.translate(delta);
    node.title.translate(delta);
    node.icon.translate(delta);
    if (!node.image.isNull()) {
        node.image.translate(delta);
    }
    for (RowBox& row : node.rows) {
        row.rect.translate(delta);
    }
    for (SocketBox& socket : node.sockets) {
        socket.centre += delta;
    }
}

void MindMapScene::moveNodes(const QStringList& ids, const QPointF& delta) {
    QHash<QString, QPointF> deltas;
    for (const QString& id : ids) {
        deltas.insert(id, delta);
    }
    moveNodes(deltas);
}

void MindMapScene::moveNodes(const QHash<QString, QPointF>& deltas) {
    for (auto it = deltas.cbegin(); it != deltas.cend(); ++it) {
        const auto found = m_index.constFind(it.key());
        if (found != m_index.cend()) {
            translate(m_nodes[*found], it.value());
        }
    }
    addLinks();
    addFrames();
    updateBounds();
}

// =============================================================================
// What the mouse is over
// =============================================================================

QString MindMapScene::nodeAt(const QPointF& point) const {
    for (auto it = m_nodes.crbegin(); it != m_nodes.crend(); ++it) {
        if (it->rect.contains(point)) {
            return it->id;
        }
    }
    return QString();
}

std::optional<SocketBox> MindMapScene::socketAt(const QPointF& point, double reach) const {
    std::optional<SocketBox> nearest;
    double best = reach * reach;
    for (const NodeBox& node : m_nodes) {
        if (!node.rect.adjusted(-reach, -reach, reach, reach).contains(point)) {
            continue;
        }
        for (const SocketBox& socket : node.sockets) {
            const QPointF way = socket.centre - point;
            const double distance = way.x() * way.x() + way.y() * way.y();
            if (distance <= best) {
                best = distance;
                nearest = socket;
            }
        }
    }
    return nearest;
}

qsizetype MindMapScene::linkAt(const QPointF& point, double reach) const {
    for (qsizetype i = m_links.size() - 1; i >= 0; --i) {
        if (labelRect(i).contains(point)) {
            return i;
        }
    }
    QPainterPathStroker stroker;
    stroker.setWidth(2.0 * reach);
    for (qsizetype i = m_links.size() - 1; i >= 0; --i) {
        const QPainterPath& path = m_links.at(i).path;
        if (!path.boundingRect().adjusted(-reach, -reach, reach, reach).contains(point)) {
            continue;
        }
        if (stroker.createStroke(path).contains(point)) {
            return i;
        }
    }
    return -1;
}

QString MindMapScene::frameAt(const QPointF& point) const {
    for (auto it = m_frames.crbegin(); it != m_frames.crend(); ++it) {
        if (it->title.contains(point)) {
            return it->id;
        }
    }
    return QString();
}

QString MindMapScene::nodeToward(const QString& id, int dx, int dy) const {
    const NodeBox* from = box(id);
    if (from == nullptr) {
        return QString();
    }
    const QPointF centre = from->rect.center();
    QString best;
    double bestScore = std::numeric_limits<double>::max();
    for (const NodeBox& other : m_nodes) {
        if (other.id == id) {
            continue;
        }
        const QPointF way = other.rect.center() - centre;
        const double along = dx != 0 ? way.x() * dx : way.y() * dy;
        if (along <= 1.0) {
            continue;
        }
        const double across = dx != 0 ? std::abs(way.y()) : std::abs(way.x());
        const double score = along + 2.0 * across;
        if (score < bestScore) {
            bestScore = score;
            best = other.id;
        }
    }
    return best;
}

}  // namespace kalahari::mindmap
