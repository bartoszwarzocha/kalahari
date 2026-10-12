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

#if defined(Q_OS_WIN)

/// Whether the number @p name in key @p key of the registry @p root is there and is not 0
bool registryFlag(HKEY root, const std::wstring& key, const wchar_t* name) {
    DWORD value = 0;
    DWORD size = static_cast<DWORD>(sizeof(value));
    return RegGetValueW(root, key.c_str(), name, RRF_RT_REG_DWORD, nullptr, &value, &size) ==
               ERROR_SUCCESS &&
           value != 0;
}

/// Whether the Recycle Bin of the drive with the root folder @p root is switched off, so that
/// Windows deletes its files for good: the writer chose "Don't move files to the Recycle Bin"
/// for the drive, or a policy of the user or of the computer says "Do not move deleted files to
/// the Recycle Bin"
bool recycleBinSwitchedOff(const std::wstring& root) {
    const std::wstring policies =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";
    if (registryFlag(HKEY_CURRENT_USER, policies, L"NoRecycleFiles") ||
        registryFlag(HKEY_LOCAL_MACHINE, policies, L"NoRecycleFiles")) {
        return true;
    }

    // The settings of a drive are under the GUID of its volume, the part in braces of the name
    // of the volume
    wchar_t volume[MAX_PATH] = {};
    if (!GetVolumeNameForVolumeMountPointW(root.c_str(), volume, MAX_PATH)) {
        return false;
    }
    const std::wstring name(volume);
    const std::size_t open = name.find(L'{');
    const std::size_t close = name.find(L'}', open);
    if (open == std::wstring::npos || close == std::wstring::npos) {
        return false;
    }
    const std::wstring drive =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\BitBucket\\Volume\\" +
        name.substr(open, close - open + 1);
    return registryFlag(HKEY_CURRENT_USER, drive, L"NukeOnDelete");
}

#endif

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
    // Only fixed drives have a Recycle Bin, and the writer or a policy can switch it off
    QString root = QDir::toNativeSeparators(storage.rootPath());
    if (!root.endsWith(QLatin1Char('\\'))) {
        root += QLatin1Char('\\');
    }
    const std::wstring rootName = root.toStdWString();
    return GetDriveTypeW(rootName.c_str()) == DRIVE_FIXED && !recycleBinSwitchedOff(rootName);
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
