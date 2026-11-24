// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_view.h
/// @brief The map of a mind map as a map of nodes: the boxes of the nodes with their rows and
/// sockets, the links and the frames, worked with the keyboard and the mouse

#ifndef KALAHARI_MINDMAP_MIND_MAP_VIEW_H
#define KALAHARI_MINDMAP_MIND_MAP_VIEW_H

#include <kalahari/core/card_relations.h>
#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_scene.h>

#include <QFont>
#include <QHash>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <memory>
#include <optional>

class QLineEdit;
class QMenu;
class QPainter;
class QTimer;
class QVariantAnimation;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::mindmap {

class MindMapContent;
class MindMapDocument;

/// @brief The map of a mind map on its tab, as the node editors of 3D programs draw them
///
/// A node is a box with its header in the color of its kind and its rows under it; the sockets
/// on its edges join it with the other nodes: the branch at the header, the relations of a card
/// at their rows, in the colors of their kinds. A link is dragged from a socket to a socket of
/// the same color; dropped on an empty place from the output of a branch it opens Add Node, the
/// new node joined. The wheel zooms around the mouse, a drag of the background or the middle
/// button moves the map; Ctrl or Shift with a click or a drag of the background selects more
/// nodes.
///
/// The keys work while the map has the focus, before the commands of the window: Tab and
/// Insert add a branch, Enter a node below (Shift+Enter above), Shift+A opens Add Node, F2
/// changes the title, H folds the node, Ctrl+J puts a frame around it, Delete deletes the node
/// or the link selected, the arrows go to the next node, Ctrl+arrows move the nodes, Ctrl+Z and
/// Ctrl+Y undo and redo, Ctrl++, Ctrl+- and Ctrl+0 zoom, Home shows the whole map.
class MindMapView : public QWidget {
    Q_OBJECT

public:
    /// @brief A link of the map: a branch (to node @p to), a connection or a relation
    struct LinkRef {
        LinkLine::Type type = LinkLine::Type::Branch;
        QString from;
        QString to;
        qsizetype connection = -1;     ///< A connection: in MindMap::connections()
        core::ShownRelation relation;  ///< A relation: as the card of @p from has it

        bool operator==(const LinkRef&) const = default;
    };

    /// @brief The map of @p document, its nodes showing @p content of @p book (nullptr: none)
    MindMapView(MindMapDocument& document, MindMapContent& content, core::ProjectManager* book,
                QWidget* parent = nullptr);
    ~MindMapView() override;

    MindMapView(const MindMapView&) = delete;
    MindMapView& operator=(const MindMapView&) = delete;
    MindMapView(MindMapView&&) = delete;
    MindMapView& operator=(MindMapView&&) = delete;

    /// @brief Draw the map again after it or the book changed
    void refresh();

    /// @brief How the map looks: after the settings or the theme changed
    void setLook(const MindMapLook& look);
    [[nodiscard]] const MindMapLook& look() const { return m_look; }

    /// @brief The title of the map, which its main topic shows without a text of its own
    void setMapTitle(const QString& title);

    /// @brief Dim the nodes that do not match @p filter: a marker, "link", "note"; empty: none
    void setHighlight(const QString& filter);
    [[nodiscard]] const QString& highlight() const { return m_highlight; }

    /// @brief The zoom: 1 is 100%
    [[nodiscard]] double zoom() const { return m_zoom; }
    /// @brief Zoom in (@p steps > 0) or out by steps, around the middle
    void zoomBy(int steps);
    /// @brief Zoom to 100%
    void resetZoom();
    /// @brief Show the whole map
    void showAll(bool animated = true);

    /// @brief The point of the map at @p point of the widget, and back
    [[nodiscard]] QPointF toMap(const QPointF& point) const;
    [[nodiscard]] QPointF toView(const QPointF& point) const;

    /// @brief Change the title of node @p id in its place
    void startEditing(const QString& id);
    /// @brief Change the label of connection @p index in its place
    void startEditingLabel(qsizetype index);
    /// @brief Change the title of frame @p id in its place
    void startEditingFrame(const QString& id);
    [[nodiscard]] bool isEditing() const;

    /// @brief Scroll so that node @p id is seen
    void ensureShown(const QString& id);

    /// @brief The drawing of the map as it is now
    [[nodiscard]] const MindMapScene* scene() const { return m_scene.get(); }

    /// @brief The link selected; none: none
    [[nodiscard]] const std::optional<LinkRef>& selectedLink() const { return m_link; }
    /// @brief Select @p link, and no node
    void selectLink(const std::optional<LinkRef>& link);
    /// @brief The frame selected; empty: none
    [[nodiscard]] const QString& selectedFrame() const { return m_frame; }
    /// @brief What the selection is and what can be done with it, for the line under the map
    [[nodiscard]] QString hint() const;
    /// @brief Select frame @p id, and no node
    void selectFrame(const QString& id);

    // =========================================================================
    // The commands of the bar, the keys and the menus (mind_map_view_commands.cpp)
    // =========================================================================

    /// @brief Add a branch to the node selected: a new idea on its right
    void addBranch();
    /// @brief Add an idea below the node selected (above it with @p above), of its branch
    void addBelow(bool above);
    /// @brief Open Add Node: the kinds and what the book has, with a search; the new node at
    /// @p mapPoint (none: right of the node selected), a branch of node @p from (empty: loose)
    void openAddNode(const std::optional<QPointF>& mapPoint = std::nullopt,
                     const QString& from = QString(), const QPoint& globalPoint = QPoint());
    /// @brief Fold the nodes selected to their headers, or unfold them
    void foldSelected();
    /// @brief Put a frame around the nodes selected
    void frameSelected();
    /// @brief Put the nodes in layout @p layout, which the map keeps (MindMapLayout::Free:
    /// the map's own, to the right when it has none)
    void arrange(MindMapLayout layout);
    /// @brief Put the cards in the generations of their families, the other nodes beside them
    void arrangeGenerations();
    /// @brief Delete what is selected: the nodes alone, a link, a frame
    void removeSelected();
    /// @brief Connect the node selected with another: a list of the nodes with a search
    void connectTo();
    /// @brief Open the element of the node selected
    void openSelected();
    /// @brief Whether the map has nodes of cards whose families can stand in generations
    [[nodiscard]] bool hasFamilies() const;

    /// @brief The menu of what is selected, as the right click shows it
    void fillMenu(QMenu& menu);

    /// @brief Select nothing: no node, no link, no frame
    void clearSelection();

    /// @brief Finish the animations at once: of the zoom, of the view, of the places
    void finishAnimations();

signals:
    /// @brief Open element @p elementId of the book, which a node is about (Ctrl+Enter)
    void openElementRequested(const QString& elementId);
    /// @brief The window of node @p id: its title, its note, its rows, its attributes
    void nodeWindowRequested(const QString& id);
    /// @brief Choose the element of the book that node @p id is about
    void linkRequested(const QString& id);
    /// @brief A message for the bar under the map; with @p undoable it offers Undo
    void told(const QString& message, bool undoable);
    /// @brief The zoom changed
    void zoomChanged(double zoom);
    /// @brief Another node, link or frame is selected
    void selectionChanged();

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool focusNextPrevChild(bool next) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    /// What the mouse drags
    enum class Drag { None, Pending, Nodes, Frame, Pan, Link, Band };

    // The view (mind_map_view.cpp)
    void rebuild();
    void placeNewNodes();
    /// The places of the cards of the family in generations, from right of @p anchor: a parent
    /// one column before its child, the other cards and the writer's nodes in later columns
    [[nodiscard]] QHash<QString, QPointF> generationPlaces(const QRectF& anchor) const;
    /// The places of the cards in a grid from @p topLeft, by kind
    [[nodiscard]] QHash<QString, QPointF> gridPlaces(const QPointF& topLeft) const;
    void setZoomAt(double zoom, const QPointF& viewPoint, bool animated);
    void animateView(double zoom, const QPointF& offset);
    void viewMoved();
    void saveView();
    [[nodiscard]] double reach(double pixels) const { return pixels / m_zoom; }
    [[nodiscard]] QRectF toView(const QRectF& rect) const;
    [[nodiscard]] QString titleOf(const QString& id) const;
    [[nodiscard]] std::optional<LinkRef> linkRefOf(qsizetype index) const;
    [[nodiscard]] qsizetype linkIndexOf(const LinkRef& link) const;
    void animateTo(const QHash<QString, QPointF>& places, const QString& what,
                   std::optional<MindMapLayout> layout);
    void applyPlaces();
    void finishEditing(bool keep);
    void placeEditor();

    // Painting (mind_map_view_paint.cpp)
    void paintBackground(QPainter& painter) const;
    void paintFrames(QPainter& painter) const;
    void paintLinks(QPainter& painter) const;
    void paintLabels(QPainter& painter) const;
    void paintNode(QPainter& painter, const NodeBox& box) const;
    void paintRows(QPainter& painter, const NodeBox& box) const;
    void paintSockets(QPainter& painter, const NodeBox& box) const;
    void paintDrag(QPainter& painter) const;
    void paintIcon(QPainter& painter, const QString& icon, const QRectF& rect) const;
    [[nodiscard]] bool matches(const QString& id) const;

    // The changes (mind_map_view_commands.cpp)
    [[nodiscard]] QPointF placeRightOf(const QString& id) const;
    [[nodiscard]] QPointF placeBelow(const QString& id, bool above) const;
    [[nodiscard]] QPointF freePlace(const QPointF& wanted, double height, bool up) const;
    [[nodiscard]] QPointF newPlace() const;
    void addEntryAt(const CatalogEntry& entry, const QPointF& place, const QString& from);
    void addOwnAt(const QString& kind, const QPointF& place, const QString& from);
    void joinSockets(const SocketBox& from, const SocketBox& to, const QPoint& globalPoint);
    void addRelation(const QString& cardId, const QString& otherId, const QString& relationId);
    void addOtherRelation(const QString& cardId, const QString& otherId);
    void changeRelation(const core::ShownRelation& relation);
    void removeRelation(const core::ShownRelation& relation);
    void addCards(const QStringList& elementIds, const QPointF& place);
    void copySelected();
    void paste();
    void moveSelectedBy(const QPointF& delta);
    void addMenuOfLink(QMenu& menu, const LinkRef& link);
    void addMenuOfFrame(QMenu& menu, const QString& frameId);
    void addMenuOfNode(QMenu& menu);
    void addMenuOfMap(QMenu& menu);
    void tellUndone(bool redo);

    // The mouse (mind_map_view_input.cpp)
    void finishDrag(const QPointF& mapPoint, const QPoint& globalPoint,
                    Qt::KeyboardModifiers modifiers);
    void finishLink(const QPointF& mapPoint, const QPoint& globalPoint);
    [[nodiscard]] std::optional<SocketBox> targetOf(const QPointF& mapPoint) const;
    [[nodiscard]] bool ownKey(const QKeyEvent& key) const;

    MindMapDocument& m_document;
    MindMapContent& m_content;
    core::ProjectManager* m_book = nullptr;
    MindMapLook m_look;
    QString m_mapTitle;
    QFont m_font;
    std::unique_ptr<MindMapScene> m_scene;
    QString m_highlight;
    mutable QHash<QString, QPixmap> m_icons;  ///< The icons of the headers drawn, by id and size

    // The view: the point of the widget where the map's 0,0 is, and the zoom
    QPointF m_offset;
    double m_zoom = 1.0;
    bool m_shown = false;  ///< The map was put in the window once
    double m_zoomTarget = 1.0;
    QPointF m_zoomAnchor;     ///< The point of the widget the zoom keeps in place
    QPointF m_zoomMapAnchor;  ///< The point of the map there
    QTimer* m_zoomTimer = nullptr;
    QTimer* m_viewTimer = nullptr;                 ///< Saves the view a moment after it changed
    QVariantAnimation* m_viewAnimation = nullptr;  ///< Whole map, 100%: the zoom and the offset
    double m_fromZoom = 1.0;
    double m_toZoom = 1.0;
    QPointF m_fromOffset;
    QPointF m_toOffset;
    QVariantAnimation* m_placesAnimation = nullptr;  ///< Arrange: the nodes to their places
    QHash<QString, QPointF> m_fromPlaces;
    QHash<QString, QPointF> m_toPlaces;
    QString m_placesWhat;
    std::optional<MindMapLayout> m_placesLayout;

    // What is selected besides the nodes
    std::optional<LinkRef> m_link;
    QString m_frame;

    // The text being changed: the title of a node, the label of a connection, of a frame
    QPointer<QLineEdit> m_editor;
    QString m_editingNode;
    qsizetype m_editingLabel = -1;
    QString m_editingFrame;

    // The mouse
    Drag m_drag = Drag::None;
    Drag m_pending = Drag::None;  ///< What a drag of the button pressed becomes
    QPoint m_pressAt;             ///< Where the button went down, in the widget
    QPointF m_pressMap;           ///< There, on the map
    QPointF m_dragMap;            ///< The point of the map under the mouse now
    QPointF m_dragDone;           ///< How far the nodes dragged are moved already
    QStringList m_dragNodes;
    QString m_pressNode;  ///< The node the button went down on
    QPointF m_panOffset;  ///< The offset when the drag of the map started
    std::optional<SocketBox> m_linkStart;
    std::optional<SocketBox> m_linkTarget;
    std::optional<SocketBox> m_hoverSocket;  ///< The socket under the mouse
    std::optional<QPointF> m_menuPoint;      ///< The point of the map the menu was opened at
    QPointF m_dropAt;  ///< A drop from the Navigator: where it would land, in the map
    bool m_dropping = false;
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_VIEW_H
