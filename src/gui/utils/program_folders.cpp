/// @file program_folders.cpp
/// @brief Implementation of ProgramFolders

#include "kalahari/gui/utils/program_folders.h"

#include "kalahari/core/settings_manager.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <vector>

namespace kalahari {
namespace gui {

namespace {

using Operation = ProgramFolders::Operation;

/// @brief The setting that keeps the folder an operation used last
const char* lastFolderSetting(Operation operation) {
    switch (operation) {
    case Operation::NewBook:
        return "project.lastFolders.newBook";
    case Operation::OpenBook:
        return "project.lastFolders.openBook";
    case Operation::OpenFile:
        return "project.lastFolders.openFile";
    case Operation::ExportArchive:
        return "project.lastFolders.exportArchive";
    case Operation::ImportArchive:
        return "project.lastFolders.importArchive";
    case Operation::ImportDestination:
        return "project.lastFolders.importDestination";
    }
    return "";
}

/// @brief A folder of the settings, or @p fallback when the setting is empty
QString settingFolder(const char* key, const QString& fallback) {
    const QString stored = ProgramFolders::cleanPath(QString::fromStdString(
        core::SettingsManager::getInstance().get<std::string>(key)));
    return stored.isEmpty() ? fallback : stored;
}

/// @brief The folder an operation starts in before it remembers one
QString settingsFolderOf(Operation operation) {
    switch (operation) {
    case Operation::NewBook:
    case Operation::OpenBook:
    case Operation::ImportDestination:
        return ProgramFolders::booksFolder();
    case Operation::ExportArchive:
    case Operation::ImportArchive:
        return ProgramFolders::archivesFolder();
    case Operation::OpenFile:
        break;
    }
    return ProgramFolders::documentsFolder();  // no setting: files can be anywhere
}

/// @brief Whether the program can create files in existing folder @p folder
bool canWriteIn(const QString& folder) {
    // A permission of a folder does not tell it on every system (Windows access lists)
    QTemporaryFile probe(QDir(folder).filePath(QStringLiteral("kalahari_XXXXXX.tmp")));
    return probe.open();
}

}  // namespace

QString ProgramFolders::documentsFolder() {
    // The tests keep away from the files of the user, as with the settings
    if (qEnvironmentVariableIsSet("KALAHARI_TEST_MODE")) {
        return cleanPath(QDir(QDir::tempPath()).filePath(QStringLiteral("Documents")));
    }
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return cleanPath(documents.isEmpty() ? QDir::homePath() : documents);
}

QString ProgramFolders::defaultBooksFolder() {
    return QDir(documentsFolder()).filePath(QStringLiteral("Kalahari"));
}

QString ProgramFolders::defaultArchivesFolder() {
    return QDir(defaultBooksFolder()).filePath(tr("Archives"));
}

QString ProgramFolders::defaultBackupsFolder() {
    return QDir(defaultBooksFolder()).filePath(tr("Backups"));
}

QString ProgramFolders::booksFolder() {
    return settingFolder(BOOKS_SETTING, defaultBooksFolder());
}

QString ProgramFolders::archivesFolder() {
    return settingFolder(ARCHIVES_SETTING, defaultArchivesFolder());
}

QString ProgramFolders::startFolder(Operation operation) {
    QString last = settingFolder(lastFolderSetting(operation), QString());
    if (!last.isEmpty() && QFileInfo(last).isDir()) {
        return last;
    }
    return settingsFolderOf(operation);
}

QString ProgramFolders::windowFolder(Operation operation) {
    QString folder = startFolder(operation);
    if (QFileInfo(folder).isDir() || QDir().mkpath(folder)) {
        return folder;
    }
    const QString existing = existingFolderAt(folder);
    return existing.isEmpty() ? documentsFolder() : existing;
}

void ProgramFolders::remember(Operation operation, const QString& folder) {
    const QString clean = cleanPath(folder);
    if (!clean.isEmpty()) {
        core::SettingsManager::getInstance().set<std::string>(lastFolderSetting(operation),
                                                              clean.toStdString());
    }
}

void ProgramFolders::forgetFolders(const std::string& settingKey) {
    std::vector<Operation> operations;
    if (settingKey == BOOKS_SETTING) {
        operations = {Operation::NewBook, Operation::OpenBook, Operation::ImportDestination};
    } else if (settingKey == ARCHIVES_SETTING) {
        operations = {Operation::ExportArchive, Operation::ImportArchive};
    }
    auto& settings = core::SettingsManager::getInstance();
    for (Operation operation : operations) {
        settings.set<std::string>(lastFolderSetting(operation), std::string());
    }
}

QString ProgramFolders::problemOf(const QString& folder) {
    const QString path = cleanPath(folder);
    if (path.isEmpty() || QDir::isRelativePath(path)) {
        return tr("Give the full path of the folder.");
    }
    const QFileInfo info(path);
    if (info.exists() && !info.isDir()) {
        return tr("This is a file, not a folder.");
    }

    // A missing folder is created when it is first needed, in the nearest existing one
    QString above = path;
    while (!QFileInfo::exists(above)) {
        const QString parent = QFileInfo(above).absolutePath();
        if (parent == above) {
            return tr("The drive of this folder is not available.");
        }
        above = parent;
    }
    if (!QFileInfo(above).isDir()) {
        return tr("Kalahari cannot create this folder.");  // a file is in the way
    }
    if (!canWriteIn(above)) {
        return info.exists() ? tr("Kalahari cannot save files in this folder.")
                             : tr("Kalahari cannot create this folder.");
    }
    return QString();
}

QString ProgramFolders::existingFolderAt(const QString& folder) {
    QString path = cleanPath(folder);
    if (path.isEmpty() || QDir::isRelativePath(path)) {
        return QString();
    }
    while (!QFileInfo(path).isDir()) {
        const QString parent = QFileInfo(path).absolutePath();
        if (parent == path) {
            return QString();
        }
        path = parent;
    }
    return path;
}

QString ProgramFolders::cleanPath(const QString& path) {
    const QString trimmed = path.trimmed();
    return trimmed.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(trimmed));
}

bool ProgramFolders::samePath(const QString& first, const QString& second) {
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity CASE = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity CASE = Qt::CaseSensitive;
#endif
    return cleanPath(first).compare(cleanPath(second), CASE) == 0;
}

}  // namespace gui
}  // namespace kalahari
