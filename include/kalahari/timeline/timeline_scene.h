// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_scene.h
/// @brief Where the axis of a timeline draws each lane, event, tick and gap

#ifndef KALAHARI_TIMELINE_TIMELINE_SCENE_H
#define KALAHARI_TIMELINE_TIMELINE_SCENE_H

#include <kalahari/timeline/time_axis.h>
#include <kalahari/timeline/timeline.h>
#include <kalahari/timeline/timeline_index.h>
#include <kalahari/timeline/timeline_look.h>

#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <optional>

namespace kalahari::timeline {

/// @brief A lane of the axis: the events of a thread, a character, a place, or all
struct TimelineLane {
    QString key;   ///< The id of its thread, character or place (or a name); "": none; "*": all
    QString name;  ///< "Wyprawa"
    QString kind;  ///< "thread", "character", "place"; empty: none
    QColor color;
    double top = 0.0;     ///< In the coordinates of the lanes
    double height = 0.0;
    int count = 0;        ///< Its events
};

/// @brief An event in a lane (an event of several characters is in each of their lanes)
struct TimelineItem {
    QString id;    ///< The event
    QString lane;  ///< The key of its lane
    QRectF rect;   ///< In the coordinates of the lanes: x from the start of the axis
    double span = 0.0;  ///< The width of its days; 0: a moment
};

/// @brief A tick or a label of the axis
struct TimelineTick {
    enum class Kind {
        Minor,   ///< A short tick
        Major,   ///< A tick with its label beside it
        Middle,  ///< A label in the middle of a day
        Upper    ///< A label of the upper row: a month, a year
    };
    double x = 0.0;
    QString label;
    Kind kind = Kind::Minor;
};

/// @brief A long gap without events, drawn short
struct TimelineGapMark {
    double x = 0.0;
    double width = 0.0;
    QString length;  ///< "≈ 3 mies."
};

/// @brief The axis of a timeline as it is drawn
///
/// +--------+---------------------------------------------+
/// | dates  | kwiecień 1931          maj 1931             |  AXIS_HEIGHT
/// |        | 14 15 16 17 ...                             |
/// +--------+---------------------------------------------+
/// | Not on the axis (2): [Selim rysuje mapę] [...]       |  trayHeight()
/// +--------+---------------------------------------------+
/// | o Wyprawa | [card] [card]                            |  the lanes
/// +--------+---------------------------------------------+
class TimelineScene {
    Q_DECLARE_TR_FUNCTIONS(TimelineScene)

public:
    static constexpr double HEAD_WIDTH = 132.0;  ///< The heads of the lanes
    static constexpr double AXIS_HEIGHT = 40.0;  ///< The axis above the lanes
    static constexpr double ORIGIN = 30.0;       ///< Room before the axis
    static constexpr double CARD_WIDTH = 160.0;
    static constexpr double TRAY_CARD_WIDTH = 170.0;

    /// @param font The font of the program: the cards take it
    TimelineScene(const Timeline& timeline, const TimelineIndex& index, const TimelineLook& look,
                  const QFont& font);

    /// @brief Lay out the axis; the tray of the events without a time is @p trayWidth wide
    void layout(double trayWidth);

    [[nodiscard]] const Timeline& timeline() const { return *m_timeline; }
    [[nodiscard]] const TimelineIndex& index() const { return *m_index; }
    [[nodiscard]] const TimelineLook& look() const { return m_look; }
    [[nodiscard]] const QFont& font() const { return m_font; }
    [[nodiscard]] const QFont& titleFont() const { return m_titleFont; }
    [[nodiscard]] const QFont& smallFont() const { return m_smallFont; }

    /// @brief Whether the axis is the order of the story
    [[nodiscard]] bool ordered() const;
    [[nodiscard]] bool cards() const;

    /// @brief The width of the axis, from its start
    [[nodiscard]] double width() const { return m_width; }
    /// @brief The height of all lanes
    [[nodiscard]] double lanesHeight() const { return m_lanesHeight; }
    /// @brief The height of the tray; 0: no events without a time
    [[nodiscard]] double trayHeight() const { return m_trayHeight; }
    /// @brief Where the lanes start in the window: under the axis and the tray
    [[nodiscard]] double lanesTop() const { return AXIS_HEIGHT + m_trayHeight; }

    [[nodiscard]] const QList<TimelineLane>& lanes() const { return m_lanes; }
    [[nodiscard]] const QList<TimelineItem>& items() const { return m_items; }
    [[nodiscard]] const QList<TimelineItem>& trayItems() const { return m_tray; }
    [[nodiscard]] const QList<TimelineTick>& ticks() const { return m_ticks; }
    [[nodiscard]] const QList<TimelineGapMark>& gaps() const { return m_gaps; }
    /// @brief The text of the tray before its events
    [[nodiscard]] const QString& trayText() const { return m_trayText; }
    [[nodiscard]] const QRectF& trayTextRect() const { return m_trayTextRect; }

    /// @brief The events on the axis, in its order: of time, or of the story
    [[nodiscard]] const QList<const TimelineEvent*>& onAxis() const { return m_onAxis; }

    /// @brief Where day @p day is, from the start of the axis
    [[nodiscard]] double xOfDay(double day) const;
    /// @brief The day at @p x from the start of the axis
    [[nodiscard]] double dayAt(double x) const;
    /// @brief The place in the order of the story (from 0) at @p x
    [[nodiscard]] int slotAt(double x) const;
    /// @brief Where place @p slot (from 0) of the order of the story is
    [[nodiscard]] double xOfSlot(int slot) const;
    /// @brief The pixels per day; 0 in the order of the story
    [[nodiscard]] double pixelsPerDay() const;

    /// @brief The event at @p point of the lanes; nullptr: none
    [[nodiscard]] const TimelineItem* itemAt(const QPointF& point) const;
    /// @brief The event at @p point of the tray; nullptr: none
    [[nodiscard]] const TimelineItem* trayItemAt(const QPointF& point) const;
    /// @brief The lane at @p y of the lanes; nullptr: none
    [[nodiscard]] const TimelineLane* laneAt(double y) const;
    [[nodiscard]] const TimelineLane* lane(const QString& key) const;
    /// @brief Event @p id in lane @p lane (empty: its first lane), on the axis or in the tray
    [[nodiscard]] const TimelineItem* itemOf(const QString& id, const QString& lane) const;
    /// @brief Where event @p id is on the axis: each of its lanes
    [[nodiscard]] QList<const TimelineItem*> itemsOf(const QString& id) const;

    /// @brief The lanes of @p event, as the lanes of the timeline go
    [[nodiscard]] QStringList laneKeysOf(const TimelineEvent& event) const;
    /// @brief The color of @p event: its own, else its thread's
    [[nodiscard]] QColor colorOf(const TimelineEvent& event) const;
    /// @brief Whether @p event is known only roughly: a month, a year, "about"
    [[nodiscard]] static bool rough(const TimelineEvent& event);

    /// @brief The lines of a card
    [[nodiscard]] QString whenOf(const TimelineEvent& event) const;
    [[nodiscard]] QString detailsOf(const TimelineEvent& event) const;
    [[nodiscard]] double cardHeight() const { return m_cardHeight; }
    [[nodiscard]] double titleHeight() const { return m_titleHeight; }
    [[nodiscard]] double smallHeight() const { return m_smallHeight; }

private:
    void layoutLanes();
    void layoutTicks();
    void layoutTray(double trayWidth);
    [[nodiscard]] double widthOf(const TimelineEvent& event, double span) const;
    [[nodiscard]] double xOf(const TimelineEvent& event) const;
    [[nodiscard]] double spanOf(const TimelineEvent& event) const;

    const Timeline* m_timeline;
    const TimelineIndex* m_index;
    TimelineLook m_look;
    QFont m_font;
    QFont m_titleFont;
    QFont m_smallFont;
    double m_titleHeight = 0.0;
    double m_smallHeight = 0.0;
    double m_cardHeight = 0.0;
    std::optional<TimeAxis> m_axis;
    double m_step = 0.0;  ///< In the order of the story: between the places
    double m_width = 0.0;
    double m_lanesHeight = 0.0;
    double m_trayHeight = 0.0;
    QString m_trayText;
    QRectF m_trayTextRect;
    QList<const TimelineEvent*> m_onAxis;
    QList<TimelineLane> m_lanes;
    QList<TimelineItem> m_items;
    QList<TimelineItem> m_tray;
    QList<TimelineTick> m_ticks;
    QList<TimelineGapMark> m_gaps;
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_SCENE_H
