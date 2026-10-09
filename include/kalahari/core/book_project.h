/// @file book_project.h
/// @brief Book project as its .klh file saves it: type, kinds, books and the Workshop
///
/// A project has a book type or, in a user project, no type and the kinds chosen from the
/// palette. It can take kinds of other types. It has a list of books and one Workshop that the
/// books share; the list has one book for now, and a series will have more. An element names
/// its kind with the package that defines it, so it keeps its kind when the kind is taken out
/// of the project or its package is not installed.
///
/// <title>.klh:
/// @code{.json}
/// {
///   "format": 2,
///   "id": "5f0c3a52-8d1e-4f7a-9b20-6c1d2e3f4a5b",
///   "created": "2026-10-09T10:00:00Z",
///   "modified": "2026-10-09T12:30:00Z",
///   "type": { "id": "kalahari.novel", "version": "1.0" },
///   "kinds": ["kalahari.nonfiction:bibliography"],
///   "books": [
///     {
///       "id": "c2a1", "title": "Moja powieść", "author": "Anna Nowak", "language": "pl",
///       "genre": "", "partsLayer": true, "folder": "book",
///       "front": [
///         { "id": "e1", "kind": "kalahari.base:title_page", "title": "Strona tytułowa",
///           "file": "book/title_page_001.kchapter", "status": "draft" }
///       ],
///       "main": [
///         { "id": "e2", "kind": "kalahari.base:part", "title": "Część pierwsza",
///           "elements": [
///             { "id": "e3", "kind": "kalahari.base:chapter", "title": "Rozdział 1",
///               "file": "book/chapter_001.kchapter", "status": "revision" }
///           ] }
///       ],
///       "back": []
///     }
///   ],
///   "workshop": {
///     "grouped": false,
///     "elements": [
///       { "id": "e4", "kind": "kalahari.base:character", "title": "Anna",
///         "file": "workshop/character_001.json" }
///     ]
///   }
/// }
/// @endcode
///
/// - "type": the book type and the version of its package; a user project has none.
/// - "kinds": kinds taken from other types; in a user project, all its kinds.
/// - A book has its data (title, author, language, genre), the name of its node ("name";
///   none: "Book" in the user's language), whether the Navigator shows its front, main and back
///   parts ("partsLayer"), the folder of its files and the elements of each part.
/// - The Workshop has the name of its node ("name"), whether it is grouped and its elements.
/// - An element has an id unique in the project, its kind, a title, a file relative to the
///   project folder (if it has one), a status ("draft", "revision" or "final"; groups have
///   none) and the elements inside it (groups). The order and nesting of elements are saved
///   only here, not in folders.
/// - Fields this version of Kalahari does not know are kept and saved back.

#pragma once

#include <kalahari/core/book_type_registry.h>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace kalahari::core {

/// @brief Element of a book or of the Workshop: a chapter, a part, a character card...
struct ProjectElement {
    QString id;                      ///< Unique in the project
    KindReference kind;              ///< Kind, with the package that defines it
    QString title;                   ///< Title
    QString file;                    ///< File, relative to the project folder; empty: none
    QString status;                  ///< "draft", "revision" or "final"; empty: none (groups)
    QList<ProjectElement> elements;  ///< Elements inside it (groups), in order
    QJsonObject extra;               ///< Fields this version does not know, saved back

    /// Not defaulted: a defaulted == would depend on the == of QList<ProjectElement>, which
    /// depends on it, so the compiler would delete it
    bool operator==(const ProjectElement& other) const;
};

/// @brief Book of a project: its data, settings and elements
struct ProjectBook {
    QString id;                           ///< Unique among the books of the project
    QString title;                        ///< Title of the book
    QString author;                       ///< Author of the book
    QString language;                     ///< Language code of the text: "pl", "en"
    QString genre;                        ///< Genre; may be empty
    QString name;                         ///< Name of the book's node; empty: "Book"
    bool partsLayer = true;               ///< Whether the Navigator shows the three parts
    QString folder;                       ///< Folder of the book's files, e.g. "book"
    QList<ProjectElement> frontElements;  ///< Elements of the front part, in reading order
    QList<ProjectElement> mainElements;   ///< Elements of the main part, in reading order
    QList<ProjectElement> backElements;   ///< Elements of the back part, in reading order
    QJsonObject extra;                    ///< Fields this version does not know, saved back

    bool operator==(const ProjectBook&) const = default;
};

/// @brief Workshop of a project: files that go with its books but not into them
struct ProjectWorkshop {
    QString name;                    ///< Name of the Workshop's node; empty: "Workshop"
    bool grouped = false;            ///< Whether cards go to Libraries and materials to Resources
    QList<ProjectElement> elements;  ///< Elements, in the order they were added
    QJsonObject extra;               ///< Fields this version does not know, saved back

    bool operator==(const ProjectWorkshop&) const = default;
};

/// @brief Book type of a project
struct ProjectType {
    QString id;         ///< Id of the type package, e.g. "kalahari.novel"
    QString version;    ///< Version of the package
    QJsonObject extra;  ///< Fields this version does not know, saved back

    bool operator==(const ProjectType&) const = default;
};

/// @brief Book project as its .klh file saves it
///
/// The kinds of the project come from the packages of a BookTypeRegistry. The program does not
/// use this model yet: ProjectManager still opens and saves projects.
struct BookProject {
    static constexpr int FORMAT = 2;  ///< Version of the .klh format this program reads
    static constexpr const char* FILE_EXTENSION = ".klh";
    static constexpr const char* BOOK_FOLDER = "book";          ///< Folder of the first book
    static constexpr const char* WORKSHOP_FOLDER = "workshop";  ///< Folder of the Workshop
    static constexpr const char* TYPES_FOLDER = "types";        ///< Copies of the packages

    QString id;                       ///< Unique id of the project
    QDateTime created;                ///< When the project was made
    QDateTime modified;               ///< When the project was last saved
    std::optional<ProjectType> type;  ///< Book type; none in a user project
    QList<KindReference> kinds;       ///< Kinds taken from other types; in a user project, all
    QList<ProjectBook> books;         ///< Books; one for now, more in a series
    ProjectWorkshop workshop;         ///< Workshop shared by the books
    QJsonObject extra;                ///< Fields this version does not know, saved back

    bool operator==(const BookProject&) const = default;

    /// @brief Whether the project has no type: the author chose its kinds from the palette
    bool isUserProject() const { return !type.has_value(); }

    // Elements. Pointers to elements and lists stay valid until the project changes.

    /// @brief Elements of @p place: of a part of book @p book, or of the Workshop
    QList<ProjectElement>& elementsIn(BookPlace place, qsizetype book = 0);
    const QList<ProjectElement>& elementsIn(BookPlace place, qsizetype book = 0) const;

    /// @brief Elements of book @p book in reading order, each group before the elements inside it
    QList<const ProjectElement*> readingOrder(qsizetype book = 0) const;

    /// @brief Element @p id of a book or of the Workshop, or nullptr
    ProjectElement* findElement(const QString& id);
    const ProjectElement* findElement(const QString& id) const;

    /// @brief List that holds element @p id: a part of a book, a group or the Workshop
    /// @param index Gets the index of the element in the list
    /// @return The list, or nullptr when the project has no element @p id
    QList<ProjectElement>* listOf(const QString& id, qsizetype* index = nullptr);

    /// @brief Take element @p id, with the elements inside it, out of the project
    std::optional<ProjectElement> takeElement(const QString& id);

    /// @brief Move element @p id to place @p index of its list
    /// @return false when the project has no element @p id or its list no place @p index
    bool moveElement(const QString& id, qsizetype index);

    /// @brief Whether an element of the project has the file @p file
    bool hasFile(const QString& file) const;

    /// @brief A new id that no element of the project has
    QString newElementId() const;

    /// @brief Kinds the project offers in @p place: those of its type, then those it took
    ///
    /// Kinds that @p registry does not have are left out: their elements stay in the book,
    /// but no new ones can be added.
    QList<KindRef> kindsIn(const BookTypeRegistry& registry, BookPlace place) const;

    /// @brief Kinds the project offers inside a group of kind @p groupKindId
    QList<KindRef> kindsInside(const BookTypeRegistry& registry,
                               const QString& groupKindId) const;

    /// @brief Kind of @p element, also when the project no longer offers it; none when
    /// @p registry does not have it
    static KindRef kindOf(const BookTypeRegistry& registry, const ProjectElement& element);

    /// @brief Packages of the type, the kinds and the elements that @p registry does not have
    QStringList missingPackages(const BookTypeRegistry& registry) const;

    /// @brief The project as the JSON object of its .klh file
    QJsonObject toJson() const;

    /// @brief Project of the JSON object of a .klh file
    /// @param problems Gets one line per problem: "<field>: <what is wrong>"
    /// @return The project, or nullopt when it has problems
    static std::optional<BookProject> fromJson(const QJsonObject& json, QStringList& problems);

    /// @brief Save the project to the .klh file @p path, replacing it in one step
    /// @return false, with the reason in the log, when the file cannot be written
    bool save(const QString& path) const;

    /// @brief Read the project of the .klh file @p path
    /// @param problems Gets one line per problem: "<field>: <what is wrong>"
    /// @return The project, or nullopt when the file cannot be read or has problems
    static std::optional<BookProject> load(const QString& path, QStringList& problems);
};

}  // namespace kalahari::core
