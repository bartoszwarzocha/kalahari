/// @file old_book_conversion.h
/// @brief A book of the first .klh format, converted to the book project of this version
///
/// Kalahari 0.3 saved a book in the first format: the data of the book in "document" and its
/// elements in "structure" (front matter, the parts of the body with their chapters, back
/// matter), each element with a type ("dedication", "epilogue"...). The status and the notes
/// of an element were in its chapter file. The conversion makes a novel of it, as the first
/// format had a novel's parts and chapters, and loses nothing:
/// - the files stay where they are and as they are; only the .klh file changes,
/// - each element keeps its id, title and file, and takes the status of its chapter file and
///   the kind of its type: a prologue and an epilogue go to the start and the end of the main
///   part, and a kind that a novel does not offer is taken from its package (an introduction,
///   an appendix),
/// - an element whose type has no kind, or one more of a kind than a book can have, goes
///   among the files of the book outside its text (Other Files), with its type,
/// - the fields the conversion does not use stay as they were: of the file in the project, of
///   "document" in the book, of an element in the element,
/// - the sections keep the names the first format showed: Front Matter, Body, Back Matter.
///
/// A book that cannot be converted so, e.g. with a file outside its folder or two elements
/// with one id, is not converted.

#pragma once

#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>

namespace kalahari::core {

/// @brief A book of the first format converted to the book project of this version
struct OldBookConversion {
    std::optional<BookProject> project;  ///< The converted book; none when it cannot be
                                         ///< converted
    QStringList problems;                ///< Why it cannot be converted, in the words of the log
    QStringList otherFiles;              ///< Titles of the elements that went among the files
                                         ///< of the book outside its text

    /// @brief Convert @p json, the .klh file of a book of the first format in folder
    /// @p projectDir (whose chapter files give the statuses), with the kinds of @p registry
    static OldBookConversion convert(const QJsonObject& json, const QString& projectDir,
                                     const BookTypeRegistry& registry);

    /// @brief File for the copy of the old .klh file @p manifestPath, beside it:
    /// "<name>.format1.klh.bak", with a number when that file exists ("<name>.format1-2...")
    static QString copyPathFor(const QString& manifestPath);
};

}  // namespace kalahari::core
