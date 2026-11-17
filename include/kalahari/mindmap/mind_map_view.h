// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_view.h
/// @brief The map of a mind map: its nodes, lines and connections, worked with the keyboard
/// and the mouse

#ifndef KALAHARI_MINDMAP_MIND_MAP_VIEW_H
#define KALAHARI_MINDMAP_MIND_MAP_VIEW_H

#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_scene.h>

#include <QAbstractScrollArea>
#include <QPointF>
#include <QPointer>
#include <QString>

#include <memory>

class QMenu;
class QPainter;
class QPlainTextEdit;

namespace kalahari::mindmap {

class MindMapDocument;

/// @brief The map of a mind map on its tab
///
/// The keys work while the map has the focus, before the commands of the window (as in the
/// Navigator): arrows go to the next node, Tab and Insert add a node inside the selected one,
/// Enter one after it (Shift+Enter before it), F2 or typing changes its text, Space collapses
/// its branch, Delete removes it, Ctrl+arrows move it, Ctrl+Z and Ctrl+Y undo and redo,
/// Ctrl++, Ctrl+- and Ctrl+0 zoom. A node is dragged onto another (into it, or before or after
/// it at its edge); in the free layout anywhere. A double click on an empty place adds a
/// loose node there.
class MindMapView : public QAbstractScrollArea {
    Q_OBJECT

public:
    /// @brief The map of @p document
    explicit MindMapView(MindMapDocument& document, QWidget* parent = nullptr);
    ~MindMapView() override;

    MindMapView(const MindMapView&) = delete;
    MindMapView& operator=(const MindMapView&) = delete;
    MindMapView(MindMapView&&) = delete;
    MindMapView& operator=(MindMapView&&) = delete;

    /// @brief Draw the map again after it changed, the main topic staying where it is
    void refresh();

    /// @brief How the map looks: after the settings or the theme changed
    void setLook(const MindMapLook& look);

    /// @brief Dim the nodes that do not match @p filter: a marker, "link", "note"; empty: none
    void setHighlight(const QString& filter);
    [[nodiscard]] const QString& highlight() const { return m_highlight; }

    /// @brief Zoom in (@p steps > 0) or out by steps of the zoom
    void zoomBy(int steps);
    /// @brief Zoom to 100%
    void resetZoom();

    /// @brief Change the text of node @p id in its place: @p seed replaces it (a key typed),
    /// else the text is selected or the cursor goes to its end
    void startEditing(const QString& id, const QString& seed = QString(), bool selectAll = true);
    /// @brief Change the label of connection @p index in its place
    void startEditingLabel(qsizetype index);
    [[nodiscard]] bool isEditing() const;

    /// @brief The next click on a node connects the selected node to it (Esc cancels)
    void startConnecting();

    /// @brief Scroll so that node @p id is seen
    void ensureShown(const QString& id);

    /// @brief The drawing of the map as it is now
    [[nodiscard]] const MindMapScene* scene() const { return m_scene.get(); }

    /// @brief The menu of the selected node or connection, as the right click shows it
    void fillMenu(QMenu& menu);

signals:
    /// @brief Open element @p elementId of the book, which a node is about (Ctrl+Enter)
    void openElementRequested(const QString& elementId);
    /// @brief The note of node @p id, in its window
    void noteRequested(const QString& id);
    /// @brief Choose the element of the book that node @p id is about
    void linkRequested(const QString& id);
    /// @brief A message for the bar under the map; with @p undoable it offers Undo
    void told(const QString& message, bool undoable);
    /// @brief The zoom changed
    void zoomChanged(double zoom);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool focusNextPrevChild(bool next) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    /// Where a dragged node goes
    struct Drop {
        enum class Where { None, Into, Before, After, Place };
        Where where = Where::None;
        QString target;  ///< The node it goes into, before or after
    };

    // Coordinates: the map's (the main topic's centre at 0,0) and the viewport's
    [[nodiscard]] double zoom() const;
    [[nodiscard]] QPointF toMap(const QPointF& point) const;
    [[nodiscard]] QPointF toViewport(const QPointF& point) const;
    [[nodiscard]] QRectF toViewport(const QRectF& rect) const;
    [[nodiscard]] QRectF content() const;
    void updateScrollBars();
    void scrollMapPointTo(const QPointF& mapPoint, const QPointF& viewportPoint);
    void setZoom(double zoom, const QPointF& viewportAnchor);
    void zoomAt(int steps, const QPointF& viewportAnchor);

    // Painting (mind_map_view_paint.cpp)
    void paintBackground(QPainter& painter) const;
    void paintLines(QPainter& painter) const;
    void paintConnections(QPainter& painter) const;
    void paintNode(QPainter& painter, const NodeBox& box) const;
    void paintNodeContent(QPainter& painter, const NodeBox& box) const;
    void paintBadge(QPainter& painter, const NodeBox& box) const;
    void paintDrag(QPainter& painter) const;
    [[nodiscard]] bool matches(const QString& id) const;

    // Changes from the keys and the menu (mind_map_view_input.cpp)
    void addChild();
    void addSibling(bool before);
    void removeSelected();
    void moveSelected(int dx, int dy);
    void openLink();
    void finishConnecting(const QString& to);
    void showMenuAt(const QPoint& viewportPoint);
    void toggleSelected();
    void copyAsText();
    void paste();
    [[nodiscard]] Drop dropAt(const QPointF& mapPoint) const;
    void finishDrag(const QPointF& mapPoint);
    void finishEditing(bool keep);
    void makeEditor();
    void placeEditor();
    void fixFreePlaces();
    void placeNewInFree(const QString& id);
    void editNew(const QString& id);

    MindMapDocument& m_document;
    MindMapLook m_look;
    std::unique_ptr<MindMapScene> m_scene;
    QString m_highlight;
    bool m_shown = false;  ///< The map was centred on its main topic once

    // The text being changed: of a node, or the label of a connection
    QPointer<QPlainTextEdit> m_editor;
    QString m_editing;
    qsizetype m_editingLabel = -1;
    bool m_editingNew = false;  ///< A node just added: it goes when nothing is written in it
    int m_wheel = 0;            ///< The turn of the wheel not yet zoomed

    // The mouse
    enum class Drag { None, Pending, Node, Pan };
    Drag m_drag = Drag::None;
    QPoint m_pressAt;           ///< Where the button went down, in the viewport
    QPointF m_dragGrab;         ///< The grabbed point of the node, from its centre
    QPointF m_dragAt;           ///< The point of the map under the mouse
    QString m_dragNode;
    Drop m_drop;
    QString m_connectFrom;      ///< The node a connection starts at; empty: not connecting
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_VIEW_H
