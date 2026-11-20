// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file installation.cpp
/// @brief The program as it is installed: where it keeps its log, and the check of an
/// installation (kalahari --self-test)

// Python before Qt: Qt's macro "slots" would change a name in the headers of Python
#include <pybind11/embed.h>

#include "kalahari/gui/installation.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/python_interpreter.h"
#include "kalahari/core/resource_paths.h"
#include "kalahari/core/unicode_paths.h"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QSqlDatabase>
#include <QSslSocket>

#include <array>
#include <cstddef>
#include <system_error>

namespace py = pybind11;

namespace kalahari::gui {

namespace {

constexpr const char* LOG_FILE_NAME = "kalahari.log";

/// The modules of Python the plugins and the program's own API need: the codecs, the formats
/// of files, the database and the secure connections, the libraries of C, the API itself
constexpr std::array PYTHON_MODULES{"encodings.idna", "json",    "sqlite3", "ssl",
                                    "zipfile",        "hashlib", "ctypes",  "kalahari_api"};

/// The image formats the program shows (portraits, covers, images in the text, its icons)
constexpr std::array IMAGE_FORMATS{"png", "jpg", "svg"};

/// The names @p names, separated by commas
template <std::size_t N>
QString joined(const std::array<const char*, N>& names)
{
    QStringList list;
    for (const char* name : names) {
        list.append(QString::fromLatin1(name));
    }
    return list.join(QStringLiteral(", "));
}

/// One line of the check: @p ok, the name of the part and what was found
void addLine(InstallationCheck& check, bool ok, const QString& part, const QString& found)
{
    check.lines.append(QStringLiteral("%1  %2: %3")
                           .arg(ok ? QStringLiteral("OK    ") : QStringLiteral("FAILED"), part,
                                found));
    check.passed = check.passed && ok;
}

/// Python of the plugins: it starts, finds its standard library and imports PYTHON_MODULES
void checkPython(InstallationCheck& check)
{
    auto& python = core::PythonInterpreter::getInstance();
    if (!python.isInitialized()) {
        try {
            python.initialize();
        } catch (const std::exception& e) {
            addLine(check, false, QStringLiteral("Python"), QString::fromStdString(e.what()));
            return;
        }
    }
    const QString home = QString::fromStdString(core::pathText(python.getPythonHome()));
    addLine(check, true, QStringLiteral("Python"),
            QStringLiteral("%1 (%2)").arg(QString::fromStdString(python.getPythonVersion()),
                                          home));

    py::gil_scoped_acquire gil;
    QStringList failed;
    for (const char* module : PYTHON_MODULES) {
        try {
            py::module_::import(module);
        } catch (const py::error_already_set& e) {
            failed.append(QStringLiteral("%1 (%2)").arg(QString::fromLatin1(module),
                                                        QString::fromStdString(e.what())));
        }
    }
    addLine(check, failed.isEmpty(), QStringLiteral("Python modules"),
            failed.isEmpty() ? joined(PYTHON_MODULES) : failed.join(QStringLiteral("; ")));
}

} // anonymous namespace

std::string logFilePath(const QString& programFolder, const std::filesystem::path& settingsFolder)
{
    std::filesystem::path folder = settingsFolder;
#ifndef Q_OS_MACOS
    if (QFileInfo(programFolder).isWritable()) {
        folder = std::filesystem::path(programFolder.toStdWString());
    }
#else
    Q_UNUSED(programFolder)
#endif
    try {
        // spdlog takes the narrow name of the file, on Windows in the code page of the system
        return (folder / LOG_FILE_NAME).string();
    } catch (const std::system_error&) {
        // A folder whose name the code page has no place for: the working folder
        return LOG_FILE_NAME;
    }
}

InstallationCheck checkInstallation()
{
    InstallationCheck check;

    const auto& resources = core::ResourcePaths::getInstance();
    addLine(check, resources.resourcesFound(), QStringLiteral("Resources"),
            resources.resourcesFound()
                ? resources.getResourcesDir()
                : QStringLiteral("not found in %1")
                      .arg(resources.getSearchPaths().join(QStringLiteral(", "))));

    addLine(check, !core::ArtProvider::getInstance().getApplicationIcon().isNull(),
            QStringLiteral("Icon of the program"), QStringLiteral("images/app_icon"));

    const QString polish = QStringLiteral(":/i18n/kalahari_pl.qm");
    addLine(check, QFile::exists(polish), QStringLiteral("Translations"), polish);

    addLine(check, !QGuiApplication::platformName().isEmpty(), QStringLiteral("Qt platform"),
            QGuiApplication::platformName());

    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    QStringList missing;
    for (const char* format : IMAGE_FORMATS) {
        if (!formats.contains(QByteArray(format))) {
            missing.append(QString::fromLatin1(format));
        }
    }
    addLine(check, missing.isEmpty(), QStringLiteral("Image formats"),
            missing.isEmpty() ? joined(IMAGE_FORMATS)
                              : QStringLiteral("missing %1").arg(missing.join(QStringLiteral(", "))));

    const bool sqlite = QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"));
    addLine(check, sqlite, QStringLiteral("Database"),
            sqlite ? QStringLiteral("QSQLITE") : QStringLiteral("no QSQLITE driver"));

    const bool tls = QSslSocket::supportsSsl();
    addLine(check, tls, QStringLiteral("Secure connections"),
            tls ? QSslSocket::activeBackend() + QStringLiteral(", ") +
                      QSslSocket::sslLibraryVersionString()
                : QStringLiteral("no TLS backend"));

    checkPython(check);

    for (const QString& line : check.lines) {
        core::Logger::getInstance().info("Installation check: {}", line.toStdString());
    }
    return check;
}

} // namespace kalahari::gui
