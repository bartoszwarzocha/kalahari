// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file database_schema_manager.h
/// @brief Database schema creation
///
/// DatabaseSchemaManager creates SQLite schema for project database.
/// Simple approach - no migrations needed in development phase.
///
/// OpenSpec #00041: SQLite Project Database

#ifndef KALAHARI_CORE_DATABASE_SCHEMA_MANAGER_H
#define KALAHARI_CORE_DATABASE_SCHEMA_MANAGER_H

#include <QString>
#include <QSqlDatabase>

namespace kalahari::core {

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
    [[nodiscard]] QString lastError() const { return m_lastError; }

private:
    /// @brief Execute SQL with error handling
    bool execSql(const QString& sql);

    QSqlDatabase& m_db;
    QString m_lastError;
};

} // namespace kalahari::core

#endif  // KALAHARI_CORE_DATABASE_SCHEMA_MANAGER_H
