/// @file data_folders.h
/// @brief Where a copy of the program keeps the writer's data: in the folders of the system,
/// or in a data folder of its own

#pragma once

#include <QString>

#include <filesystem>
#include <string>

namespace kalahari {
namespace core {

/// @brief Where the program keeps the writer's data
///
/// The data of the writer: the settings (with the layout of the window and of the toolbars,
/// the recent books, the Dashboard, the folders used last), the custom book types and the user
/// dictionary next to them, the grammar components, the unpacked plugins, the dictionaries the
/// writer added, and the Documents folder the books and the windows choosing files start in.
///
/// A program keeps them in the folders of the system: on Windows %APPDATA%\\Kalahari,
/// %LOCALAPPDATA%\\Kalahari and Documents. A copy of the program that has the file
/// kalahari-data.json next to its executable keeps all of them in the folder that file names
/// instead, and nothing in the folders of the system. So a test copy, or a copy on a USB stick,
/// never reads or changes the data of the writer's own program. The file:
///
/// @code
/// {
///     "folder": "../../program-data",
///     "name": "test copy"
/// }
/// @endcode
///
/// "folder" is absolute, or relative to the folder of the executable; it is made when it does
/// not exist yet. "name" may be left out: the title bar shows it after the name of the program,
/// "Kalahari (test copy)", so that the writer sees in which copy they work.
///
/// A file that cannot be used (not JSON, no folder, a folder that cannot be made or written
/// to) does not let the program start: main() says why, and the settings are neither read nor
/// saved meanwhile, so the folders of the system stay untouched also then.
///
/// The tests (KALAHARI_TEST_MODE) keep their own temporary folders and do not read the file.
class DataFolders {
public:
    /// @brief The file next to the executable that gives a copy a data folder of its own
    static constexpr const char* FILE_NAME = "kalahari-data.json";

    /// @brief Where the data of a copy is kept
    enum class Kind {
        System,  ///< In the folders of the system: the copy has no file
        Own,     ///< In the folder of the file
        Broken,  ///< The copy has a file that cannot be used: the program does not start
    };

    /// @brief Why a file cannot be used
    enum class Problem {
        None,               ///< It can (or there is no file)
        Unreadable,         ///< The file cannot be read
        NotJson,            ///< The file is not a JSON object
        NoFolder,           ///< The file has no "folder" text
        NameNotText,        ///< "name" is not text
        FolderNotMade,      ///< The folder does not exist and cannot be made
        FolderNotWritable,  ///< The program cannot write in the folder
    };

    /// @brief The data folders of the program that runs, read once from the folder of its
    /// executable; in the tests (KALAHARI_TEST_MODE) the folders of the system, whatever the
    /// folder of the executable has
    static const DataFolders& current();

    /// @brief The data folders of a copy whose executable is in folder @p programFolder: what
    /// its file says, with the copy's own folder made when it does not exist yet
    static DataFolders read(const std::filesystem::path& programFolder);

    /// @brief The folder of the executable of the program that runs; empty when the system
    /// does not tell it
    static std::filesystem::path programFolder();

    /// @brief Where the data is kept
    Kind kind() const { return m_kind; }

    /// @brief Whether the copy keeps its data in a folder of its own
    bool hasOwnFolder() const { return m_kind == Kind::Own; }

    /// @brief The copy's own folder, absolute; empty unless hasOwnFolder()
    const std::filesystem::path& folder() const { return m_folder; }

    /// @brief Subfolder @p name of the copy's own folder ("grammar", "Documents"), as Qt
    /// text; empty unless hasOwnFolder()
    QString ownFolder(const QString& name) const;

    /// @brief The name of the copy from its file; empty when it has none
    const QString& name() const { return m_name; }

    /// @brief The name of the program in the title bar of its window: "Kalahari", or with
    /// the name of the copy after it, "Kalahari (test copy)"
    QString programName() const;

    /// @brief The file next to the executable (it may not exist)
    const std::filesystem::path& file() const { return m_file; }

    /// @brief Why the file cannot be used; Problem::None unless the kind is Broken
    Problem problem() const { return m_problem; }

    /// @brief The folder the file names, absolute, also when it cannot be used; empty when
    /// the file names none
    const std::filesystem::path& namedFolder() const { return m_namedFolder; }

    /// @brief What the system or the JSON reader said of the problem (in English); may be
    /// empty
    const std::string& detail() const { return m_detail; }

    /// @brief The problem in English, for the log: "the folder ... cannot be made: ..."
    std::string problemText() const;

private:
    Kind m_kind = Kind::System;
    std::filesystem::path m_file;
    std::filesystem::path m_folder;
    std::filesystem::path m_namedFolder;
    QString m_name;
    Problem m_problem = Problem::None;
    std::string m_detail;
};

} // namespace core
} // namespace kalahari
