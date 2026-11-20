// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file volume_copies.h
/// @brief A volume of a series copied out as a book of its own, a book copied into a series as
///        a volume, and the files that go with them

#ifndef KALAHARI_CORE_VOLUME_COPIES_H
#define KALAHARI_CORE_VOLUME_COPIES_H

#include <kalahari/core/book_project.h>

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace kalahari::core {

class BookDictionary;

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

/// @brief A card of a book joined to a series, and the card of the series of its kind and title
struct CardPair {
    QString card;        ///< The id of the card of the book
    QString seriesCard;  ///< The id of the card of the series

    bool operator==(const CardPair&) const = default;
};

/// @brief Why a book cannot be joined to a series as a volume
enum class JoinProblem {
    None,           ///< It can be
    Series,         ///< It is a series itself, or has no book
    OtherType,      ///< It has another type, or one of them is a user project and the other not
    VolumeOfSeries  ///< It is the series, or a volume taken out of it that it still has: the
                    ///< series has its book or its texts
};

/// @brief Where a book joined to a series goes, and the cards it shares with the series
struct JoinRequest {
    qsizetype index = -1;       ///< Its place among the volumes; -1 or past the end: the last
    QString folder;             ///< Its folder in the folder of the series: "book2"...
    QList<CardPair> sameCards;  ///< The cards of the book that are cards of the series
    /// Whether the folder of the series has a file of this path (relative, with "/"); none:
    /// it has only the files of its elements
    std::function<bool(const QString& path)> onDisk;
};

/// @brief A book joined to a series as a volume: the series with it, and what it copies from
///        the folder of the book
struct JoinedVolume {
    BookProject project;          ///< The series, with the volume and the Workshop joined
    QString bookId;               ///< The id of the volume: that of the book
    QHash<QString, QString> ids;  ///< The ids of the cards of the book that are cards of the
                                  ///< series, and those cards' ids
    QList<ProjectCopy> files;     ///< The files of its elements, with their versions
    QList<ProjectCopy> folders;   ///< Folders copied without the files the series has: the
                                  ///< images of the volume, the copies of the packages
};

/// @brief A volume of a series taken out as a book of its own, which the series keeps, and a
/// book joined to a series as a volume, which stays as it is
///
/// The book has the volume as its only book, in the folder BookProject::BOOK_FOLDER: the files
/// of its texts in that folder keep their names, a file in the folder of another volume (a text
/// moved between the volumes) gets a free name there. Its Workshop is that of the series with
/// the same ids, without the materials of the cover and the texts for publishers of the other
/// volumes (those of the whole series and the volume's own are the book's), and without the
/// links and portraits that named what it does not have. It has the kinds, the type and the
/// paragraph styles of the series.
///
/// A book joins a series of its type (or both are user projects, whose kinds join) as a volume
/// with the same id, in a folder of its own: its files there keep their names, a file of another
/// folder gets a free one there. Its Workshop joins that of the series: an element with an id
/// of the series (of a book taken out before) is the element of the series, which keeps what
/// it has and gains the links it lacks; so is a card of the pairs that the writer chose
/// (sameCards()), and the links, portraits and fields that name the book's card name the
/// series' one. The other elements come at the end of their lists, their files under free
/// names; its materials of the cover and its texts for publishers are for the new volume. The
/// relations between cards join, the styles, the fields of the cards and the kinds of the
/// series stay, and those it lacks come along.
struct VolumeCopies {
    /// @brief Volume @p bookId of @p series as a book of its own
    /// @return nullopt when @p series has no such book or no other one
    static std::optional<TakenOutVolume> takeOut(const BookProject& series,
                                                 const QString& bookId);

    /// @brief Whether book @p book can join @p series as a volume
    static JoinProblem joinProblem(const BookProject& series, const BookProject& book);

    /// @brief The cards of the Workshop of @p book that @p series does not have, each with a
    ///        card of @p series of the same kind and title (in any case), in the order of the
    ///        Workshop of @p book; a card of the series is in one pair at most, and never one
    ///        that @p book has
    /// @param isCard Whether an element is a card
    static QList<CardPair> sameCards(const BookProject& series, const BookProject& book,
                                     const std::function<bool(const ProjectElement&)>& isCard);

    /// @brief @p series with @p book as a volume, as @p request says
    /// @return nullopt when @p book cannot join @p series (joinProblem())
    static std::optional<JoinedVolume> join(const BookProject& series, const BookProject& book,
                                            const JoinRequest& request);

    /// @brief The names, the rules and the words of @p other that @p into does not have go to
    ///        it; a name of a card of @p cards names the card of the series, and a card or a
    ///        text that has a name in @p into keeps it
    static void joinDictionary(BookDictionary& into, const BookDictionary& other,
                               const QHash<QString, QString>& cards);

    /// @brief Copy @p files, with the versions of each (ChapterVersions), and the files of
    ///        @p folders from the folder @p fromDir to the folder @p toDir, making the folders
    ///        they need; a file of a folder that @p toDir has already stays as it is
    /// @param written Gets the files written, absolute; nullptr: not wanted
    /// @return false, with what failed in @p problems, when a file cannot be copied; what is
    ///         copied stays for the caller to remove
    static bool copy(const QString& fromDir, const QString& toDir,
                     const QList<ProjectCopy>& files, const QList<ProjectCopy>& folders,
                     QStringList& problems, QStringList* written = nullptr);
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_VOLUME_COPIES_H
