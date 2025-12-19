# 00041: SQLite Project Database - Tasks

## Implementation

### Preparation ✅
- [x] Add Qt6::Sql to CMakeLists.txt
- [x] Add sql feature to vcpkg.json
- [x] Create database_types.h with DTOs

### Infrastructure
- [x] DatabaseSchemaManager ✅
  - [x] createEmptyDatabase() - creates new DB with schema
  - [x] createSchema() - creates all tables
- [x] ProjectLock ✅
  - [x] Lock file creation/deletion
  - [x] PID tracking
  - [x] Stale lock detection
- [x] BackupManager ✅
  - [x] Backup creation (file copy)
  - [x] Backup rotation (keep last N)
  - [x] Restore from backup

### ProjectDatabase ✅
- [x] Class skeleton + open/close with WAL mode
- [x] Book metadata (get/set key-value)
- [x] Chapters metadata CRUD
- [x] Character/Location/Item library CRUD
- [x] Session statistics (record/query)
- [x] Paragraph/Character styles CRUD
- [x] Project settings (get/set)

### Integration
- [x] Integrate with ProjectManager ✅
- [x] Update examples/ to new format ✅
- [x] Deploy sqldrivers plugin (CMakeLists.txt) ✅

### Testing
- [x] Unit tests for all classes ✅ (107 assertions, 5 test cases)
- [x] Manual testing ✅

