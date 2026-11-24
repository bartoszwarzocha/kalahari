// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_view_paint.cpp
/// @brief The painting of the map of nodes: its background, frames, links, nodes and sockets

#include <kalahari/core/art_provider.h>
#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_view.h>

#include <QFontMetricsF>
#include <QIcon>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace kalahari::mindmap {

namespace {

/// The dots and the lines of the grid are at least this far apart on the screen
constexpr double LEAST_GRID = 7.0;
constexpr double DOT = 2.6;
/// The corners of the nodes and of the frames
constexpr double RADIUS = 6.0;
constexpr double SQUARE_RADIUS = 1.0;
/// The shadow under a node
constexpr double SHADOW = 2.0;
constexpr double SHADOW_OPACITY = 0.45;
/// The lines, in the coordinates of the map
constexpr double LINE = 2.2;
constexpr double SELECTED_LINE = 3.6;
constexpr double DASH = 4.0;
/// On the screen: the ring of what is selected, of a socket under the mouse
constexpr double SELECTION = 2.0;
constexpr double SELECTION_GAP = 2.0;
constexpr double HOT = 3.0;
constexpr double SOCKET_EDGE = 2.0;
/// The text of the rows starts this far from the edges of a node
constexpr double ROW_SIDE = 12.0;
constexpr double ROW_GAP = 8.0;
constexpr double WIDE_ROOM = 4.0;
constexpr double LINE_GAP = 2.0;
/// The markers: circles with their signs
constexpr double MARKER = 14.0;
constexpr double MARKER_GAP = 4.0;
constexpr double MARKER_TEXT = 0.7;
/// What is dimmed: a node that does not match the highlight, a socket that does not join
constexpr double DIMMED = 0.28;
constexpr double NOT_JOINING = 0.3;
constexpr double BAND_FILL = 0.12;
constexpr double DROP_FILL = 0.25;
constexpr double MISSING = 0.45;  ///< The color of its kind in the header of a missing node
constexpr double FRAME_TEXT_SIDE = 8.0;

/// @p a mixed with @p b: @p part of @p a
QColor mixed(const QColor& a, const QColor& b, double part) {
    const double rest = 1.0 - part;
    return QColor::fromRgbF(static_cast<float>(a.redF() * part + b.redF() * rest),
                            static_cast<float>(a.greenF() * part + b.greenF() * rest),
                            static_cast<float>(a.blueF() * part + b.blueF() * rest),
                            static_cast<float>(a.alphaF() * part + b.alphaF() * rest));
}

/// @p color with opacity @p alpha
QColor faded(QColor color, double alpha) {
    color.setAlphaF(static_cast<float>(color.alphaF() * alpha));
    return color;
}

/// A pen of @p width pixels on the screen, whatever the zoom
QPen screenPen(const QColor& color, double width) {
    QPen pen(color, width);
    pen.setCosmetic(true);
    return pen;
}

/// Whether @p a and @p b are the same socket
bool sameSocket(const SocketBox& a, const SocketBox& b) {
    return a.node == b.node && a.row == b.row && a.right == b.right;
}

}  // namespace

void MindMapView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    paintBackground(painter);
    if (!m_scene) {
        return;
    }
    painter.translate(m_offset);
    painter.scale(m_zoom, m_zoom);
    paintFrames(painter);
    paintLinks(painter);
    for (const NodeBox& box : m_scene->nodes()) {
        paintNode(painter, box);
    }
    paintLabels(painter);
    paintDrag(painter);
}

void MindMapView::paintBackground(QPainter& painter) const {
    const QColor background = m_look.background == MapBackground::Own ? m_look.ownBackground
                                                                      : m_look.mapBackground;
    painter.fillRect(rect(), background);
    if (m_look.background != MapBackground::Grid && m_look.background != MapBackground::Dots) {
        return;
    }
    // The pattern goes with the map: it moves and grows with it
    double step = NodeMetrics::GRID * m_zoom;
    const double major = step * NodeMetrics::MAJOR_GRID;
    if (m_look.background == MapBackground::Dots) {
        while (step < LEAST_GRID * 1.5) {
            step *= NodeMetrics::MAJOR_GRID;
        }
        QPen pen(m_look.dots, DOT);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        QList<QPointF> dots;
        const double startX = std::fmod(m_offset.x(), step) - step;
        const double startY = std::fmod(m_offset.y(), step) - step;
        for (double y = startY; y <= height() + step; y += step) {
            for (double x = startX; x <= width() + step; x += step) {
                dots.append(QPointF(x, y));
            }
        }
        painter.drawPoints(dots.constData(), static_cast<int>(dots.size()));
        return;
    }
    const auto lines = [&painter, this](double gap, const QColor& color) {
        painter.setPen(QPen(color, 1.0));
        QList<QLineF> drawn;
        for (double x = std::fmod(m_offset.x(), gap) - gap; x <= width() + gap; x += gap) {
            const double at = std::floor(x) + 0.5;
            drawn.append(QLineF(at, 0.0, at, height()));
        }
        for (double y = std::fmod(m_offset.y(), gap) - gap; y <= height() + gap; y += gap) {
            const double at = std::floor(y) + 0.5;
            drawn.append(QLineF(0.0, at, width(), at));
        }
        painter.drawLines(drawn);
    };
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    if (step >= LEAST_GRID) {
        lines(step, m_look.grid);
    }
    if (major >= LEAST_GRID) {
        lines(major, m_look.gridMajor);
    }
    painter.restore();
}

void MindMapView::paintFrames(QPainter& painter) const {
    QFont title = m_scene->textFont();
    title.setBold(true);
    painter.setFont(title);
    const QFontMetricsF metrics(title);
    for (const FrameBox& frame : m_scene->frames()) {
        painter.setPen(screenPen(frame.color, 1.0));
        painter.setBrush(m_look.frame);
        painter.drawRoundedRect(frame.rect, RADIUS, RADIUS);
        if (frame.id == m_frame) {
            const double gap = SELECTION_GAP / m_zoom;
            painter.setPen(screenPen(m_look.selection, SELECTION));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(frame.rect.adjusted(-gap, -gap, gap, gap), RADIUS, RADIUS);
        }
        if (frame.id == m_editingFrame) {
            continue;
        }
        const QRectF text = frame.title.adjusted(FRAME_TEXT_SIDE, 0.0, -FRAME_TEXT_SIDE, 0.0);
        painter.setPen(m_look.mutedText);
        painter.drawText(text, Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(frame.text, Qt::ElideRight, text.width()));
    }
}

void MindMapView::paintLinks(QPainter& painter) const {
    painter.setBrush(Qt::NoBrush);
    const qsizetype selected = m_link ? linkIndexOf(*m_link) : -1;
    const QList<LinkLine>& links = m_scene->links();
    for (qsizetype i = 0; i < links.size(); ++i) {
        const LinkLine& link = links.at(i);
        QPen pen(link.color, i == selected ? SELECTED_LINE : LINE);
        pen.setCapStyle(Qt::RoundCap);
        if (link.type == LinkLine::Type::Connection) {
            pen.setDashPattern({DASH, DASH / 1.5});
        }
        painter.setPen(pen);
        painter.setOpacity(matches(link.from) && matches(link.to) ? 1.0 : DIMMED);
        painter.drawPath(link.path);
    }
    painter.setOpacity(1.0);
}

void MindMapView::paintLabels(QPainter& painter) const {
    painter.setFont(m_scene->labelFont());
    const qsizetype selected = m_link ? linkIndexOf(*m_link) : -1;
    const QList<LinkLine>& links = m_scene->links();
    for (qsizetype i = 0; i < links.size(); ++i) {
        const LinkLine& link = links.at(i);
        if (link.label.isEmpty() ||
            (link.type == LinkLine::Type::Connection && link.index == m_editingLabel)) {
            continue;
        }
        const QRectF box = m_scene->labelRect(i);
        painter.setOpacity(matches(link.from) && matches(link.to) ? 1.0 : DIMMED);
        painter.setPen(screenPen(i == selected ? m_look.selection : link.color,
                                 i == selected ? SELECTION : 1.0));
        painter.setBrush(m_look.node);
        painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
        painter.setPen(m_look.text);
        painter.drawText(box, Qt::AlignCenter, link.label);
    }
    painter.setOpacity(1.0);
}

void MindMapView::paintIcon(QPainter& painter, const QString& icon, const QRectF& rect) const {
    if (icon.isEmpty()) {
        return;
    }
    const int pixels = std::max(4, static_cast<int>(
                                       std::lround(rect.width() * m_zoom * devicePixelRatioF())));
    const QString key = icon + QLatin1Char(':') + QString::number(pixels);
    auto it = m_icons.find(key);
    if (it == m_icons.end()) {
        const QIcon drawn = core::ArtProvider::getInstance().getIcon(icon);
        it = m_icons.insert(key, drawn.pixmap(QSize(pixels, pixels), 1.0));
    }
    if (!it->isNull()) {
        painter.drawPixmap(rect, *it, QRectF(it->rect()));
    }
}

void MindMapView::paintNode(QPainter& painter, const NodeBox& box) const {
    const bool selected = m_document.selection().contains(box.id);
    const double radius = m_look.shape == NodeShape::Rounded ? RADIUS : SQUARE_RADIUS;
    painter.setOpacity(matches(box.id) ? 1.0 : DIMMED);

    QPainterPath body;
    body.addRoundedRect(box.rect, radius, radius);
    painter.fillPath(body.translated(0.0, SHADOW), faded(m_look.nodeLine, SHADOW_OPACITY));
    painter.fillPath(body, m_look.node);
    QPainterPath header;
    header.addRect(box.header);
    header = body.intersected(header);
    QColor kind = m_look.kindColor(box.content.color);
    if (box.content.missing) {
        kind = mixed(kind, m_look.node, MISSING);
    }
    painter.fillPath(header, kind);
    QPen edge = screenPen(m_look.nodeLine, 1.0);
    if (box.content.missing) {
        edge.setStyle(Qt::DashLine);
    }
    painter.setPen(edge);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(body);

    // The header: the arrow that folds it, the icon of its kind, its title
    QPen arrow(faded(m_look.headerText, 0.8), 1.6);
    arrow.setCapStyle(Qt::RoundCap);
    arrow.setJoinStyle(Qt::RoundJoin);
    painter.setPen(arrow);
    const QPointF c = box.fold.center();
    const double a = box.fold.width() * 0.22;
    if (box.folded) {
        painter.drawPolyline(QPolygonF(
            {c + QPointF(-a / 2.0, -a), c + QPointF(a / 2.0, 0.0), c + QPointF(-a / 2.0, a)}));
    } else {
        painter.drawPolyline(QPolygonF(
            {c + QPointF(-a, -a / 2.0), c + QPointF(0.0, a / 2.0), c + QPointF(a, -a / 2.0)}));
    }
    paintIcon(painter, box.content.icon, box.icon);
    if (box.id != m_editingNode) {
        painter.setFont(m_scene->titleFont());
        painter.setPen(box.content.missing ? m_look.mutedText : m_look.headerText);
        painter.drawText(box.title, Qt::AlignLeft | Qt::AlignVCenter, box.elidedTitle);
    }

    if (!box.folded) {
        if (!box.image.isNull() && !box.content.image.isNull()) {
            // The picture fills its place, cut at the edges
            const QImage& image = box.content.image;
            const double scale = std::max(box.image.width() / image.width(),
                                          box.image.height() / image.height());
            const QSizeF shown(box.image.width() / scale, box.image.height() / scale);
            const QRectF source(QPointF((image.width() - shown.width()) / 2.0,
                                        (image.height() - shown.height()) / 2.0),
                                shown);
            QPainterPath clip;
            clip.addRoundedRect(box.image, radius / 1.5, radius / 1.5);
            painter.save();
            painter.setClipPath(clip, Qt::IntersectClip);
            painter.drawImage(box.image, image, source);
            painter.restore();
        }
        paintRows(painter, box);
    }
    paintSockets(painter, box);

    if (selected) {
        const double gap = SELECTION_GAP / m_zoom;
        painter.setPen(screenPen(m_look.selection, SELECTION));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box.rect.adjusted(-gap, -gap, gap, gap), radius + gap, radius + gap);
    }
    painter.setOpacity(1.0);
}

void MindMapView::paintRows(QPainter& painter, const NodeBox& box) const {
    const QFont& font = m_scene->textFont();
    const QFontMetricsF metrics(font);
    painter.setFont(font);
    for (const RowBox& drawn : box.rows) {
        const NodeRow& row = drawn.row;
        const QRectF inner = drawn.rect.adjusted(ROW_SIDE, 0.0, -ROW_SIDE, 0.0);
        if (!row.markers.isEmpty()) {
            QFont sign = font;
            sign.setBold(true);
            sign.setPixelSize(std::max(1, static_cast<int>(std::lround(MARKER * MARKER_TEXT))));
            painter.setFont(sign);
            double x = inner.left();
            for (const QString& marker : row.markers) {
                const QRectF circle(x, drawn.rect.center().y() - MARKER / 2.0, MARKER, MARKER);
                painter.setPen(Qt::NoPen);
                painter.setBrush(m_look.markerColor(marker));
                painter.drawEllipse(circle);
                painter.setPen(m_look.markerText);
                painter.drawText(circle, Qt::AlignCenter, markerSign(marker));
                x += MARKER + MARKER_GAP;
            }
            painter.setFont(font);
            continue;
        }
        if (row.wide) {
            painter.setPen(m_look.text);
            const double lineHeight = metrics.height() + LINE_GAP;
            double y = drawn.rect.top() + WIDE_ROOM;
            for (const QString& line : drawn.lines) {
                painter.drawText(QRectF(inner.left(), y, inner.width(), lineHeight),
                                 Qt::AlignLeft | Qt::AlignVCenter, line);
                y += lineHeight;
            }
            continue;
        }
        if (row.socket) {
            // The relations: their name at the side of their socket, how many on the other
            const bool right = row.right && !row.left;
            painter.setPen(m_look.text);
            painter.drawText(inner, (right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter,
                             metrics.elidedText(row.label, Qt::ElideRight, inner.width()));
            if (!row.value.isEmpty()) {
                painter.setPen(m_look.mutedText);
                painter.drawText(inner, (right ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
                                 row.value);
            }
            continue;
        }
        const double labelWidth = row.label.isEmpty()
                                      ? 0.0
                                      : std::min(metrics.horizontalAdvance(row.label),
                                                 inner.width() / 2.0);
        if (!row.label.isEmpty()) {
            painter.setPen(m_look.mutedText);
            painter.drawText(QRectF(inner.left(), inner.top(), labelWidth, inner.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             metrics.elidedText(row.label, Qt::ElideRight, labelWidth));
        }
        const double valueWidth = inner.width() - labelWidth - (row.label.isEmpty() ? 0 : ROW_GAP);
        painter.setPen(m_look.text);
        painter.drawText(QRectF(inner.right() - valueWidth, inner.top(), valueWidth, inner.height()),
                         (row.label.isEmpty() ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
                         metrics.elidedText(row.value, Qt::ElideRight, valueWidth));
    }
}

void MindMapView::paintSockets(QPainter& painter, const NodeBox& box) const {
    const double radius = NodeMetrics::SOCKET;
    for (const SocketBox& socket : box.sockets) {
        const bool joins = m_drag != Drag::Link || !m_linkStart ||
                           sameSocket(*m_linkStart, socket) ||
                           MindMapScene::joinable(*m_linkStart, socket).has_value();
        const double opacity = painter.opacity();
        if (!joins) {
            painter.setOpacity(opacity * NOT_JOINING);
        }
        painter.setPen(screenPen(m_look.nodeLine, SOCKET_EDGE * std::min(1.0, m_zoom)));
        painter.setBrush(m_look.socketColor(socket.kind));
        painter.drawEllipse(socket.centre, radius, radius);
        const bool hot = (m_linkTarget && sameSocket(*m_linkTarget, socket)) ||
                         (m_drag == Drag::None && m_hoverSocket &&
                          sameSocket(*m_hoverSocket, socket));
        if (hot) {
            const double ring = radius + HOT / 2.0 / m_zoom;
            painter.setPen(screenPen(m_look.selection, HOT));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(socket.centre, ring, ring);
        }
        painter.setOpacity(opacity);
    }
}

void MindMapView::paintDrag(QPainter& painter) const {
    if (m_drag == Drag::Link && m_linkStart) {
        const QPointF end = m_linkTarget ? m_linkTarget->centre : m_dragMap;
        const bool endRight = m_linkTarget ? m_linkTarget->right : !m_linkStart->right;
        QPen pen(m_look.socketColor(m_linkStart->kind), LINE);
        pen.setDashPattern({DASH, DASH / 1.5});
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(MindMapScene::pathBetween(m_linkStart->centre, m_linkStart->right, end,
                                                   endRight, m_look.lines));
    }
    if (m_drag == Drag::Band) {
        const QRectF band = QRectF(m_pressMap, m_dragMap).normalized();
        painter.setPen(screenPen(m_look.selection, 1.0));
        painter.setBrush(faded(m_look.selection, BAND_FILL));
        painter.drawRect(band);
    }
    if (m_dropping) {
        const QRectF ghost(m_dropAt, QSizeF(NodeMetrics::WIDTH, NodeMetrics::HEADER));
        QPen pen = screenPen(m_look.drop, SELECTION);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(faded(m_look.drop, DROP_FILL));
        painter.drawRoundedRect(ghost, RADIUS, RADIUS);
    }
}

}  // namespace kalahari::mindmap
