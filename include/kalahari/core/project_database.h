// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file project_database.h
/// @brief SQLite database for project metadata and settings
///
/// ProjectDatabase provides CRUD operations for all project data stored in SQLite.
/// Content files (.kchapter) remain as files - only metadata is in the database.
///
/// OpenSpec #00041: SQLite Project Database

#ifndef KALAHARI_CORE_PROJECT_DATABASE_H
#define KALAHARI_CORE_PROJECT_DATABASE_H

#include "kalahari/core/database_types.h"
#include "kalahari/core/writing_activity.h"

#include <QString>
#include <QSqlDatabase>
#include <QList>
#include <QVariant>
#include <QDateTime>
#include <functional>

namespace kalahari::core {

/// @brief SQLite database wrapper for project data
///
/// Manages project.db file with:
/// - Book metadata (title, author, etc.)
/// - Chapter metadata (not content)
/// - Session statistics
/// - Styles and settings
class ProjectDatabase {
public:
    ProjectDatabase();
    ~ProjectDatabase();

    // Non-copyable
    ProjectDatabase(const ProjectDatabase&) = delete;
    ProjectDatabase& operator=(const ProjectDatabase&) = delete;

    // =========================================================================
    // Connection Management
    // =========================================================================

    /// @brief Open database for project
    /// @param projectPath Path to project directory
    /// @return true on success
    bool open(const QString& projectPath);

    /// @brief Close database connection
    void close();

    /// @brief Check if database is open
    [[nodiscard]] bool isOpen() const;

    /// @brief Write what the database's write-ahead log holds to the database file, so that a
    /// copy of project.db alone has everything
    /// @return false when the database is not open or the log cannot be written
    bool checkpoint();

    /// @brief Get project path
    [[nodiscard]] QString projectPath() const { return m_projectPath; }

    // =========================================================================
    // Book Metadata
    // =========================================================================

    [[nodiscard]] QString getMetadata(const QString& key, const QString& defaultValue = {}) const;
    void setMetadata(const QString& key, const QString& value);

    // =========================================================================
    // Chapters
    // =========================================================================

    [[nodiscard]] QList<ChapterInfo> getAllChapters() const;
    [[nodiscard]] ChapterInfo getChapter(const QString& id) const;
    void saveChapter(const ChapterInfo& chapter);
    void deleteChapter(const QString& id);
    void addChapterHistory(const QString& chapterId, const QString& action,
                          const QString& author = {});

    // =========================================================================
    // Session Statistics
    // =========================================================================

    void recordSessionStats(const SessionStats& stats);
    [[nodiscard]] QList<SessionStats> getStatsBetween(const QDateTime& from,
                                                      const QDateTime& to) const;
    [[nodiscard]] AggregatedStats getAggregatedStats() const;

    // =========================================================================
    // Record of writing (the statistics and goals of writing)
    // =========================================================================

    /// @brief Adds what was written in this book to its record of writing
    bool addWritingActivity(const QList<WritingActivity>& activity);

    /// @brief The record of writing of this book on the days of the calendar from @p from to
    /// @p to (both included)
    [[nodiscard]] QList<WritingActivity> writingActivity(const QDate& from, const QDate& to) const;

    // =========================================================================
    // Styles
    // =========================================================================

    [[nodiscard]] QList<ParagraphStyle> getParagraphStyles() const;
    void saveParagraphStyle(const ParagraphStyle& style);
    void deleteParagraphStyle(const QString& id);

    [[nodiscard]] QList<CharacterStyle> getCharacterStyles() const;
    void saveCharacterStyle(const CharacterStyle& style);
    void deleteCharacterStyle(const QString& id);

    // =========================================================================
    // Settings
    // =========================================================================

    [[nodiscard]] QVariant getSetting(const QString& key, const QVariant& defaultValue = {}) const;
    void setSetting(const QString& key, const QVariant& value);

    // =========================================================================
    // Utility
    // =========================================================================

    bool executeInTransaction(std::function<bool()> operation);
    [[nodiscard]] QString lastError() const { return m_lastError; }

private:
    bool execSql(const QString& sql);
    void configurePragmas();

    QSqlDatabase m_db;
    QString m_projectPath;
    QString m_connectionName;
    QString m_lastError;
};

} // namespace kalahari::core

#endif  // KALAHARI_CORE_PROJECT_DATABASE_H
