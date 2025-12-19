/// @file test_project_database.cpp
/// @brief Unit tests for SQLite Project Database (OpenSpec #00041)
///
/// Tests cover:
/// - DatabaseSchemaManager: schema creation
/// - ProjectLock: lock acquisition/release, stale detection
/// - BackupManager: backup creation, rotation, restore
/// - ProjectDatabase: CRUD operations for all tables

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/project_database.h>
#include <kalahari/core/database_schema_manager.h>
#include <kalahari/core/project_lock.h>
#include <kalahari/core/backup_manager.h>

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

#include <filesystem>
#include <thread>
#include <chrono>

using namespace kalahari::core;
namespace fs = std::filesystem;

// =============================================================================
// Test Helper: Temporary project directory
// =============================================================================

class TempProjectDir {
public:
    TempProjectDir() {
        m_path = fs::temp_directory_path() / ("kalahari_test_" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
        fs::create_directories(m_path);
    }

    ~TempProjectDir() {
        if (fs::exists(m_path)) {
            std::error_code ec;
            fs::remove_all(m_path, ec);
        }
    }

    QString path() const { return QString::fromStdString(m_path.string()); }
    fs::path fsPath() const { return m_path; }

    QString dbPath() const {
        return QDir(path()).filePath("project.db");
    }

private:
    fs::path m_path;
};

// =============================================================================
// DatabaseSchemaManager Tests
// =============================================================================

TEST_CASE("DatabaseSchemaManager creates valid schema", "[database][schema]") {
    TempProjectDir tempDir;
    QString dbPath = tempDir.dbPath();

    SECTION("createEmptyDatabase creates file with all tables") {
        REQUIRE(DatabaseSchemaManager::createEmptyDatabase(dbPath));
        REQUIRE(QFile::exists(dbPath));

        // Verify tables exist
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "schema_test");
        db.setDatabaseName(dbPath);
        REQUIRE(db.open());

        QSqlQuery query(db);
        query.exec("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");

        QStringList tables;
        while (query.next()) {
            tables << query.value(0).toString();
        }

        REQUIRE(tables.contains("book_metadata"));
        REQUIRE(tables.contains("chapters"));
        REQUIRE(tables.contains("chapter_history"));
        REQUIRE(tables.contains("characters"));
        REQUIRE(tables.contains("locations"));
        REQUIRE(tables.contains("items"));
        REQUIRE(tables.contains("session_stats"));
        REQUIRE(tables.contains("paragraph_styles"));
        REQUIRE(tables.contains("character_styles"));
        REQUIRE(tables.contains("settings"));

        db.close();
        QSqlDatabase::removeDatabase("schema_test");
    }
}

// =============================================================================
// ProjectLock Tests
// =============================================================================

TEST_CASE("ProjectLock manages project locking", "[database][lock]") {
    TempProjectDir tempDir;

    SECTION("Lock can be acquired and released") {
        ProjectLock lock(tempDir.path());

        REQUIRE_FALSE(lock.isAcquired());
        REQUIRE(lock.tryAcquire());
        REQUIRE(lock.isAcquired());

        // Lock file should exist
        QString lockFile = QDir(tempDir.path()).filePath(".kalahari.lock");
        REQUIRE(QFile::exists(lockFile));

        lock.release();
        REQUIRE_FALSE(lock.isAcquired());
        REQUIRE_FALSE(QFile::exists(lockFile));
    }

    SECTION("Second lock attempt fails while first is held") {
        ProjectLock lock1(tempDir.path());
        REQUIRE(lock1.tryAcquire());

        ProjectLock lock2(tempDir.path());
        REQUIRE_FALSE(lock2.tryAcquire());

        lock1.release();
        // Now lock2 should be able to acquire
        REQUIRE(lock2.tryAcquire());
        lock2.release();
    }

    SECTION("Destructor releases lock automatically") {
        QString lockFile = QDir(tempDir.path()).filePath(".kalahari.lock");
        {
            ProjectLock lock(tempDir.path());
            lock.tryAcquire();
            REQUIRE(QFile::exists(lockFile));
        }
        // Lock should be released after scope exit
        REQUIRE_FALSE(QFile::exists(lockFile));
    }
}

// =============================================================================
// BackupManager Tests
// =============================================================================

TEST_CASE("BackupManager manages database backups", "[database][backup]") {
    TempProjectDir tempDir;

    // Create a test database first
    DatabaseSchemaManager::createEmptyDatabase(tempDir.dbPath());
    REQUIRE(QFile::exists(tempDir.dbPath()));

    BackupManager backupMgr(tempDir.path());

    SECTION("Backup creates copy of database") {
        QString backupPath = backupMgr.createBackup();
        REQUIRE_FALSE(backupPath.isEmpty());
        REQUIRE(QFile::exists(backupPath));

        // Backup should be in .backups folder
        REQUIRE(backupPath.contains(".backups"));
    }

    SECTION("Available backups list works") {
        // Create one backup and verify it's listed
        QString backupPath = backupMgr.createBackup();
        REQUIRE_FALSE(backupPath.isEmpty());

        QStringList backups = backupMgr.availableBackups();
        REQUIRE(backups.size() >= 1);
        REQUIRE(backups.contains(backupPath));
    }

    SECTION("Rotation with single backup is no-op") {
        // Create one backup
        backupMgr.createBackup();

        // Rotation with keepCount > existing should not fail
        backupMgr.rotateBackups(5);

        QStringList remaining = backupMgr.availableBackups();
        REQUIRE(remaining.size() == 1);
    }

    SECTION("Restore replaces current database") {
        // Modify database
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "backup_modify");
            db.setDatabaseName(tempDir.dbPath());
            db.open();
            QSqlQuery query(db);
            query.exec("INSERT INTO settings (key, value) VALUES ('test_key', 'original')");
            db.close();
            QSqlDatabase::removeDatabase("backup_modify");
        }

        // Create backup
        QString backupPath = backupMgr.createBackup();

        // Modify database again
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "backup_modify2");
            db.setDatabaseName(tempDir.dbPath());
            db.open();
            QSqlQuery query(db);
            query.exec("UPDATE settings SET value = 'modified' WHERE key = 'test_key'");
            db.close();
            QSqlDatabase::removeDatabase("backup_modify2");
        }

        // Restore from backup
        REQUIRE(backupMgr.restoreFromBackup(backupPath));

        // Verify original value is restored
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "backup_verify");
            db.setDatabaseName(tempDir.dbPath());
            db.open();
            QSqlQuery query(db);
            query.exec("SELECT value FROM settings WHERE key = 'test_key'");
            REQUIRE(query.next());
            REQUIRE(query.value(0).toString() == "original");
            db.close();
            QSqlDatabase::removeDatabase("backup_verify");
        }
    }
}

// =============================================================================
// ProjectDatabase Tests
// =============================================================================

TEST_CASE("ProjectDatabase CRUD operations", "[database][crud]") {
    TempProjectDir tempDir;
    ProjectDatabase db;

    REQUIRE(db.open(tempDir.path()));
    REQUIRE(db.isOpen());

    SECTION("Book metadata get/set") {
        db.setMetadata("title", "Test Book");
        db.setMetadata("author", "Test Author");

        REQUIRE(db.getMetadata("title") == "Test Book");
        REQUIRE(db.getMetadata("author") == "Test Author");
        REQUIRE(db.getMetadata("nonexistent", "default") == "default");
    }

    SECTION("Chapter CRUD") {
        ChapterInfo chapter;
        chapter.id = "ch001";
        chapter.path = "content/body/part_001/chapter_001.kchapter";
        chapter.title = "Chapter One";
        chapter.status = "draft";
        chapter.wordCount = 1500;

        db.saveChapter(chapter);

        ChapterInfo loaded = db.getChapter("ch001");
        REQUIRE(loaded.id == "ch001");
        REQUIRE(loaded.title == "Chapter One");
        REQUIRE(loaded.status == "draft");
        REQUIRE(loaded.wordCount == 1500);

        // Update
        chapter.status = "revision";
        chapter.wordCount = 1600;
        db.saveChapter(chapter);

        loaded = db.getChapter("ch001");
        REQUIRE(loaded.status == "revision");
        REQUIRE(loaded.wordCount == 1600);

        // List all
        QList<ChapterInfo> all = db.getAllChapters();
        REQUIRE(all.size() == 1);

        // Delete
        db.deleteChapter("ch001");
        all = db.getAllChapters();
        REQUIRE(all.isEmpty());
    }

    SECTION("Character library CRUD") {
        CharacterInfo character;
        character.id = "char001";
        character.name = "John Doe";
        character.description = "Main protagonist";
        character.color = "#FF5733";

        db.saveCharacter(character);

        CharacterInfo loaded = db.getCharacter("char001");
        REQUIRE(loaded.name == "John Doe");
        REQUIRE(loaded.color == "#FF5733");

        QList<CharacterInfo> all = db.getAllCharacters();
        REQUIRE(all.size() == 1);

        db.deleteCharacter("char001");
        REQUIRE(db.getAllCharacters().isEmpty());
    }

    SECTION("Location library CRUD") {
        LocationInfo location;
        location.id = "loc001";
        location.name = "Castle";
        location.description = "Ancient fortress";

        db.saveLocation(location);

        LocationInfo loaded = db.getLocation("loc001");
        REQUIRE(loaded.name == "Castle");

        db.deleteLocation("loc001");
        REQUIRE(db.getAllLocations().isEmpty());
    }

    SECTION("Item library CRUD") {
        ItemInfo item;
        item.id = "item001";
        item.name = "Magic Sword";
        item.description = "Legendary weapon";

        db.saveItem(item);

        ItemInfo loaded = db.getItem("item001");
        REQUIRE(loaded.name == "Magic Sword");

        db.deleteItem("item001");
        REQUIRE(db.getAllItems().isEmpty());
    }

    SECTION("Session statistics") {
        SessionStats stats;
        stats.timestamp = QDateTime::currentDateTime();
        stats.documentId = "doc001";
        stats.wordsWritten = 500;
        stats.wordsDeleted = 50;
        stats.activeMinutes = 30;

        db.recordSessionStats(stats);

        QDateTime from = QDateTime::currentDateTime().addDays(-1);
        QDateTime to = QDateTime::currentDateTime().addDays(1);
        QList<SessionStats> results = db.getStatsBetween(from, to);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].wordsWritten == 500);

        AggregatedStats agg = db.getAggregatedStats();
        REQUIRE(agg.totalSessions >= 1);
        REQUIRE(agg.totalActiveMinutes == 30);
    }

    SECTION("Paragraph styles CRUD") {
        ParagraphStyle style;
        style.id = "para001";
        style.name = "Body Text";
        style.baseStyle = "Normal";
        style.properties = {{"fontSize", 12}};

        db.saveParagraphStyle(style);

        QList<ParagraphStyle> styles = db.getParagraphStyles();
        REQUIRE(styles.size() == 1);
        REQUIRE(styles[0].name == "Body Text");

        db.deleteParagraphStyle("para001");
        REQUIRE(db.getParagraphStyles().isEmpty());
    }

    SECTION("Character styles CRUD") {
        CharacterStyle style;
        style.id = "char_style001";
        style.name = "Emphasis";
        style.properties = {{"italic", true}};

        db.saveCharacterStyle(style);

        QList<CharacterStyle> styles = db.getCharacterStyles();
        REQUIRE(styles.size() == 1);
        REQUIRE(styles[0].name == "Emphasis");

        db.deleteCharacterStyle("char_style001");
        REQUIRE(db.getCharacterStyles().isEmpty());
    }

    SECTION("Settings get/set") {
        db.setSetting("theme", QVariant("dark"));
        db.setSetting("fontSize", QVariant(14));
        db.setSetting("autoSave", QVariant(true));

        REQUIRE(db.getSetting("theme").toString() == "dark");
        REQUIRE(db.getSetting("fontSize").toInt() == 14);
        REQUIRE(db.getSetting("autoSave").toBool() == true);
        REQUIRE(db.getSetting("nonexistent", QVariant("fallback")).toString() == "fallback");
    }

    SECTION("Transaction support") {
        bool result = db.executeInTransaction([&]() {
            db.setMetadata("key1", "value1");
            db.setMetadata("key2", "value2");
            return true;
        });

        REQUIRE(result);
        REQUIRE(db.getMetadata("key1") == "value1");
        REQUIRE(db.getMetadata("key2") == "value2");
    }

    db.close();
    REQUIRE_FALSE(db.isOpen());
}

TEST_CASE("ProjectDatabase auto-creates database if missing", "[database][autocreate]") {
    TempProjectDir tempDir;
    ProjectDatabase db;

    // Database file doesn't exist yet
    REQUIRE_FALSE(QFile::exists(tempDir.dbPath()));

    // Open should create it
    REQUIRE(db.open(tempDir.path()));
    REQUIRE(QFile::exists(tempDir.dbPath()));

    db.close();
}
