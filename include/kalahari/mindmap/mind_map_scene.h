// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_scene.h
/// @brief The drawing of a mind map: where each node, line, badge and connection is, for
/// painting it and for finding what the mouse is over

#ifndef KALAHARI_MINDMAP_MIND_MAP_SCENE_H
#define KALAHARI_MINDMAP_MIND_MAP_SCENE_H

#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_layout.h>
#include <kalahari/mindmap/mind_map_look.h>

#include <QColor>
#include <QFont>
#include <QHash>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <functional>

namespace kalahari::mindmap {

/// @brief A node as it is drawn
struct NodeBox {
    QString id;
    QRectF rect;           ///< In the coordinates of the map: the main topic's centre at 0,0
    QStringList lines;     ///< Its text, wrapped
    QFont font;            ///< The font of its text
    double lineHeight = 0.0;
    int depth = 0;         ///< 0 the main topic, 1 a main branch or a loose node
    int side = 0;          ///< MindMapPlace::side
    bool root = false;
    bool loose = false;    ///< In a loose branch
    bool looseTop = false; ///< The top of a loose branch: a dashed frame
    QColor branch;         ///< The color of its branch
    QStringList markers;
    bool link = false;     ///< It is about an element of the book
    bool note = false;
    int hidden = 0;        ///< The nodes a collapsed node hides
    QPointF badge;         ///< The centre of the badge with their number, when it hides some
    double paddingX = 0.0;
    double paddingY = 0.0;
    double border = 0.0;

    /// @brief The circle of marker number @p index, in the coordinates of the map
    [[nodiscard]] QRectF markerRect(qsizetype index) const;
    /// @brief The rectangle of the text
    [[nodiscard]] QRectF textRect() const;
    /// @brief The icon of the link, at the right end; empty when it has none
    [[nodiscard]] QRectF linkRect() const;
    /// @brief The icon of the note, at the right end; empty when it has none
    [[nodiscard]] QRectF noteRect() const;
};

/// @brief The line from a node to one of its nodes
struct BranchLine {
    QString child;  ///< The node it leads to
    QPainterPath path;
    QColor color;
    double width = 2.0;
};

/// @brief A connection as it is drawn: a dashed line with an arrow and its label
struct ConnectionLine {
    qsizetype index = -1;  ///< In MindMap::connections()
    QPainterPath path;
    QPointF labelCentre;
    QPointF tip;           ///< The end of the arrow
    double angle = 0.0;    ///< The direction of the arrow at its tip, in degrees
};

/// @brief The drawing of a map in a look, with the texts its nodes show
class MindMapScene {
public:
    /// The text that node @p node shows: its text, else the title of its element or, for the
    /// main topic, of the map
    using ShownText = std::function<QString(const MindMapNode& node)>;

    /// @brief The drawing of @p map in @p look, its text in @p font
    MindMapScene(const MindMap& map, const MindMapLook& look, const QFont& font,
                 const ShownText& shownText);

    [[nodiscard]] const QList<NodeBox>& nodes() const { return m_nodes; }
    [[nodiscard]] const QList<BranchLine>& lines() const { return m_lines; }
    [[nodiscard]] const QList<ConnectionLine>& connections() const { return m_connections; }

    /// @brief Node @p id as it is drawn; nullptr when it does not show (inside a collapsed node)
    [[nodiscard]] const NodeBox* box(const QString& id) const;

    /// @brief The node at @p point, the top one; empty: none
    [[nodiscard]] QString nodeAt(const QPointF& point) const;

    /// @brief The collapsed node whose badge is at @p point; empty: none
    [[nodiscard]] QString badgeAt(const QPointF& point) const;

    /// @brief The connection whose label or line is at @p point; -1: none
    [[nodiscard]] qsizetype connectionAt(const QPointF& point) const;

    /// @brief The rectangle of the label of connection @p index, for text @p label
    [[nodiscard]] QRectF labelRect(qsizetype index, const QString& label) const;

    /// @brief All that is drawn, with the labels of the connections
    [[nodiscard]] QRectF bounds() const { return m_bounds; }

    /// @brief The places of the nodes that the layout gave them
    [[nodiscard]] const QHash<QString, MindMapPlace>& places() const { return m_places; }

    /// @brief The font of the labels of the connections
    [[nodiscard]] const QFont& labelFont() const { return m_labelFont; }

    /// @brief The size of a node: its frame around its text, wrapped, and its icons
    [[nodiscard]] QSizeF measure(const MindMapNode& node, int depth);

    /// @brief The color of the branch of node @p id in @p map, in look @p look: its own, else
    /// the one of the branch above it; of a main branch the next of the palette
    [[nodiscard]] static QColor branchColorOf(const MindMap& map, const MindMapLook& look,
                                              const QString& id);

    /// @brief The next node from @p id in direction @p dx, @p dy (-1, 0, 1), as the arrows go;
    /// empty: none there
    [[nodiscard]] QString nodeToward(const QString& id, int dx, int dy) const;

private:
    void placeNodes();
    void addLines();
    void addConnections();
    [[nodiscard]] QColor branchColor(const QString& id) const;
    [[nodiscard]] QPainterPath linePath(const NodeBox& parent, const NodeBox& child) const;

    const MindMap& m_map;
    MindMapLook m_look;
    QFont m_font;
    QFont m_labelFont;
    ShownText m_shownText;
    QHash<QString, MindMapPlace> m_places;
    QHash<QString, qsizetype> m_index;  ///< Node id -> its place in m_nodes
    QHash<QString, NodeBox> m_measured;  ///< Wrapped texts of the nodes measured
    QList<NodeBox> m_nodes;
    QList<BranchLine> m_lines;
    QList<ConnectionLine> m_connections;
    QRectF m_bounds;
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_SCENE_H
