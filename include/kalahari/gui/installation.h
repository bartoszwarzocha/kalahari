// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file installation.h
/// @brief The program as it is installed: where it keeps its log, and the check of an
/// installation (kalahari --self-test)

#ifndef KALAHARI_GUI_INSTALLATION_H
#define KALAHARI_GUI_INSTALLATION_H

#include <QString>
#include <QStringList>

#include <filesystem>
#include <string>

namespace kalahari::gui {

/// @brief The log file of the program (kalahari.log), as spdlog takes its name
///
/// In @p programFolder, the folder of the program file, when the writer may write there (a
/// build, the ZIP of Windows, an installation for one user); else, and always on macOS (a
/// bundle keeps no files of its own), in @p settingsFolder. So an installation for all users
/// and the packages of Linux keep their log out of the system folders and the writer's home.
/// @return The path in the narrow encoding spdlog opens (on Windows the code page of the
///         system); just the name, for the working folder, when that code page has no place
///         for the name of the folder
[[nodiscard]] std::string logFilePath(const QString& programFolder,
                                      const std::filesystem::path& settingsFolder);

/// @brief The result of checkInstallation()
struct InstallationCheck {
    QStringList lines;   ///< One line for each part: "OK" or "FAILED", the part, what was found
    bool passed = true;  ///< Whether every part works
};

/// @brief Check what an installation of the program needs: its resources, its icon and
/// translations, the Qt plugins of images, of the database and of secure connections, and the
/// Python of the plugins, which it starts
///
/// `kalahari --self-test` prints it and ends; scripts/package.py checks each package with it.
[[nodiscard]] InstallationCheck checkInstallation();

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_INSTALLATION_H
