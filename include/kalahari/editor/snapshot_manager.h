/// @file snapshot_manager.h
/// @brief Chapter snapshot (restore points) management (OpenSpec #00042 Task 7.13)
///
/// SnapshotManager provides:
/// - On-demand snapshot creation with timestamp
/// - Snapshot listing for a chapter
/// - Restore from snapshot
/// - Auto-snapshot capability (optional)
///
/// Snapshots are stored in: {project}/.kalahari/snapshots/{chapterId}/
/// - index.json - metadata for all snapshots
/// - {snapshotId}.kml - actual content files

#pragma once

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include <QString>
#include <QList>

#include <optional>

namespace kalahari::editor {

/// @brief Information about a saved snapshot
///
/// Contains metadata for a single snapshot restore point.
/// The actual content is stored in a separate .kml file.
struct Snapshot {
    QString id;             ///< Unique ID (UUID)
    QString chapterId;      ///< Chapter this belongs to
    QString name;           ///< User-provided name (optional)
    QDateTime createdAt;    ///< When snapshot was created
    int wordCount;          ///< Word count at snapshot time
    QString contentHash;    ///< SHA256 hash for quick comparison
    QString filePath;       ///< Path to snapshot .kml file

    /// @brief Check if snapshot is valid
    /// @return true if snapshot has required fields
    bool isValid() const {
        return !id.isEmpty() && !chapterId.isEmpty() && !filePath.isEmpty();
    }
};

/// @brief Manages chapter snapshots (restore points)
///
/// Provides a simple mechanism for creating manual save points during editing.
/// Each snapshot captures the full KML content of a chapter at a specific moment.
///
/// Usage:
/// @code
/// auto manager = new SnapshotManager(this);
/// manager->setStorageDir("/path/to/project/.kalahari/snapshots");
/// manager->setChapterId("chapter-001");
///
/// // Create a snapshot
/// auto snapshot = manager->createSnapshot(document->toKml(), "Before rewrite");
///
/// // List snapshots
/// auto snapshots = manager->listSnapshots();
///
/// // Restore from snapshot
/// QString content = manager->loadSnapshotContent(snapshot.id);
/// document->fromKml(content);
/// @endcode
class SnapshotManager : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a snapshot manager
    /// @param parent Parent QObject for ownership
    explicit SnapshotManager(QObject* parent = nullptr);

    /// @brief Destructor
    ~SnapshotManager() override;

    // Non-copyable
    SnapshotManager(const SnapshotManager&) = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /// @brief Set storage directory for snapshots
    /// @param dir Base directory for snapshot storage (typically {project}/.kalahari/snapshots)
    void setStorageDir(const QString& dir);

    /// @brief Get current storage directory
    /// @return Storage directory path
    QString storageDir() const;

    /// @brief Set current chapter ID
    /// @param chapterId Chapter identifier
    void setChapterId(const QString& chapterId);

    /// @brief Get current chapter ID
    /// @return Current chapter identifier
    QString chapterId() const;

    // =========================================================================
    // Snapshot Operations
    // =========================================================================

    /// @brief Create snapshot of current content
    /// @param content The KML content to snapshot
    /// @param name Optional user-provided name for the snapshot
    /// @return Created snapshot info
    Snapshot createSnapshot(const QString& content, const QString& name = QString());

    /// @brief List all snapshots for current chapter
    /// @return List of snapshots sorted by creation time (newest first)
    QList<Snapshot> listSnapshots() const;

    /// @brief Get specific snapshot by ID
    /// @param snapshotId Snapshot identifier
    /// @return Snapshot info if found, nullopt otherwise
    std::optional<Snapshot> getSnapshot(const QString& snapshotId) const;

    /// @brief Load content from snapshot
    /// @param snapshotId Snapshot identifier
    /// @return KML content from snapshot, empty string if not found
    QString loadSnapshotContent(const QString& snapshotId) const;

    /// @brief Delete a snapshot
    /// @param snapshotId Snapshot identifier
    /// @return true if successfully deleted
    bool deleteSnapshot(const QString& snapshotId);

    /// @brief Delete all snapshots for current chapter
    void deleteAllSnapshots();

    /// @brief Rename a snapshot
    /// @param snapshotId Snapshot identifier
    /// @param newName New name for the snapshot
    /// @return true if successfully renamed
    bool renameSnapshot(const QString& snapshotId, const QString& newName);

    /// @brief Get count of snapshots for current chapter
    /// @return Number of snapshots
    int snapshotCount() const;

    // =========================================================================
    // Auto-snapshot
    // =========================================================================

    /// @brief Enable auto-snapshot at regular intervals
    /// @param minutes Interval in minutes (0 to disable)
    void setAutoSnapshotInterval(int minutes);

    /// @brief Get current auto-snapshot interval
    /// @return Interval in minutes (0 if disabled)
    int autoSnapshotInterval() const;

    /// @brief Check if content changed since last snapshot
    /// @param content Current content to check
    /// @return true if content differs from last snapshot
    bool hasChangedSinceLastSnapshot(const QString& content) const;

    /// @brief Set content provider callback for auto-snapshot
    /// @param callback Function returning current KML content
    void setContentProvider(std::function<QString()> callback);

signals:
    /// @brief Emitted when a snapshot is created
    /// @param snapshot The created snapshot info
    void snapshotCreated(const Snapshot& snapshot);

    /// @brief Emitted when a snapshot is deleted
    /// @param snapshotId ID of the deleted snapshot
    void snapshotDeleted(const QString& snapshotId);

    /// @brief Emitted when a snapshot is restored
    /// @param snapshotId ID of the restored snapshot
    void snapshotRestored(const QString& snapshotId);

    /// @brief Emitted when a snapshot is renamed
    /// @param snapshotId ID of the renamed snapshot
    /// @param newName New name
    void snapshotRenamed(const QString& snapshotId, const QString& newName);

    /// @brief Emitted when auto-snapshot triggers
    void autoSnapshotTriggered();

private slots:
    /// @brief Handle auto-snapshot timer
    void onAutoSnapshotTimer();

private:
    /// @brief Generate storage path for a snapshot
    /// @param snapshotId Snapshot identifier
    /// @return Full path to the .kml file
    QString generateSnapshotPath(const QString& snapshotId) const;

    /// @brief Get path to chapter's snapshot directory
    /// @return Directory path
    QString chapterSnapshotDir() const;

    /// @brief Get path to index file
    /// @return Full path to index.json
    QString indexFilePath() const;

    /// @brief Load snapshot index from disk
    void loadSnapshotIndex();

    /// @brief Save snapshot index to disk
    void saveSnapshotIndex();

    /// @brief Compute SHA256 hash of content
    /// @param content Content to hash
    /// @return Hex-encoded SHA256 hash
    QString computeHash(const QString& content) const;

    /// @brief Count words in content
    /// @param content KML content
    /// @return Word count (plain text words)
    int countWords(const QString& content) const;

    /// @brief Generate unique snapshot ID
    /// @return UUID-based identifier
    QString generateSnapshotId() const;

    /// @brief Ensure chapter directory exists
    /// @return true if directory exists or was created
    bool ensureChapterDir() const;

    // Configuration
    QString m_storageDir;
    QString m_chapterId;

    // Cached snapshot list
    QList<Snapshot> m_snapshots;

    // Auto-snapshot
    QTimer* m_autoSnapshotTimer{nullptr};
    int m_autoSnapshotMinutes{0};
    QString m_lastContentHash;
    std::function<QString()> m_contentProvider;
};

}  // namespace kalahari::editor
