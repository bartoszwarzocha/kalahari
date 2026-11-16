// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map.h
/// @brief A mind map of the Workshop: a main topic with branches, loose nodes and connections,
/// as its .kmap file keeps them

#ifndef KALAHARI_CORE_MIND_MAP_H
#define KALAHARI_CORE_MIND_MAP_H

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace kalahari::core {

/// @brief A node of a mind map
struct MindMapNode {
    /// @name Markers of a node
    /// @{
    static constexpr const char* QUESTION = "question";
    static constexpr const char* IMPORTANT = "important";
    static constexpr const char* IDEA = "idea";
    static constexpr const char* DONE = "done";
    static constexpr const char* CHECK = "check";  ///< To check
    /// @}

    QString id;                        ///< Unique in the map
    QString text;                      ///< Its text; empty: the title of its element, or of the
                                       ///< map for the main topic
    QString link;                      ///< Id of the element of the book it is about; empty:
                                       ///< none
    QString note;                      ///< The writer's note; empty: none
    QStringList markers;               ///< QUESTION..., in the writer's order
    QString color;                     ///< Key of a color of the palette for its branch;
                                       ///< empty: the color it takes from its branch
    QStringList children;              ///< Ids of its nodes, in order
    bool collapsed = false;            ///< Its nodes are hidden
    std::optional<QPointF> freePlace;  ///< Its centre in the free layout, from the main topic
    std::optional<QPointF> loosePlace; ///< Of a loose node: its centre in the other layouts
    QJsonObject extra;                 ///< Fields this version does not know, saved back

    bool operator==(const MindMapNode&) const = default;
};

/// @brief A connection between two nodes of a mind map, across its branches
struct MindMapConnection {
    QString from;       ///< Id of the node it starts at
    QString to;         ///< Id of the node its arrow points at
    QString label;      ///< "ta sama wyprawa"; empty: none
    QJsonObject extra;  ///< Fields this version does not know, saved back

    bool operator==(const MindMapConnection&) const = default;
};

/// @brief How a mind map places its nodes
enum class MindMapLayout {
    BothSides,  ///< The main branches on both sides of the main topic
    Right,      ///< All branches to the right
    Down,       ///< A tree going down
    Free        ///< Where the writer puts the nodes
};

/// @brief What the window of a mind map shows
enum class MindMapMode {
    Map,  ///< The nodes and their lines
    List  ///< The nodes as an outline
};

/// @brief A mind map of the Workshop, as its .kmap file keeps it
///
/// Its title and its description are those of its element in the book file. The main topic is
/// a node: without its own text it shows the title of the map. A loose node has no parent: it
/// stands beside the tree, with its own branch. The file:
/// @code{.json}
/// {
///   "format": 1,
///   "root": "n1",
///   "nodes": [
///     { "id": "n1", "text": "", "children": ["n2"] },
///     { "id": "n2", "text": "Kompas", "children": ["n3"], "color": "amber" },
///     { "id": "n3", "text": "Kto go teraz nosi?", "markers": ["question"],
///       "note": "...", "link": "<element id>" },
///     { "id": "n4", "text": "Sprawdzić burze", "loose": [420, -180] }
///   ],
///   "connections": [ { "from": "n3", "to": "n4", "label": "ta sama wyprawa" } ],
///   "view": { "layout": "bothSides", "mode": "map", "zoom": 1 }
/// }
/// @endcode
class MindMap {
public:
    static constexpr int FORMAT = 1;  ///< Version of the format of the file

    /// @brief A map with an empty main topic
    MindMap();

    /// @brief Id of the main topic
    [[nodiscard]] const QString& root() const { return m_root; }

    /// @brief Node @p id, or nullptr
    [[nodiscard]] const MindMapNode* node(const QString& id) const;

    /// @brief Put @p node in place of the node of its id; its children and its place in the
    /// tree stay as they are
    /// @return False when the map has no node of its id
    bool setNode(const MindMapNode& node);

    /// @brief Ids of all nodes: the tree from the main topic, then the loose nodes with theirs
    [[nodiscard]] QStringList nodeIds() const;

    /// @brief Number of nodes
    [[nodiscard]] qsizetype size() const { return m_nodes.size(); }

    /// @brief Ids of the loose nodes, in the order they were put on the map
    [[nodiscard]] const QStringList& looseNodes() const { return m_loose; }

    /// @brief Id of the parent of node @p id; empty for the main topic and a loose node
    [[nodiscard]] QString parentOf(const QString& id) const;

    /// @brief How deep node @p id is: 0 the main topic, 1 a main branch or a loose node
    [[nodiscard]] int depthOf(const QString& id) const;

    /// @brief Ids of the nodes inside node @p id, at every depth
    [[nodiscard]] QStringList descendantsOf(const QString& id) const;

    /// @brief Id of the main branch, or of the loose node, that node @p id belongs to; empty
    /// for the main topic
    [[nodiscard]] QString branchOf(const QString& id) const;

    /// @brief Whether node @p id is loose, or inside a loose node
    [[nodiscard]] bool isInLooseBranch(const QString& id) const;

    // =========================================================================
    // Changes
    // =========================================================================

    /// @brief Add a node with text @p text inside node @p parent, at place @p index (the end
    /// when negative)
    /// @return Its id, or empty when the map has no node @p parent
    QString addChild(const QString& parent, const QString& text, qsizetype index = -1);

    /// @brief Add a node with text @p text beside node @p id, after it (before it with
    /// @p before); beside the main topic it goes inside it, beside a loose node it is loose
    /// @return Its id, or empty when the map has no node @p id
    QString addSibling(const QString& id, const QString& text, bool before = false);

    /// @brief Add a loose node with text @p text, its centre at @p place
    QString addLoose(const QString& text, const QPointF& place);

    /// @brief Take node @p id out, with the nodes inside it and their connections; the main
    /// topic stays
    /// @return The ids taken out, the node first
    QStringList remove(const QString& id);

    /// @brief Put node @p id inside node @p parent at place @p index (the end when out of the
    /// list); a loose node joins the tree
    /// @return False for the main topic, or a parent inside the node itself
    bool move(const QString& id, const QString& parent, qsizetype index);

    /// @brief Take node @p id out of its branch as a loose node, its centre at @p place
    bool makeLoose(const QString& id, const QPointF& place);

    /// @brief Move node @p id @p steps places among the nodes beside it (up when negative)
    /// @return False when it cannot go there
    bool moveAmongSiblings(const QString& id, int steps);

    /// @brief Put node @p id inside the node before it, at its end
    bool indent(const QString& id);

    /// @brief Put node @p id after its parent, beside it
    bool outdent(const QString& id);

    /// @brief Connect node @p from with node @p to
    /// @return False for a node with itself, a node the map has not, or a connection the
    /// map has already
    bool connect(const QString& from, const QString& to, const QString& label = QString());

    [[nodiscard]] const QList<MindMapConnection>& connections() const { return m_connections; }

    /// @brief Put @p connection in place of connection @p index
    bool setConnection(qsizetype index, const MindMapConnection& connection);

    /// @brief Take connection @p index out
    bool disconnect(qsizetype index);

    /// @brief Forget element @p elementId of the book, which is gone: no node is about it
    /// @param title Its title, which the nodes without their own text keep
    /// @return Whether a node changed
    bool forgetElement(const QString& elementId, const QString& title);

    // =========================================================================
    // Outlines
    // =========================================================================

    /// @brief Node @p id and the nodes inside it as an outline: a line a node, a tab more for
    /// each level
    /// @param shownText The text a node shows: its own, else the title of its element or, of
    /// the main topic, of the map
    [[nodiscard]] QString outlineOf(
        const QString& id, const std::function<QString(const MindMapNode&)>& shownText) const;

    /// @brief Add the lines of outline @p text inside node @p parent from place @p index:
    /// deeper indents (tabs, spaces) go inside the line before; bullets ("-", "*", "•", "1.")
    /// fall away
    /// @return Ids of the nodes the outline's first level made
    QStringList addOutline(const QString& parent, const QString& text, qsizetype index = -1);

    // =========================================================================
    // View and file
    // =========================================================================

    [[nodiscard]] MindMapLayout layout() const { return m_layout; }
    void setLayout(MindMapLayout layout) { m_layout = layout; }
    [[nodiscard]] MindMapMode mode() const { return m_mode; }
    void setMode(MindMapMode mode) { m_mode = mode; }
    [[nodiscard]] double zoom() const { return m_zoom; }
    void setZoom(double zoom);

    /// @brief Give each node in @p places that has no place in the free layout that place
    void fixFreePlaces(const QHash<QString, QPointF>& places);

    /// @brief The map as its file keeps it
    [[nodiscard]] QJsonObject toJson() const;

    /// @brief Read the map from what its file keeps, in place of what it had
    /// @param error Gets what is wrong (in English, for the log)
    /// @return False, and an empty map, for a later format or a map without its main topic
    bool fromJson(const QJsonObject& json, QString* error = nullptr);

    /// @brief Read the map from its file
    bool load(const QString& path, QString* error = nullptr);

    /// @brief Write the map to its file, replacing it in one step
    bool save(const QString& path, QString* error = nullptr) const;

    /// @brief The name of a layout in the file, and back
    [[nodiscard]] static QString nameOf(MindMapLayout layout);
    [[nodiscard]] static std::optional<MindMapLayout> layoutOf(const QString& name);

    bool operator==(const MindMap&) const = default;

private:
    [[nodiscard]] QString newId() const;
    MindMapNode* findNode(const QString& id);

    /// @brief Take node @p id out of the list of its parent, or of the loose nodes
    void detach(const QString& id);

    QString m_root;
    QHash<QString, MindMapNode> m_nodes;
    QHash<QString, QString> m_parents;  ///< Node -> its parent
    QStringList m_loose;
    QList<MindMapConnection> m_connections;
    MindMapLayout m_layout = MindMapLayout::BothSides;
    MindMapMode m_mode = MindMapMode::Map;
    double m_zoom = 1.0;
    QJsonObject m_viewExtra;
    QJsonObject m_extra;
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_MIND_MAP_H
