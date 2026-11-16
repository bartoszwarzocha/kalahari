// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_axis_view.h
/// @brief The axis of a timeline: its lanes and events, the keys and the mouse that change them

#ifndef KALAHARI_TIMELINE_TIMELINE_AXIS_VIEW_H
#define KALAHARI_TIMELINE_TIMELINE_AXIS_VIEW_H

#include <kalahari/timeline/timeline_index.h>
#include <kalahari/timeline/timeline_look.h>
#include <kalahari/timeline/timeline_scene.h>

#include <QAbstractScrollArea>
#include <QPoint>
#include <QPointF>
#include <QString>

#include <memory>
#include <optional>

class QLineEdit;
class QMenu;
class QPainter;

namespace kalahari::timeline {

class TimelineDocument;

/// @brief The axis of a timeline, with the keys of the mockup of the Workshop
///
/// Left and Right go to the event before or after in time, Up and Down to the nearest event of
/// the lane above or below; Ctrl with them moves the event in time (a day, a month or a year,
/// as the zoom shows) or to the next lane. Insert adds an event after the selected one, Enter
/// opens the window "Event", F2 or typing changes its title, Delete deletes it. The mouse moves
/// an event in time and between lanes, and puts an event without a time on the axis; a double
/// click on a lane adds an event there.
class TimelineAxisView : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit TimelineAxisView(TimelineDocument& document, QWidget* parent = nullptr);
    ~TimelineAxisView() override;

    TimelineAxisView(const TimelineAxisView&) = delete;
    TimelineAxisView& operator=(const TimelineAxisView&) = delete;
    TimelineAxisView(TimelineAxisView&&) = delete;
    TimelineAxisView& operator=(TimelineAxisView&&) = delete;

    [[nodiscard]] TimelineDocument& document() { return m_document; }
    [[nodiscard]] const TimelineScene& scene() const { return *m_scene; }
    [[nodiscard]] const TimelineLook& look() const { return m_look; }

    /// @brief What the timeline knows of its book; the editor owns it
    void setIndex(const TimelineIndex* index);
    void setLook(const TimelineLook& look);
    /// @brief The events that @p filter does not match are dimmed (TimelineEdits::matches())
    void setHighlight(const QString& filter);
    /// @brief Lay the axis out again, after a change of the timeline or of its view
    void refresh();

    /// @brief Zoom in (@p steps > 0) or out, keeping the middle of the window
    void zoomBy(int steps);
    void resetZoom();
    /// @brief The zoom in percent: 100 at the step of the program's start
    [[nodiscard]] int zoomPercent() const;

    /// @brief Scroll so that event @p id shows
    void ensureShown(const QString& id);

    /// @brief Change the title of event @p id in a field on its card; @p seed replaces it
    void startEditing(const QString& id, const QString& seed = QString());
    [[nodiscard]] bool isEditing() const;

    // The changes of the keys and the menus
    void addAfterSelected();
    void removeSelected();
    void shiftSelected(int direction);
    void moveSelectedToLane(int direction);

signals:
    /// @brief The view says @p message in the bar under it; @p undoable offers Undo
    void told(const QString& message, bool undoable);
    /// @brief Open the window "Event" of event @p id
    void eventRequested(const QString& id);
    /// @brief Open the first chapter of event @p id
    void chapterRequested(const QString& id);
    /// @brief Open the window of the timeline (day one, threads)
    void timelineRequested();
    /// @brief The zoom changed
    void zoomChanged();

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
    /// An event dragged with the mouse
    struct Drag {
        QString id;
        QString lane;           ///< The lane it is dragged from; empty: the tray
        bool fromTray = false;
        QPoint pressedAt;       ///< In the viewport
        QPointF grip;           ///< Where it was taken, from the top left of its card
        QSizeF size;            ///< Its card
        bool started = false;
        bool valid = false;     ///< It is over a lane
        QString toLane;
        double day = 0.0;       ///< Where it goes
        int slot = 0;           ///< In the order of the story
        double dropX = 0.0;     ///< In the coordinates of the lanes
        QString label;          ///< What the drop does
        QPoint at;              ///< The mouse, in the viewport
    };

    // Coordinates
    [[nodiscard]] QPointF toLanes(const QPointF& point) const;
    [[nodiscard]] QPointF fromLanes(const QPointF& point) const;
    [[nodiscard]] QPointF toTray(const QPointF& point) const;
    [[nodiscard]] bool inLanes(const QPointF& point) const;
    [[nodiscard]] bool inTray(const QPointF& point) const;
    /// @brief The event at @p point of the viewport: its id and lane
    [[nodiscard]] const TimelineItem* itemAt(const QPointF& point, bool* inTray) const;
    /// @brief Where the card of @p item is in the viewport
    [[nodiscard]] QRectF viewRectOf(const TimelineItem& item, bool tray) const;
    void updateScrollBars();

    // Painting (timeline_axis_view_paint.cpp)
    void paintHeader(QPainter& painter) const;
    void paintTray(QPainter& painter) const;
    void paintLanes(QPainter& painter) const;
    void paintHeads(QPainter& painter) const;
    void paintItem(QPainter& painter, const TimelineItem& item, bool tray) const;
    void paintDrag(QPainter& painter) const;

    // Keys and the mouse (timeline_axis_view_input.cpp)
    void selectNeighbour(int direction);
    void selectInLane(int direction);
    void selectEdge(bool last);
    void finishEditing(bool keep);
    /// @brief A new event at @p at of the viewport: in its lane, at its day
    void addAt(const QPointF& at);
    void updateDrag(const QPoint& at);
    void finishDrag();
    void showMenuAt(const QPoint& at, const TimelineItem* item, bool tray);
    void fillEventMenu(QMenu& menu, const QString& id);
    void tellResult(const QString& message, bool done);
    void zoomTo(int step, double anchorX);
    [[nodiscard]] static bool typed(const QKeyEvent& key);

    TimelineDocument& m_document;
    const TimelineIndex* m_index;
    TimelineLook m_look;
    std::unique_ptr<TimelineScene> m_scene;
    QString m_highlight;
    QLineEdit* m_editor = nullptr;
    QString m_editingId;
    QString m_editingLane;
    bool m_editingNew = false;
    std::optional<Drag> m_drag;
    bool m_panning = false;
    QPoint m_panFrom;
    QPoint m_scrollFrom;
    int m_wheel = 0;
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_AXIS_VIEW_H
