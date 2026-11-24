// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file file_associations.h
/// @brief The files of Kalahari and the program the system opens them with
///
/// The program opens two kinds of files by itself: books (.klh) and documents (.kchapter).
/// The installers make Kalahari their program; Settings > Files > File Associations shows
/// which program opens them now and makes it Kalahari again. What the program may do depends
/// on the system:
/// - Windows (8 and later) lets only the writer choose the default program of a type of files:
///   the program registers itself as one that opens them (Software\Classes, Capabilities,
///   RegisteredApplications) and opens the Default Apps page of Windows Settings
/// - Linux: the program sets itself as the default (xdg-mime default); an AppImage first
///   installs the types and its menu entry for the writer (xdg-mime, xdg-desktop-menu)
/// - macOS 12 and later: the program sets itself as the default (NSWorkspace); the system may
///   ask the writer first

#ifndef KALAHARI_GUI_UTILS_FILE_ASSOCIATIONS_H
#define KALAHARI_GUI_UTILS_FILE_ASSOCIATIONS_H

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

namespace kalahari::gui {

/// @brief A kind of files the program opens by itself
struct KalahariFileType {
    QString extension;    ///< Without the dot: "klh"
    QString mimeType;     ///< Linux (shared-mime-info): "application/x-kalahari-book"
    QString progId;       ///< Windows: "Kalahari.Book"
    QString contentType;  ///< macOS (UTI): "io.github.bartoszwarzocha.kalahari.book"
};

/// @brief The books (.klh) and the documents (.kchapter), in this order
[[nodiscard]] const std::vector<KalahariFileType>& kalahariFileTypes();

/// @brief The name of the program in RegisteredApplications of Windows and in its Settings
constexpr const char* WINDOWS_REGISTERED_NAME = "Kalahari";

/// @brief The menu entry of the program on Linux (the desktop file)
constexpr const char* LINUX_DESKTOP_ID = "kalahari.desktop";

/// @brief Which program opens a kind of files now
struct FileAssociation {
    enum class State {
        Kalahari,      ///< This program
        OtherProgram,  ///< Another program (program: its name)
        NoProgram,     ///< No program: the system asks the writer
        Unknown        ///< The system does not tell (program: why)
    };
    State state = State::Unknown;
    QString program;  ///< The name of the other program, or why the state is unknown
};

/// @brief What the system lets the program do with the program of its files
class FileAssociationSystem {
public:
    virtual ~FileAssociationSystem() = default;

    /// @brief Which program opens files of @p type now
    [[nodiscard]] virtual FileAssociation current(const KalahariFileType& type) const = 0;

    /// @brief Make Kalahari the program of all its files, as far as the system lets it
    /// @param done Called once it is done (at once or later, on the thread of the window),
    ///             with an empty text or what went wrong
    virtual void associate(std::function<void(const QString& problem)> done) = 0;

    /// @brief Whether the system has a window of its settings where the writer chooses the
    ///        program of a type of files (Windows)
    [[nodiscard]] virtual bool hasSettingsWindow() const = 0;

    /// @brief Open that window of the system's settings
    /// @return false when it could not be opened
    virtual bool openSettingsWindow() = 0;
};

/// @brief The system the program runs on (a test may put another one in its place)
[[nodiscard]] FileAssociationSystem& fileAssociationSystem();

/// @brief A system in place of the real one, for tests; nullptr gives the real one back
void setFileAssociationSystemForTests(std::unique_ptr<FileAssociationSystem> system);

// ---------------------------------------------------------------------------------------------
// Windows: what the program writes to the registry (testable on every system)
// ---------------------------------------------------------------------------------------------

/// @brief One value of the registry of Windows, under HKEY_CURRENT_USER
struct WindowsRegistryValue {
    QString key;    ///< "Software\Classes\.klh"
    QString name;   ///< Empty: the default value of the key
    QString value;  ///< A string (REG_SZ)
};

/// @brief The values that register program @p program (its full path, with backslashes) as
///        the one that opens the files of Kalahari, as the installer writes them: the ProgIDs
///        with the command and the icon, the extensions with their ProgID and OpenWithProgids,
///        the Capabilities of the program and its name in RegisteredApplications
/// @param typeNames The name of each type the writer sees (in the order of kalahariFileTypes())
[[nodiscard]] std::vector<WindowsRegistryValue> windowsRegistration(const QString& program,
                                                                    const QStringList& typeNames);

/// @brief The address of the Default Apps page of Windows Settings: the page of Kalahari when
///        it is in RegisteredApplications of the writer (@p user) or of the computer
///        (@p machine), else the page of all programs
[[nodiscard]] QString windowsSettingsAddress(bool user, bool machine);

// ---------------------------------------------------------------------------------------------
// Linux: the commands of xdg-utils the program runs (testable on every system)
// ---------------------------------------------------------------------------------------------

/// @brief A command and its arguments
struct SystemCommand {
    QString program;
    QStringList arguments;
};

/// @brief The menu entry of the program for the writer: @p entry (the text of kalahari.desktop)
///        with the command that starts @p program
[[nodiscard]] QString linuxDesktopEntry(const QString& entry, const QString& program);

/// @brief The name of the icon of files of @p type in the icon theme: their MIME type with "-"
///        in place of "/" ("application-x-kalahari-book"), as shared-mime-info names it
[[nodiscard]] QString linuxMimeTypeIcon(const KalahariFileType& type);

/// @brief The commands that make Kalahari the program of its files on Linux
/// @param install Whether the types and the menu entry are to be installed for the writer
///        first (an AppImage, or a system that does not know them): from @p mimeFile,
///        @p desktopFile and @p iconFile (empty: no icon), which becomes the icon of the
///        program and of each type of its files
[[nodiscard]] std::vector<SystemCommand> linuxAssociateCommands(bool install,
                                                                const QString& mimeFile,
                                                                const QString& desktopFile,
                                                                const QString& iconFile);

/// @brief The state of @p output of "xdg-mime query default" (a desktop file id, or nothing)
/// @param name The name of another program from its desktop file (its id when empty)
[[nodiscard]] FileAssociation linuxAssociation(const QString& output, const QString& name);

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_UTILS_FILE_ASSOCIATIONS_H
