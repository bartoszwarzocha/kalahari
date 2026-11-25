// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_book_view.h
/// @brief The time and the book: the chapters top to bottom, the time of their events left to
/// right, so that flashbacks and jumps in time show

#ifndef KALAHARI_TIMELINE_TIMELINE_BOOK_VIEW_H
#define KALAHARI_TIMELINE_TIMELINE_BOOK_VIEW_H

#include <kalahari/timeline/time_axis.h>
#include <kalahari/timeline/timeline_index.h>
#include <kalahari/timeline/timeline_look.h>

#include <QAbstractScrollArea>
#include <QList>
#include <QRectF>
#include <QSet>
#include <QString>

#include <optional>

namespace kalahari::timeline {

class TimelineDocument;

/// @brief The time and the book
///
/// +-------------------+----------------------------------------------+
/// |                   | 14 kwietnia 1931       ≈ 11 mies.   marzec   |
/// | Prolog            |  o                                           |
/// | Rozdział 1        |      o--                                     |
/// | Rozdział 5        |  (o)        o                                |  (o): a flashback
/// +-------------------+----------------------------------------------+
/// The book does not tell yet (2): [Ktoś zasypuje źródło] [...]
///
/// A dashed line joins the chapters in the order of reading; a ring marks a flashback: an
/// event earlier than what happened in the chapter before. A click selects an event, a double
/// click opens the window "Event"; Insert asks for a new event, Delete for the deletion of the
/// event selected (the tab adds it in the window "Event" and deletes it).
class TimelineBookView : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit TimelineBookView(TimelineDocument& document, QWidget* parent = nullptr);

    void setIndex(const TimelineIndex* index);
    void setLook(const TimelineLook& look);
    void setHighlight(const QString& filter);
    void refresh();

    /// @brief An event of a chapter, where it is drawn
    struct Mark {
        QString id;
        int row = 0;
        double from = 0.0;  ///< In the coordinates of the content
        double to = 0.0;    ///< Its end; from for a moment
        double y = 0.0;
        bool flashback = false;
    };

    /// @brief An event outside the drawing: not in the book, or without a time
    struct Chip {
        QString id;
        QRectF rect;
    };

    [[nodiscard]] const QList<Mark>& marks() const { return m_marks; }
    [[nodiscard]] const QList<Chip>& chips() const { return m_chips; }

signals:
    void told(const QString& message, bool undoable);
    void eventRequested(const QString& id);
    void chapterRequested(const QString& id);
    /// @brief Insert: a new event
    void addRequested();
    /// @brief Delete: the event selected deleted
    void deleteRequested();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void layout();
    void layoutChips(double y);
    [[nodiscard]] QString idAt(const QPointF& point) const;
    [[nodiscard]] QRectF markRect(const Mark& mark) const;
    [[nodiscard]] QColor colorOf(const QString& id) const;
    void selectAcross(int direction);
    void selectDown(int direction);

    TimelineDocument& m_document;
    const TimelineIndex* m_index;
    TimelineLook m_look;
    QString m_highlight;
    std::optional<TimeAxis> m_axis;
    QList<Mark> m_marks;
    QList<Chip> m_chips;
    QList<int> m_mainOfRow;     ///< The index in m_marks of the main event of each row; -1
    QString m_unbookedText;     ///< "The book does not tell yet (2):"
    QString m_untimedText;      ///< "In the book, but without a time (1):"
    QString m_legend;           ///< What the drawing shows
    QRectF m_unbookedRect;
    QRectF m_untimedRect;
    QRectF m_legendRect;
    double m_contentWidth = 0.0;
    double m_contentHeight = 0.0;
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_BOOK_VIEW_H
