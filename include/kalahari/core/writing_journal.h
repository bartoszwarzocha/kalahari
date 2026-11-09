/// @file writing_journal.h
/// @brief The places that keep the record of writing: the table of a database, and the
/// writer's journal of all the books
///
/// A project keeps the record of its own book in its project.db (ProjectDatabase), and the
/// writer's journal keeps the record of all the books in writing_journal.db next to the
/// settings, for the goals of the writer that are not the goals of one book. Both keep it in
/// the same table, by the functions of writing_activity_store.

#pragma once

#include "kalahari/core/writing_activity.h"

#include <QDate>
#include <QList>
#include <QSqlDatabase>
#include <QString>

namespace kalahari::core {

namespace writing_activity_store {

/// @brief Makes the table of the record of writing when the database does not have it
/// @return false when it cannot be made
bool createTable(QSqlDatabase& db);

/// @brief Adds @p activity to the record: an entry of a slot, book and text already there
/// grows by its counts
/// @return false when it cannot be written; then nothing of it is written
bool add(QSqlDatabase& db, const QList<WritingActivity>& activity);

/// @brief The record of the days of the calendar from @p from to @p to (both included), of all
/// the books, or of the book @p bookId when it is not empty, by day and slot
QList<WritingActivity> read(const QSqlDatabase& db, const QDate& from, const QDate& to,
                            const QString& bookId = {});

}  // namespace writing_activity_store

/// @brief The writer's journal: the record of writing of all the books
class WritingJournal {
public:
    WritingJournal() = default;
    ~WritingJournal();

    WritingJournal(const WritingJournal&) = delete;
    WritingJournal& operator=(const WritingJournal&) = delete;

    /// @brief The journal of the writer: writing_journal.db in the folder of the settings
    static QString defaultPath();

    /// @brief Opens the journal in the file @p path, made when it does not exist
    /// @return false when it cannot be opened (lastError() says why)
    bool open(const QString& path = defaultPath());

    void close();
    bool isOpen() const;

    /// @brief Adds what was written to the journal
    bool add(const QList<WritingActivity>& activity);

    /// @brief The record of the days of the calendar from @p from to @p to, of all the books or
    /// of one
    QList<WritingActivity> activity(const QDate& from, const QDate& to,
                                    const QString& bookId = {}) const;

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;
};

}  // namespace kalahari::core
