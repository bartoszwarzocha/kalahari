// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file file_associations.cpp
/// @brief The files of Kalahari and the program the system opens them with: Windows and Linux
/// (macOS: file_associations_mac.mm)
///
/// The types are the ones the installers give to the program: packaging/windows/kalahari.nsi,
/// packaging/linux/kalahari-mime.xml and kalahari.desktop, packaging/macos/Info.plist.in.

#include "kalahari/gui/utils/file_associations.h"
#include "kalahari/core/logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <utility>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX  // std::min and std::max, not the macros of windows.h
#endif
#include <windows.h>
#include <shlobj.h>   // SHChangeNotify
#include <shlwapi.h>  // AssocQueryStringW

#include <QDesktopServices>
#include <QUrl>

#include <array>
#include <string>
#elif defined(Q_OS_MACOS)
#include "file_associations_mac.h"
#endif

namespace kalahari::gui {

namespace {

/// The key of the Capabilities of the program (Windows), under HKEY_CURRENT_USER or
/// HKEY_LOCAL_MACHINE
const QString CAPABILITIES_KEY = QStringLiteral("Software\\Kalahari\\Capabilities");

std::unique_ptr<FileAssociationSystem>& testSystem()
{
    static std::unique_ptr<FileAssociationSystem> system;
    return system;
}

/// @p argument as one argument of the command line of a desktop entry (Exec): quoted when it
/// has a reserved character, with the escapes of a string of a desktop entry
QString desktopExecArgument(const QString& argument)
{
    static const QString RESERVED = QStringLiteral(" \t\n\"'\\><~|&;$*?#()`");
    QString text = argument;
    bool quoted = false;
    for (const QChar character : argument) {
        if (RESERVED.contains(character)) {
            quoted = true;
            break;
        }
    }
    if (quoted) {
        QString inner;
        for (const QChar character : argument) {
            if (character == u'"' || character == u'`' || character == u'$' || character == u'\\') {
                inner += u'\\';
            }
            inner += character;
        }
        text = u'"' + inner + u'"';
    }
    // The escapes of a string apply before the quoting; a percent sign is a field code
    text.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    text.replace(QStringLiteral("%"), QStringLiteral("%%"));
    return text;
}

#if defined(Q_OS_WIN)

/// Whether @p first and @p second name the same file (Windows: whatever the letters)
bool samePath(const QString& first, const QString& second)
{
    const auto normal = [](const QString& path) {
        const QFileInfo info(path);
        const QString canonical = info.canonicalFilePath();
        return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
    };
    return normal(first).compare(normal(second), Qt::CaseInsensitive) == 0;
}

/// A string of the association of @p extension (".klh"); empty when Windows has none
QString associationText(const QString& extension, ASSOCSTR what)
{
    std::array<wchar_t, 2048> buffer{};
    auto size = static_cast<DWORD>(buffer.size());
    const std::wstring name = extension.toStdWString();
    const HRESULT result = AssocQueryStringW(ASSOCF_INIT_IGNOREUNKNOWN, what, name.c_str(), L"open",
                                             buffer.data(), &size);
    return SUCCEEDED(result) ? QString::fromWCharArray(buffer.data()) : QString();
}

/// Whether Kalahari is in RegisteredApplications under @p root
bool registeredUnder(HKEY root)
{
    return RegGetValueW(root, L"Software\\RegisteredApplications", L"Kalahari", RRF_RT_REG_SZ,
                        nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

/// Windows 8 and later: the program registers itself, the writer chooses the default
class WindowsFileAssociations final : public FileAssociationSystem {
public:
    FileAssociation current(const KalahariFileType& type) const override
    {
        const QString extension = u'.' + type.extension;
        const QString program = associationText(extension, ASSOCSTR_EXECUTABLE);
        if (program.isEmpty()) {
            return {FileAssociation::State::NoProgram, {}};
        }
        if (samePath(program, QCoreApplication::applicationFilePath())) {
            return {FileAssociation::State::Kalahari, {}};
        }
        const QString name = associationText(extension, ASSOCSTR_FRIENDLYAPPNAME);
        return {FileAssociation::State::OtherProgram,
                name.isEmpty() ? QFileInfo(program).fileName() : name};
    }

    void associate(std::function<void(const QString&)> done) override
    {
        const QStringList names{
            //: The type of the .klh files in Windows Explorer. In Polish: Książka programu
            //: Kalahari
            QCoreApplication::translate("kalahari::gui::FileAssociations", "Kalahari book"),
            //: The type of the .kchapter files in Windows Explorer. In Polish: Dokument programu
            //: Kalahari
            QCoreApplication::translate("kalahari::gui::FileAssociations", "Kalahari document")};
        const QString program = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        for (const WindowsRegistryValue& value : windowsRegistration(program, names)) {
            const std::wstring key = value.key.toStdWString();
            const std::wstring name = value.name.toStdWString();
            const std::wstring data = value.value.toStdWString();
            const auto bytes = static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t));
            const LSTATUS status = RegSetKeyValueW(HKEY_CURRENT_USER, key.c_str(),
                                                   value.name.isEmpty() ? nullptr : name.c_str(),
                                                   REG_SZ, data.c_str(), bytes);
            if (status != ERROR_SUCCESS) {
                core::Logger::getInstance().warn("File associations: HKCU\\{} not written ({})",
                                                 value.key.toStdString(), status);
                //: %1 is a key of the registry. In Polish: System Windows nie pozwolił programowi
                //: zapisać rejestracji (%1).
                done(QCoreApplication::translate("kalahari::gui::FileAssociations",
                                                 "Windows did not let the program "
                                                 "write its registration (%1).")
                         .arg(value.key));
                return;
            }
        }
        // Explorer shows the new types at once
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        core::Logger::getInstance().info("File associations: registered {} for the writer",
                                         program.toStdString());
        done({});
    }

    bool hasSettingsWindow() const override { return true; }

    bool openSettingsWindow() override
    {
        return QDesktopServices::openUrl(QUrl(windowsSettingsAddress(
            registeredUnder(HKEY_CURRENT_USER), registeredUnder(HKEY_LOCAL_MACHINE))));
    }
};

#elif !defined(Q_OS_MACOS)

constexpr int COMMAND_TIMEOUT_MS = 15000;

/// The text of xdg-utils missing, for the state and for a failed association
QString noXdgUtils()
{
    //: In Polish: Program potrzebuje xdg-utils (polecenia xdg-mime), których system nie ma.
    return QCoreApplication::translate("kalahari::gui::FileAssociations",
                                       "The program needs xdg-utils (the command "
                                       "xdg-mime), which the system does not have.");
}

/// The name of the program of desktop file @p id, in the language of the program if it has it
QString desktopName(const QString& id)
{
    QFile file(QStandardPaths::locate(QStandardPaths::ApplicationsLocation, id));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QString language = QLocale().name().section(u'_', 0, 0);
    QString name;
    QString localized;
    bool entry = false;
    for (const QString& line : QString::fromUtf8(file.readAll()).split(u'\n')) {
        if (line.startsWith(u'[')) {
            entry = line.trimmed() == QLatin1String("[Desktop Entry]");
        } else if (entry && line.startsWith(QLatin1String("Name="))) {
            name = line.mid(5).trimmed();
        } else if (entry && line.startsWith(QStringLiteral("Name[%1]=").arg(language))) {
            localized = line.section(u'=', 1).trimmed();
        }
    }
    return localized.isEmpty() ? name : localized;
}

/// Linux: xdg-utils of the desktop
class LinuxFileAssociations final : public FileAssociationSystem {
public:
    FileAssociation current(const KalahariFileType& type) const override
    {
        const QString xdgMime = QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
        if (xdgMime.isEmpty()) {
            return {FileAssociation::State::Unknown, noXdgUtils()};
        }
        QProcess process;
        process.start(xdgMime, {QStringLiteral("query"), QStringLiteral("default"), type.mimeType});
        if (!process.waitForFinished(COMMAND_TIMEOUT_MS) || process.exitCode() != 0) {
            return {FileAssociation::State::Unknown,
                    QString::fromLocal8Bit(process.readAllStandardError()).trimmed()};
        }
        const QString id =
            QString::fromLocal8Bit(process.readAllStandardOutput()).section(u'\n', 0, 0).trimmed();
        const bool named = !id.isEmpty() && id != QLatin1String(LINUX_DESKTOP_ID);
        return linuxAssociation(id, named ? desktopName(id) : QString());
    }

    void associate(std::function<void(const QString&)> done) override { done(run()); }

    bool hasSettingsWindow() const override { return false; }

    bool openSettingsWindow() override { return false; }

private:
    /// Whether the system knows the types of the files of Kalahari
    static bool typesKnown()
    {
        const QMimeDatabase database;
        for (const KalahariFileType& type : kalahariFileTypes()) {
            if (!database.mimeTypeForName(type.mimeType).isValid()) {
                return false;
            }
        }
        return true;
    }

    /// Make Kalahari the program of its files; empty, or what went wrong
    static QString run()
    {
        const QString appImage = qEnvironmentVariable("APPIMAGE");
        const bool install = !appImage.isEmpty() || !typesKnown() ||
                             QStandardPaths::locate(QStandardPaths::ApplicationsLocation,
                                                    QString::fromLatin1(LINUX_DESKTOP_ID))
                                 .isEmpty();
        // The files of share/ next to bin/ of the program (cmake --install, the AppImage)
        const QDir share(QCoreApplication::applicationDirPath() + QStringLiteral("/../share"));
        const QString mimeFile =
            share.absoluteFilePath(QStringLiteral("mime/packages/kalahari.xml"));
        const QString desktopSource =
            share.absoluteFilePath(QStringLiteral("applications/kalahari.desktop"));
        const QString icon =
            share.absoluteFilePath(QStringLiteral("icons/hicolor/256x256/apps/kalahari.png"));
        const QTemporaryDir folder;
        const QString desktopFile = folder.filePath(QString::fromLatin1(LINUX_DESKTOP_ID));
        if (install) {
            QFile source(desktopSource);
            QFile entry(desktopFile);
            if (!QFileInfo::exists(mimeFile) || !source.open(QIODevice::ReadOnly) ||
                !folder.isValid() || !entry.open(QIODevice::WriteOnly)) {
                //: In Polish: Program nie ma opisu swoich plików (%1).
                return QCoreApplication::translate("kalahari::gui::FileAssociations",
                                                   "The program does not have the "
                                                   "description of its files (%1).")
                    .arg(QDir::toNativeSeparators(share.absolutePath()));
            }
            const QString program =
                appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
            entry.write(linuxDesktopEntry(QString::fromUtf8(source.readAll()), program).toUtf8());
        }
        const auto commands = linuxAssociateCommands(install, mimeFile, desktopFile,
                                                     QFileInfo::exists(icon) ? icon : QString());
        for (const SystemCommand& command : commands) {
            const QString path = QStandardPaths::findExecutable(command.program);
            if (path.isEmpty()) {
                return noXdgUtils();
            }
            QProcess process;
            process.start(path, command.arguments);
            if (!process.waitForFinished(COMMAND_TIMEOUT_MS) || process.exitCode() != 0) {
                const QString error =
                    QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
                core::Logger::getInstance().warn(
                    "File associations: {} {} failed: {}", command.program.toStdString(),
                    command.arguments.join(u' ').toStdString(), error.toStdString());
                return QStringLiteral("%1: %2").arg(
                    command.program, error.isEmpty() ? process.errorString() : error);
            }
        }
        core::Logger::getInstance().info("File associations: {} is the program of the files{}",
                                         LINUX_DESKTOP_ID,
                                         install ? " (installed for the writer)" : "");
        return {};
    }
};

#endif

}  // namespace

const std::vector<KalahariFileType>& kalahariFileTypes()
{
    static const std::vector<KalahariFileType> TYPES{
        {QStringLiteral("klh"), QStringLiteral("application/x-kalahari-book"),
         QStringLiteral("Kalahari.Book"),
         QStringLiteral("io.github.bartoszwarzocha.kalahari.book")},
        {QStringLiteral("kchapter"), QStringLiteral("application/x-kalahari-document"),
         QStringLiteral("Kalahari.Document"),
         QStringLiteral("io.github.bartoszwarzocha.kalahari.document")},
    };
    return TYPES;
}

FileAssociationSystem& fileAssociationSystem()
{
    if (testSystem()) {
        return *testSystem();
    }
#if defined(Q_OS_WIN)
    static WindowsFileAssociations system;
    return system;
#elif defined(Q_OS_MACOS)
    static const std::unique_ptr<FileAssociationSystem> SYSTEM = macFileAssociationSystem();
    return *SYSTEM;
#else
    static LinuxFileAssociations system;
    return system;
#endif
}

void setFileAssociationSystemForTests(std::unique_ptr<FileAssociationSystem> system)
{
    testSystem() = std::move(system);
}

std::vector<WindowsRegistryValue> windowsRegistration(const QString& program,
                                                      const QStringList& typeNames)
{
    const QString classes = QStringLiteral("Software\\Classes\\");
    const QString associations = CAPABILITIES_KEY + QStringLiteral("\\FileAssociations");
    std::vector<WindowsRegistryValue> values;
    const auto& types = kalahariFileTypes();
    for (std::size_t index = 0; index < types.size(); ++index) {
        const KalahariFileType& type = types[index];
        const QString progId = classes + type.progId;
        const QString extension = u'.' + type.extension;
        values.push_back({progId, {}, typeNames.value(static_cast<qsizetype>(index))});
        values.push_back(
            {progId + QStringLiteral("\\DefaultIcon"), {}, program + QStringLiteral(",0")});
        values.push_back({progId + QStringLiteral("\\shell\\open\\command"),
                          {},
                          QStringLiteral("\"%1\" \"%2\"").arg(program, QStringLiteral("%1"))});
        values.push_back({classes + extension, {}, type.progId});
        values.push_back(
            {classes + extension + QStringLiteral("\\OpenWithProgids"), type.progId, {}});
        values.push_back({associations, extension, type.progId});
    }
    values.push_back({CAPABILITIES_KEY, QStringLiteral("ApplicationName"),
                      QString::fromLatin1(WINDOWS_REGISTERED_NAME)});
    //: The description of Kalahari in Windows Settings > Apps > Default apps. In Polish:
    //: Pisanie, planowanie i wydawanie książek
    values.push_back({CAPABILITIES_KEY, QStringLiteral("ApplicationDescription"),
                      QCoreApplication::translate("kalahari::gui::FileAssociations",
                                                  "Write, plan and publish books")});
    values.push_back({QStringLiteral("Software\\RegisteredApplications"),
                      QString::fromLatin1(WINDOWS_REGISTERED_NAME), CAPABILITIES_KEY});
    return values;
}

QString windowsSettingsAddress(bool user, bool machine)
{
    QString page = QStringLiteral("ms-settings:defaultapps");
    if (user) {
        page += QStringLiteral("?registeredAppUser=") + QLatin1String(WINDOWS_REGISTERED_NAME);
    } else if (machine) {
        page += QStringLiteral("?registeredAppMachine=") + QLatin1String(WINDOWS_REGISTERED_NAME);
    }
    return page;
}

QString linuxDesktopEntry(const QString& entry, const QString& program)
{
    QStringList lines = entry.split(u'\n');
    for (QString& line : lines) {
        if (line.startsWith(QLatin1String("Exec="))) {
            line = QStringLiteral("Exec=%1 %f").arg(desktopExecArgument(program));
        } else if (line.startsWith(QLatin1String("TryExec="))) {
            line = QStringLiteral("TryExec=") + desktopExecArgument(program);
        }
    }
    return lines.join(u'\n');
}

QString linuxMimeTypeIcon(const KalahariFileType& type)
{
    QString name = type.mimeType;
    return name.replace(u'/', u'-');
}

std::vector<SystemCommand> linuxAssociateCommands(bool install, const QString& mimeFile,
                                                  const QString& desktopFile,
                                                  const QString& iconFile)
{
    const QStringList user{QStringLiteral("install"), QStringLiteral("--mode"),
                           QStringLiteral("user"), QStringLiteral("--novendor")};
    std::vector<SystemCommand> commands;
    if (install) {
        commands.push_back({QStringLiteral("xdg-mime"), user + QStringList{mimeFile}});
        if (!iconFile.isEmpty()) {
            const QStringList size{QStringLiteral("--size"), QStringLiteral("256"), iconFile};
            commands.push_back({QStringLiteral("xdg-icon-resource"),
                                user + size + QStringList{QStringLiteral("kalahari")}});
            // The icon of the files: the one named after their type (shared-mime-info)
            for (const KalahariFileType& type : kalahariFileTypes()) {
                commands.push_back(
                    {QStringLiteral("xdg-icon-resource"),
                     user + QStringList{QStringLiteral("--context"), QStringLiteral("mimetypes")} +
                         size + QStringList{linuxMimeTypeIcon(type)}});
            }
        }
        commands.push_back({QStringLiteral("xdg-desktop-menu"), user + QStringList{desktopFile}});
    }
    QStringList defaults{QStringLiteral("default"), QString::fromLatin1(LINUX_DESKTOP_ID)};
    for (const KalahariFileType& type : kalahariFileTypes()) {
        defaults.append(type.mimeType);
    }
    commands.push_back({QStringLiteral("xdg-mime"), defaults});
    return commands;
}

FileAssociation linuxAssociation(const QString& output, const QString& name)
{
    const QString id = output.trimmed();
    if (id.isEmpty()) {
        return {FileAssociation::State::NoProgram, {}};
    }
    if (id == QLatin1String(LINUX_DESKTOP_ID)) {
        return {FileAssociation::State::Kalahari, {}};
    }
    return {FileAssociation::State::OtherProgram, name.isEmpty() ? id : name};
}

}  // namespace kalahari::gui
