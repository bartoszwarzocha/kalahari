/// @file file_trash.cpp
/// @brief Files to the Trash of the system (the Recycle Bin on Windows)

#include <kalahari/core/file_trash.h>
#include <kalahari/core/logger.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>

#include <utility>

#if defined(Q_OS_WIN)
#include <string>

#include <windows.h>
#elif !defined(Q_OS_MACOS)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace kalahari::core {

namespace {

/// The Trash of a test (setTrashForTests()): whether it takes a file, and moving it there
struct TestTrash {
    std::function<bool(const QString&)> canMove;
    std::function<bool(const QString&)> move;
};

TestTrash& testTrash() {
    static TestTrash trash;
    return trash;
}

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)

/// Whether folder @p name in @p parent can hold the user's deleted files: it is a folder the
/// user can write to, or it does not exist and the user can make it
bool usableTrashFolder(const QString& parent, const QString& name) {
    const QFileInfo folder(QDir(parent).filePath(name));
    if (folder.exists() || folder.isSymLink()) {
        return folder.isDir() && !folder.isSymLink() && folder.isWritable();
    }
    return QFileInfo(parent).isWritable();
}

/// The folder of the user's data ($XDG_DATA_HOME, else ~/.local/share), which has the user's
/// Trash; empty when there is none. A missing folder is made, for the user only, as the XDG
/// Base Directory specification says: Qt 6.4 does not make it, and moves the files to the
/// program's working folder then
QString userDataFolder() {
    QString folder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (folder.isEmpty() || QFileInfo(folder).isDir()) {
        return folder;
    }
    if (!QDir().mkpath(folder)) {
        Logger::getInstance().warn("Trash: the folder of the user's data {} cannot be made",
                                   folder.toStdString());
        return QString();
    }
    QFile::setPermissions(folder,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return folder;
}

/// Whether the user's Trash ($XDG_DATA_HOME/Trash) can take files
bool homeTrashTakes() {
    const QString data = userDataFolder();
    return !data.isEmpty() && usableTrashFolder(data, QStringLiteral("Trash"));
}

/// Whether the Trash of the freedesktop.org specification takes files of @p storage
bool freedesktopTrashTakes(const QStorageInfo& storage) {
    // The drive of the home folder has the user's Trash
    if (storage == QStorageInfo(QDir::homePath())) {
        return homeTrashTakes();
    }

    // Another drive: $topdir/.Trash/$uid, when an administrator made $topdir/.Trash a shared
    // folder with the sticky bit, else $topdir/.Trash-$uid
    const QString top = storage.rootPath();
    const QString user = QString::number(::getuid());
    const QString shared = QDir(top).filePath(QStringLiteral(".Trash"));
    const QFileInfo sharedInfo(shared);
    struct stat status {};
    if (sharedInfo.isDir() && !sharedInfo.isSymLink() &&
        ::stat(QFile::encodeName(shared).constData(), &status) == 0 &&
        (status.st_mode & S_ISVTX) != 0 && usableTrashFolder(shared, user)) {
        return true;
    }
    return usableTrashFolder(top, QStringLiteral(".Trash-") + user);
}

#endif

}  // namespace

bool canMoveToTrash(const QString& path) {
    if (!QFileInfo::exists(path)) {
        return false;
    }
    if (testTrash().canMove) {
        return testTrash().canMove(path);
    }
    const QStorageInfo storage(path);
    if (!storage.isValid() || !storage.isReady() || storage.isReadOnly()) {
        return false;
    }
#if defined(Q_OS_WIN)
    // Only fixed drives have a Recycle Bin
    QString root = QDir::toNativeSeparators(storage.rootPath());
    if (!root.endsWith(QLatin1Char('\\'))) {
        root += QLatin1Char('\\');
    }
    const std::wstring rootName = root.toStdWString();
    return GetDriveTypeW(rootName.c_str()) == DRIVE_FIXED;
#elif defined(Q_OS_MACOS)
    // Local volumes are devices; network volumes are named after their server
    return storage.device().startsWith("/dev/");
#else
    return freedesktopTrashTakes(storage);
#endif
}

bool moveToTrash(const QString& path) {
    auto& logger = Logger::getInstance();
    QFile file(path);
    if (!file.exists()) {
        logger.warn("moveToTrash: {} does not exist", path.toStdString());
        return false;
    }
    if (testTrash().move) {
        return testTrash().move(path) && !QFileInfo::exists(path);
    }
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    // Qt takes the user's Trash for a file whose drive has none; Qt 6.4 moves a file for which
    // it finds no Trash to the program's working folder, so such a file stays
    if (!freedesktopTrashTakes(QStorageInfo(path)) && !homeTrashTakes()) {
        logger.warn("moveToTrash: no Trash takes {}", path.toStdString());
        return false;
    }
#endif
    if (!file.moveToTrash()) {
        logger.warn("moveToTrash: {} cannot go to the Trash: {}", path.toStdString(),
                    file.errorString().toStdString());
        return false;
    }
    if (QFileInfo::exists(path)) {
        logger.warn("moveToTrash: {} is still in its folder", path.toStdString());
        return false;
    }
    logger.info("moveToTrash: {} is in the Trash: {}", path.toStdString(),
                file.fileName().toStdString());
    return true;
}

void setTrashForTests(std::function<bool(const QString&)> canMove,
                      std::function<bool(const QString&)> move) {
    testTrash().canMove = std::move(canMove);
    testTrash().move = std::move(move);
}

}  // namespace kalahari::core
