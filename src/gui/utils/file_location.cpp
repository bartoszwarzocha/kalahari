/// @file file_location.cpp
/// @brief A file or a folder shown in the file manager of the system, and a file opened in the
/// program the system gives it

#include "kalahari/gui/utils/file_location.h"
#include "kalahari/core/logger.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QUrl>

#include <utility>

namespace kalahari {
namespace gui {

namespace {

std::function<bool(const QString&)>& testFileManager() {
    static std::function<bool(const QString&)> show;
    return show;
}

std::function<bool(const QString&)>& testFileOpener() {
    static std::function<bool(const QString&)> open;
    return open;
}

std::function<bool(const QUrl&)>& testBrowser() {
    static std::function<bool(const QUrl&)> open;
    return open;
}

/// Start the file manager of the system on @p info
bool startFileManager(const QFileInfo& info) {
    const QString path = QDir::toNativeSeparators(info.absoluteFilePath());
#if defined(Q_OS_WIN)
    // Explorer selects the file in its folder
    QStringList arguments;
    if (!info.isDir()) {
        arguments << QStringLiteral("/select,");
    }
    arguments << path;
    return QProcess::startDetached(QStringLiteral("explorer.exe"), arguments);
#elif defined(Q_OS_MACOS)
    // The Finder selects the file in its folder
    return QProcess::startDetached(QStringLiteral("/usr/bin/open"),
                                   info.isDir() ? QStringList{path}
                                                : QStringList{QStringLiteral("-R"), path});
#else
    // The file managers open the folder: there is no common way to select a file in it
    return QDesktopServices::openUrl(
        QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath()));
#endif
}

} // namespace

bool showInFolder(const QString& path) {
    auto& logger = core::Logger::getInstance();
    const QFileInfo info(path);
    if (!info.exists()) {
        logger.warn("showInFolder: {} does not exist", path.toStdString());
        return false;
    }
    const bool shown = testFileManager() ? testFileManager()(info.absoluteFilePath())
                                         : startFileManager(info);
    if (shown) {
        logger.info("showInFolder: {}", path.toStdString());
    } else {
        logger.warn("showInFolder: The file manager could not show {}", path.toStdString());
    }
    return shown;
}

void setFileManagerForTests(std::function<bool(const QString&)> show) {
    testFileManager() = std::move(show);
}

bool openWithSystem(const QString& path) {
    auto& logger = core::Logger::getInstance();
    const QFileInfo info(path);
    if (!info.isFile()) {
        logger.warn("openWithSystem: {} is not a file", path.toStdString());
        return false;
    }
    const bool opened =
        testFileOpener() ? testFileOpener()(info.absoluteFilePath())
                         : QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
    if (opened) {
        logger.info("openWithSystem: {}", path.toStdString());
    } else {
        logger.warn("openWithSystem: The system could not open {}", path.toStdString());
    }
    return opened;
}

void setFileOpenerForTests(std::function<bool(const QString&)> open) {
    testFileOpener() = std::move(open);
}

bool openInBrowser(const QUrl& url) {
    auto& logger = core::Logger::getInstance();
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
        logger.warn("openInBrowser: {} is not a web address", url.toString().toStdString());
        return false;
    }
    const bool opened = testBrowser() ? testBrowser()(url) : QDesktopServices::openUrl(url);
    if (opened) {
        logger.info("openInBrowser: {}", url.toString().toStdString());
    } else {
        logger.warn("openInBrowser: The system could not open {}", url.toString().toStdString());
    }
    return opened;
}

void setBrowserForTests(std::function<bool(const QUrl&)> open) {
    testBrowser() = std::move(open);
}

} // namespace gui
} // namespace kalahari
