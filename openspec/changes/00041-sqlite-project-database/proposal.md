# 00041: SQLite Project Database

## Status
PENDING

## Summary

Refactor project file storage architecture to use SQLite for metadata, configuration, and statistics. Content files (.kchapter) remain as files for git-friendliness and disaster recovery.

**Motivation:** Current design proliferates multiple file formats (JSON, KSTAT, etc.) which is error-prone and inefficient. SQLite provides ACID transactions, fast queries, and single-file management.

## Goal

Replace scattered configuration/metadata files with a single SQLite database per project:

```
MyNovel/
├── project.db              # SQLite: metadata, config, statistics, libraries
├── content/
│   ├── body/
│   │   ├── part_001/
│   │   │   ├── chapter_001.kchapter
│   │   │   └── chapter_002.kchapter
│   ├── front/
│   └── back/
└── resources/
    └── images/
```

## Scope

### Included

**Data to migrate to SQLite:**
- Book metadata (title, author, language, description)
- Chapter metadata (title, status, order, word count - NOT content)
- Chapter history (created/edited/reviewed by whom, when)
- Character library (id, name, description, color, notes)
- Location library (id, name, description, notes)
- Item library (id, name, description, notes)
- Session statistics (timestamp, document_id, words_written, words_deleted, active_minutes, hour)
- Paragraph styles and character styles
- Project settings (editor preferences, view modes, etc.)

**Features:**
- Plain SQLite (no encryption needed)
- Schema versioning with migrations
- Backup/restore functionality
- Transaction-based writes (no partial updates)

**Remains as files:**
- Chapter content (.kchapter with KML)
- Images and media resources
- Export templates

### Excluded

- Real-time sync/collaboration (future)
- Cloud backup (future)
- Multi-user access (future)

## Acceptance Criteria

- [ ] ProjectDatabase class with Qt's QSqlDatabase + SQLite
- [ ] Schema v1.0 with all required tables
- [ ] Migration system for future schema changes
- [ ] Existing projects migrated on first open
- [ ] Unit tests for all CRUD operations
- [ ] Performance acceptable (< 50ms for typical queries)
- [ ] Backup/restore functionality

## Design

### Database Schema (v1.0)

```sql
-- Version tracking
schema_version (version INTEGER, applied_at TEXT)

-- Book metadata (key-value for flexibility)
book_metadata (
    key TEXT PRIMARY KEY,
    value TEXT
)

-- Chapters (metadata only, content in .kchapter files)
chapters (
    id TEXT PRIMARY KEY,
    path TEXT NOT NULL,           -- relative path to .kchapter
    title TEXT,
    status TEXT DEFAULT 'draft',  -- draft, revision, final
    word_count INTEGER DEFAULT 0,
    character_count INTEGER DEFAULT 0,
    order_index INTEGER,
    created_at TEXT,
    modified_at TEXT
)

-- Chapter history
chapter_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    chapter_id TEXT NOT NULL,
    action TEXT NOT NULL,         -- created, edited, reviewed
    author TEXT,
    timestamp TEXT,
    FOREIGN KEY (chapter_id) REFERENCES chapters(id)
)

-- Character library
characters (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    color TEXT,                   -- hex color for UI
    notes TEXT,
    created_at TEXT,
    modified_at TEXT
)

-- Location library
locations (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    notes TEXT,
    created_at TEXT,
    modified_at TEXT
)

-- Item library
items (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    notes TEXT,
    created_at TEXT,
    modified_at TEXT
)

-- Session statistics
session_stats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,      -- ISO 8601
    document_id TEXT,
    words_written INTEGER DEFAULT 0,
    words_deleted INTEGER DEFAULT 0,
    active_minutes INTEGER DEFAULT 0,
    hour INTEGER                  -- 0-23 for productivity analysis
)

-- Paragraph styles
paragraph_styles (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    base_style TEXT,              -- inheritance
    properties TEXT               -- JSON blob
)

-- Character styles
character_styles (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    properties TEXT               -- JSON blob
)

-- Project settings
settings (
    key TEXT PRIMARY KEY,
    value TEXT
)
```

### Class Design

```cpp
namespace kalahari::core {

class ProjectDatabase : public QObject {
public:
    static ProjectDatabase& getInstance();

    bool open(const QString& projectPath);
    void close();
    bool isOpen() const;

    // Migrations
    bool migrateSchema();
    int schemaVersion() const;

    // Book metadata
    QString getMetadata(const QString& key, const QString& defaultValue = {});
    void setMetadata(const QString& key, const QString& value);

    // Chapters
    QList<ChapterInfo> getAllChapters();
    ChapterInfo getChapter(const QString& id);
    void updateChapterMetadata(const ChapterInfo& info);
    void addChapterHistory(const QString& chapterId, const QString& action, const QString& author);

    // Libraries
    QList<CharacterInfo> getAllCharacters();
    void saveCharacter(const CharacterInfo& character);
    void deleteCharacter(const QString& id);
    // ... similar for locations, items

    // Statistics
    void recordSessionStats(const SessionStats& stats);
    QList<SessionStats> getStatsBetween(const QDateTime& from, const QDateTime& to);
    AggregatedStats getAggregatedStats();

    // Styles
    QList<ParagraphStyle> getParagraphStyles();
    void saveParagraphStyle(const ParagraphStyle& style);
    // ... similar for character styles

    // Settings
    QVariant getSetting(const QString& key, const QVariant& defaultValue = {});
    void setSetting(const QString& key, const QVariant& value);

    // Backup
    bool backup(const QString& backupPath);
    bool restore(const QString& backupPath);

signals:
    void databaseOpened();
    void databaseClosed();
    void schemaUpgraded(int oldVersion, int newVersion);

private:
    QSqlDatabase m_db;
    QString m_projectPath;
};

} // namespace kalahari::core
```

### DatabaseSchemaManager (zarządzanie schematem)

Osobna klasa odpowiedzialna za tworzenie i migrację schematu bazy:

```cpp
namespace kalahari::core {

class DatabaseSchemaManager {
public:
    explicit DatabaseSchemaManager(QSqlDatabase& db);

    // Tworzenie pustej bazy
    static bool createEmptyDatabase(const QString& path);

    // Migracje
    bool needsMigration() const;
    int currentVersion() const;
    static constexpr int targetVersion() { return CURRENT_SCHEMA_VERSION; }
    bool migrate();  // wykonuje wszystkie pending migracje

private:
    // Każda migracja jako osobna metoda
    bool migrateV0toV1();  // initial schema
    bool migrateV1toV2();  // future: np. dodanie indeksów FTS
    // ... kolejne migracje dodawane w miarę potrzeb

    bool runMigration(int fromVersion, std::function<bool()> migrationFn);
    void recordMigration(int version);

    QSqlDatabase& m_db;
    static constexpr int CURRENT_SCHEMA_VERSION = 1;
};

} // namespace kalahari::core
```

**Wzorzec:** Inspirowany Rails Migrations / Flyway - każda wersja ma dedykowaną metodę migracji.

### Project Lock (jeden projekt = jedna instancja)

Mechanizm zapobiegający otwarciu tego samego projektu w wielu instancjach:

```cpp
namespace kalahari::core {

class ProjectLock {
public:
    explicit ProjectLock(const QString& projectPath);
    ~ProjectLock();

    bool tryAcquire();
    void release();
    bool isLocked() const;

    // Sprawdź czy lock jest stale (proces nie żyje)
    static bool isStale(const QString& lockFile);

private:
    QString m_lockFile;  // {project}/.kalahari.lock
    bool m_acquired = false;

    void writeLockFile();
    static qint64 readPidFromLock(const QString& path);
    static bool isProcessAlive(qint64 pid);
};

} // namespace kalahari::core
```

**Użycie w ProjectManager:**
```cpp
bool ProjectManager::openProject(const QString& path) {
    m_lock = std::make_unique<ProjectLock>(path);

    if (!m_lock->tryAcquire()) {
        emit errorOccurred(tr("Project is already open in another instance"));
        return false;
    }

    // ... kontynuuj otwieranie
}
```

### Detekcja formatu projektu

```cpp
namespace kalahari::core {

enum class ProjectFormat {
    Unknown,   // nie rozpoznany
    Legacy,    // stary format (book.json, styles.json, etc.)
    SQLite     // nowy format (project.db)
};

class ProjectFormatDetector {
public:
    static ProjectFormat detect(const QString& projectPath) {
        if (QFile::exists(projectPath + "/project.db")) {
            return ProjectFormat::SQLite;
        }
        if (QFile::exists(projectPath + "/book.json") ||
            QFile::exists(projectPath + "/book.kbook")) {
            return ProjectFormat::Legacy;
        }
        return ProjectFormat::Unknown;
    }

    static bool needsMigration(const QString& projectPath) {
        return detect(projectPath) == ProjectFormat::Legacy;
    }
};

} // namespace kalahari::core
```

### Error Handling i WAL Mode

```cpp
bool ProjectDatabase::open(const QString& projectPath) {
    m_db = QSqlDatabase::addDatabase("QSQLITE", "project");
    m_db.setDatabaseName(projectPath + "/project.db");

    if (!m_db.open()) {
        emit errorOccurred(tr("Failed to open database: %1").arg(m_db.lastError().text()));
        return false;
    }

    // WAL mode dla lepszej wydajności i odporności na crash
    m_db.exec("PRAGMA journal_mode=WAL");
    m_db.exec("PRAGMA synchronous=NORMAL");
    m_db.exec("PRAGMA foreign_keys=ON");

    return true;
}

// Transakcje dla atomowości operacji
template<typename Func>
bool ProjectDatabase::executeInTransaction(Func&& operation) {
    if (!m_db.transaction()) {
        emit errorOccurred(tr("Failed to start transaction"));
        return false;
    }

    try {
        if (operation()) {
            m_db.commit();
            return true;
        } else {
            m_db.rollback();
            return false;
        }
    } catch (const std::exception& e) {
        m_db.rollback();
        emit errorOccurred(tr("Database error: %1").arg(e.what()));
        return false;
    }
}
```

### BackupManager

```cpp
namespace kalahari::core {

class BackupManager {
public:
    explicit BackupManager(const QString& projectPath);

    // Backup przy zamykaniu projektu
    QString createBackup();

    // Backup przed migracją (obowiązkowy, z dedykowaną nazwą)
    QString createPreMigrationBackup();

    // Rotacja: zachowaj ostatnie N backupów
    void rotateBackups(int keepCount = 5);

    // Lista dostępnych backupów
    QStringList availableBackups() const;

    // Przywracanie
    bool restoreFromBackup(const QString& backupPath);

private:
    QString m_projectPath;
    QString m_backupDir;  // {project}/.backups/

    // Format nazwy: project_YYYYMMDD_HHMMSS.db
    QString generateBackupName(const QString& suffix = {});

    void ensureBackupDirExists();
};

} // namespace kalahari::core
```

**Strategia backupów:**
- Lokalizacja: `{project}/.backups/`
- Automatyczny backup przy zamykaniu projektu
- Obowiązkowy backup przed migracją schematu
- Rotacja: domyślnie 5 ostatnich backupów
- Format nazwy: `project_20251219_143052.db`

### Migration from Current Format

1. Detect old project format (presence of `book.json`, `styles.json`, etc.)
2. Create new `project.db`
3. Import data from old files
4. Rename old files to `.backup`
5. Update project version marker

### Files to Modify

- `src/core/project_manager.cpp` - use ProjectDatabase instead of JSON files
- `include/kalahari/core/project_manager.h` - add ProjectDatabase dependency

### New Files

- `include/kalahari/core/project_database.h`
- `src/core/project_database.cpp`
- `include/kalahari/core/database_schema_manager.h`
- `src/core/database_schema_manager.cpp`
- `include/kalahari/core/project_lock.h`
- `src/core/project_lock.cpp`
- `include/kalahari/core/backup_manager.h`
- `src/core/backup_manager.cpp`
- `tests/core/project_database_test.cpp`
- `tests/core/database_schema_manager_test.cpp`

## Dependencies

- **Depends on:**
  - Project File System (1.2) - COMPLETE
  - Qt SQL module (built-in)

- **Required by:**
  - OpenSpec #00042: Custom Text Editor (statistics, styles)
  - Statistics Module (1.7)
  - Future: Character/Location libraries

## Implementation Phases

### Phase 1: Core Database (this OpenSpec)
- ProjectDatabase class
- Schema v1.0
- Basic CRUD operations
- Migration from old format

### Phase 2: Advanced Features (future OpenSpec)
- Full-text search indexes
- Backup scheduling
- Database optimization (VACUUM, ANALYZE)

## Notes

- Qt has built-in SQLite support via QSqlDatabase
- WAL mode recommended for better concurrent access
- Consider PRAGMA settings for performance tuning
- Backup before any schema migration

## References

- Qt SQL Documentation: https://doc.qt.io/qt-6/sql-programming.html
- SQLite Documentation: https://sqlite.org/docs.html
- SQLCipher: https://www.zetetic.net/sqlcipher/
