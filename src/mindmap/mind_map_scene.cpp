// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_scene.cpp
/// @brief The drawing of a mind map: where each node, line, badge and connection is

#include <kalahari/mindmap/mind_map_scene.h>

#include <QFontMetricsF>
#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace kalahari::mindmap {

namespace {

/// The text of the nodes, in pixels of a map at 100%: the main topic, a main branch, the rest
constexpr double ROOT_PIXELS = 15.0;
constexpr double MAIN_PIXELS = 13.5;
constexpr double OTHER_PIXELS = 12.5;
constexpr double ROOT_LINE = 20.0;
constexpr double MAIN_LINE = 18.0;
constexpr double OTHER_LINE = 17.0;
constexpr double LABEL_PIXELS = 11.5;
/// Points per pixel: a map at 100% has the sizes of a screen of 96 dpi
constexpr double POINTS_PER_PIXEL = 0.75;

/// The widest text before it wraps: the main topic, the others; the tree going down is narrower
constexpr double ROOT_WIDTH = 200.0;
constexpr double OTHER_WIDTH = 168.0;
constexpr double DOWN_ROOT_WIDTH = 150.0;
constexpr double DOWN_OTHER_WIDTH = 118.0;

constexpr double MARKER_SPACE = 19.0;
constexpr double MARKER_SIZE = 15.0;
constexpr double ICON_SPACE = 18.0;
constexpr double ICON_SIZE = 14.0;
constexpr double SLACK = 3.0;

constexpr double ROOT_PAD_X = 14.0;
constexpr double ROOT_PAD_Y = 8.0;
constexpr double PAD_X = 8.0;
constexpr double PAD_Y = 5.0;
constexpr double OVAL_PAD_X = 12.0;
constexpr double LINE_PAD_X = 4.0;
constexpr double LINE_PAD_Y = 2.5;
constexpr double ROOT_BORDER = 2.0;
constexpr double BORDER = 1.5;

constexpr double BADGE_GAP = 12.0;
constexpr double BADGE_SIZE = 18.0;

/// The lines of the branches: thinner deeper in them
constexpr double MAIN_WIDTH = 3.2;
constexpr double SECOND_WIDTH = 2.1;
constexpr double DEEP_WIDTH = 1.5;
constexpr double EVEN_WIDTH = 2.0;

/// A connection between nodes on one side bulges outward around the nodes between them
constexpr double BULGE = 70.0;
constexpr double MOST_BULGE = 80.0;
constexpr double BULGE_PART = 0.25;
constexpr double MOST_BEND = 70.0;
constexpr double BEND_PART = 0.28;
constexpr double CLIP_GAP = 4.0;
constexpr double ARROW_GAP = 3.0;
constexpr double STEEP = 1.1;
constexpr double STACKED = 0.5;
constexpr double HIT_WIDTH = 8.0;
constexpr double LABEL_PAD_X = 6.0;
constexpr double LABEL_PAD_Y = 2.0;

double sign(double value) {
    return value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0);
}

/// The words of @p text in lines no wider than @p width; a word wider than it has its own line
QStringList wrapped(const QString& text, const QFontMetricsF& metrics, double width) {
    QStringList lines;
    for (const QString& paragraph : text.split(QLatin1Char('\n'))) {
        QString line;
        for (const QString& word : paragraph.split(QLatin1Char(' '))) {
            const QString longer = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
            if (line.isEmpty() || metrics.horizontalAdvance(longer) <= width) {
                line = longer;
            } else {
                lines.append(line);
                line = word;
            }
        }
        lines.append(line);
    }
    return lines;
}

/// The point where the line from the centre of a box toward @p target leaves the box
QPointF leaving(const QRectF& box, const QPointF& target) {
    const QPointF centre = box.center();
    const double dx = target.x() - centre.x();
    const double dy = target.y() - centre.y();
    if (dx == 0.0 && dy == 0.0) {
        return centre;
    }
    const double sx = dx != 0.0 ? (box.width() / 2.0 + CLIP_GAP) / std::abs(dx)
                                : std::numeric_limits<double>::max();
    const double sy = dy != 0.0 ? (box.height() / 2.0 + CLIP_GAP) / std::abs(dy)
                                : std::numeric_limits<double>::max();
    const double scale = std::min(sx, sy);
    return {centre.x() + dx * scale, centre.y() + dy * scale};
}

}  // namespace

// =============================================================================
// NodeBox
// =============================================================================

QRectF NodeBox::markerRect(qsizetype index) const {
    const double left = rect.left() + border + paddingX + MARKER_SPACE * static_cast<double>(index);
    return {left, rect.center().y() - MARKER_SIZE / 2.0, MARKER_SIZE, MARKER_SIZE};
}

QRectF NodeBox::textRect() const {
    const double left =
        rect.left() + border + paddingX + MARKER_SPACE * static_cast<double>(markers.size());
    const double right =
        rect.right() - border - paddingX - (link ? ICON_SPACE : 0.0) - (note ? ICON_SPACE : 0.0);
    const double height = lineHeight * static_cast<double>(lines.size());
    return {left, rect.center().y() - height / 2.0, std::max(0.0, right - left), height};
}

QRectF NodeBox::linkRect() const {
    if (!link) {
        return {};
    }
    const double right = rect.right() - border - paddingX - (note ? ICON_SPACE : 0.0);
    return {right - ICON_SIZE, rect.center().y() - ICON_SIZE / 2.0, ICON_SIZE, ICON_SIZE};
}

QRectF NodeBox::noteRect() const {
    if (!note) {
        return {};
    }
    const double right = rect.right() - border - paddingX;
    return {right - ICON_SIZE, rect.center().y() - ICON_SIZE / 2.0, ICON_SIZE, ICON_SIZE};
}

// =============================================================================
// MindMapScene
// =============================================================================

MindMapScene::MindMapScene(const MindMap& map, const MindMapLook& look, const QFont& font,
                           const ShownText& shownText)
    : m_map(map), m_look(look), m_font(font), m_labelFont(font), m_shownText(shownText) {
    m_labelFont.setPointSizeF(LABEL_PIXELS * POINTS_PER_PIXEL * look.textScale);
    m_labelFont.setItalic(true);
    placeNodes();
    addLines();
    addConnections();
}

QSizeF MindMapScene::measure(const MindMapNode& node, int depth) {
    const bool root = node.id == m_map.root();
    const bool main = !root && depth <= 1;
    const bool looseTop = m_map.looseNodes().contains(node.id);
    const double scale = m_look.textScale;

    NodeBox box;
    box.id = node.id;
    box.root = root;
    box.font = m_font;
    box.font.setPointSizeF((root ? ROOT_PIXELS : main ? MAIN_PIXELS : OTHER_PIXELS) *
                           POINTS_PER_PIXEL * scale);
    box.font.setWeight(root ? QFont::Bold : main ? QFont::DemiBold : QFont::Normal);
    box.lineHeight = (root ? ROOT_LINE : main ? MAIN_LINE : OTHER_LINE) * scale;
    const bool down = m_map.layout() == MindMapLayout::Down;
    const double widest = down ? (root ? DOWN_ROOT_WIDTH : DOWN_OTHER_WIDTH)
                               : (root ? ROOT_WIDTH : OTHER_WIDTH);
    QString text = m_shownText ? m_shownText(node) : node.text;
    if (text.isEmpty()) {
        text = QStringLiteral(" ");
    }
    const QFontMetricsF metrics(box.font);
    box.lines = wrapped(text, metrics, widest);
    double textWidth = 0.0;
    for (const QString& line : std::as_const(box.lines)) {
        textWidth = std::max(textWidth, metrics.horizontalAdvance(line));
    }
    box.markers = node.markers;
    box.link = !node.link.isEmpty();
    box.note = !node.note.isEmpty();
    const double icons = MARKER_SPACE * static_cast<double>(node.markers.size()) +
                         (box.link ? ICON_SPACE : 0.0) + (box.note ? ICON_SPACE : 0.0);
    const bool underline = m_look.shape == NodeShape::Underline && !root && !looseTop;
    box.paddingX = root ? ROOT_PAD_X
                   : underline ? LINE_PAD_X
                   : m_look.shape == NodeShape::Oval ? OVAL_PAD_X
                                                     : PAD_X;
    box.paddingY = root ? ROOT_PAD_Y : underline ? LINE_PAD_Y : PAD_Y;
    box.border = root ? ROOT_BORDER : BORDER;
    const QSizeF size(std::ceil(textWidth + icons + 2.0 * box.paddingX + 2.0 * box.border + SLACK),
                      std::ceil(static_cast<double>(box.lines.size()) * box.lineHeight +
                                2.0 * box.paddingY + 2.0 * box.border));
    m_measured.insert(node.id, box);
    return size;
}

void MindMapScene::placeNodes() {
    m_places = layOutMindMap(m_map, m_map.layout(), [this](const MindMapNode& node, int depth) {
        return measure(node, depth);
    });
    for (const QString& id : m_map.nodeIds()) {
        const auto place = m_places.constFind(id);
        const MindMapNode* node = m_map.node(id);
        if (place == m_places.cend() || node == nullptr) {
            continue;
        }
        NodeBox box = m_measured.value(id);
        box.rect = QRectF(place->centre.x() - place->size.width() / 2.0,
                          place->centre.y() - place->size.height() / 2.0, place->size.width(),
                          place->size.height());
        box.depth = place->depth;
        box.side = place->side;
        box.loose = place->loose;
        box.looseTop = m_map.looseNodes().contains(id);
        box.branch = branchColor(id);
        if (node->collapsed && !node->children.isEmpty()) {
            box.hidden = static_cast<int>(m_map.descendantsOf(id).size());
            const bool down = m_map.layout() == MindMapLayout::Down;
            const double side = box.side != 0 ? box.side : 1.0;
            box.badge = down ? QPointF(box.rect.center().x(), box.rect.bottom() + BADGE_GAP)
                             : QPointF(box.rect.center().x() +
                                           side * (box.rect.width() / 2.0 + BADGE_GAP),
                                       box.rect.center().y());
        }
        m_index.insert(id, m_nodes.size());
        m_nodes.append(box);
        m_bounds = m_bounds.united(box.rect);
        if (box.hidden > 0) {
            m_bounds = m_bounds.united(QRectF(box.badge.x() - BADGE_SIZE / 2.0,
                                              box.badge.y() - BADGE_SIZE / 2.0, BADGE_SIZE,
                                              BADGE_SIZE));
        }
    }
}

QColor MindMapScene::branchColor(const QString& id) const {
    return branchColorOf(m_map, m_look, id);
}

QColor MindMapScene::branchColorOf(const MindMap& map, const MindMapLook& look,
                                   const QString& id) {
    if (id == map.root()) {
        return look.root;
    }
    if (look.colors == BranchColors::None) {
        return look.plain;
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
            return look.mutedText;  // the top of a loose branch
        }
        if (parent == map.root()) {
            const qsizetype index = map.node(parent)->children.indexOf(at);
            return look.palette.at(static_cast<std::size_t>(index) % look.palette.size());
        }
        at = parent;
    }
    return look.mutedText;
}

QPainterPath MindMapScene::linePath(const NodeBox& parent, const NodeBox& child) const {
    const MindMapLayout layout = m_map.layout();
    QPointF from;
    QPointF to;
    bool vertical = false;
    if (layout == MindMapLayout::Down) {
        from = QPointF(parent.rect.center().x(), parent.rect.bottom());
        to = QPointF(child.rect.center().x(), child.rect.top());
        vertical = true;
    } else {
        double side = child.side != 0 ? child.side : 1.0;
        const double dx = child.rect.center().x() - parent.rect.center().x();
        const double dy = child.rect.center().y() - parent.rect.center().y();
        if (layout == MindMapLayout::Free && std::abs(dy) > std::abs(dx) * STEEP) {
            const double way = sign(dy);
            from = QPointF(parent.rect.center().x(),
                           parent.rect.center().y() + way * parent.rect.height() / 2.0);
            to = QPointF(child.rect.center().x(),
                         child.rect.center().y() - way * child.rect.height() / 2.0);
            vertical = true;
        } else {
            if (layout == MindMapLayout::Free) {
                side = dx >= 0.0 ? 1.0 : -1.0;
            }
            const bool underline = m_look.shape == NodeShape::Underline;
            const double fromY = underline && !parent.root && !parent.looseTop
                                     ? parent.rect.bottom() - 1.0
                                     : parent.rect.center().y();
            const double toY = underline && !child.looseTop ? child.rect.bottom() - 1.0
                                                            : child.rect.center().y();
            from = QPointF(parent.rect.center().x() + side * parent.rect.width() / 2.0, fromY);
            to = QPointF(child.rect.center().x() - side * child.rect.width() / 2.0, toY);
        }
    }

    QPainterPath path(from);
    if (m_look.lines == LineStyle::Straight) {
        path.lineTo(to);
    } else if (m_look.lines == LineStyle::Elbow) {
        if (vertical) {
            const double middle = (from.y() + to.y()) / 2.0;
            path.lineTo(from.x(), middle);
            path.lineTo(to.x(), middle);
        } else {
            const double middle = (from.x() + to.x()) / 2.0;
            path.lineTo(middle, from.y());
            path.lineTo(middle, to.y());
        }
        path.lineTo(to);
    } else if (vertical) {
        const double half = (to.y() - from.y()) / 2.0;
        path.cubicTo(from.x(), from.y() + half, to.x(), to.y() - half, to.x(), to.y());
    } else {
        const double half = (to.x() - from.x()) / 2.0;
        path.cubicTo(from.x() + half, from.y(), to.x() - half, to.y(), to.x(), to.y());
    }
    return path;
}

void MindMapScene::addLines() {
    for (const NodeBox& child : std::as_const(m_nodes)) {
        const NodeBox* parent = box(m_map.parentOf(child.id));
        if (parent == nullptr) {
            continue;
        }
        BranchLine line;
        line.child = child.id;
        line.path = linePath(*parent, child);
        line.color = child.branch;
        line.width = !m_look.thinner     ? EVEN_WIDTH
                     : child.depth <= 1  ? MAIN_WIDTH
                     : child.depth == 2  ? SECOND_WIDTH
                                         : DEEP_WIDTH;
        m_lines.append(line);
    }
}

void MindMapScene::addConnections() {
    const NodeBox* root = box(m_map.root());
    const QList<MindMapConnection>& connections = m_map.connections();
    for (qsizetype index = 0; index < connections.size(); ++index) {
        const NodeBox* from = box(connections.at(index).from);
        const NodeBox* to = box(connections.at(index).to);
        if (from == nullptr || to == nullptr) {
            continue;
        }
        ConnectionLine line;
        line.index = index;
        const QPointF a = from->rect.center();
        const QPointF b = to->rect.center();
        const double sideA = root != nullptr ? sign(a.x() - root->rect.center().x()) : 0.0;
        const double sideB = root != nullptr ? sign(b.x() - root->rect.center().x()) : 0.0;
        if (m_map.layout() != MindMapLayout::Down && sideA != 0.0 && sideA == sideB &&
            std::abs(a.y() - b.y()) > std::abs(a.x() - b.x()) * STACKED) {
            // Two nodes on one side: out of their outer edges, around the nodes between them
            const double side = sideA;
            const double top = std::min(a.y(), b.y());
            const double bottom = std::max(a.y(), b.y());
            const auto edge = [side](const NodeBox& node) {
                return node.rect.center().x() + side * node.rect.width() / 2.0;
            };
            double outer = side > 0.0 ? std::max(edge(*from), edge(*to))
                                      : std::min(edge(*from), edge(*to));
            for (const NodeBox& other : std::as_const(m_nodes)) {
                const double y = other.rect.center().y();
                if (y > top && y < bottom &&
                    sign(other.rect.center().x() - root->rect.center().x()) == side) {
                    outer = side > 0.0 ? std::max(outer, edge(other)) : std::min(outer, edge(other));
                }
            }
            const QPointF start(edge(*from), a.y());
            const QPointF end(edge(*to) + side * ARROW_GAP, b.y());
            const double bend = outer + side * (BULGE + std::min(MOST_BULGE, (bottom - top) *
                                                                                  BULGE_PART));
            line.path = QPainterPath(start);
            line.path.cubicTo(bend, start.y(), bend, end.y(), end.x(), end.y());
            line.labelCentre = QPointF((start.x() + 6.0 * bend + end.x()) / 8.0,
                                       (start.y() + end.y()) / 2.0);
        } else {
            const QPointF start = leaving(from->rect, b);
            const QPointF end = leaving(to->rect, a);
            const QPointF middle = (start + end) / 2.0;
            const double length = std::max(1.0, std::hypot(end.x() - start.x(),
                                                           end.y() - start.y()));
            const double bend = std::min(MOST_BEND, length * BEND_PART);
            const QPointF control(middle.x() - (end.y() - start.y()) / length * bend,
                                  middle.y() + (end.x() - start.x()) / length * bend);
            line.path = QPainterPath(start);
            line.path.quadTo(control, end);
            line.labelCentre = start * 0.25 + control * 0.5 + end * 0.25;
        }
        line.tip = line.path.currentPosition();
        line.angle = line.path.angleAtPercent(1.0);
        m_connections.append(line);
        m_bounds = m_bounds.united(line.path.boundingRect());
        m_bounds = m_bounds.united(labelRect(index, connections.at(index).label));
    }
}

QRectF MindMapScene::labelRect(qsizetype index, const QString& label) const {
    const auto line = std::find_if(m_connections.cbegin(), m_connections.cend(),
                                   [index](const ConnectionLine& each) {
                                       return each.index == index;
                                   });
    if (line == m_connections.cend() || label.isEmpty()) {
        return {};
    }
    const QFontMetricsF metrics(m_labelFont);
    const double width = metrics.horizontalAdvance(label) + 2.0 * LABEL_PAD_X;
    const double height = metrics.height() + 2.0 * LABEL_PAD_Y;
    return {line->labelCentre.x() - width / 2.0, line->labelCentre.y() - height / 2.0, width,
            height};
}

const NodeBox* MindMapScene::box(const QString& id) const {
    const auto found = m_index.constFind(id);
    return found != m_index.cend() ? &m_nodes.at(*found) : nullptr;
}

QString MindMapScene::nodeAt(const QPointF& point) const {
    for (auto it = m_nodes.crbegin(); it != m_nodes.crend(); ++it) {
        if (it->rect.contains(point)) {
            return it->id;
        }
    }
    return QString();
}

QString MindMapScene::badgeAt(const QPointF& point) const {
    for (const NodeBox& node : m_nodes) {
        if (node.hidden > 0 && std::hypot(point.x() - node.badge.x(),
                                          point.y() - node.badge.y()) <= BADGE_SIZE / 2.0) {
            return node.id;
        }
    }
    return QString();
}

qsizetype MindMapScene::connectionAt(const QPointF& point) const {
    QPainterPathStroker stroker;
    stroker.setWidth(HIT_WIDTH);
    const QList<MindMapConnection>& connections = m_map.connections();
    for (const ConnectionLine& line : m_connections) {
        if (labelRect(line.index, connections.at(line.index).label).contains(point) ||
            stroker.createStroke(line.path).contains(point)) {
            return line.index;
        }
    }
    return -1;
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
