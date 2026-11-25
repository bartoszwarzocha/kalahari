// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_dialogs.h
/// @brief The windows of a timeline: "Event", with all the fields of an event, and "Timeline",
/// with day one of the story and the threads

#ifndef KALAHARI_TIMELINE_TIMELINE_DIALOGS_H
#define KALAHARI_TIMELINE_TIMELINE_DIALOGS_H

#include <kalahari/gui/dialogs/kalahari_dialog.h>
#include <kalahari/timeline/timeline.h>
#include <kalahari/timeline/timeline_index.h>
#include <kalahari/timeline/timeline_look.h>

#include <QList>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTableWidget;

namespace kalahari::timeline {

/// @brief The window "Event": the title, when, the thread, the characters, the place, the
/// chapters that tell it, the description and the color
///
/// Under "When" it says at once what the program reads in it ("Recognized: 16 kwietnia 1931,
/// wieczorem – Dzień 3, wieczorem"); a time it does not read stays as the writer typed it, and
/// the event waits in the tray of the axis.
class TimelineEventDialog : public gui::dialogs::KalahariDialog {
    Q_OBJECT

public:
    TimelineEventDialog(const Timeline& timeline, const TimelineEvent& event,
                        const TimelineIndex& index, const TimelineLook& look,
                        QWidget* parent = nullptr);

    /// @brief The event as the window leaves it
    [[nodiscard]] TimelineEvent result() const;

    /// @brief Whether the field "When" holds a time the program reads
    [[nodiscard]] bool whenRecognized() const;

private:
    void updateNote();

    const Timeline& m_timeline;
    TimelineEvent m_event;
    const TimelineIndex& m_index;
    TimelineLook m_look;
    QLineEdit* m_title = nullptr;
    QLineEdit* m_when = nullptr;
    QLabel* m_note = nullptr;
    QComboBox* m_thread = nullptr;
    QLineEdit* m_characters = nullptr;
    QLineEdit* m_place = nullptr;
    QListWidget* m_chapters = nullptr;
    QPlainTextEdit* m_description = nullptr;
    QComboBox* m_color = nullptr;
};

/// @brief The window "Timeline": day one of the story, from which the days of the story count,
/// and the threads with their colors
class TimelinePropertiesDialog : public gui::dialogs::KalahariDialog {
    Q_OBJECT

public:
    TimelinePropertiesDialog(const Timeline& timeline, const TimelineLook& look,
                             QWidget* parent = nullptr);

    /// @brief Julian day of day one, as the window leaves it
    [[nodiscard]] qint64 dayOne() const { return m_dayOne; }

    /// @brief The threads, in their order; a new thread has no id
    [[nodiscard]] QList<TimelineThread> threads() const;

private:
    void addRow(const TimelineThread& thread);
    void updateNote();

    const Timeline& m_timeline;
    TimelineLook m_look;
    qint64 m_dayOne = 0;
    QLineEdit* m_day = nullptr;
    QLabel* m_note = nullptr;
    QTableWidget* m_threads = nullptr;
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_DIALOGS_H
