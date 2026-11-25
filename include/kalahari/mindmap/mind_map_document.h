// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_document.h
/// @brief A mind map open on its tab: its file, its changes and their undoing, the node
/// selected

#ifndef KALAHARI_MINDMAP_MIND_MAP_DOCUMENT_H
#define KALAHARI_MINDMAP_MIND_MAP_DOCUMENT_H

#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_content.h>

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>

#include <functional>
#include <utility>

namespace kalahari::mindmap {

/// @brief A mind map open on its tab: each change is saved to its file at once and can be
/// undone
///
/// The views (the map and the list) change the map only through change(), which keeps the
/// map as it was for Undo, saves the file and tells the views. The selected node is the same
/// in both views; the map can select more nodes besides it. A change of the book that the map
/// shows (a relation of two cards drawn on it) is a step of Undo too (changeWithBook()).
class MindMapDocument : public QObject {
    Q_OBJECT

public:
    /// The text a node shows: its text, else the title of its element or, for the main topic,
    /// of the map
    using ShownText = std::function<QString(const MindMapNode& node)>;

    /// @brief The map of file @p path, empty until load()
    explicit MindMapDocument(const QString& path, QObject* parent = nullptr);

    /// @brief Read the file; a file that is missing or cannot be read leaves an empty map
    /// @return Whether the file was read
    bool load(QString* error = nullptr);

    [[nodiscard]] const MindMap& map() const { return m_map; }
    [[nodiscard]] const QString& path() const { return m_path; }

    /// @brief The texts the nodes show
    void setShownText(ShownText shownText) { m_shownText = std::move(shownText); }
    [[nodiscard]] QString shownText(const MindMapNode& node) const;
    [[nodiscard]] QString shownText(const QString& id) const;

    /// @brief Change the map with @p change, as one step of Undo called @p what
    ///
    /// The map is kept as it was when @p change returns false, or changes nothing. Saves the
    /// file and emits changed().
    /// @param what What Undo undoes: "adding a node"
    /// @return Whether the map changed
    bool change(const QString& what, const std::function<bool(MindMap&)>& change);

    /// @brief Change the map with @p change and the book with @p doBook, as one step of Undo
    /// called @p what, which takes the change of the book back with @p undoBook
    ///
    /// Nothing changes when @p doBook fails. Redo does @p doBook again.
    /// @param change Changes the map; nullptr: the map stays as it is
    /// @return Whether the book changed
    bool changeWithBook(const QString& what, const std::function<bool(MindMap&)>& change,
                        const std::function<bool()>& doBook, const std::function<bool()>& undoBook);

    /// @brief Change the view of the map (its zoom, its mode, the places of new nodes): saved,
    /// but not a step of Undo
    void changeView(const std::function<void(MindMap&)>& change);

    [[nodiscard]] bool canUndo() const { return !m_undo.isEmpty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.isEmpty(); }
    /// @brief What Undo undoes: "adding a node"; empty: nothing
    [[nodiscard]] QString undoText() const;
    /// @brief What Redo does again
    [[nodiscard]] QString redoText() const;
    void undo();
    void redo();

    /// @brief The node selected; empty: none
    [[nodiscard]] const QString& selected() const { return m_selected; }
    /// @brief Select node @p id (one that the map has), alone, and no connection
    void select(const QString& id);
    /// @brief Select no node and no connection: a click on an empty place of the map
    void clearSelection();
    /// @brief The nodes selected, the node selected first
    [[nodiscard]] const QStringList& selection() const { return m_selection; }
    /// @brief Select nodes @p ids, the first one as the node selected
    void setSelection(const QStringList& ids);
    /// @brief Add node @p id to the nodes selected, or take it out (the last one too)
    void toggleSelection(const QString& id);
    /// @brief The connection selected; -1: none
    [[nodiscard]] qsizetype selectedConnection() const { return m_selectedConnection; }
    /// @brief Select connection @p index, and no node
    void selectConnection(qsizetype index);

    /// @brief Whether the last save failed
    [[nodiscard]] bool saveFailed() const { return m_saveFailed; }

    // =========================================================================
    // The changes of both views, each a step of Undo; a new node is selected
    // =========================================================================

    /// @brief A new empty node at the end of the nodes of @p parent; its id, empty: none
    QString addChild(const QString& parent);
    /// @brief A new empty node after node @p id, or before it; inside the main topic for the
    /// main topic, beside a loose node a loose one
    QString addSibling(const QString& id, bool before);
    /// @brief A new empty loose node at @p place
    QString addLoose(const QPointF& place);
    /// @brief Remove node @p id with its branch (not the main topic)
    /// @return How many nodes went; 0: none
    int removeNode(const QString& id);
    /// @brief The text of node @p id; the text it would show anyway (the title of its element
    /// or of the map) leaves it empty, so that it follows that title
    bool setText(const QString& id, const QString& text);
    bool toggleCollapsed(const QString& id);
    bool toggleMarker(const QString& id, const QString& marker);
    /// @brief The color of the branch of node @p id: a key of the palette, empty: its branch's
    bool setColor(const QString& id, const QString& color);
    bool setNote(const QString& id, const QString& note);
    /// @brief The element of the book node @p id is about; empty: none
    bool setLink(const QString& id, const QString& elementId);
    /// @brief Node @p id with its branch inside node @p parent at place @p index
    bool moveNode(const QString& id, const QString& parent, qsizetype index);
    bool moveAmongSiblings(const QString& id, int steps);
    bool indent(const QString& id);
    bool outdent(const QString& id);
    /// @brief Node @p id with its branch out of its branch, a loose node at @p place
    bool makeLoose(const QString& id, const QPointF& place);
    /// @brief The nodes to new places: the free places in the free layout, the loose places of
    /// loose nodes in the other layouts (MindMap::setNode())
    bool movePlaces(const QHash<QString, QPointF>& places, bool free);
    /// @brief A connection from node @p from to node @p to
    bool connectNodes(const QString& from, const QString& to);
    bool setConnectionLabel(qsizetype index, const QString& label);
    bool reverseConnection(qsizetype index);
    bool removeConnection(qsizetype index);
    /// @brief The layout of the map
    bool setLayout(MindMapLayout layout);
    /// @brief An outline of text with indents inside node @p parent at place @p index
    /// @return The nodes added at the top of the outline
    QStringList pasteOutline(const QString& parent, qsizetype index, const QString& text);

    // =========================================================================
    // The changes of the map of nodes
    // =========================================================================

    /// @brief Whether new nodes show only their headers (Settings > Mind Maps)
    void setNewFolded(bool folded) { m_newFolded = folded; }

    /// @brief A new node of kind @p kind (MindMapNode::IDEA_KIND...) with text @p text, its
    /// top left corner at @p place: a branch of node @p parent, or loose without one
    /// @return Its id; empty: none
    QString addNode(const QString& kind, const QString& text, const QPointF& place,
                    const QString& parent = QString());
    /// @brief A new node of what @p entry has of the book (a card, a text, a scene, a thread)
    /// at @p place, a branch of node @p parent or loose
    QString addEntry(const CatalogEntry& entry, const QPointF& place,
                     const QString& parent = QString());
    /// @brief A new idea after node @p id among the nodes of its parent (before it with
    /// @p before), at @p place; beside a loose node a loose one, beside the main topic inside it
    QString addSiblingAt(const QString& id, const QPointF& place, bool before, const QString& text);
    /// @brief Remove nodes @p ids alone: the nodes inside them stay, loose (not the main topic)
    /// @return How many went
    int removeNodes(const QStringList& ids);
    /// @brief Move nodes @p ids by @p delta
    bool moveNodesBy(const QStringList& ids, const QPointF& delta);
    /// @brief Put the nodes at @p places (their top left corners), as one step called @p what
    bool setPlaces(const QHash<QString, QPointF>& places, const QString& what);
    /// @brief Fold nodes @p ids to their headers, or unfold them: all unfolded when one is
    /// folded
    bool toggleFolded(const QStringList& ids);
    /// @brief Join the output of node @p from with the input of node @p to: a branch, when @p to
    /// is loose and not above @p from; else a connection
    bool joinNodes(const QString& from, const QString& to);
    /// @brief Take node @p id out of its branch, a loose node where it is
    bool detachBranch(const QString& id);
    /// @brief A frame titled @p title around nodes @p nodes
    /// @return Its id; empty: none
    QString addFrame(const QStringList& nodes, const QString& title);
    /// @brief Put @p frame in place of the frame of its id
    bool setFrame(const MindMapFrame& frame);
    /// @brief Remove frame @p id; its nodes stay
    bool removeFrame(const QString& id);
    /// @brief Put @p node in place of the node of its id, as one step called @p what
    bool setNode(const MindMapNode& node, const QString& what);

    /// @brief The most steps Undo keeps
    static constexpr qsizetype MOST_STEPS = 200;

signals:
    /// @brief The map changed: a change, Undo or Redo
    void changed();
    /// @brief The view of the map changed (changeView())
    void viewChanged();
    /// @brief Another node or connection is selected
    void selectionChanged();
    /// @brief The file could not be written; @p error says why
    void saveError(const QString& error);
    /// @brief Undo or Redo could not change the book back: @p what
    void bookStepFailed(const QString& what);

private:
    /// A step of Undo or Redo: the map before it, and the node selected then; a change of the
    /// book, with what takes it back and what does it again
    struct Step {
        QJsonObject map;
        QString selected;
        QString what;
        std::function<bool()> undoBook;
        std::function<bool()> redoBook;
    };

    void pushStep(Step step);
    void keepSelection();

    void save();
    void restore(const Step& step);

    QString m_path;
    MindMap m_map;
    ShownText m_shownText;
    QList<Step> m_undo;
    QList<Step> m_redo;
    QString m_selected;
    QStringList m_selection;
    qsizetype m_selectedConnection = -1;
    bool m_saveFailed = false;
    bool m_newFolded = false;
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_DOCUMENT_H
