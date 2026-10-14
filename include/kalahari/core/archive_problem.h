/// @file archive_problem.h
/// @brief Why an archive of a book could not be written, read or unpacked

#pragma once

#include <QStringList>

namespace kalahari::core {

/// @brief Why an archive of a book could not be written, read or unpacked
///
/// The kind lets the windows say it in the language of the program. The lines keep the
/// description of Kalahari and of libzip, in English, as the log has it, for reports.
struct ArchiveProblem {
    /// @brief What went wrong
    enum class Kind {
        None,             ///< Nothing went wrong
        Other,            ///< Something the kinds below do not name; only the lines say what
        ChangesNotSaved,  ///< The changes of the book could not be saved before the export;
                          ///< the caller, which saves them, says it
        UnreadableFile,   ///< A file of the book cannot be read for its archive
        CannotWrite,      ///< The archive or the files of the book cannot be written in their
                          ///< folder: the system does not allow it, or the disk fails
        DiskFull,         ///< The disk has no room left
        NotFound,         ///< The archive does not exist
        CannotRead,       ///< The archive cannot be read: the system does not allow it, or the
                          ///< disk fails
        NotArchive,       ///< The file is not a ZIP archive, or its end is missing
        Damaged,          ///< The data of the archive are damaged
        Unsupported,      ///< The archive is encrypted, or packed in a way libzip does not read
        NoBook,           ///< The archive has no .klh file
        DamagedBook,      ///< The .klh file of the archive is not a book that Kalahari reads
        NewerBook,        ///< The book of the archive was saved by a newer version of Kalahari
        FolderExists,     ///< The folder of the book is already there
        NotOpened         ///< The unpacked book cannot be opened
    };

    Kind kind = Kind::None;  ///< What went wrong
    QStringList lines;       ///< How it went wrong, one problem per line
};

}  // namespace kalahari::core
