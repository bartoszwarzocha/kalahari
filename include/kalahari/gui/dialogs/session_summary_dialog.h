/// @file session_summary_dialog.h
/// @brief The summary of the session of writing, when the book or the program closes: the
/// words of today, a graph of the day, the time of writing, the goal and this session

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTime>

#include <utility>

class QCheckBox;

namespace kalahari::gui {

class WritingStatsModel;

namespace dialogs {

/// @brief What the summary of a session says
struct SessionSummary {
    Q_DECLARE_TR_FUNCTIONS(SessionSummary)

public:
    /// @brief A pause this long or longer ends a session
    static constexpr int SESSION_GAP_MINUTES = 30;

    QDate day;                ///< The writing day
    QString bookTitle;        ///< The open book; empty: none
    int words = 0;            ///< The words of today that count
    int written = 0;          ///< Written today
    int deleted = 0;          ///< Deleted today
    qint64 activeMs = 0;      ///< The time of writing today
    int goal = 0;             ///< The daily goal; 0: none
    int streak = 0;           ///< The days in a row
    int sessions = 0;         ///< The sessions of today
    QTime sessionFrom;        ///< This session: its start, by the clock
    QTime sessionTo;          ///< Its end
    int sessionWords = 0;     ///< Its words that count
    int bookWords = 0;        ///< The words of the book now
    int bookTarget = 0;       ///< Its length; 0: none
    QList<double> pieces;     ///< The words of each piece of the day shown in the graph
    int pieceMinutes = 10;    ///< The minutes of a piece
    QTime graphFrom;          ///< The first hour of the graph, by the clock
    QTime graphTo;            ///< The last
    bool writtenSince = false;  ///< Something was written since the session started

    /// @brief The summary of today in @p model; the session is the last one of the day
    /// @param since The session started then: something written since makes writtenSince
    /// @param title The title of the open book
    static SessionSummary of(const WritingStatsModel& model, const QDateTime& since,
                             const QString& title);

    /// @brief Its lines as text, for Copy
    QString text() const;

    /// @brief Its heading: "Today: 1,240 words"
    QString heading() const;

    /// @brief The line under the heading: the book, the day and the sessions
    QString subheading() const;

    /// @brief Its details: a name and a value each
    QList<std::pair<QString, QString>> details() const;
};

/// @brief The summary of the session, with Copy and OK, and a box that turns it off
///
/// The box "Show the summary when the book closes" changes the setting stats.sessionSummary at
/// once.
class SessionSummaryDialog : public KalahariDialog {
    Q_OBJECT

public:
    explicit SessionSummaryDialog(const SessionSummary& summary, QWidget* parent = nullptr);

    /// @brief The summary it shows
    const SessionSummary& summary() const { return m_summary; }

    /// @brief The box of the setting
    QCheckBox* showAgainBox() const { return m_showAgain; }

private:
    SessionSummary m_summary;
    QCheckBox* m_showAgain = nullptr;
};

}  // namespace dialogs

}  // namespace kalahari::gui
