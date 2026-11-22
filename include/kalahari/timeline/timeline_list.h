// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_list.h
/// @brief A timeline as a table: when, the event, its thread, characters, place and chapters

#ifndef KALAHARI_TIMELINE_TIMELINE_LIST_H
#define KALAHARI_TIMELINE_TIMELINE_LIST_H

#include <kalahari/timeline/timeline_index.h>
#include <kalahari/timeline/timeline_look.h>

#include <QString>
#include <QTableWidget>

namespace kalahari::timeline {

class TimelineDocument;

/// @brief The events of a timeline in a table, in the order of time; the events without a
/// time after them
///
/// A cell changes with F2, a double click or typing; Tab goes to the next cell. Insert adds an
/// event after the selected one, Enter opens the window "Event", Delete deletes the event.
class TimelineList : public QTableWidget {
    Q_OBJECT

public:
    /// @brief The columns
    enum Column { When, Title, Thread, Characters, Place, Book, COLUMNS };

    static constexpr int ID_ROLE = Qt::UserRole + 1;

    explicit TimelineList(TimelineDocument& document, QWidget* parent = nullptr);

    [[nodiscard]] TimelineDocument& document() { return m_document; }

    void setIndex(const TimelineIndex* index);
    void setLook(const TimelineLook& look);
    void setHighlight(const QString& filter);
    /// @brief Fill the table again, once the events are handled (not while a cell changes)
    void refresh();

    /// @brief The event of the current row; empty: none (a gap)
    [[nodiscard]] QString currentId() const;

    /// @brief The text of column @p column of event @p id in the field that changes it
    [[nodiscard]] QString editText(const QString& id, int column) const;

    /// @brief Change column @p column of event @p id to @p value, as a cell does
    void applyEdit(const QString& id, int column, const QString& value);

    void addAfterSelected();
    void removeSelected();

    /// @brief What the list says while the timeline has no events: how to add the first
    [[nodiscard]] static QString emptyHint();

signals:
    void told(const QString& message, bool undoable);
    void eventRequested(const QString& id);
    void chapterRequested(const QString& id);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint) override;

private:
    void rebuild();
    void sizeColumns();
    void showSelected();
    [[nodiscard]] QString cellText(const TimelineEvent& event, int column) const;

    TimelineDocument& m_document;
    const TimelineIndex* m_index;
    TimelineLook m_look;
    QString m_highlight;
    bool m_rebuildPending = false;
    bool m_filling = false;
    QString m_newId;      ///< A new event whose title is being typed
    QString m_editAfter;  ///< Start typing the title of this event after the next rebuild
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_LIST_H
