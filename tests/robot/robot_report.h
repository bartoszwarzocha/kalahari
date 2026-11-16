// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file robot_report.h
/// @brief What the robot did and noticed: a record for each step, written as the step goes, so
/// that a crash or a hang still names its step, and the pages a person reads (index.html with
/// the screenshots, summary.md for the summary of a run on GitHub)

#pragma once

#include <QFile>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <mutex>

class QPixmap;

namespace kalahari::robot {

/// @brief How much a finding matters
enum class Severity {
    Error,    ///< Something is broken: a crash, a hang, an error in the log, a system window
    Warning,  ///< Something is wrong or slow, and a person decides
    Note      ///< Worth knowing, not wrong in itself
};

/// @brief Something the robot noticed in a step
struct Finding {
    QString check;                          ///< The check, a stable id (tests/robot/README.md)
    Severity severity = Severity::Warning;  ///< How much it matters
    QString message;                        ///< What happened, in a sentence
};

/// @brief A window a step showed: a dialog, a menu that popped up, a window of its own
struct ShownWindow {
    QString className;         ///< Its class, as Qt names it
    QString title;             ///< Its title
    QString texts;             ///< The texts of its labels, in one line
    QString shot;              ///< Its screenshot, relative to the report folder; empty: none
    bool own = false;          ///< A window of the program, not one of Qt or of the system
    bool modal = false;        ///< The program waited for it to close
    bool cancellable = false;  ///< It had a button that closes it without doing anything
    qint64 shownAfterMs = -1;  ///< When it showed, from the start of the step; -1: not seen
    QString closedBy;          ///< What closed it ("Cancel", "Escape", "close()", ...)
    bool stayed = false;       ///< Nothing closed it
};

/// @brief What one step did and what the robot noticed
struct StepRecord {
    int index = 0;               ///< Its number in the situation, from 1; 0: the setup
    QString command;             ///< The id of the command ("edit.copy")
    QString label;               ///< The command's text
    QString menuPath;            ///< Its place in the menus ("EDIT/Copy"); empty: none
    QString keys;                ///< Its keyboard shortcut; empty: none
    QString outcome;             ///< "ran", "greyed", "in-preparation", "skipped", "setup"
    QString reason;              ///< Why it is greyed or skipped
    qint64 durationMs = 0;       ///< From its start until the program was idle again
    qint64 stallMs = 0;          ///< The longest time the program did not respond in it
    QList<ShownWindow> windows;  ///< The windows it showed
    QStringList log;             ///< The warnings and errors of the program's log
    QStringList qtMessages;      ///< The warnings of Qt
    QStringList opened;          ///< What the program asked the system to open
    QStringList settings;        ///< The settings it changed
    QString shot;                ///< The window after the step; empty: none
    QList<Finding> findings;     ///< What the robot noticed

    /// @brief Add a finding
    void add(const QString& check, Severity severity, const QString& message) {
        findings.append({check, severity, message});
    }
};

/// @brief The name of @p severity in the records ("error", "warning", "note")
QString severityName(Severity severity);

/// @brief The record of a step, as the report keeps it
QJsonObject toJson(const StepRecord& step);

/// @brief A step from its record
StepRecord stepFromJson(const QJsonObject& record);

/// @brief The records of one situation of a run: its folder in the report folder, with the file
/// of the records (steps.jsonl, a JSON object a line) and the screenshots (shots/)
class Report {
public:
    /// @brief Start the records of a situation
    /// @param folder The report folder of the run, shared by its situations
    /// @param situation The id of the situation: the name of its folder
    /// @param description What the situation is, for a person
    Report(const QString& folder, const QString& situation, const QString& description);

    /// @brief Ends the file of the records
    ~Report();

    Report(const Report&) = delete;
    Report& operator=(const Report&) = delete;
    Report(Report&&) = delete;
    Report& operator=(Report&&) = delete;

    /// @brief Whether the records can be written
    [[nodiscard]] bool isOpen() const { return m_file.isOpen(); }

    /// @brief The folder of the situation
    [[nodiscard]] QString folder() const { return m_folder; }

    /// @brief The step gets to @p phase ("prepare", "run", "check"); the record is on the disk
    /// at once, so that a crash names the step and its phase
    void phase(const StepRecord& step, const QString& phase);

    /// @brief The step ended
    void end(const StepRecord& step);

    /// @brief The robot gives up on step @p index: the program does not respond. Called from
    /// any thread (the watchdog's).
    void hang(int index, const QString& command, const QString& message);

    /// @brief Save @p shot as shots/@p name.png
    /// @return Its path, relative to the report folder of the run; empty when it was not saved
    QString saveShot(const QPixmap& shot, const QString& name);

    /// @brief The situation ended after @p steps steps in @p ms milliseconds
    void finish(int steps, qint64 ms);

    /// @brief Write index.html and summary.md of the report folder @p folder: every situation
    /// found there, also one whose run crashed
    /// @return false when a page could not be written
    static bool writePages(const QString& folder);

private:
    /// @brief Append @p record to the file, a line
    void write(const QJsonObject& record);

    QString m_folder;       ///< The folder of the situation
    QString m_situation;    ///< The id of the situation
    QFile m_file;           ///< steps.jsonl
    std::timed_mutex m_mutex;  ///< The main thread and the watchdog write to m_file
};

}  // namespace kalahari::robot
