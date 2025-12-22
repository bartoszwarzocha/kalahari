/// @file snapshot_manager.cpp
/// @brief Chapter snapshot (restore points) implementation (OpenSpec #00042 Task 7.13)

#include <kalahari/editor/snapshot_manager.h>
#include <kalahari/core/logger.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QUuid>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// SnapshotManager
// =============================================================================

SnapshotManager::SnapshotManager(QObject* parent)
    : QObject(parent)
    , m_autoSnapshotTimer(new QTimer(this))
{
    connect(m_autoSnapshotTimer, &QTimer::timeout,
            this, &SnapshotManager::onAutoSnapshotTimer);

    core::Logger::getInstance().debug("SnapshotManager created");
}

SnapshotManager::~SnapshotManager()
{
    if (m_autoSnapshotTimer->isActive()) {
        m_autoSnapshotTimer->stop();
    }

    core::Logger::getInstance().debug("SnapshotManager destroyed");
}

// =============================================================================
// Setup
// =============================================================================

void SnapshotManager::setStorageDir(const QString& dir)
{
    if (m_storageDir == dir) {
        return;
    }

    m_storageDir = dir;
    m_snapshots.clear();

    // Reload snapshots for current chapter if set
    if (!m_chapterId.isEmpty()) {
        loadSnapshotIndex();
    }

    core::Logger::getInstance().debug("SnapshotManager storage dir set: {}", dir.toStdString());
}

QString SnapshotManager::storageDir() const
{
    return m_storageDir;
}

void SnapshotManager::setChapterId(const QString& chapterId)
{
    if (m_chapterId == chapterId) {
        return;
    }

    m_chapterId = chapterId;
    m_snapshots.clear();
    m_lastContentHash.clear();

    // Load snapshots for new chapter
    if (!m_storageDir.isEmpty() && !m_chapterId.isEmpty()) {
        loadSnapshotIndex();
    }

    core::Logger::getInstance().debug("SnapshotManager chapter set: {}", chapterId.toStdString());
}

QString SnapshotManager::chapterId() const
{
    return m_chapterId;
}

// =============================================================================
// Snapshot Operations
// =============================================================================

Snapshot SnapshotManager::createSnapshot(const QString& content, const QString& name)
{
    Snapshot snapshot;

    if (m_storageDir.isEmpty() || m_chapterId.isEmpty()) {
        core::Logger::getInstance().warn("SnapshotManager::createSnapshot - storage dir or chapter not set");
        return snapshot;
    }

    // Ensure directory exists
    if (!ensureChapterDir()) {
        core::Logger::getInstance().error("SnapshotManager::createSnapshot - failed to create directory");
        return snapshot;
    }

    // Generate snapshot info
    snapshot.id = generateSnapshotId();
    snapshot.chapterId = m_chapterId;
    snapshot.name = name.isEmpty() ? QString() : name;
    snapshot.createdAt = QDateTime::currentDateTime();
    snapshot.wordCount = countWords(content);
    snapshot.contentHash = computeHash(content);
    snapshot.filePath = generateSnapshotPath(snapshot.id);

    // Write content to file
    QFile file(snapshot.filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        core::Logger::getInstance().error("SnapshotManager::createSnapshot - failed to open file: {}",
                                          snapshot.filePath.toStdString());
        return Snapshot();
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    file.close();

    // Add to cache and save index
    m_snapshots.prepend(snapshot);  // Newest first
    saveSnapshotIndex();

    // Update last content hash
    m_lastContentHash = snapshot.contentHash;

    core::Logger::getInstance().info("Snapshot created: {} ({})",
                                     snapshot.id.toStdString(),
                                     name.isEmpty() ? "unnamed" : name.toStdString());

    emit snapshotCreated(snapshot);

    return snapshot;
}

QList<Snapshot> SnapshotManager::listSnapshots() const
{
    return m_snapshots;
}

std::optional<Snapshot> SnapshotManager::getSnapshot(const QString& snapshotId) const
{
    auto it = std::find_if(m_snapshots.begin(), m_snapshots.end(),
                           [&snapshotId](const Snapshot& s) { return s.id == snapshotId; });

    if (it != m_snapshots.end()) {
        return *it;
    }

    return std::nullopt;
}

QString SnapshotManager::loadSnapshotContent(const QString& snapshotId) const
{
    auto snapshot = getSnapshot(snapshotId);
    if (!snapshot) {
        core::Logger::getInstance().warn("SnapshotManager::loadSnapshotContent - snapshot not found: {}",
                                         snapshotId.toStdString());
        return QString();
    }

    QFile file(snapshot->filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        core::Logger::getInstance().error("SnapshotManager::loadSnapshotContent - failed to open file: {}",
                                          snapshot->filePath.toStdString());
        return QString();
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    file.close();

    core::Logger::getInstance().debug("SnapshotManager::loadSnapshotContent - loaded {} bytes",
                                      content.size());

    return content;
}

bool SnapshotManager::deleteSnapshot(const QString& snapshotId)
{
    auto it = std::find_if(m_snapshots.begin(), m_snapshots.end(),
                           [&snapshotId](const Snapshot& s) { return s.id == snapshotId; });

    if (it == m_snapshots.end()) {
        core::Logger::getInstance().warn("SnapshotManager::deleteSnapshot - snapshot not found: {}",
                                         snapshotId.toStdString());
        return false;
    }

    // Delete the file
    QString filePath = it->filePath;
    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            core::Logger::getInstance().error("SnapshotManager::deleteSnapshot - failed to delete file: {}",
                                              filePath.toStdString());
            return false;
        }
    }

    // Remove from cache
    m_snapshots.erase(it);

    // Save index
    saveSnapshotIndex();

    core::Logger::getInstance().info("Snapshot deleted: {}", snapshotId.toStdString());

    emit snapshotDeleted(snapshotId);

    return true;
}

void SnapshotManager::deleteAllSnapshots()
{
    if (m_snapshots.isEmpty()) {
        return;
    }

    // Delete all snapshot files
    for (const auto& snapshot : m_snapshots) {
        if (QFile::exists(snapshot.filePath)) {
            QFile::remove(snapshot.filePath);
        }
    }

    // Clear cache
    QStringList deletedIds;
    for (const auto& snapshot : m_snapshots) {
        deletedIds.append(snapshot.id);
    }

    m_snapshots.clear();

    // Delete index file
    QString indexPath = indexFilePath();
    if (QFile::exists(indexPath)) {
        QFile::remove(indexPath);
    }

    // Emit signals for each deleted snapshot
    for (const QString& id : deletedIds) {
        emit snapshotDeleted(id);
    }

    core::Logger::getInstance().info("All snapshots deleted for chapter: {}", m_chapterId.toStdString());
}

bool SnapshotManager::renameSnapshot(const QString& snapshotId, const QString& newName)
{
    auto it = std::find_if(m_snapshots.begin(), m_snapshots.end(),
                           [&snapshotId](const Snapshot& s) { return s.id == snapshotId; });

    if (it == m_snapshots.end()) {
        return false;
    }

    it->name = newName;
    saveSnapshotIndex();

    core::Logger::getInstance().debug("Snapshot renamed: {} -> {}",
                                      snapshotId.toStdString(), newName.toStdString());

    emit snapshotRenamed(snapshotId, newName);

    return true;
}

int SnapshotManager::snapshotCount() const
{
    return m_snapshots.size();
}

// =============================================================================
// Auto-snapshot
// =============================================================================

void SnapshotManager::setAutoSnapshotInterval(int minutes)
{
    m_autoSnapshotMinutes = minutes;

    if (minutes <= 0) {
        m_autoSnapshotTimer->stop();
        core::Logger::getInstance().debug("Auto-snapshot disabled");
    } else {
        m_autoSnapshotTimer->setInterval(minutes * 60 * 1000);
        m_autoSnapshotTimer->start();
        core::Logger::getInstance().debug("Auto-snapshot enabled: {} minutes", minutes);
    }
}

int SnapshotManager::autoSnapshotInterval() const
{
    return m_autoSnapshotMinutes;
}

bool SnapshotManager::hasChangedSinceLastSnapshot(const QString& content) const
{
    if (m_lastContentHash.isEmpty()) {
        return true;  // No previous snapshot
    }

    QString currentHash = computeHash(content);
    return currentHash != m_lastContentHash;
}

void SnapshotManager::setContentProvider(std::function<QString()> callback)
{
    m_contentProvider = std::move(callback);
}

// =============================================================================
// Private Slots
// =============================================================================

void SnapshotManager::onAutoSnapshotTimer()
{
    if (!m_contentProvider) {
        core::Logger::getInstance().debug("Auto-snapshot skipped - no content provider");
        return;
    }

    QString content = m_contentProvider();

    if (content.isEmpty()) {
        core::Logger::getInstance().debug("Auto-snapshot skipped - empty content");
        return;
    }

    if (!hasChangedSinceLastSnapshot(content)) {
        core::Logger::getInstance().debug("Auto-snapshot skipped - no changes");
        return;
    }

    // Create auto-snapshot with timestamp name
    QString autoName = tr("Auto-save %1").arg(
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));

    createSnapshot(content, autoName);

    emit autoSnapshotTriggered();

    core::Logger::getInstance().info("Auto-snapshot created");
}

// =============================================================================
// Private Methods
// =============================================================================

QString SnapshotManager::generateSnapshotPath(const QString& snapshotId) const
{
    return chapterSnapshotDir() + "/" + snapshotId + ".kml";
}

QString SnapshotManager::chapterSnapshotDir() const
{
    return m_storageDir + "/" + m_chapterId;
}

QString SnapshotManager::indexFilePath() const
{
    return chapterSnapshotDir() + "/index.json";
}

void SnapshotManager::loadSnapshotIndex()
{
    m_snapshots.clear();

    QString indexPath = indexFilePath();
    if (!QFile::exists(indexPath)) {
        core::Logger::getInstance().debug("SnapshotManager::loadSnapshotIndex - no index file");
        return;
    }

    QFile file(indexPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        core::Logger::getInstance().error("SnapshotManager::loadSnapshotIndex - failed to open: {}",
                                          indexPath.toStdString());
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        core::Logger::getInstance().error("SnapshotManager::loadSnapshotIndex - JSON parse error: {}",
                                          parseError.errorString().toStdString());
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray snapshotsArray = root["snapshots"].toArray();

    for (const auto& value : snapshotsArray) {
        QJsonObject obj = value.toObject();

        Snapshot snapshot;
        snapshot.id = obj["id"].toString();
        snapshot.chapterId = obj["chapterId"].toString();
        snapshot.name = obj["name"].toString();
        snapshot.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        snapshot.wordCount = obj["wordCount"].toInt();
        snapshot.contentHash = obj["contentHash"].toString();
        snapshot.filePath = generateSnapshotPath(snapshot.id);

        // Verify file exists
        if (QFile::exists(snapshot.filePath)) {
            m_snapshots.append(snapshot);
        } else {
            core::Logger::getInstance().warn("SnapshotManager::loadSnapshotIndex - missing file: {}",
                                             snapshot.filePath.toStdString());
        }
    }

    // Sort by creation time (newest first)
    std::sort(m_snapshots.begin(), m_snapshots.end(),
              [](const Snapshot& a, const Snapshot& b) {
                  return a.createdAt > b.createdAt;
              });

    // Update last content hash from most recent snapshot
    if (!m_snapshots.isEmpty()) {
        m_lastContentHash = m_snapshots.first().contentHash;
    }

    core::Logger::getInstance().debug("SnapshotManager::loadSnapshotIndex - loaded {} snapshots",
                                      m_snapshots.size());
}

void SnapshotManager::saveSnapshotIndex()
{
    if (!ensureChapterDir()) {
        return;
    }

    QJsonObject root;
    root["chapterId"] = m_chapterId;

    QJsonArray snapshotsArray;
    for (const auto& snapshot : m_snapshots) {
        QJsonObject obj;
        obj["id"] = snapshot.id;
        obj["chapterId"] = snapshot.chapterId;
        obj["name"] = snapshot.name;
        obj["createdAt"] = snapshot.createdAt.toString(Qt::ISODate);
        obj["wordCount"] = snapshot.wordCount;
        obj["contentHash"] = snapshot.contentHash;
        snapshotsArray.append(obj);
    }

    root["snapshots"] = snapshotsArray;

    QJsonDocument doc(root);

    QString indexPath = indexFilePath();
    QFile file(indexPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        core::Logger::getInstance().error("SnapshotManager::saveSnapshotIndex - failed to open: {}",
                                          indexPath.toStdString());
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    core::Logger::getInstance().debug("SnapshotManager::saveSnapshotIndex - saved {} snapshots",
                                      m_snapshots.size());
}

QString SnapshotManager::computeHash(const QString& content) const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(content.toUtf8());
    return "sha256:" + QString::fromLatin1(hash.result().toHex());
}

int SnapshotManager::countWords(const QString& content) const
{
    if (content.isEmpty()) {
        return 0;
    }

    // Strip XML/KML tags for word counting
    QString plainText = content;
    plainText.remove(QRegularExpression("<[^>]*>"));

    // Count words
    static QRegularExpression wordRe("\\b\\w+\\b");
    auto matches = wordRe.globalMatch(plainText);

    int count = 0;
    while (matches.hasNext()) {
        matches.next();
        ++count;
    }

    return count;
}

QString SnapshotManager::generateSnapshotId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool SnapshotManager::ensureChapterDir() const
{
    QString dirPath = chapterSnapshotDir();
    QDir dir(dirPath);

    if (dir.exists()) {
        return true;
    }

    if (!dir.mkpath(".")) {
        core::Logger::getInstance().error("SnapshotManager::ensureChapterDir - failed to create: {}",
                                          dirPath.toStdString());
        return false;
    }

    return true;
}

}  // namespace kalahari::editor
