// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_content.h
/// @brief What the nodes of a mind map show of the book: the rows of the fields of their cards,
/// their relations as sockets, the scenes of the texts, the threads of the timelines, the
/// portraits; and what the book has to put on a map

#ifndef KALAHARI_MINDMAP_MIND_MAP_CONTENT_H
#define KALAHARI_MINDMAP_MIND_MAP_CONTENT_H

#include <kalahari/core/card_relations.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_look.h>

#include <QCoreApplication>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::mindmap {

/// @brief The ids of the rows of the nodes (MindMapNode::rows)
struct NodeRows {
    static constexpr const char* NOTE = "note";                ///< The writer's note
    static constexpr const char* MARKERS = "markers";          ///< The markers
    static constexpr const char* LINK = "link";                ///< The element a node is about
    static constexpr const char* DESCRIPTION = "description";  ///< In short; of a scene its
                                                               ///< description
    static constexpr const char* STATUS = "status";            ///< Of a text, of a scene
    static constexpr const char* WORDS = "words";              ///< Of a text, of a scene
    static constexpr const char* POV = "pov";                  ///< Of a text, of a scene
    static constexpr const char* PLACE = "place";              ///< Of a text, of a scene
    static constexpr const char* TIME = "time";                ///< Of a text, of a scene
    static constexpr const char* EVENTS = "events";            ///< Of a thread
    static constexpr const char* FIELD = "field:";             ///< A field of a card: "field:role"
    static constexpr const char* ATTRIBUTE = "attr:";          ///< An attribute: "attr:Motyw"
    /// The relations of a card with the cards of a kind: "rel:<kind>|<other kind>|<group>",
    /// the group "family" or empty (between cards of one kind: the relations outside the
    /// family; else all)
    static constexpr const char* RELATIONS = "rel:";

    /// @brief The id of the row of the relations of a card of kind @p kind with the cards of
    /// kind @p otherKind in group @p group
    [[nodiscard]] static QString relationsRow(const QString& kind, const QString& otherKind,
                                              const QString& group);
};

/// @brief A row of a node as it shows: a field and its value, a text across the node, or the
/// relations of a card with their socket
struct NodeRow {
    QString id;                        ///< NodeRows
    QString label;                     ///< "Rola"; empty: none (a text across the node)
    QString value;                     ///< "główna postać"
    bool wide = false;                 ///< The value across the node, wrapped
    std::optional<SocketKind> socket;  ///< The relations of a card: the kind of their socket
    bool left = false;                 ///< Its socket on the left edge
    bool right = false;                ///< Its socket on the right edge
    QString otherKind;                 ///< The relations: the kind of the other cards
    QString group;                     ///< The relations: "family" or empty
    QStringList markers;               ///< The markers

    bool operator==(const NodeRow&) const = default;
};

/// @brief What a node shows
struct NodeContent {
    QString title;     ///< The title of its header
    QString kindName;  ///< "Postać", "Pomysł"
    NodeColor color = NodeColor::Idea;
    QString icon;          ///< The id of the icon of its header
    QList<NodeRow> rows;   ///< In order
    QImage image;          ///< Its picture; null: none
    bool missing = false;  ///< Its element, its scene or its thread is not in the book
    QString elementId;     ///< The element it opens; empty: none
    QString cardKind;      ///< Of a card: its kind ("kalahari.base:character"); else empty
};

/// @brief A row a node can show, for the window of the node
struct RowChoice {
    QString id;     ///< NodeRows
    QString label;  ///< "Rola", "Rodzina"

    bool operator==(const RowChoice&) const = default;
};

/// @brief A relation between two cards whose nodes are on the map: a link between their
/// sockets, drawn from what the cards have
struct RelationLink {
    QString from;     ///< The node of the card whose link it is
    QString to;       ///< The node of the other card
    QString fromRow;  ///< The row of its socket at @p from (NodeRows::relationsRow())
    QString toRow;    ///< The row of its socket at @p to
    SocketKind kind = SocketKind::Relations;
    QString label;                 ///< "dziadek – wnuczka"
    core::ShownRelation relation;  ///< As the card of @p from has it (CardRelations::of())
};

/// @brief Something of the book to put on a map: a card, a text, a scene, a thread
struct CatalogEntry {
    QString kind;          ///< MindMapNode::ELEMENT_KIND, SCENE_KIND or THREAD_KIND
    QString elementId;     ///< The element: the card, the text, the timeline
    int scene = -1;        ///< SCENE_KIND: its scene, from 0
    bool planned = false;  ///< SCENE_KIND: a scene of the plan
    QString thread;        ///< THREAD_KIND: the id of its thread
    QString title;         ///< "Amara", "Rozdział 2 › Scena 3", "Oś czasu › Wyprawa"
    QString group;         ///< "Postacie", "Teksty", "Sceny", "Wątki"
    NodeColor color = NodeColor::Element;
};

/// @brief What the nodes of a map show of the book of its window
///
/// It reads the book when asked and keeps what it read (the scenes of the texts, the threads of
/// the timelines, the pictures) until forget(): after the book changed.
class MindMapContent {
    Q_DECLARE_TR_FUNCTIONS(MindMapContent)

public:
    /// @param book The book of the window; nullptr: none (the nodes of the writer only)
    explicit MindMapContent(const core::ProjectManager* book);

    /// @brief Read the book again: it changed
    void forget();

    /// @brief What node @p node of @p map shows, the main topic titled @p mapTitle without a
    /// text of its own, in look @p look
    [[nodiscard]] NodeContent contentOf(const MindMap& map, const MindMapNode& node,
                                        const QString& mapTitle, const MindMapLook& look) const;

    /// @brief The title of node @p node: its text, else that of its element, its scene or its
    /// thread; of the main topic, of the map
    [[nodiscard]] QString titleOf(const MindMap& map, const MindMapNode& node,
                                  const QString& mapTitle) const;

    /// @brief The rows of the book node @p node can show, in their order
    [[nodiscard]] QList<RowChoice> rowChoices(const MindMapNode& node) const;

    /// @brief The rows node @p node shows without a choice of the writer
    [[nodiscard]] QStringList defaultRows(const MindMapNode& node) const;

    /// @brief The relations of the cards whose nodes are on @p map, each once, as its options
    /// of a map of relations let them show
    [[nodiscard]] QList<RelationLink> relationLinks(const MindMap& map) const;

    /// @brief What the book has to put on a map, in the groups of the menu Add Node
    [[nodiscard]] QList<CatalogEntry> catalog() const;

    /// @brief The relations card @p cardId can have with card @p otherId whose links join
    /// sockets of @p kind (the family, the others, all)
    [[nodiscard]] QList<core::RelationInfo> relationChoices(const QString& cardId,
                                                            const QString& otherId,
                                                            SocketKind kind) const;

    /// @brief The kind of the sockets of the relations of a card of kind @p kind with a card
    /// of kind @p otherKind in group @p group
    [[nodiscard]] static SocketKind socketOf(const QString& kind, const QString& otherKind,
                                             const QString& group);

    /// @brief The group of relation @p relationId of a card of kind @p kind: "family" or empty
    [[nodiscard]] QString groupOf(const QString& relationId, const QString& kind) const;

    /// @brief The kind of card @p elementId: "kalahari.base:character"; empty for no card
    [[nodiscard]] QString cardKindOf(const QString& elementId) const;

    /// @brief The plural name of kind @p kind: "Postacie"
    [[nodiscard]] QString pluralOf(const QString& kind) const;

    /// @brief The color of the header of a node of element @p element
    [[nodiscard]] NodeColor colorOf(const core::ProjectElement& element) const;

    /// @brief The name of the kind of a node of the writer: "Pomysł"
    [[nodiscard]] static QString ownKindName(const QString& kind);

    /// @brief The kinds of the cards of the book: "kalahari.base:character"...
    [[nodiscard]] QStringList cardKinds() const;

    /// @brief Whether a map of relations of @p options shows the relations of a card of kind
    /// @p kind with a card of kind @p otherKind in group @p group
    [[nodiscard]] static bool shows(const RelationMapOptions& options, const QString& kind,
                                    const QString& otherKind, const QString& group);

    /// @brief The book; nullptr: none
    [[nodiscard]] const core::ProjectManager* book() const { return m_book; }

private:
    /// A thread of a timeline, as its file has it
    struct ThreadInfo {
        QString name;
        int events = 0;
    };

    [[nodiscard]] QList<NodeRow> relationRows(const QString& kind) const;
    [[nodiscard]] const QList<core::KmlScene>& scenesOf(const QString& textId) const;
    void addCardRows(const MindMapNode& node, const core::ProjectElement& card,
                     const QStringList& rows, NodeContent& content) const;
    void addTextRows(const MindMapNode& node, const core::ProjectElement& text,
                     const QStringList& rows, NodeContent& content) const;
    void addSceneRows(const MindMapNode& node, const QStringList& rows, NodeContent& content) const;
    void addThreadRows(const MindMapNode& node, const QStringList& rows, NodeContent& content) const;
    [[nodiscard]] QImage imageOf(const MindMapNode& node, const MindMapLook& look) const;
    [[nodiscard]] QList<std::pair<QString, ThreadInfo>> threadsOf(const QString& timelineId) const;
    [[nodiscard]] QString sceneTitle(const QString& textId, int scene, bool planned) const;
    [[nodiscard]] static QString statusName(const QString& status);

    const core::ProjectManager* m_book = nullptr;
    mutable QHash<QString, QJsonObject> m_timelines;         ///< The files of the timelines read
    mutable QHash<QString, QList<core::KmlScene>> m_scenes;  ///< The scenes of the texts read
    mutable QHash<QString, QImage> m_images;                 ///< The pictures made, by their keys
    mutable QHash<QString, QList<NodeRow>> m_relationRows;   ///< By the kind of the card
    mutable std::optional<QStringList> m_cardKinds;
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_CONTENT_H
