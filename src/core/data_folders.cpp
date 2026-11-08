/// @file data_folders.cpp
/// @brief Implementation of DataFolders

#include <kalahari/core/data_folders.h>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/unicode_paths.h>

#include <QDir>
#include <QTemporaryFile>

#include <nlohmann/json.hpp>

#include <fstream>
#include <system_error>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX  // std::min and std::max, not the macros of windows.h
    #endif
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <vector>
#endif

namespace kalahari {
namespace core {

namespace {

/// Path @p path as Qt text
QString qtPath(const std::filesystem::path& path) {
    return QString::fromStdU16String(path.u16string());
}

/// What the system says of @p error, as UTF-8 text: on Windows the message comes in the code
/// page of the system
std::string errorText(const std::error_code& error) {
    return QString::fromLocal8Bit(error.message().c_str()).toStdString();
}

/// Whether the program can create files in existing folder @p folder: a permission of a
/// folder does not tell it on every system (Windows access lists), as in ProgramFolders
bool canWriteIn(const std::filesystem::path& folder) {
    QTemporaryFile probe(QDir(qtPath(folder)).filePath(QStringLiteral("kalahari_XXXXXX.tmp")));
    return probe.open();
}

}  // namespace

const DataFolders& DataFolders::current() {
    // The tests keep their own temporary folders: their executables are next to the program's
    static const DataFolders folders =
        SettingsManager::isTestMode() ? DataFolders() : read(programFolder());
    return folders;
}

DataFolders DataFolders::read(const std::filesystem::path& programFolder) {
    DataFolders folders;
    if (programFolder.empty()) {
        return folders;  // without the folder of the executable there is no file to read
    }
    folders.m_file = programFolder / FILE_NAME;
    folders.m_kind = Kind::Broken;  // until the file is read and its folder is found usable

    // No file: the folders of the system. A file that cannot even be looked at may be there:
    // the program does not start then, rather than take the folders of the system
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(folders.m_file, error);
    if (status.type() == std::filesystem::file_type::not_found) {
        folders.m_kind = Kind::System;
        return folders;
    }
    if (error || !std::filesystem::is_regular_file(status)) {
        folders.m_problem = Problem::Unreadable;
        folders.m_detail = error ? errorText(error) : std::string();
        return folders;
    }

    nlohmann::json json;
    {
        std::ifstream file(folders.m_file, std::ios::binary);
        if (!file.is_open()) {
            folders.m_problem = Problem::Unreadable;
            return folders;
        }
        try {
            json = nlohmann::json::parse(file);
        } catch (const nlohmann::json::exception& exception) {
            folders.m_problem = Problem::NotJson;
            folders.m_detail = exception.what();
            return folders;
        }
    }
    if (!json.is_object()) {
        folders.m_problem = Problem::NotJson;
        return folders;
    }

    const auto folderValue = json.find("folder");
    if (folderValue == json.end() || !folderValue->is_string() ||
        QString::fromStdString(folderValue->get<std::string>()).trimmed().isEmpty()) {
        folders.m_problem = Problem::NoFolder;
        return folders;
    }
    const auto nameValue = json.find("name");
    if (nameValue != json.end()) {
        if (!nameValue->is_string()) {
            folders.m_problem = Problem::NameNotText;
            return folders;
        }
        folders.m_name = QString::fromStdString(nameValue->get<std::string>()).trimmed();
    }

    // An absolute folder, or one relative to the folder of the executable; without "..", "."
    // and a separator at its end, as the log and the messages show it
    std::filesystem::path folder = pathOfText(
        QString::fromStdString(folderValue->get<std::string>()).trimmed().toStdString());
    if (folder.is_relative()) {
        folder = programFolder / folder;
    }
    folder = std::filesystem::absolute(folder, error).lexically_normal();
    if (!folder.has_filename() && folder.has_parent_path() && folder != folder.root_path()) {
        folder = folder.parent_path();
    }
    folders.m_namedFolder = folder;

    std::filesystem::create_directories(folder, error);
    std::error_code directoryError;
    if (!std::filesystem::is_directory(folder, directoryError)) {
        folders.m_problem = Problem::FolderNotMade;
        folders.m_detail = errorText(error ? error : directoryError);
        return folders;
    }
    if (!canWriteIn(folder)) {
        folders.m_problem = Problem::FolderNotWritable;
        return folders;
    }

    folders.m_kind = Kind::Own;
    folders.m_folder = folder;
    return folders;
}

std::filesystem::path DataFolders::programFolder() {
#ifdef _WIN32
    // The buffer grows until the whole path fits: a path can be longer than MAX_PATH
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length =
            GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            return {};
        }
        if (length < path.size()) {
            path.resize(length);
            break;
        }
        path.resize(path.size() * 2);
    }
    return std::filesystem::path(path).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> path(size + 1, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        return {};
    }
    std::error_code error;
    const std::filesystem::path file = std::filesystem::canonical(path.data(), error);
    return error ? std::filesystem::path(path.data()).parent_path() : file.parent_path();
#else
    std::error_code error;
    const std::filesystem::path file = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::path() : file.parent_path();
#endif
}

QString DataFolders::ownFolder(const QString& name) const {
    // With forward slashes, as Qt gives paths
    return hasOwnFolder() ? QDir(QDir::fromNativeSeparators(qtPath(m_folder))).filePath(name)
                          : QString();
}

QString DataFolders::programName() const {
    const QString program = QStringLiteral("Kalahari");
    if (!hasOwnFolder() || m_name.isEmpty()) {
        return program;
    }
    return program + QStringLiteral(" (") + m_name + QLatin1Char(')');
}

std::string DataFolders::problemText() const {
    const std::string file = pathText(m_file);
    const std::string folder = pathText(m_namedFolder);
    const std::string detail = m_detail.empty() ? std::string() : ": " + m_detail;
    switch (m_problem) {
    case Problem::None:
        break;
    case Problem::Unreadable:
        return "the file " + file + " cannot be read" + detail;
    case Problem::NotJson:
        return "the file " + file + " is not a JSON object" + detail;
    case Problem::NoFolder:
        return "the file " + file + " names no folder (\"folder\")";
    case Problem::NameNotText:
        return "\"name\" of the file " + file + " is not text";
    case Problem::FolderNotMade:
        return "the folder " + folder + " of the file " + file + " cannot be made" + detail;
    case Problem::FolderNotWritable:
        return "the program cannot write in the folder " + folder + " of the file " + file;
    }
    return std::string();
}

} // namespace core
} // namespace kalahari
