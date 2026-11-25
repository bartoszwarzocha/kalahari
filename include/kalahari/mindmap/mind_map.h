// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map.h
/// @brief A mind map of the Workshop: a main topic with branches, loose nodes and connections,
/// as its .kmap file keeps them

#ifndef KALAHARI_MINDMAP_MIND_MAP_H
#define KALAHARI_MINDMAP_MIND_MAP_H

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace kalahari::mindmap {

/// @brief An attribute of a node of a mind map that the writer named and filled: "Motyw":
/// "zdrada"
struct NodeAttribute {
    QString name;       ///< Its name
    QString value;      ///< Its value; may be empty
    QJsonObject extra;  ///< Fields this version does not know, saved back

    bool operator==(const NodeAttribute&) const = default;
};

/// @brief A node of a mind map
///
/// On the map a node is a box with its header in the color of its kind and its rows under
/// it, at its place (its top left corner). A node of the writer (a topic, an idea, a question,
/// a note) has its text as its title, its note as a text under it; a node of an element of the
/// book (a card, a text, another element), of a scene of a text or of a thread of a timeline
/// shows what the book has of it, in the rows the writer chose.
struct MindMapNode {
    /// @name Markers of a node
    /// @{
    static constexpr const char* QUESTION = "question";
    static constexpr const char* IMPORTANT = "important";
    static constexpr const char* IDEA = "idea";
    static constexpr const char* DONE = "done";
    static constexpr const char* CHECK = "check";  ///< To check
    /// @}

    /// @name Kinds of a node
    /// @{
    static constexpr const char* TOPIC_KIND = "topic";        ///< The main topic
    static constexpr const char* IDEA_KIND = "idea";          ///< An idea (a node of earlier
                                                              ///< versions too)
    static constexpr const char* QUESTION_KIND = "question";  ///< A question
    static constexpr const char* NOTE_KIND = "note";          ///< A note
    static constexpr const char* ELEMENT_KIND = "element";    ///< An element of the book (link):
                                                              ///< a card, a text, a material
    static constexpr const char* SCENE_KIND = "scene";        ///< A scene of a text (link,
                                                              ///< scene)
    static constexpr const char* THREAD_KIND = "thread";      ///< A thread of a timeline (link,
                                                              ///< thread)
    /// @}

    /// @name The picture of a node (image)
    /// @{
    static constexpr const char* NO_IMAGE = "none";      ///< None, whatever the settings say
    static constexpr const char* PORTRAIT = "portrait";  ///< The main portrait of its card
    /// @}

    QString id;                         ///< Unique in the map
    QString text;                       ///< Its text, the title of its header; empty: the title
                                        ///< of its element, or of the map for the main topic
    QString link;                       ///< Id of the element of the book it is about; empty:
                                        ///< none
    QString note;                       ///< The writer's note, a text under its title; empty:
                                        ///< none
    QStringList markers;                ///< QUESTION..., in the writer's order
    QString color;                      ///< Key of a color of the palette for its branch;
                                        ///< empty: the color it takes from its branch
    QStringList children;               ///< Ids of its nodes, in order
    bool collapsed = false;             ///< The list: its nodes are hidden under it
    QString kind;                       ///< TOPIC_KIND...; empty: an idea (the main topic: the
                                        ///< main topic)
    bool folded = false;                ///< The map: only its header shows
    std::optional<QPointF> place;       ///< The map: its top left corner; none: not placed yet
    std::optional<QStringList> rows;    ///< The rows the writer chose, in order; none: those of
                                        ///< its kind (the ids of mind_map_content.h)
    QList<NodeAttribute> attributes;    ///< The writer's own attributes, in order
    QString image;                      ///< Its picture: NO_IMAGE, PORTRAIT or the id of a
                                        ///< picture of the Workshop; empty: as the settings say
    int scene = -1;                     ///< SCENE_KIND: its scene in its text, from 0
    bool planned = false;               ///< SCENE_KIND: a scene in the plan of the text, scene
                                        ///< its place in the plan
    QString thread;                     ///< THREAD_KIND: the id of its thread in the timeline
    std::optional<QPointF> freePlace;   ///< Read from a map of format 1: its centre in the free
                                        ///< layout, from the main topic; not saved
    std::optional<QPointF> loosePlace;  ///< Read from a map of format 1: of a loose node, its
                                        ///< centre in the other layouts; not saved
    QJsonObject extra;                  ///< Fields this version does not know, saved back

    /// @brief Its kind, the kind of the main topic or of a node without one too: TOPIC_KIND,
    /// IDEA_KIND...
    [[nodiscard]] QString kindOrIdea(bool root) const;

    /// @brief Whether it is a node of the writer: a topic, an idea, a question or a note
    [[nodiscard]] bool isOwn(bool root) const;

    bool operator==(const MindMapNode&) const = default;
};

/// @brief A frame of a mind map: a title over nodes kept together, which move with it
struct MindMapFrame {
    QString id;         ///< Unique in the map
    QString title;      ///< "Wyprawa 1931"
    QStringList nodes;  ///< The nodes in it, each in one frame at most
    QString color;      ///< Key of a color of the palette; empty: the color of the frames
    QJsonObject extra;  ///< Fields this version does not know, saved back

    bool operator==(const MindMapFrame&) const = default;
};

/// @brief What a map of relations draws: the cards of the kinds chosen, and their relations
struct RelationMapOptions {
    QStringList kinds;        ///< The kinds of the cards: "kalahari.base:character"
    bool family = true;       ///< The relations of the family
    bool others = true;       ///< The other relations between cards of the same kind
    bool across = true;       ///< The relations between cards of different kinds (the places and
                              ///< the items of the characters)
    bool generations = true;  ///< The family in generations; else the cards in a grid
    QJsonObject extra;        ///< Fields this version does not know, saved back

    bool operator==(const RelationMapOptions&) const = default;
};

/// @brief A connection between two nodes of a mind map besides their branches: from the output
/// of a node to the input of another, as a branch is drawn
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
/// a node: without its own text it shows the title of the map. The branches make a tree: a
/// node has one parent at most, whose output its input is joined with; a loose node has none.
/// Connections join other nodes besides the tree, frames keep nodes together. The relations of
/// the cards are not in the file: the map draws those of the cards it has (mind_map_content.h).
/// The file:
/// @code{.json}
/// {
///   "format": 2,
///   "root": "n1",
///   "nodes": [
///     { "id": "n1", "text": "", "children": ["n2"], "place": [0, 0] },
///     { "id": "n2", "text": "Kompas", "children": ["n3"], "place": [260, 0] },
///     { "id": "n3", "text": "Kto go teraz nosi?", "kind": "question", "markers": ["question"],
///       "note": "...", "place": [520, 0], "attributes": [ { "name": "Motyw", "value": "..." } ] },
///     { "id": "n4", "kind": "element", "link": "<card id>", "place": [520, 160],
///       "rows": ["role", "rel:kalahari.base:character|kalahari.base:character|family"],
///       "image": "portrait", "folded": true },
///     { "id": "n5", "kind": "scene", "link": "<chapter id>", "scene": 1, "place": [0, 300] },
///     { "id": "n6", "kind": "thread", "link": "<timeline id>", "thread": "t1", "place": [...] }
///   ],
///   "connections": [ { "from": "n3", "to": "n6", "label": "ta sama wyprawa" } ],
///   "frames": [ { "id": "f1", "title": "Wyprawa 1931", "nodes": ["n3", "n4"] } ],
///   "relations": { "kinds": ["kalahari.base:character"], "family": true, "others": true,
///                  "across": false, "generations": true },
///   "view": { "layout": "bothSides", "mode": "map", "zoom": 1, "centre": [300, 120] }
/// }
/// @endcode
/// A map of format 1 (of earlier versions) is read too: its nodes are ideas without places,
/// which the map puts where its layout had them ("free", "loose"); its connections stay.
class MindMap {
public:
    static constexpr int FORMAT = 2;  ///< Version of the format of the file

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

    /// @brief Connection from node @p from to node @p to, either way when @p eitherWay; -1:
    /// none
    [[nodiscard]] qsizetype connectionOf(const QString& from, const QString& to,
                                         bool eitherWay = true) const;

    // =========================================================================
    // The map of nodes
    // =========================================================================

    /// @brief Add a node of kind @p kind (MindMapNode::IDEA_KIND...) with text @p text, its top
    /// left corner at @p place: inside node @p parent at its end, or loose when it is empty
    /// @return Its id, or empty when the map has no node @p parent
    QString addNode(const QString& kind, const QString& text, const QPointF& place,
                    const QString& parent = QString());

    /// @brief Put node @p id at place @p place (its top left corner)
    bool setPlace(const QString& id, const QPointF& place);

    /// @brief Take node @p id out alone: the nodes inside it become loose, where they are
    /// @return False for the main topic, or a node the map has not
    bool removeAlone(const QString& id);

    /// @brief The node that node @p id is the main topic of, or of a loose branch: the top of
    /// its tree
    [[nodiscard]] QString topOf(const QString& id) const;

    /// @brief The node of element @p elementId (MindMapNode::ELEMENT_KIND); empty: none
    [[nodiscard]] QString nodeOfElement(const QString& elementId) const;

    [[nodiscard]] const QList<MindMapFrame>& frames() const { return m_frames; }

    /// @brief The frame with id @p id; nullptr: none
    [[nodiscard]] const MindMapFrame* frame(const QString& id) const;

    /// @brief The frame node @p id is in; nullptr: none
    [[nodiscard]] const MindMapFrame* frameOf(const QString& nodeId) const;

    /// @brief A frame titled @p title around nodes @p nodes, which leave their frames
    /// @return Its id; empty when the map has none of the nodes
    QString addFrame(const QString& title, const QStringList& nodes);

    /// @brief Put @p frame in place of the frame of its id; its nodes leave their other frames
    bool setFrame(const MindMapFrame& frame);

    /// @brief Take frame @p id out; its nodes stay
    bool removeFrame(const QString& id);

    /// @brief Put node @p nodeId into frame @p frameId (empty: out of its frame)
    bool setFrameOf(const QString& nodeId, const QString& frameId);

    /// @brief What a map of relations draws; none: the map is no map of relations
    [[nodiscard]] const std::optional<RelationMapOptions>& relationMap() const {
        return m_relationMap;
    }
    void setRelationMap(const std::optional<RelationMapOptions>& options) {
        m_relationMap = options;
    }

    /// @brief Whether the main topic is drawn: a map of relations shows only its cards, until
    /// the writer gives the main topic its own nodes
    [[nodiscard]] bool showsRoot() const;

    /// @brief The names of the attributes of the nodes, each once, in the order they come
    [[nodiscard]] QStringList attributeNames() const;

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
    /// @brief The point of the map in the middle of the window; none: not seen yet
    [[nodiscard]] const std::optional<QPointF>& centre() const { return m_centre; }
    void setCentre(const std::optional<QPointF>& centre) { m_centre = centre; }

    /// @brief Whether a node has no place on the map yet (MindMapNode::place)
    [[nodiscard]] bool hasNodesWithoutPlace() const;

    /// @brief The smallest and the largest zoom
    static constexpr double MIN_ZOOM = 0.2;
    static constexpr double MAX_ZOOM = 4.0;

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
    [[nodiscard]] QString newFrameId() const;
    MindMapNode* findNode(const QString& id);

    /// @brief Take node @p id out of the list of its parent, or of the loose nodes
    void detach(const QString& id);

    QString m_root;
    QHash<QString, MindMapNode> m_nodes;
    QHash<QString, QString> m_parents;  ///< Node -> its parent
    QStringList m_loose;
    QList<MindMapConnection> m_connections;
    QList<MindMapFrame> m_frames;
    std::optional<RelationMapOptions> m_relationMap;
    MindMapLayout m_layout = MindMapLayout::BothSides;
    MindMapMode m_mode = MindMapMode::Map;
    double m_zoom = 1.0;
    std::optional<QPointF> m_centre;
    QJsonObject m_viewExtra;
    QJsonObject m_extra;
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_H
