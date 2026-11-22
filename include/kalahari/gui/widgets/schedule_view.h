// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file schedule_view.h
/// @brief The chart of the schedule of a book: its texts on a time axis, a bar of writing and
/// a hatched bar of revisions for each, the days they were written on, today, the deadlines
/// and the forecast of the end of the book
///
/// The list of the texts stays on the left and the scale on top while the chart scrolls. The
/// bars move with the mouse and with the keyboard; each change is one planEdited().

#ifndef KALAHARI_GUI_WIDGETS_SCHEDULE_VIEW_H
#define KALAHARI_GUI_WIDGETS_SCHEDULE_VIEW_H

#include <kalahari/core/writing_goals.h>
#include <kalahari/core/writing_plan.h>
#include <kalahari/core/writing_statistics.h>
#include <kalahari/gui/widgets/stats_painter.h>

#include <QAbstractScrollArea>
#include <QDate>
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>

#include <array>
#include <utility>

namespace kalahari::gui {

/// @brief A row of the schedule: the book, a part of it or a text
struct ScheduleRow {
    enum class Kind { Book, Part, Text };

    Kind kind = Kind::Text;
    QString id;        ///< The element; empty for the book
    QString title;     ///< Its title
    QString kindName;  ///< Of a text the name of its kind ("Chapter", "Prologue")
    QString iconId;    ///< ArtProvider id of its icon
    QString status;    ///< Of a text: "draft", "revision" or "final"
    int words = 0;     ///< Its words; of a part or the book those of its texts
    QString partId;    ///< Of a text: its part; empty outside the parts

    bool operator==(const ScheduleRow&) const = default;
};

/// @brief What the schedule shows besides the plan
struct ScheduleFacts {
    QDate today;                              ///< The writing day of now
    core::WritingRules rules;                 ///< The writing days and the days off
    int bookTargetWords = 0;                  ///< The length of the book; 0: none
    QDate bookDeadline;                       ///< The deadline of the book; invalid: none
    QDate forecast;                           ///< The day the book will be written; invalid: none
    QList<core::WritingDeadline> deadlines;   ///< The deadlines of the parts and texts
    QHash<QString, QMap<QDate, int>> written; ///< The words written on each day, by text
};

/// @brief The chart of the schedule (the tab Writing Schedule)
class ScheduleView : public QAbstractScrollArea {
    Q_OBJECT

public:
    /// @brief The scale of the time axis
    enum class Scale { Days, Weeks, Months };

    /// @brief A bar of a text
    enum class Bar { Writing, Revision };

    explicit ScheduleView(QWidget* parent = nullptr);

    /// @brief The book, its parts and its texts, in the order of the book
    void setRows(const QList<ScheduleRow>& rows);
    [[nodiscard]] const QList<ScheduleRow>& rows() const { return m_rows; }

    /// @brief The plan it shows
    void setPlan(const core::WritingPlan& plan);
    [[nodiscard]] const core::WritingPlan& plan() const { return m_plan; }

    /// @brief Today, the deadlines, the forecast and the days written
    void setFacts(const ScheduleFacts& facts);
    [[nodiscard]] const ScheduleFacts& facts() const { return m_facts; }

    /// @brief The scale of the axis; the day in the middle stays in the middle
    void setScale(Scale scale);
    [[nodiscard]] Scale scale() const { return m_scale; }

    /// @brief Whether the days off are shaded
    void setDaysOffShown(bool shown);
    [[nodiscard]] bool daysOffShown() const { return m_daysOffShown; }

    /// @brief The plan does not change: the book is open read-only
    void setReadOnly(bool readOnly);
    [[nodiscard]] bool isReadOnly() const { return m_readOnly; }

    /// @brief The chosen row; -1 when there are none
    [[nodiscard]] int currentRow() const { return m_current; }
    void setCurrentRow(int row);

    /// @brief The text of the chosen row; empty when it is the book or a part
    [[nodiscard]] QString currentText() const;

    /// @brief The row of @p id (an empty id: the book); -1 when there is none
    [[nodiscard]] int rowOf(const QString& id) const;

    /// @brief Scroll so that @p day is near the left of the chart
    void showDate(const QDate& day);

    /// @brief What row @p row shows, in words: the line under the chart and the screen readers
    [[nodiscard]] QString describe(int row) const;

    /// @brief The colors of the theme again
    void refreshColors();

    /// @name The geometry, in the coordinates of the viewport (for the tests too)
    /// @{
    [[nodiscard]] QDate firstDay() const { return m_first; }  ///< The first day of the axis
    [[nodiscard]] QDate lastDay() const { return m_last; }    ///< The last day of the axis
    [[nodiscard]] int dayWidth() const;
    [[nodiscard]] int leftWidth() const { return m_leftWidth; }
    [[nodiscard]] int headerHeight() const;
    [[nodiscard]] int rowHeight() const;
    /// @brief The left edge of @p day on the chart
    [[nodiscard]] int xOf(const QDate& day) const;
    /// @brief The day at @p x on the chart
    [[nodiscard]] QDate dateAt(int x) const;
    [[nodiscard]] QRect rowRect(int row) const;
    /// @brief The bar @p bar of the text of row @p row; empty without one
    [[nodiscard]] QRect barRect(int row, Bar bar) const;
    /// @}

    /// @brief The bar is past its end and its work is not done: writing with fewer words than
    /// planned and not final, revisions of a text not final
    [[nodiscard]] bool isLate(int row, Bar bar) const;

    QSize sizeHint() const override;

signals:
    /// @brief The writer changed the plan: the whole new plan
    void planEdited(const core::WritingPlan& plan);

    /// @brief Enter or a double click on a text: its plan in its window
    void editRequested(const QString& elementId);

    /// @brief Another row was chosen
    void currentChanged(int row);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    /// @brief What a press of the mouse took hold of
    enum class Grip { None, Move, End, Create };

    struct Drag {
        Grip grip = Grip::None;
        Bar bar = Bar::Writing;
        int row = -1;
        QDate pressDay;
        core::TextPlan before;  ///< The plan of the text when the press came
        bool moved = false;
    };

    /// @brief The first and the last day of the axis and the scroll bars
    void updateAxis();
    void updateScrollBars();
    void updateLeftWidth();

    /// @brief The plan of @p elementId, an empty one with its id when it has none
    [[nodiscard]] core::TextPlan planOf(const QString& elementId) const;

    /// @brief Keep @p plan of a text and say so (planEdited())
    void changePlan(const core::TextPlan& plan);

    /// @brief The plan of row @p row while it is dragged, or as it is
    void applyDrag(const QDate& day);

    /// @brief The first and the last planned day of the texts of a part (@p partId) or of the
    /// book (empty @p partId with @p wholeBook)
    [[nodiscard]] std::pair<QDate, QDate> spanOf(const QString& partId, bool wholeBook) const;

    /// @brief The row under @p y; -1 when none
    [[nodiscard]] int rowAt(int y) const;

    /// @brief The deadlines shown in row @p row
    [[nodiscard]] QList<core::WritingDeadline> deadlinesOf(int row) const;

    /// @brief Paint parts of the chart
    void paintLeft(QPainter& painter, int row) const;
    void paintLane(QPainter& painter, int row) const;
    void paintTextBars(QPainter& painter, int row) const;
    void paintDeadlines(QPainter& painter, int row) const;
    void paintDaysOff(QPainter& painter, const QRect& body) const;
    void paintHeader(QPainter& painter) const;
    void paintLines(QPainter& painter, const QRect& body) const;

    /// @brief Scroll so that @p day is @p share of the width of the chart from its left
    void scrollToDay(const QDate& day, double share);

    /// @brief The day at the left edge of the chart now
    [[nodiscard]] QDate leftDay() const;

    /// @brief The texts of the plan and the words of the texts of a part or of the book
    [[nodiscard]] std::pair<int, int> wordsAndTargetOf(int row) const;

    /// @brief A day as the chart writes it: short ("16 Nov") or with the year
    [[nodiscard]] static QString dayText(const QDate& day, bool withYear = false);

    QList<ScheduleRow> m_rows;
    core::WritingPlan m_plan;
    ScheduleFacts m_facts;
    Scale m_scale = Scale::Weeks;
    bool m_daysOffShown = true;
    bool m_readOnly = false;
    int m_current = -1;
    QDate m_first;
    QDate m_last;
    int m_leftWidth = 0;
    std::array<int, 4> m_columns{};  ///< The widths of the columns on the left
    bool m_scrolledOnce = false;      ///< Scrolled to today once it was shown
    Drag m_drag;
    QPoint m_pressPos;
    StatsColors m_colors;
    QHash<QString, QColor> m_statusColors;  ///< By the state of a text
    QColor m_muted;                          ///< Texts less important
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_SCHEDULE_VIEW_H
