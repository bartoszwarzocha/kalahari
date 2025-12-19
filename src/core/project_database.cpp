/// @file project_database.cpp
/// @brief Implementation of ProjectDatabase
///
/// OpenSpec #00041: SQLite Project Database

#include "kalahari/core/project_database.h"
#include "kalahari/core/database_schema_manager.h"
#include "kalahari/core/logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>

namespace kalahari {
namespace core {

ProjectDatabase::ProjectDatabase() = default;

ProjectDatabase::~ProjectDatabase()
{
    close();
}

// =============================================================================
// Connection Management
// =============================================================================

bool ProjectDatabase::open(const QString& projectPath)
{
    if (isOpen()) {
        close();
    }

    m_projectPath = projectPath;
    m_connectionName = "project_" + QUuid::createUuid().toString(QUuid::WithoutBraces);

    QString dbPath = QDir(projectPath).filePath("project.db");

    // Create database if it doesn't exist
    if (!QFile::exists(dbPath)) {
        Logger::getInstance().info("ProjectDatabase: Creating new database");
        if (!DatabaseSchemaManager::createEmptyDatabase(dbPath)) {
            m_lastError = "Failed to create database";
            return false;
        }
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        Logger::getInstance().error("ProjectDatabase: Cannot open: {}", m_lastError.toStdString());
        return false;
    }

    configurePragmas();

    Logger::getInstance().info("ProjectDatabase: Opened {}", dbPath.toStdString());
    return true;
}

void ProjectDatabase::close()
{
    if (!isOpen()) {
        return;
    }

    QString connName = m_connectionName;
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
    m_connectionName.clear();

    Logger::getInstance().info("ProjectDatabase: Closed");
}

bool ProjectDatabase::isOpen() const
{
    return m_db.isOpen();
}

void ProjectDatabase::configurePragmas()
{
    QSqlQuery query(m_db);
    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA synchronous=NORMAL");
    query.exec("PRAGMA foreign_keys=ON");
}

bool ProjectDatabase::execSql(const QString& sql)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool ProjectDatabase::executeInTransaction(std::function<bool()> operation)
{
    if (!m_db.transaction()) {
        m_lastError = "Failed to start transaction";
        return false;
    }

    if (operation()) {
        m_db.commit();
        return true;
    } else {
        m_db.rollback();
        return false;
    }
}

// =============================================================================
// Book Metadata
// =============================================================================

QString ProjectDatabase::getMetadata(const QString& key, const QString& defaultValue) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM book_metadata WHERE key = ?");
    query.addBindValue(key);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

void ProjectDatabase::setMetadata(const QString& key, const QString& value)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO book_metadata (key, value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value);
    query.exec();
}

// =============================================================================
// Chapters
// =============================================================================

QList<ChapterInfo> ProjectDatabase::getAllChapters() const
{
    QList<ChapterInfo> chapters;
    QSqlQuery query(m_db);
    query.exec("SELECT id, path, title, status, word_count, character_count, "
               "order_index, created_at, modified_at FROM chapters ORDER BY order_index");

    while (query.next()) {
        ChapterInfo ch;
        ch.id = query.value(0).toString();
        ch.path = query.value(1).toString();
        ch.title = query.value(2).toString();
        ch.status = query.value(3).toString();
        ch.wordCount = query.value(4).toInt();
        ch.characterCount = query.value(5).toInt();
        ch.orderIndex = query.value(6).toInt();
        ch.createdAt = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
        ch.modifiedAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
        chapters.append(ch);
    }
    return chapters;
}

ChapterInfo ProjectDatabase::getChapter(const QString& id) const
{
    ChapterInfo ch;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, path, title, status, word_count, character_count, "
                  "order_index, created_at, modified_at FROM chapters WHERE id = ?");
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        ch.id = query.value(0).toString();
        ch.path = query.value(1).toString();
        ch.title = query.value(2).toString();
        ch.status = query.value(3).toString();
        ch.wordCount = query.value(4).toInt();
        ch.characterCount = query.value(5).toInt();
        ch.orderIndex = query.value(6).toInt();
        ch.createdAt = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
        ch.modifiedAt = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
    }
    return ch;
}

void ProjectDatabase::saveChapter(const ChapterInfo& chapter)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO chapters "
                  "(id, path, title, status, word_count, character_count, order_index, created_at, modified_at) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(chapter.id);
    query.addBindValue(chapter.path);
    query.addBindValue(chapter.title);
    query.addBindValue(chapter.status);
    query.addBindValue(chapter.wordCount);
    query.addBindValue(chapter.characterCount);
    query.addBindValue(chapter.orderIndex);
    query.addBindValue(chapter.createdAt.toString(Qt::ISODate));
    query.addBindValue(chapter.modifiedAt.toString(Qt::ISODate));
    query.exec();
}

void ProjectDatabase::deleteChapter(const QString& id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM chapters WHERE id = ?");
    query.addBindValue(id);
    query.exec();
}

void ProjectDatabase::addChapterHistory(const QString& chapterId, const QString& action,
                                        const QString& author)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO chapter_history (chapter_id, action, author, timestamp) "
                  "VALUES (?, ?, ?, ?)");
    query.addBindValue(chapterId);
    query.addBindValue(action);
    query.addBindValue(author);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.exec();
}

// =============================================================================
// Character Library
// =============================================================================

QList<CharacterInfo> ProjectDatabase::getAllCharacters() const
{
    QList<CharacterInfo> characters;
    QSqlQuery query(m_db);
    query.exec("SELECT id, name, description, color, notes, created_at, modified_at "
               "FROM characters ORDER BY name");

    while (query.next()) {
        CharacterInfo c;
        c.id = query.value(0).toString();
        c.name = query.value(1).toString();
        c.description = query.value(2).toString();
        c.color = query.value(3).toString();
        c.notes = query.value(4).toString();
        c.createdAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        c.modifiedAt = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);
        characters.append(c);
    }
    return characters;
}

CharacterInfo ProjectDatabase::getCharacter(const QString& id) const
{
    CharacterInfo c;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, description, color, notes, created_at, modified_at "
                  "FROM characters WHERE id = ?");
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        c.id = query.value(0).toString();
        c.name = query.value(1).toString();
        c.description = query.value(2).toString();
        c.color = query.value(3).toString();
        c.notes = query.value(4).toString();
        c.createdAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        c.modifiedAt = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);
    }
    return c;
}

void ProjectDatabase::saveCharacter(const CharacterInfo& character)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO characters "
                  "(id, name, description, color, notes, created_at, modified_at) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(character.id);
    query.addBindValue(character.name);
    query.addBindValue(character.description);
    query.addBindValue(character.color);
    query.addBindValue(character.notes);
    query.addBindValue(character.createdAt.toString(Qt::ISODate));
    query.addBindValue(character.modifiedAt.toString(Qt::ISODate));
    query.exec();
}

void ProjectDatabase::deleteCharacter(const QString& id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM characters WHERE id = ?");
    query.addBindValue(id);
    query.exec();
}

// =============================================================================
// Location Library
// =============================================================================

QList<LocationInfo> ProjectDatabase::getAllLocations() const
{
    QList<LocationInfo> locations;
    QSqlQuery query(m_db);
    query.exec("SELECT id, name, description, notes, created_at, modified_at "
               "FROM locations ORDER BY name");

    while (query.next()) {
        LocationInfo l;
        l.id = query.value(0).toString();
        l.name = query.value(1).toString();
        l.description = query.value(2).toString();
        l.notes = query.value(3).toString();
        l.createdAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
        l.modifiedAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        locations.append(l);
    }
    return locations;
}

LocationInfo ProjectDatabase::getLocation(const QString& id) const
{
    LocationInfo l;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, description, notes, created_at, modified_at "
                  "FROM locations WHERE id = ?");
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        l.id = query.value(0).toString();
        l.name = query.value(1).toString();
        l.description = query.value(2).toString();
        l.notes = query.value(3).toString();
        l.createdAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
        l.modifiedAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
    }
    return l;
}

void ProjectDatabase::saveLocation(const LocationInfo& location)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO locations "
                  "(id, name, description, notes, created_at, modified_at) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(location.id);
    query.addBindValue(location.name);
    query.addBindValue(location.description);
    query.addBindValue(location.notes);
    query.addBindValue(location.createdAt.toString(Qt::ISODate));
    query.addBindValue(location.modifiedAt.toString(Qt::ISODate));
    query.exec();
}

void ProjectDatabase::deleteLocation(const QString& id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM locations WHERE id = ?");
    query.addBindValue(id);
    query.exec();
}

// =============================================================================
// Item Library
// =============================================================================

QList<ItemInfo> ProjectDatabase::getAllItems() const
{
    QList<ItemInfo> items;
    QSqlQuery query(m_db);
    query.exec("SELECT id, name, description, notes, created_at, modified_at "
               "FROM items ORDER BY name");

    while (query.next()) {
        ItemInfo i;
        i.id = query.value(0).toString();
        i.name = query.value(1).toString();
        i.description = query.value(2).toString();
        i.notes = query.value(3).toString();
        i.createdAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
        i.modifiedAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        items.append(i);
    }
    return items;
}

ItemInfo ProjectDatabase::getItem(const QString& id) const
{
    ItemInfo i;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, description, notes, created_at, modified_at "
                  "FROM items WHERE id = ?");
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        i.id = query.value(0).toString();
        i.name = query.value(1).toString();
        i.description = query.value(2).toString();
        i.notes = query.value(3).toString();
        i.createdAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
        i.modifiedAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
    }
    return i;
}

void ProjectDatabase::saveItem(const ItemInfo& item)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO items "
                  "(id, name, description, notes, created_at, modified_at) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(item.id);
    query.addBindValue(item.name);
    query.addBindValue(item.description);
    query.addBindValue(item.notes);
    query.addBindValue(item.createdAt.toString(Qt::ISODate));
    query.addBindValue(item.modifiedAt.toString(Qt::ISODate));
    query.exec();
}

void ProjectDatabase::deleteItem(const QString& id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM items WHERE id = ?");
    query.addBindValue(id);
    query.exec();
}

// =============================================================================
// Session Statistics
// =============================================================================

void ProjectDatabase::recordSessionStats(const SessionStats& stats)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO session_stats "
                  "(timestamp, document_id, words_written, words_deleted, active_minutes, hour) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(stats.timestamp.toString(Qt::ISODate));
    query.addBindValue(stats.documentId);
    query.addBindValue(stats.wordsWritten);
    query.addBindValue(stats.wordsDeleted);
    query.addBindValue(stats.activeMinutes);
    query.addBindValue(stats.hour);
    query.exec();
}

QList<SessionStats> ProjectDatabase::getStatsBetween(const QDateTime& from,
                                                     const QDateTime& to) const
{
    QList<SessionStats> stats;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, timestamp, document_id, words_written, words_deleted, "
                  "active_minutes, hour FROM session_stats "
                  "WHERE timestamp >= ? AND timestamp <= ? ORDER BY timestamp");
    query.addBindValue(from.toString(Qt::ISODate));
    query.addBindValue(to.toString(Qt::ISODate));

    if (query.exec()) {
        while (query.next()) {
            SessionStats s;
            s.id = query.value(0).toInt();
            s.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
            s.documentId = query.value(2).toString();
            s.wordsWritten = query.value(3).toInt();
            s.wordsDeleted = query.value(4).toInt();
            s.activeMinutes = query.value(5).toInt();
            s.hour = query.value(6).toInt();
            stats.append(s);
        }
    }
    return stats;
}

AggregatedStats ProjectDatabase::getAggregatedStats() const
{
    AggregatedStats agg;
    QSqlQuery query(m_db);

    query.exec("SELECT COUNT(*), SUM(word_count), SUM(character_count) FROM chapters");
    if (query.next()) {
        agg.totalChapters = query.value(0).toInt();
        agg.totalWords = query.value(1).toInt();
        agg.totalCharacters = query.value(2).toInt();
    }

    query.exec("SELECT COUNT(*), SUM(active_minutes), MIN(timestamp), MAX(timestamp) "
               "FROM session_stats");
    if (query.next()) {
        agg.totalSessions = query.value(0).toInt();
        agg.totalActiveMinutes = query.value(1).toInt();
        agg.firstSession = QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
        agg.lastSession = QDateTime::fromString(query.value(3).toString(), Qt::ISODate);
    }

    query.exec("SELECT AVG(words_written) FROM session_stats");
    if (query.next()) {
        agg.averageWordsPerSession = query.value(0).toInt();
    }

    return agg;
}

// =============================================================================
// Styles
// =============================================================================

QList<ParagraphStyle> ProjectDatabase::getParagraphStyles() const
{
    QList<ParagraphStyle> styles;
    QSqlQuery query(m_db);
    query.exec("SELECT id, name, base_style, properties FROM paragraph_styles ORDER BY name");

    while (query.next()) {
        ParagraphStyle s;
        s.id = query.value(0).toString();
        s.name = query.value(1).toString();
        s.baseStyle = query.value(2).toString();

        QString propsJson = query.value(3).toString();
        if (!propsJson.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(propsJson.toUtf8());
            s.properties = doc.object().toVariantMap();
        }
        styles.append(s);
    }
    return styles;
}

void ProjectDatabase::saveParagraphStyle(const ParagraphStyle& style)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO paragraph_styles "
                  "(id, name, base_style, properties) VALUES (?, ?, ?, ?)");
    query.addBindValue(style.id);
    query.addBindValue(style.name);
    query.addBindValue(style.baseStyle);

    QJsonDocument doc(QJsonObject::fromVariantMap(style.properties));
    query.addBindValue(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    query.exec();
}

void ProjectDatabase::deleteParagraphStyle(const QString& id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM paragraph_styles WHERE id = ?");
    query.addBindValue(id);
    query.exec();
}

QList<CharacterStyle> ProjectDatabase::getCharacterStyles() const
{
    QList<CharacterStyle> styles;
    QSqlQuery query(m_db);
    query.exec("SELECT id, name, properties FROM character_styles ORDER BY name");

    while (query.next()) {
        CharacterStyle s;
        s.id = query.value(0).toString();
        s.name = query.value(1).toString();

        QString propsJson = query.value(2).toString();
        if (!propsJson.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(propsJson.toUtf8());
            s.properties = doc.object().toVariantMap();
        }
        styles.append(s);
    }
    return styles;
}

void ProjectDatabase::saveCharacterStyle(const CharacterStyle& style)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO character_styles "
                  "(id, name, properties) VALUES (?, ?, ?)");
    query.addBindValue(style.id);
    query.addBindValue(style.name);

    QJsonDocument doc(QJsonObject::fromVariantMap(style.properties));
    query.addBindValue(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    query.exec();
}

void ProjectDatabase::deleteCharacterStyle(const QString& id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM character_styles WHERE id = ?");
    query.addBindValue(id);
    query.exec();
}

// =============================================================================
// Settings
// =============================================================================

QVariant ProjectDatabase::getSetting(const QString& key, const QVariant& defaultValue) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = ?");
    query.addBindValue(key);

    if (query.exec() && query.next()) {
        return query.value(0);
    }
    return defaultValue;
}

void ProjectDatabase::setSetting(const QString& key, const QVariant& value)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value.toString());
    query.exec();
}

} // namespace core
} // namespace kalahari
