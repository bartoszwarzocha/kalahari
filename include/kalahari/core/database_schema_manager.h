/// @file database_schema_manager.h
/// @brief Database schema creation
///
/// DatabaseSchemaManager creates SQLite schema for project database.
/// Simple approach - no migrations needed in development phase.
///
/// OpenSpec #00041: SQLite Project Database

#pragma once

#include <QString>
#include <QSqlDatabase>

namespace kalahari {
namespace core {

/// @brief Creates database schema for project database
///
/// Simple schema creator - creates all tables in one go.
/// No migration mechanism (not needed in development phase).
class DatabaseSchemaManager {
public:
    /// @brief Construct schema manager for database
    /// @param db Open database connection
    explicit DatabaseSchemaManager(QSqlDatabase& db);

    /// @brief Create new database file with schema
    /// @param path Full path to database file
    /// @return true on success
    static bool createEmptyDatabase(const QString& path);

    /// @brief Create all tables in database
    /// @return true on success
    bool createSchema();

    /// @brief Get last error message
    QString lastError() const { return m_lastError; }

private:
    /// @brief Execute SQL with error handling
    bool execSql(const QString& sql);

    QSqlDatabase& m_db;
    QString m_lastError;
};

} // namespace core
} // namespace kalahari
