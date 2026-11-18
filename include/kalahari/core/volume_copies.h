// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file volume_copies.h
/// @brief A volume of a series copied out as a book of its own, and the files that go with it

#ifndef KALAHARI_CORE_VOLUME_COPIES_H
#define KALAHARI_CORE_VOLUME_COPIES_H

#include <kalahari/core/book_project.h>

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace kalahari::core {

/// @brief A file or a folder of one project copied to another: its path in each, relative to
///        the folder of its project, with "/"
struct ProjectCopy {
    QString from;  ///< In the project it comes from
    QString to;    ///< In the new one

    bool operator==(const ProjectCopy&) const = default;
};

/// @brief A volume of a series as a book of its own: its project, and what it copies from the
///        folder of the series
struct TakenOutVolume {
    BookProject project;         ///< The book: the volume, with the Workshop it keeps
    QList<ProjectCopy> files;    ///< The files of its elements, with their versions
    QList<ProjectCopy> folders;  ///< Folders copied whole: the images of the volume
};

/// @brief A volume of a series taken out as a book of its own, which the series keeps
///
/// The book has the volume as its only book, in the folder BookProject::BOOK_FOLDER: the files
/// of its texts in that folder keep their names, a file in the folder of another volume (a text
/// moved between the volumes) gets a free name there. Its Workshop is that of the series with
/// the same ids, without the materials of the cover and the texts for publishers of the other
/// volumes (those of the whole series and the volume's own are the book's), and without the
/// links and portraits that named what it does not have. It has the kinds, the type and the
/// paragraph styles of the series.
struct VolumeCopies {
    /// @brief Volume @p bookId of @p series as a book of its own
    /// @return nullopt when @p series has no such book or no other one
    static std::optional<TakenOutVolume> takeOut(const BookProject& series,
                                                 const QString& bookId);

    /// @brief Copy @p files, with the versions of each (ChapterVersions), and @p folders whole
    ///        from the folder @p fromDir to the folder @p toDir, making the folders they need
    /// @return false, with what failed in @p problems, when a file cannot be copied; what is
    ///         copied stays for the caller to remove
    static bool copy(const QString& fromDir, const QString& toDir,
                     const QList<ProjectCopy>& files, const QList<ProjectCopy>& folders,
                     QStringList& problems);
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_VOLUME_COPIES_H
