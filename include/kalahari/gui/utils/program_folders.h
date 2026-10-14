/// @file program_folders.h
/// @brief The folders of the program and the folders the windows choosing files start in

#pragma once

#include <QCoreApplication>
#include <QString>

#include <string>

namespace kalahari {
namespace gui {

/// @brief Where the program keeps books and archives, and where the windows that choose files
/// and folders start
///
/// The Folders page of the Settings sets the folder of the books and the folder of the
/// archives; an empty setting means the default folder in Documents\Kalahari. Each operation
/// remembers the folder used last, also after a restart: its window starts there and, when
/// that folder is gone (the first use, a disconnected drive), in the folder of the settings.
/// The system windows choosing files and folders stay (decision 6 of the proposals after the
/// test of PR #61); the program gives them only the folder to start in.
class ProgramFolders {
    Q_DECLARE_TR_FUNCTIONS(ProgramFolders)

public:
    /// @brief An operation that remembers the folder used last
    enum class Operation {
        NewBook,            ///< The folder a new book is created in
        OpenBook,           ///< Open Book: the folder with the folder of the book
        OpenFile,           ///< Open File: the folder of the file
        ExportArchive,      ///< The folder a book is exported to
        ImportArchive,      ///< The folder of the archive a book is imported from
        ImportDestination,  ///< The folder an imported book goes to
    };

    /// @brief Setting of the folder of the books; empty: defaultBooksFolder()
    static constexpr const char* BOOKS_SETTING = "project.defaultLocation";
    /// @brief Setting of the folder of the archives; empty: defaultArchivesFolder()
    static constexpr const char* ARCHIVES_SETTING = "project.archiveLocation";

    /// @brief The Documents folder of the user; the home folder when the system has none
    ///
    /// The tests (KALAHARI_TEST_MODE) get a Documents folder in their temporary folder.
    static QString documentsFolder();

    /// @brief The default folder of the books: Kalahari in the Documents folder
    static QString defaultBooksFolder();

    /// @brief The default folder of the archives: Archives in the default folder of the books
    static QString defaultArchivesFolder();

    /// @brief The folder suggested for the database backups of all books: Backups in the
    /// default folder of the books
    static QString defaultBackupsFolder();

    /// @brief The folder of the books: the one of the settings, or the default one
    static QString booksFolder();

    /// @brief The folder of the archives: the one of the settings, or the default one
    static QString archivesFolder();

    /// @brief The folder of operation @p operation: the folder it used last while that folder
    /// exists, else the folder of the settings for it, which may not exist yet
    static QString startFolder(Operation operation);

    /// @brief The folder a system window of operation @p operation opens in: startFolder(),
    /// created when it does not exist yet (its first use), or the nearest existing folder
    /// above it when it cannot be created
    static QString windowFolder(Operation operation);

    /// @brief Remember folder @p folder as the one operation @p operation used last
    static void remember(Operation operation, const QString& folder);

    /// @brief Forget the folders used last by the operations that start in the folder of
    /// setting @p settingKey, so that their windows start in that folder again
    static void forgetFolders(const std::string& settingKey);

    /// @brief Why the program cannot keep files in folder @p folder; empty when it can, also
    /// when the folder does not exist yet and can be created
    static QString problemOf(const QString& folder);

    /// @brief The nearest existing folder at or above folder @p folder; empty when there is
    /// none (a drive that is not connected)
    static QString existingFolderAt(const QString& folder);

    /// @brief Path @p path the way the settings keep it: '/' between the folders, without
    /// "." and ".." and the spaces at its ends
    static QString cleanPath(const QString& path);

    /// @brief Whether @p first and @p second name the same folder (on Windows letters of
    /// either case are the same)
    static bool samePath(const QString& first, const QString& second);
};

}  // namespace gui
}  // namespace kalahari
