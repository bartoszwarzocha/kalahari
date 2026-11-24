// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_scene.h
/// @brief The drawing of a mind map as a map of nodes: where each node, its rows and its
/// sockets, each link and each frame are, for painting them and for finding what the mouse is
/// over

#ifndef KALAHARI_MINDMAP_MIND_MAP_SCENE_H
#define KALAHARI_MINDMAP_MIND_MAP_SCENE_H

#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_content.h>
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
#include <optional>

namespace kalahari::mindmap {

/// @brief The sizes of the map of nodes, in the coordinates of the map (100%: pixels)
struct NodeMetrics {
    static constexpr double WIDTH = 188.0;      ///< Of a node
    static constexpr double HEADER = 28.0;      ///< Of its header
    static constexpr double ROW = 22.0;         ///< Of a row
    static constexpr double PADDING = 6.0;      ///< Around its rows
    static constexpr double MARKERS = 20.0;     ///< Of the row of its markers
    static constexpr double SOCKET = 5.0;       ///< The radius of a socket
    static constexpr double IMAGE = 120.0;      ///< The height of its picture
    static constexpr double GRID = 20.0;        ///< A square of the grid
    static constexpr int MAJOR_GRID = 5;        ///< Every fifth line of the grid is darker
    static constexpr double FRAME_SIDE = 14.0;  ///< A frame around its nodes
    static constexpr double FRAME_TOP = 32.0;   ///< Its title above them
    static constexpr double TITLE_SIZE = 13.0;  ///< The text of a header
    static constexpr double TEXT_SIZE = 12.0;   ///< The text of the rows
    static constexpr double LABEL_SIZE = 11.0;  ///< The labels of the links
    static constexpr int WIDE_LINES = 6;        ///< The most lines of a text across a node
    static constexpr double GAP_ACROSS = 70.0;  ///< A new branch stands this far right
    static constexpr double GAP_ALONG = 18.0;   ///< A new node this far below the one above
};

/// @brief A row of a node as it is drawn
struct RowBox {
    NodeRow row;
    QRectF rect;        ///< In the coordinates of the map
    QStringList lines;  ///< A text across the node, wrapped
};

/// @brief A socket of a node: where links start and end
struct SocketBox {
    QString node;  ///< Its node
    QString row;   ///< Its row (NodeRows); empty: the branch, at the header
    SocketKind kind = SocketKind::Branch;
    bool right = false;  ///< On the right edge (the output of a branch)
    QPointF centre;
    QString otherKind;  ///< A row of relations: the kind of the other cards
    QString group;      ///< A row of relations: "family" or empty
    QString cardKind;   ///< A row of relations: the kind of the card of its node

    bool operator==(const SocketBox&) const = default;
};

/// @brief A node as it is drawn
struct NodeBox {
    QString id;
    QRectF rect;    ///< In the coordinates of the map
    QRectF header;  ///< Its header
    QRectF fold;    ///< The arrow that folds it, in the header
    QRectF title;   ///< The text of its header
    QRectF icon;    ///< The icon of its kind
    QRectF image;   ///< Its picture; empty: none
    NodeContent content;
    QList<RowBox> rows;
    QList<SocketBox> sockets;
    bool root = false;
    bool folded = false;
    QColor branch;        ///< The color of the branches that leave it
    QString elidedTitle;  ///< Its title as it fits its header

    /// @brief Row @p id; nullptr: it does not show
    [[nodiscard]] const RowBox* row(const QString& id) const;
};

/// @brief A link as it is drawn: a branch, a connection or a relation of two cards
struct LinkLine {
    enum class Type {
        Branch,      ///< From a node to one of its nodes
        Connection,  ///< MindMap::connections()
        Relation     ///< MindMapContent::relationLinks()
    };
    Type type = Type::Branch;
    QString from;
    QString to;
    qsizetype index = -1;  ///< Connection: in MindMap::connections(); relation: in relations()
    SocketKind kind = SocketKind::Branch;
    QColor color;
    QPainterPath path;
    QPointF start;
    QPointF end;
    bool startRight = true;
    bool endRight = false;
    QString label;
    QPointF labelCentre;
};

/// @brief A frame as it is drawn
struct FrameBox {
    QString id;
    QRectF rect;   ///< Around its nodes, with its title
    QRectF title;  ///< The strip of its title, which drags the frame
    QString text;
    QColor color;
};

/// @brief The drawing of a map in a look, with what its nodes show of the book
class MindMapScene {
public:
    /// What node @p node shows
    using Content = std::function<NodeContent(const MindMapNode& node)>;

    /// @brief The drawing of @p map in @p look with @p font, its nodes showing @p content, with
    /// the relations @p relations of its cards
    MindMapScene(const MindMap& map, const MindMapLook& look, const QFont& font,
                 const Content& content, QList<RelationLink> relations);

    [[nodiscard]] const QList<NodeBox>& nodes() const { return m_nodes; }
    [[nodiscard]] const QList<LinkLine>& links() const { return m_links; }
    [[nodiscard]] const QList<FrameBox>& frames() const { return m_frames; }
    [[nodiscard]] const QList<RelationLink>& relations() const { return m_relations; }

    /// @brief Node @p id as it is drawn; nullptr: none
    [[nodiscard]] const NodeBox* box(const QString& id) const;

    /// @brief The node at @p point, the top one; empty: none
    [[nodiscard]] QString nodeAt(const QPointF& point) const;

    /// @brief The socket within @p reach of @p point; none: none
    [[nodiscard]] std::optional<SocketBox> socketAt(const QPointF& point, double reach) const;

    /// @brief The link whose label is at @p point, or whose line is within @p reach of it;
    /// -1: none
    [[nodiscard]] qsizetype linkAt(const QPointF& point, double reach) const;

    /// @brief The frame whose title is at @p point; empty: none
    [[nodiscard]] QString frameAt(const QPointF& point) const;

    /// @brief The rectangle of the label of link @p index; empty: it has none
    [[nodiscard]] QRectF labelRect(qsizetype index) const;

    /// @brief The link of connection @p index (MindMap::connections()); -1: none
    [[nodiscard]] qsizetype linkOfConnection(qsizetype index) const;

    /// @brief All that is drawn
    [[nodiscard]] QRectF bounds() const { return m_bounds; }

    /// @brief The font of the headers, of the rows and of the labels
    [[nodiscard]] const QFont& titleFont() const { return m_titleFont; }
    [[nodiscard]] const QFont& textFont() const { return m_textFont; }
    [[nodiscard]] const QFont& labelFont() const { return m_labelFont; }

    /// @brief Move nodes @p ids by @p delta, with their links and frames, as a drag shows them
    void moveNodes(const QStringList& ids, const QPointF& delta);
    /// @brief Move each node of @p deltas by its delta, as an animation shows them
    void moveNodes(const QHash<QString, QPointF>& deltas);

    /// @brief The size each node takes, for a layout (MindMapSizes)
    [[nodiscard]] QSizeF sizeOf(const QString& id) const;

    /// @brief The next node from @p id in direction @p dx, @p dy (-1, 0, 1), as the arrows go;
    /// empty: none there
    [[nodiscard]] QString nodeToward(const QString& id, int dx, int dy) const;

    /// @brief The line from @p start to @p end in @p style: leaving @p start to the right when
    /// @p startRight, entering @p end from the right when @p endRight
    [[nodiscard]] static QPainterPath pathBetween(const QPointF& start, bool startRight,
                                                  const QPointF& end, bool endRight,
                                                  LineStyle style);

    /// @brief The color of the branches that leave node @p id of @p map in @p look: as the
    /// settings color the branches
    [[nodiscard]] static QColor branchColorOf(const MindMap& map, const MindMapLook& look,
                                              const QString& id);

    /// @brief Whether sockets @p a and @p b can be joined, and the kind of their link; none:
    /// they cannot
    [[nodiscard]] static std::optional<SocketKind> joinable(const SocketBox& a, const SocketBox& b);

private:
    void placeNodes();
    [[nodiscard]] NodeBox measure(const MindMapNode& node) const;
    void addLinks();
    void placeLabels();
    void addFrames();
    void updateBounds();
    static void translate(NodeBox& node, const QPointF& delta);
    [[nodiscard]] QPointF socketPoint(const NodeBox& box, const QString& row, bool right) const;
    [[nodiscard]] QStringList wrap(const QString& text, const QFont& font, double width,
                                   int most) const;

    const MindMap& m_map;
    MindMapLook m_look;
    QFont m_titleFont;
    QFont m_textFont;
    QFont m_labelFont;
    Content m_content;
    QList<RelationLink> m_relations;
    QHash<QString, qsizetype> m_index;  ///< Node id -> its place in m_nodes
    QList<NodeBox> m_nodes;
    QList<LinkLine> m_links;
    QList<FrameBox> m_frames;
    QRectF m_bounds;
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_SCENE_H
