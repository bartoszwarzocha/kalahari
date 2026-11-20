// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_installation.cpp
/// @brief The program as it is installed: where it keeps its log, and --self-test

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/art_provider.h>
#include <kalahari/core/resource_paths.h>
#include <kalahari/gui/installation.h>

#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#include <QSize>
#include <QTemporaryDir>

#include <algorithm>
#include <filesystem>

using kalahari::gui::checkInstallation;
using kalahari::gui::InstallationCheck;
using kalahari::gui::logFilePath;

namespace {

/// The line of the check about @p part, or an empty text
QString lineOf(const InstallationCheck& check, const QString& part)
{
    const auto found = std::find_if(check.lines.begin(), check.lines.end(),
                                    [&part](const QString& line) {
                                        return line.mid(8).startsWith(part + QLatin1Char(':'));
                                    });
    return found == check.lines.end() ? QString() : *found;
}

} // namespace

TEST_CASE("The log is in the folder of the program where the writer may write there",
          "[gui][installation]")
{
    QTemporaryDir program;
    QTemporaryDir settings;
    REQUIRE(program.isValid());
    REQUIRE(settings.isValid());
    const std::filesystem::path settingsFolder(settings.path().toStdWString());

#ifdef Q_OS_MACOS
    // A bundle keeps no files of its own
    CHECK(std::filesystem::path(logFilePath(program.path(), settingsFolder)) ==
          settingsFolder / "kalahari.log");
#else
    CHECK(std::filesystem::path(logFilePath(program.path(), settingsFolder)) ==
          std::filesystem::path(program.path().toStdWString()) / "kalahari.log");
#endif
}

TEST_CASE("The log is in the folder of the settings where the program may not write",
          "[gui][installation]")
{
    QTemporaryDir settings;
    REQUIRE(settings.isValid());
    const std::filesystem::path settingsFolder(settings.path().toStdWString());
    // A folder that is not there is not one to write to, for any user (also for root, who
    // may write into a folder without the right to write)
    const QString missing = QDir(settings.path()).filePath(QStringLiteral("no-such-folder"));

    CHECK(std::filesystem::path(logFilePath(missing, settingsFolder)) ==
          settingsFolder / "kalahari.log");
}

TEST_CASE("The check of an installation names each part", "[gui][installation]")
{
    const InstallationCheck check = checkInstallation();

    for (const QString& part :
         {QStringLiteral("Resources"), QStringLiteral("Icon of the program"),
          QStringLiteral("Translations"), QStringLiteral("Qt platform"),
          QStringLiteral("Image formats"), QStringLiteral("Database"),
          QStringLiteral("Secure connections"), QStringLiteral("Python"),
          QStringLiteral("Python modules")}) {
        INFO(part.toStdString());
        CHECK_FALSE(lineOf(check, part).isEmpty());
    }

    // The parts the tests have as an installation has them: the resources next to the
    // program, its icon and translations, the Python of the plugins and its modules. The Qt
    // plugins are those of the Qt the tests are built with
    for (const QString& part :
         {QStringLiteral("Resources"), QStringLiteral("Icon of the program"),
          QStringLiteral("Translations"), QStringLiteral("Database"),
          QStringLiteral("Python"), QStringLiteral("Python modules")}) {
        INFO(lineOf(check, part).toStdString());
        CHECK(lineOf(check, part).startsWith(QStringLiteral("OK")));
    }
    const bool everyPartWorks =
        std::all_of(check.lines.begin(), check.lines.end(),
                    [](const QString& line) { return line.startsWith(QStringLiteral("OK")); });
    CHECK(check.passed == everyPartWorks);
}

TEST_CASE("The icon of the program has each size of the logo", "[gui][installation]")
{
    const QIcon icon = kalahari::core::ArtProvider::getInstance().getApplicationIcon();
    REQUIRE_FALSE(icon.isNull());
    const QList<QSize> sizes = icon.availableSizes();
    for (const int side : {16, 24, 32, 48, 64, 128, 256, 512}) {
        INFO(side);
        CHECK(sizes.contains(QSize(side, side)));
    }
}

#ifdef Q_OS_LINUX
TEST_CASE("The resources of an installation are found next to its bin folder",
          "[gui][installation]")
{
    // The AppImage (usr/bin, usr/share), the .deb package (/opt/kalahari) and cmake --install
    const QString installed = QDir::cleanPath(QCoreApplication::applicationDirPath() +
                                              QStringLiteral("/../share/kalahari/resources"));
    CHECK(kalahari::core::ResourcePaths::getInstance().getSearchPaths().contains(installed));
}
#endif
