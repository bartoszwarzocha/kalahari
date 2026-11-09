/// @file chapter_versions.cpp
/// @brief Versions of the text of a chapter file

#include <kalahari/core/chapter_versions.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/file_trash.h>
#include <kalahari/core/logger.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace kalahari {
namespace core {

namespace {

/// Extension of the chapter files, left out of the names of the folders of their versions
constexpr QLatin1String CHAPTER_EXTENSION(".kchapter");

/// Extension of the files of the versions
constexpr QLatin1String VERSION_EXTENSION(".kml");

/// Whether @p name can be the file of a version in the folder of the versions: a plain name of
/// a .kml file, not a path (index.json written by hand cannot reach other files)
bool isVersionFileName(const QString& name) {
    return name.endsWith(VERSION_EXTENSION) && name.size() > VERSION_EXTENSION.size() &&
           !name.startsWith(QLatin1Char('.')) && !name.contains(QLatin1Char('/')) &&
           !name.contains(QLatin1Char('\\')) && !name.contains(QLatin1Char(':'));
}

/// Write @p data to file @p path in one step: a failed write leaves the file as it was
bool writeWhole(const QString& path, const QByteArray& data) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::getInstance().error("ChapterVersions: Cannot write {}: {}", path.toStdString(),
                                    file.errorString().toStdString());
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        Logger::getInstance().error("ChapterVersions: Cannot write {}: {}", path.toStdString(),
                                    file.errorString().toStdString());
        file.cancelWriting();
        return false;
    }
    return true;
}

/// Newest first; versions kept at the same moment in the order of index.json
void sortNewestFirst(QList<ChapterVersion>& versions) {
    std::stable_sort(versions.begin(), versions.end(),
                     [](const ChapterVersion& a, const ChapterVersion& b) {
                         return a.created > b.created;
                     });
}

}  // namespace

QString versionOriginName(VersionOrigin origin) {
    switch (origin) {
    case VersionOrigin::Manual:
        return QStringLiteral("manual");
    case VersionOrigin::Save:
        return QStringLiteral("save");
    case VersionOrigin::ReplaceAll:
        return QStringLiteral("replaceAll");
    case VersionOrigin::Restore:
        return QStringLiteral("restore");
    }
    return QStringLiteral("manual");
}

std::optional<VersionOrigin> versionOriginNamed(const QString& name) {
    for (const VersionOrigin origin : {VersionOrigin::Manual, VersionOrigin::Save,
                                       VersionOrigin::ReplaceAll, VersionOrigin::Restore}) {
        if (versionOriginName(origin) == name) {
            return origin;
        }
    }
    return std::nullopt;
}

int ChapterVersions::countAll(const QString& projectDir) {
    if (projectDir.isEmpty()) {
        return 0;
    }
    // A folder of versions is named after its file without ".kchapter": the file with it again
    // has that folder
    const QDir root(QDir(projectDir).filePath(QString::fromLatin1(FOLDER)));
    int count = 0;
    QDirIterator indexes(root.path(), {QString::fromLatin1(INDEX_FILE)}, QDir::Files,
                         QDirIterator::Subdirectories);
    while (indexes.hasNext()) {
        const QString relative = root.relativeFilePath(QFileInfo(indexes.next()).path());
        if (relative == QLatin1String(".") || relative.startsWith(QLatin1String(".."))) {
            continue;
        }
        count += static_cast<int>(
            ChapterVersions(projectDir, relative + CHAPTER_EXTENSION).count());
    }
    return count;
}

QString ChapterVersions::folderOf(const QString& projectDir, const QString& file) {
    QString path = file;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (projectDir.isEmpty() || path.isEmpty() || QDir::isAbsolutePath(path) ||
        path.startsWith(QLatin1Char('/')) || path.contains(QLatin1Char(':'))) {
        return QString();
    }
    QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    parts.removeAll(QStringLiteral("."));
    if (parts.isEmpty() || parts.contains(QStringLiteral(".."))) {
        return QString();
    }
    QString& name = parts.last();
    if (name.endsWith(CHAPTER_EXTENSION, Qt::CaseInsensitive)) {
        name.chop(CHAPTER_EXTENSION.size());
    }
    if (name.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(QDir(projectDir).filePath(QString::fromLatin1(FOLDER) +
                                                     QLatin1Char('/') +
                                                     parts.join(QLatin1Char('/'))));
}

ChapterVersions::ChapterVersions(const QString& projectDir, const QString& file)
    : m_file(file),
      m_folder(folderOf(projectDir, file)),
      m_root(m_folder.isEmpty() ? QString()
                                : QDir::cleanPath(QDir(projectDir).filePath(
                                      QString::fromLatin1(FOLDER)))) {}

QList<ChapterVersion> ChapterVersions::list() const {
    return read().versions;
}

std::optional<ChapterVersion> ChapterVersions::newest() const {
    const QList<ChapterVersion> versions = list();
    if (versions.isEmpty()) {
        return std::nullopt;
    }
    return versions.first();
}

std::optional<ChapterVersion> ChapterVersions::find(const QString& versionFile) const {
    for (const ChapterVersion& version : list()) {
        if (version.file == versionFile) {
            return version;
        }
    }
    return std::nullopt;
}

std::optional<QString> ChapterVersions::text(const QString& versionFile) const {
    if (!isValid() || !isVersionFileName(versionFile) || !find(versionFile)) {
        return std::nullopt;
    }
    QFile file(QDir(m_folder).filePath(versionFile));
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::getInstance().warn("ChapterVersions: Cannot read {}: {}",
                                   file.fileName().toStdString(),
                                   file.errorString().toStdString());
        return std::nullopt;
    }
    return QString::fromUtf8(file.readAll());
}

bool ChapterVersions::newestHas(const QString& kml) const {
    const std::optional<ChapterVersion> last = newest();
    if (!last) {
        return false;
    }
    const std::optional<QString> lastText = text(last->file);
    return lastText && *lastText == kml;
}

std::optional<QDateTime> ChapterVersions::lastCreated(VersionOrigin origin) const {
    for (const ChapterVersion& version : list()) {
        if (version.origin == origin) {
            return version.created;
        }
    }
    return std::nullopt;
}

std::optional<ChapterVersion> ChapterVersions::add(const QString& kml,
                                                   const ChapterVersion& about,
                                                   const QDateTime& now) {
    auto& logger = Logger::getInstance();
    if (!isValid()) {
        logger.warn("ChapterVersions::add: {} is not a file of the book", m_file.toStdString());
        return std::nullopt;
    }
    Index index = read();
    if (index.format > FORMAT) {
        logger.warn("ChapterVersions::add: {} is of a newer format ({}); it is not changed",
                    QDir(m_folder).filePath(QString::fromLatin1(INDEX_FILE)).toStdString(),
                    index.format);
        return std::nullopt;
    }
    if (!QDir().mkpath(m_folder)) {
        logger.error("ChapterVersions::add: Cannot make folder {}", m_folder.toStdString());
        return std::nullopt;
    }

    const QDateTime created = (now.isValid() ? now : QDateTime::currentDateTimeUtc()).toUTC();

    ChapterVersion version;
    version.file = newVersionFile(created, index.versions);
    version.created = created;
    version.origin = about.origin;
    version.name = about.name;
    if (about.origin == VersionOrigin::ReplaceAll) {
        version.found = about.found;
        version.replacement = about.replacement;
        version.replaced = about.replaced;
    }
    if (about.origin == VersionOrigin::Restore && about.restored.isValid()) {
        version.restored = about.restored.toUTC();
    }
    version.words = tallyWords(ChapterDocument::kmlToPlainText(kml));

    const QString path = QDir(m_folder).filePath(version.file);
    if (!writeWhole(path, kml.toUtf8())) {
        removeEmptyFolders();
        return std::nullopt;
    }
    index.versions.prepend(version);
    sortNewestFirst(index.versions);
    if (!write(index)) {
        QFile::remove(path);
        removeEmptyFolders();
        return std::nullopt;
    }
    logger.info("ChapterVersions: Kept a version of {} ({}): {}", m_file.toStdString(),
                versionOriginName(version.origin).toStdString(), version.file.toStdString());
    return version;
}

bool ChapterVersions::rename(const QString& versionFile, const QString& name) {
    Index index = read();
    for (ChapterVersion& version : index.versions) {
        if (version.file == versionFile) {
            version.name = name;
            return write(index);
        }
    }
    return false;
}

bool ChapterVersions::remove(const QString& versionFile) {
    Index index = read();
    const auto found =
        std::find_if(index.versions.begin(), index.versions.end(),
                     [&versionFile](const ChapterVersion& version) {
                         return version.file == versionFile;
                     });
    if (found == index.versions.end()) {
        return false;
    }
    index.versions.erase(found);
    if (!write(index)) {
        return false;
    }
    const QString path = QDir(m_folder).filePath(versionFile);
    if (!QFile::remove(path)) {
        Logger::getInstance().warn("ChapterVersions::remove: {} stays on the disk",
                                   path.toStdString());
    }
    removeEmptyFolders();
    return true;
}

int ChapterVersions::removeOldAutomatic(int keep) {
    Index index = read();
    QStringList gone;
    int automatic = 0;
    QList<ChapterVersion> kept;
    for (const ChapterVersion& version : std::as_const(index.versions)) {  // newest first
        if (version.isAutomatic() && ++automatic > std::max(keep, 0)) {
            gone << version.file;
        } else {
            kept << version;
        }
    }
    if (gone.isEmpty()) {
        return 0;
    }
    index.versions = kept;
    if (!write(index)) {
        return -1;
    }
    for (const QString& file : std::as_const(gone)) {
        QFile::remove(QDir(m_folder).filePath(file));
    }
    removeEmptyFolders();
    Logger::getInstance().info("ChapterVersions: {} old versions of {} deleted", gone.size(),
                               m_file.toStdString());
    return static_cast<int>(gone.size());
}

ChapterVersions::Index ChapterVersions::read() const {
    Index index;
    if (!isValid()) {
        return index;
    }
    QFile file(QDir(m_folder).filePath(QString::fromLatin1(INDEX_FILE)));
    if (!file.exists()) {
        return index;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::getInstance().warn("ChapterVersions: Cannot read {}: {}",
                                   file.fileName().toStdString(),
                                   file.errorString().toStdString());
        index.format = FORMAT + 1;  // not written over
        return index;
    }
    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !json.isObject()) {
        Logger::getInstance().warn("ChapterVersions: {} is not a list of versions: {}",
                                   file.fileName().toStdString(),
                                   error.errorString().toStdString());
        index.format = FORMAT + 1;  // the writer's file, kept as it is
        return index;
    }
    const QJsonObject root = json.object();
    index.format = root.value(QLatin1String("format")).toInt(FORMAT);

    const QDir folder(m_folder);
    const QJsonArray entries = root.value(QLatin1String("versions")).toArray();
    for (const auto& value : entries) {
        const QJsonObject entry = value.toObject();
        ChapterVersion version;
        version.file = entry.value(QLatin1String("file")).toString();
        if (!isVersionFileName(version.file) ||
            !QFileInfo(folder.filePath(version.file)).isFile()) {
            continue;
        }
        version.created = QDateTime::fromString(entry.value(QLatin1String("created")).toString(),
                                                Qt::ISODateWithMs)
                              .toUTC();
        // An origin the program does not know is the writer's: the program does not delete it
        version.origin =
            versionOriginNamed(entry.value(QLatin1String("origin")).toString())
                .value_or(VersionOrigin::Manual);
        version.name = entry.value(QLatin1String("name")).toString();
        version.found = entry.value(QLatin1String("find")).toString();
        version.replacement = entry.value(QLatin1String("replace")).toString();
        version.replaced = std::max(entry.value(QLatin1String("replaced")).toInt(), 0);
        if (entry.contains(QLatin1String("restored"))) {
            version.restored =
                QDateTime::fromString(entry.value(QLatin1String("restored")).toString(),
                                      Qt::ISODateWithMs)
                    .toUTC();
        }
        version.words.words = std::max(entry.value(QLatin1String("words")).toInt(), 0);
        version.words.dashes = std::max(entry.value(QLatin1String("dashes")).toInt(), 0);
        index.versions << version;
    }
    sortNewestFirst(index.versions);
    return index;
}

bool ChapterVersions::write(const Index& index) const {
    const QString path = QDir(m_folder).filePath(QString::fromLatin1(INDEX_FILE));
    if (!isValid() || index.format > FORMAT) {
        Logger::getInstance().warn("ChapterVersions: {} is not written", path.toStdString());
        return false;
    }
    if (index.versions.isEmpty()) {
        return !QFileInfo::exists(path) || QFile::remove(path);
    }

    QJsonArray versions;
    for (const ChapterVersion& version : index.versions) {
        QJsonObject entry;
        entry.insert(QLatin1String("file"), version.file);
        entry.insert(QLatin1String("created"), version.created.toUTC().toString(Qt::ISODateWithMs));
        entry.insert(QLatin1String("origin"), versionOriginName(version.origin));
        if (!version.name.isEmpty()) {
            entry.insert(QLatin1String("name"), version.name);
        }
        if (version.origin == VersionOrigin::ReplaceAll) {
            entry.insert(QLatin1String("find"), version.found);
            entry.insert(QLatin1String("replace"), version.replacement);
            entry.insert(QLatin1String("replaced"), version.replaced);
        }
        if (version.restored.isValid()) {
            entry.insert(QLatin1String("restored"),
                         version.restored.toUTC().toString(Qt::ISODateWithMs));
        }
        entry.insert(QLatin1String("words"), version.words.words);
        entry.insert(QLatin1String("dashes"), version.words.dashes);
        versions.append(entry);
    }
    QJsonObject root;
    root.insert(QLatin1String("format"), FORMAT);
    root.insert(QLatin1String("file"), m_file);
    root.insert(QLatin1String("versions"), versions);
    return writeWhole(path, QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool ChapterVersions::removeFolder(bool trash) {
    if (!isValid()) {
        return false;
    }
    if (!QFileInfo(m_folder).isDir()) {
        return true;
    }
    const bool gone = trash ? moveToTrash(m_folder) : QDir(m_folder).removeRecursively();
    if (!gone || QFileInfo::exists(m_folder)) {
        Logger::getInstance().warn("ChapterVersions: The versions of {} stay in {}",
                                   m_file.toStdString(), m_folder.toStdString());
        return false;
    }
    Logger::getInstance().info("ChapterVersions: The versions of {} went {}",
                               m_file.toStdString(), trash ? "to the Trash" : "for good");
    removeEmptyFolders();
    return true;
}

void ChapterVersions::removeEmptyFolders() const {
    if (!isValid()) {
        return;
    }
    // m_folder is below m_root (folderOf()); versions/ goes too when no file has versions
    QString path = m_folder;
    while (path == m_root || path.startsWith(m_root + QLatin1Char('/'))) {
        const QFileInfo info(path);
        // A folder that is gone already leaves the ones above it to this loop
        if (info.exists() && (!QDir(path).isEmpty(QDir::AllEntries | QDir::Hidden |
                                                  QDir::System | QDir::NoDotAndDotDot) ||
                              !info.dir().rmdir(info.fileName()))) {
            return;
        }
        path = info.path();
    }
}

QString ChapterVersions::newVersionFile(const QDateTime& created,
                                        const QList<ChapterVersion>& listed) const {
    const QString base = created.toLocalTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss"));
    const QDir folder(m_folder);
    for (int number = 1;; ++number) {
        const QString name = (number == 1 ? base : QStringLiteral("%1_%2").arg(base).arg(number)) +
                             VERSION_EXTENSION;
        const bool taken = QFileInfo::exists(folder.filePath(name)) ||
                           std::any_of(listed.cbegin(), listed.cend(),
                                       [&name](const ChapterVersion& version) {
                                           return version.file.compare(
                                                      name, Qt::CaseInsensitive) == 0;
                                       });
        if (!taken) {
            return name;
        }
    }
}

}  // namespace core
}  // namespace kalahari
