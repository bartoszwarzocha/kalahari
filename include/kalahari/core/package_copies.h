/// @file package_copies.h
/// @brief Copies of the book type packages in the folder of a book, so that the book opens and
/// works the same on every computer
///
/// A book keeps in its folder types/ a copy of the data of each package it uses: its type, the
/// packages the type uses (the base package) and names kinds of, the packages of the kinds it
/// took from other types and the packages of the kinds of its elements, with the packages each
/// of these uses in turn. The copy is the manifest, the styles file and the templates of the
/// kinds; a tool with code of its own does not go into it.
///
/// The book opens with the packages of its copies, so it works the same where its type is not
/// installed or is installed in an older version. A newer installed version replaces the copy
/// when the book opens; the styles the book uses are in its database and do not change. The
/// copies serve their book only: the program does not install them, and the New Book window,
/// the palette and the other books do not see them.
///
/// @code
/// Moja powieść/
/// └─ types/
///    ├─ kalahari.base/      booktype.json, styles.json, templates/title_page.kchapter
///    └─ kalahari.novel/     booktype.json, styles.json
/// @endcode

#pragma once

#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <QString>
#include <QStringList>
#include <optional>

namespace kalahari::core {

/// @brief What PackageCopies::update() found and did
struct PackageCopiesUpdate {
    QStringList folders;    ///< Folders the packages of the book are read from: its copies, or
                            ///< the installed package when its copy cannot be written
    QStringList made;       ///< Ids of the packages copied into the book for the first time
    QStringList refreshed;  ///< Ids of the copies replaced with a newer installed version
    QStringList problems;   ///< "<package>: <what is wrong>": copies that cannot be read or
                            ///< written
};

/// @brief Copies of the packages a book uses, in the folder types/ of the book
struct PackageCopies {
    /// @brief Folder of the copies of the book in @p projectDir
    static QString folderOf(const QString& projectDir);

    /// @brief Folder of the copy of package @p packageId in the book in @p projectDir
    static QString folderOf(const QString& projectDir, const QString& packageId);

    /// @brief The copy of package @p packageId in the book in @p projectDir, or nullopt when the
    /// book has none or it cannot be read
    static std::optional<BookTypePackage> read(const QString& projectDir,
                                               const QString& packageId);

    /// @brief Make or refresh the copies of the packages that @p project uses, from the
    /// packages of @p installed
    ///
    /// A package that the book has no copy of is copied, and a copy older than the installed
    /// package is replaced with it; a copy of the version installed, or of a newer one, stays.
    /// A package that is neither installed nor copied is left out.
    /// @param projectDir Folder of the book
    static PackageCopiesUpdate update(const QString& projectDir, const BookProject& project,
                                      const BookTypeRegistry& installed);

    /// @brief Write the data of @p package to folder @p destination, replacing what is there
    ///
    /// The new copy is written beside the folder and takes its place when it is complete, so
    /// a copy that cannot be written leaves the folder as it was.
    /// @param problem Gets what is wrong when the copy cannot be written
    static bool write(const BookTypePackage& package, const QString& destination,
                      QString* problem = nullptr);

    /// @brief Whether version @p version, e.g. "1.10", is newer than @p than, e.g. "1.9"
    static bool isNewer(const QString& version, const QString& than);
};

}  // namespace kalahari::core
