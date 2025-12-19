/// @file database_schema_manager.cpp
/// @brief Implementation of DatabaseSchemaManager
///
/// OpenSpec #00041: SQLite Project Database

#include "kalahari/core/database_schema_manager.h"
#include "kalahari/core/logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QFile>

namespace kalahari {
namespace core {

DatabaseSchemaManager::DatabaseSchemaManager(QSqlDatabase& db)
    : m_db(db)
{
}

bool DatabaseSchemaManager::createEmptyDatabase(const QString& path)
{
    if (QFile::exists(path)) {
        if (!QFile::remove(path)) {
            Logger::getInstance().error("DatabaseSchemaManager: Cannot remove existing file: {}",
                                       path.toStdString());
            return false;
        }
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "schema_creator");
    db.setDatabaseName(path);

    if (!db.open()) {
        Logger::getInstance().error("DatabaseSchemaManager: Cannot create database: {}",
                                   db.lastError().text().toStdString());
        QSqlDatabase::removeDatabase("schema_creator");
        return false;
    }

    DatabaseSchemaManager schema(db);
    bool success = schema.createSchema();

    db.close();
    QSqlDatabase::removeDatabase("schema_creator");

    return success;
}

bool DatabaseSchemaManager::execSql(const QString& sql)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        Logger::getInstance().error("DatabaseSchemaManager: SQL error: {}",
                                   m_lastError.toStdString());
        return false;
    }
    return true;
}

bool DatabaseSchemaManager::createSchema()
{
    Logger::getInstance().info("DatabaseSchemaManager: Creating schema...");

    // Book metadata (key-value)
    if (!execSql(R"(
        CREATE TABLE book_metadata (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )")) return false;

    // Chapters metadata
    if (!execSql(R"(
        CREATE TABLE chapters (
            id TEXT PRIMARY KEY,
            path TEXT NOT NULL,
            title TEXT,
            status TEXT DEFAULT 'draft',
            word_count INTEGER DEFAULT 0,
            character_count INTEGER DEFAULT 0,
            order_index INTEGER,
            created_at TEXT,
            modified_at TEXT
        )
    )")) return false;

    if (!execSql("CREATE INDEX idx_chapters_order ON chapters(order_index)")) return false;

    // Chapter history
    if (!execSql(R"(
        CREATE TABLE chapter_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            chapter_id TEXT NOT NULL,
            action TEXT NOT NULL,
            author TEXT,
            timestamp TEXT,
            FOREIGN KEY (chapter_id) REFERENCES chapters(id) ON DELETE CASCADE
        )
    )")) return false;

    // Character library
    if (!execSql(R"(
        CREATE TABLE characters (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            color TEXT,
            notes TEXT,
            created_at TEXT,
            modified_at TEXT
        )
    )")) return false;

    // Location library
    if (!execSql(R"(
        CREATE TABLE locations (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            notes TEXT,
            created_at TEXT,
            modified_at TEXT
        )
    )")) return false;

    // Item library
    if (!execSql(R"(
        CREATE TABLE items (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            notes TEXT,
            created_at TEXT,
            modified_at TEXT
        )
    )")) return false;

    // Session statistics
    if (!execSql(R"(
        CREATE TABLE session_stats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            document_id TEXT,
            words_written INTEGER DEFAULT 0,
            words_deleted INTEGER DEFAULT 0,
            active_minutes INTEGER DEFAULT 0,
            hour INTEGER
        )
    )")) return false;

    if (!execSql("CREATE INDEX idx_stats_timestamp ON session_stats(timestamp)")) return false;

    // Paragraph styles
    if (!execSql(R"(
        CREATE TABLE paragraph_styles (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            base_style TEXT,
            properties TEXT
        )
    )")) return false;

    // Character styles
    if (!execSql(R"(
        CREATE TABLE character_styles (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            properties TEXT
        )
    )")) return false;

    // Project settings
    if (!execSql(R"(
        CREATE TABLE settings (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )")) return false;

    Logger::getInstance().info("DatabaseSchemaManager: Schema created successfully");
    return true;
}

} // namespace core
} // namespace kalahari
