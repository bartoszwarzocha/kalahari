/// @file project_database.h
/// @brief SQLite database for project metadata and settings
///
/// ProjectDatabase provides CRUD operations for all project data stored in SQLite.
/// Content files (.kchapter) remain as files - only metadata is in the database.
///
/// OpenSpec #00041: SQLite Project Database

#pragma once

#include "kalahari/core/database_types.h"

#include <QString>
#include <QSqlDatabase>
#include <QList>
#include <QVariant>
#include <QDateTime>
#include <functional>

namespace kalahari {
namespace core {

/// @brief SQLite database wrapper for project data
///
/// Manages project.db file with:
/// - Book metadata (title, author, etc.)
/// - Chapter metadata (not content)
/// - Character/Location/Item libraries
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
    bool isOpen() const;

    /// @brief Get project path
    QString projectPath() const { return m_projectPath; }

    // =========================================================================
    // Book Metadata
    // =========================================================================

    QString getMetadata(const QString& key, const QString& defaultValue = {}) const;
    void setMetadata(const QString& key, const QString& value);

    // =========================================================================
    // Chapters
    // =========================================================================

    QList<ChapterInfo> getAllChapters() const;
    ChapterInfo getChapter(const QString& id) const;
    void saveChapter(const ChapterInfo& chapter);
    void deleteChapter(const QString& id);
    void addChapterHistory(const QString& chapterId, const QString& action,
                          const QString& author = {});

    // =========================================================================
    // Character Library
    // =========================================================================

    QList<CharacterInfo> getAllCharacters() const;
    CharacterInfo getCharacter(const QString& id) const;
    void saveCharacter(const CharacterInfo& character);
    void deleteCharacter(const QString& id);

    // =========================================================================
    // Location Library
    // =========================================================================

    QList<LocationInfo> getAllLocations() const;
    LocationInfo getLocation(const QString& id) const;
    void saveLocation(const LocationInfo& location);
    void deleteLocation(const QString& id);

    // =========================================================================
    // Item Library
    // =========================================================================

    QList<ItemInfo> getAllItems() const;
    ItemInfo getItem(const QString& id) const;
    void saveItem(const ItemInfo& item);
    void deleteItem(const QString& id);

    // =========================================================================
    // Session Statistics
    // =========================================================================

    void recordSessionStats(const SessionStats& stats);
    QList<SessionStats> getStatsBetween(const QDateTime& from, const QDateTime& to) const;
    AggregatedStats getAggregatedStats() const;

    // =========================================================================
    // Styles
    // =========================================================================

    QList<ParagraphStyle> getParagraphStyles() const;
    void saveParagraphStyle(const ParagraphStyle& style);
    void deleteParagraphStyle(const QString& id);

    QList<CharacterStyle> getCharacterStyles() const;
    void saveCharacterStyle(const CharacterStyle& style);
    void deleteCharacterStyle(const QString& id);

    // =========================================================================
    // Settings
    // =========================================================================

    QVariant getSetting(const QString& key, const QVariant& defaultValue = {}) const;
    void setSetting(const QString& key, const QVariant& value);

    // =========================================================================
    // Utility
    // =========================================================================

    bool executeInTransaction(std::function<bool()> operation);
    QString lastError() const { return m_lastError; }

private:
    bool execSql(const QString& sql);
    void configurePragmas();

    QSqlDatabase m_db;
    QString m_projectPath;
    QString m_connectionName;
    QString m_lastError;
};

} // namespace core
} // namespace kalahari
