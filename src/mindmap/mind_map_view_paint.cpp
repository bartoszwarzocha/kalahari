// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_view_paint.cpp
/// @brief The painting of the map of a mind map: its background, lines, connections and nodes

#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_menus.h>
#include <kalahari/mindmap/mind_map_view.h>

#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace kalahari::mindmap {

namespace {

/// The step of the dots and of the grid, in the map's coordinates
constexpr double PATTERN_STEP = 20.0;
/// The dots and the grid are at least this far apart on the screen
constexpr double LEAST_PATTERN_STEP = 10.0;
constexpr double DOT_SIZE = 1.6;

constexpr double ROOT_RADIUS = 10.0;
constexpr double RADIUS = 7.0;
constexpr double RECTANGLE_RADIUS = 2.0;
constexpr double ROOT_RECTANGLE_RADIUS = 3.0;
constexpr double MAIN_FILL = 0.2;      ///< The color of its branch in a main branch's fill
constexpr double DEEP_FRAME = 0.55;    ///< The color of its branch in a deeper node's frame
constexpr double LOOSE_FILL = 0.85;    ///< How much of the node's color a loose node fills with
constexpr double UNDERLINE = 2.0;
constexpr double MAIN_UNDERLINE = 3.0;

constexpr double SELECTION = 2.0;      ///< The ring of the selected node
constexpr double SOFT_SELECTION = 2.0; ///< The soft ring around it
constexpr double SOFT_OPACITY = 0.3;
constexpr double OUTLINE_GAP = 3.0;    ///< The outlines of a target and of a connected node
constexpr double OUTLINE = 2.0;
constexpr double DROP_BAR = 4.0;       ///< The bar before or after the node a node goes to

constexpr double DIMMED = 0.28;        ///< A node that does not match the highlight
constexpr double DIMMED_LINE = 0.3;
constexpr double DRAGGED = 0.55;       ///< The node being dragged

constexpr double CONNECTION_WIDTH = 1.4;
constexpr double DASH = 5.0;
constexpr double GAP = 4.0;
constexpr double ARROW_LENGTH = 9.8;
constexpr double ARROW_HALF = 4.9;
constexpr double LABEL_BORDER = 1.0;

constexpr double SIGN_PIXELS = 10.0;   ///< The sign of a marker
constexpr double BADGE_PIXELS = 10.5;  ///< The number on a badge
constexpr double POINTS_PER_PIXEL = 0.75;
constexpr double BADGE_SIZE = 18.0;
constexpr double BADGE_BORDER = 1.5;
constexpr double BADGE_PAD = 4.0;

/// @p a mixed with @p b: @p part of @p a
QColor mixed(const QColor& a, const QColor& b, double part) {
    const double rest = 1.0 - part;
    return QColor::fromRgbF(static_cast<float>(a.redF() * part + b.redF() * rest),
                            static_cast<float>(a.greenF() * part + b.greenF() * rest),
                            static_cast<float>(a.blueF() * part + b.blueF() * rest),
                            static_cast<float>(a.alphaF() * part + b.alphaF() * rest));
}

/// The radius of the corners of a node in look @p look
double radiusOf(const NodeBox& box, const MindMapLook& look) {
    switch (look.shape) {
    case NodeShape::Oval:
        return box.rect.height() / 2.0;
    case NodeShape::Rectangle:
        return box.root ? ROOT_RECTANGLE_RADIUS : RECTANGLE_RADIUS;
    case NodeShape::Rounded:
    case NodeShape::Underline:
        break;
    }
    return box.root ? ROOT_RADIUS : RADIUS;
}

/// Whether node @p box is drawn as its text over a line
bool underlined(const NodeBox& box, const MindMapLook& look) {
    return look.shape == NodeShape::Underline && !box.root && !box.looseTop;
}

}  // namespace

void MindMapView::paintEvent(QPaintEvent* event) {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setClipRect(event->rect());
    paintBackground(painter);
    if (!m_scene) {
        return;
    }
    const QPointF origin = toViewport(QPointF());
    painter.translate(origin);
    painter.scale(zoom(), zoom());
    paintLines(painter);
    paintConnections(painter);
    for (const NodeBox& box : m_scene->nodes()) {
        if (m_drag == Drag::Node && box.id == m_dragNode && m_drop.where != Drop::Where::None) {
            continue;  // It is drawn where it is dragged (paintDrag())
        }
        paintNode(painter, box);
        paintBadge(painter, box);
    }
    paintDrag(painter);
}

void MindMapView::paintBackground(QPainter& painter) const {
    const QRectF all(QPointF(), QSizeF(viewport()->size()));
    const QColor fill =
        m_look.background == MapBackground::Own ? m_look.ownBackground : m_look.mapBackground;
    painter.fillRect(all, fill);
    if (m_look.background != MapBackground::Dots && m_look.background != MapBackground::Grid) {
        return;
    }
    // The pattern goes with the map, so that the map moves over it
    double step = PATTERN_STEP;
    while (step * zoom() < LEAST_PATTERN_STEP) {
        step *= 2.0;
    }
    const QPointF from = toMap(all.topLeft());
    const QPointF to = toMap(all.bottomRight());
    const double left = std::floor(from.x() / step) * step;
    const double top = std::floor(from.y() / step) * step;
    painter.save();
    if (m_look.background == MapBackground::Dots) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_look.dots);
        const double size = DOT_SIZE * std::min(1.0, zoom());
        for (double y = top; y <= to.y(); y += step) {
            for (double x = left; x <= to.x(); x += step) {
                const QPointF at = toViewport(QPointF(x, y));
                painter.drawEllipse(at, size / 2.0, size / 2.0);
            }
        }
    } else {
        painter.setPen(QPen(m_look.dots, 1.0));
        for (double x = left; x <= to.x(); x += step) {
            const double at = toViewport(QPointF(x, 0.0)).x();
            painter.drawLine(QPointF(at, all.top()), QPointF(at, all.bottom()));
        }
        for (double y = top; y <= to.y(); y += step) {
            const double at = toViewport(QPointF(0.0, y)).y();
            painter.drawLine(QPointF(all.left(), at), QPointF(all.right(), at));
        }
    }
    painter.restore();
}

void MindMapView::paintLines(QPainter& painter) const {
    painter.save();
    painter.setBrush(Qt::NoBrush);
    for (const BranchLine& line : m_scene->lines()) {
        QPen pen(line.color, line.width);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setOpacity(matches(line.child) ? 1.0 : DIMMED_LINE);
        painter.drawPath(line.path);
    }
    painter.restore();
}

void MindMapView::paintConnections(QPainter& painter) const {
    const QList<MindMapConnection>& connections = m_document.map().connections();
    painter.save();
    for (const ConnectionLine& line : m_scene->connections()) {
        const MindMapConnection& connection = connections.at(line.index);
        const bool selected = line.index == m_document.selectedConnection();
        const bool shown = matches(connection.from) || matches(connection.to);
        painter.setOpacity(shown ? 1.0 : DIMMED_LINE);
        const QColor color = selected ? m_look.selection : m_look.connection;

        QPen pen(color, selected ? CONNECTION_WIDTH + 0.6 : CONNECTION_WIDTH);
        pen.setDashPattern({DASH / CONNECTION_WIDTH, GAP / CONNECTION_WIDTH});
        pen.setCapStyle(Qt::FlatCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(line.path);

        // The arrow at its end
        painter.save();
        painter.translate(line.tip);
        painter.rotate(-line.angle);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF({QPointF(0.0, 0.0), QPointF(-ARROW_LENGTH, -ARROW_HALF),
                                       QPointF(-ARROW_LENGTH, ARROW_HALF)}));
        painter.restore();

        if (connection.label.isEmpty()) {
            continue;
        }
        const QRectF label = m_scene->labelRect(line.index, connection.label);
        QPen frame(color, selected ? LABEL_BORDER * 2.0 : LABEL_BORDER);
        if (!selected) {
            frame.setStyle(Qt::DashLine);
        }
        painter.setPen(frame);
        painter.setBrush(m_look.background == MapBackground::Own ? m_look.ownBackground
                                                                 : m_look.mapBackground);
        painter.drawRoundedRect(label, label.height() / 2.0, label.height() / 2.0);
        painter.setFont(m_scene->labelFont());
        painter.setPen(m_look.text);
        painter.drawText(label, Qt::AlignCenter, connection.label);
    }
    painter.restore();
}

void MindMapView::paintNode(QPainter& painter, const NodeBox& box) const {
    painter.save();
    if (!matches(box.id)) {
        painter.setOpacity(painter.opacity() * DIMMED);
    }
    const double radius = radiusOf(box, m_look);
    const QRectF frame = box.rect.adjusted(box.border / 2.0, box.border / 2.0,
                                           -box.border / 2.0, -box.border / 2.0);
    if (box.root) {
        painter.setPen(QPen(m_look.root, box.border));
        painter.setBrush(m_look.root);
        painter.drawRoundedRect(frame, radius, radius);
    } else if (box.looseTop) {
        QPen pen(m_look.mutedText, box.border);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        QColor fill = m_look.node;
        fill.setAlphaF(static_cast<float>(fill.alphaF() * LOOSE_FILL));
        painter.setBrush(fill);
        painter.drawRoundedRect(frame, radius, radius);
    } else if (underlined(box, m_look)) {
        const double width = box.depth <= 1 ? MAIN_UNDERLINE : UNDERLINE;
        QPen pen(box.branch, width);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        const double y = box.rect.bottom() - width / 2.0;
        painter.drawLine(QPointF(box.rect.left(), y), QPointF(box.rect.right(), y));
    } else if (box.depth <= 1) {
        painter.setPen(QPen(box.branch, box.border));
        painter.setBrush(mixed(box.branch, m_look.node, MAIN_FILL));
        painter.drawRoundedRect(frame, radius, radius);
    } else {
        painter.setPen(QPen(mixed(box.branch, m_look.mid, DEEP_FRAME), box.border));
        painter.setBrush(m_look.node);
        painter.drawRoundedRect(frame, radius, radius);
    }
    paintNodeContent(painter, box);

    // The selected node, the node a connection starts at
    painter.setBrush(Qt::NoBrush);
    if (box.id == m_document.selected() && m_document.selectedConnection() < 0) {
        QColor soft = m_look.selection;
        soft.setAlphaF(static_cast<float>(SOFT_OPACITY));
        const double outer = SELECTION + SOFT_SELECTION / 2.0;
        painter.setPen(QPen(soft, SOFT_SELECTION));
        painter.drawRoundedRect(box.rect.adjusted(-outer, -outer, outer, outer), radius + outer,
                                radius + outer);
        painter.setPen(QPen(m_look.selection, SELECTION));
        const double inner = SELECTION / 2.0;
        painter.drawRoundedRect(box.rect.adjusted(-inner, -inner, inner, inner), radius + inner,
                                radius + inner);
    }
    if (box.id == m_connectFrom) {
        painter.setPen(QPen(m_look.palette.at(1), OUTLINE));
        const double gap = OUTLINE_GAP + OUTLINE / 2.0;
        painter.drawRoundedRect(box.rect.adjusted(-gap, -gap, gap, gap), radius + gap,
                                radius + gap);
    }
    painter.restore();
}

void MindMapView::paintNodeContent(QPainter& painter, const NodeBox& box) const {
    // The markers: their signs in circles of their colors
    QFont sign = box.font;
    sign.setPointSizeF(SIGN_PIXELS * POINTS_PER_PIXEL * m_look.textScale);
    sign.setWeight(QFont::Bold);
    for (qsizetype i = 0; i < box.markers.size(); ++i) {
        const QRectF circle = box.markerRect(i);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_look.markerColor(box.markers.at(i)));
        painter.drawEllipse(circle);
        painter.setFont(sign);
        painter.setPen(m_look.markerText);
        painter.drawText(circle, Qt::AlignCenter, markerSign(box.markers.at(i)));
    }

    // The text, a line under another
    const QColor textColor = box.root ? m_look.rootText : m_look.text;
    painter.setFont(box.font);
    painter.setPen(textColor);
    const QRectF text = box.textRect();
    const Qt::Alignment alignment =
        (box.root ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::AlignVCenter;
    for (qsizetype i = 0; i < box.lines.size(); ++i) {
        const QRectF line(text.left(), text.top() + box.lineHeight * static_cast<double>(i),
                          text.width() + 1.0, box.lineHeight);
        painter.drawText(line, alignment | Qt::TextSingleLine, box.lines.at(i));
    }

    if (box.link) {
        MindMapMenus::paintLinkIcon(painter, box.linkRect(), textColor);
    }
    if (box.note) {
        MindMapMenus::paintNoteIcon(painter, box.noteRect(), textColor);
    }
}

void MindMapView::paintBadge(QPainter& painter, const NodeBox& box) const {
    if (box.hidden <= 0) {
        return;
    }
    painter.save();
    if (!matches(box.id)) {
        painter.setOpacity(painter.opacity() * DIMMED);
    }
    QFont font = box.font;
    font.setPointSizeF(BADGE_PIXELS * POINTS_PER_PIXEL * m_look.textScale);
    font.setWeight(QFont::Bold);
    const QString number = QString::number(box.hidden);
    const double width =
        std::max(BADGE_SIZE, QFontMetricsF(font).horizontalAdvance(number) + 2.0 * BADGE_PAD);
    const QRectF badge(box.badge.x() - width / 2.0, box.badge.y() - BADGE_SIZE / 2.0, width,
                       BADGE_SIZE);
    const QRectF frame = badge.adjusted(BADGE_BORDER / 2.0, BADGE_BORDER / 2.0,
                                        -BADGE_BORDER / 2.0, -BADGE_BORDER / 2.0);
    painter.setPen(QPen(box.root ? m_look.root : box.branch, BADGE_BORDER));
    painter.setBrush(m_look.node);
    painter.drawRoundedRect(frame, frame.height() / 2.0, frame.height() / 2.0);
    painter.setFont(font);
    painter.setPen(m_look.text);
    painter.drawText(badge, Qt::AlignCenter, number);
    painter.restore();
}

void MindMapView::paintDrag(QPainter& painter) const {
    if (m_drag != Drag::Node || m_drop.where == Drop::Where::None) {
        return;
    }
    painter.save();
    QPen pen(m_look.drop, OUTLINE);
    if (const NodeBox* target = m_scene->box(m_drop.target); target != nullptr) {
        const double radius = radiusOf(*target, m_look);
        const QRectF rect = target->rect;
        const bool across = m_document.map().layout() == MindMapLayout::Down && !target->root;
        painter.setBrush(Qt::NoBrush);
        switch (m_drop.where) {
        case Drop::Where::Into: {
            pen.setStyle(Qt::DashLine);
            painter.setPen(pen);
            const double gap = OUTLINE_GAP + OUTLINE / 2.0;
            painter.drawRoundedRect(rect.adjusted(-gap, -gap, gap, gap), radius + gap,
                                    radius + gap);
            break;
        }
        case Drop::Where::Before:
        case Drop::Where::After: {
            const bool before = m_drop.where == Drop::Where::Before;
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_look.drop);
            const QRectF bar =
                across ? QRectF(before ? rect.left() - DROP_BAR : rect.right(), rect.top(),
                                DROP_BAR, rect.height())
                       : QRectF(rect.left(), before ? rect.top() - DROP_BAR : rect.bottom(),
                                rect.width(), DROP_BAR);
            painter.drawRoundedRect(bar, DROP_BAR / 2.0, DROP_BAR / 2.0);
            break;
        }
        case Drop::Where::None:
        case Drop::Where::Place:
            break;
        }
    }
    // The node where the mouse takes it
    if (const NodeBox* dragged = m_scene->box(m_dragNode); dragged != nullptr) {
        NodeBox ghost = *dragged;
        ghost.rect.moveCenter(m_dragAt - m_dragGrab);
        painter.setOpacity(DRAGGED);
        paintNode(painter, ghost);
    }
    painter.restore();
}

}  // namespace kalahari::mindmap
