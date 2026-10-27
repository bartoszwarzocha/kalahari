/// @file custom_book_type.h
/// @brief The writer's own book types: a book type, or none, with kinds of other types, saved
/// to be chosen again in the New Book window
///
/// The writer saves the kinds of a book ("Save as Type...") as a type of their own, which the
/// New Book window offers after the book types. It names a book type, or none for a user
/// project, and the kinds of the packages it takes, never their data: a book made of it is a
/// book of that type with those kinds taken from other types, or a user project with those
/// kinds, so it opens wherever the program has the packages. The types are files in the folder
/// booktypes beside the program's settings file (SettingsManager::getSettingsFilePath()), one
/// file each, named after its id.
///
/// <id>.json:
/// @code{.json}
/// {
///   "format": 1,
///   "id": "6a1f2c4e-3b7d-4c55-9e0a-1f2b3c4d5e6f",
///   "name": "Powieść historyczna",
///   "description": "Powieść z bibliografią i aneksem.",
///   "type": "kalahari.novel",
///   "kinds": ["kalahari.nonfiction:bibliography", "kalahari.nonfiction:annex"],
///   "start": []
/// }
/// @endcode
///
/// - "type": the package of the book type; none: a user project.
/// - "kinds": with a type, the kinds taken from other types; without one, all the kinds.
/// - "start": without a type, the elements a new book starts with, each a kind and a place:
///   { "kind": "kalahari.base:chapter", "place": "main" }. A book of a type starts with the
///   elements of its type.
/// - Fields this version does not know are kept and saved back.

#pragma once

#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace kalahari::core {

struct BookProject;
struct NewProject;
struct PaletteChoice;

/// @brief An element a new book of the writer's own type starts with
struct CustomStartElement {
    KindReference kind;                 ///< Its kind
    BookPlace place = BookPlace::Main;  ///< Its place in the book, or the Workshop

    bool operator==(const CustomStartElement&) const = default;
};

/// @brief A book type of the writer's own
struct CustomBookType {
    static constexpr int FORMAT = 1;  ///< Version of the format this program reads
    static constexpr const char* FILE_EXTENSION = ".json";
    static constexpr const char* FOLDER = "booktypes";  ///< Beside the program's settings

    QString id;                        ///< Unique id; it names the file
    QString name;                      ///< Name in the New Book window
    QString description;               ///< The writer's sentence about it; may be empty
    QString typeId;                    ///< Package of the book type; empty: a user project
    QList<KindReference> kinds;        ///< Kinds taken from other types; without a type, all
    QList<CustomStartElement> start;   ///< Without a type: elements a new book starts with
    QJsonObject extra;                 ///< Fields this version does not know, saved back
    QString file;                      ///< Its file (absolute); empty: not saved

    bool operator==(const CustomBookType&) const = default;

    /// @brief Whether it makes a user project: a book without a type
    bool isUserProject() const { return typeId.isEmpty(); }

    /// @brief The kinds of @p project: its type and the kinds it took, or, in a user project,
    /// its kinds with an element of the project's main text kind to start with
    /// @param registry Packages of the kinds, for the main text kind of a user project
    static CustomBookType of(const BookProject& project, const BookTypeRegistry& registry);

    /// @brief A user project with the kinds and the elements of @p choice
    static CustomBookType of(const PaletteChoice& choice);

    /// @brief The kinds of @p registry that it names, in its order; those that @p registry
    /// does not have are left out
    QList<KindRef> kindsIn(const BookTypeRegistry& registry) const;

    /// @brief The kinds it names that @p registry does not have ("kalahari.poetry:poem")
    QStringList missingKinds(const BookTypeRegistry& registry) const;

    /// @brief The elements a new book of it starts with, as ProjectManager makes them: the
    /// text elements its type starts with, or, without a type, its own elements that
    /// applyTo() keeps
    QList<StartElement> startElementsIn(const BookTypeRegistry& registry) const;

    /// @brief Give @p project its type, its taken kinds, or its palette: a book of a type
    /// takes the kinds, a user project the kinds and the elements it starts with
    ///
    /// Kinds and elements that @p registry does not have are left out; so is an element of a
    /// kind the type does not name or in a place its kind does not allow.
    void applyTo(NewProject& project, const BookTypeRegistry& registry) const;

    /// @brief The type as the JSON object of its file
    QJsonObject toJson() const;

    /// @brief The type of the JSON object of a file
    /// @param problems Gets one line per problem: "<field>: <what is wrong>"
    /// @return The type, or nullopt when it has problems
    static std::optional<CustomBookType> fromJson(const QJsonObject& json, QStringList& problems);
};

/// @brief The writer's own book types, in their folder
struct CustomBookTypes {
    /// @brief The folder of the writer's own types: booktypes beside the program's settings
    static QString folder();

    /// @brief The types in @p folder, sorted by name; files that cannot be read are left out,
    /// with their problems in the log
    static QList<CustomBookType> load(const QString& folder = CustomBookTypes::folder());

    /// @brief Save @p type to @p folder, in the file of its id, replacing the file in one step
    ///
    /// A type without an id gets a new one. @p type's file names the saved file.
    /// @param problem Gets what is wrong when it cannot be saved
    /// @return false when it has no name or the file cannot be written
    static bool save(CustomBookType& type, const QString& folder = CustomBookTypes::folder(),
                     QString* problem = nullptr);

    /// @brief Delete the file of @p type
    /// @return false when it has no file or the file cannot be deleted
    static bool remove(const CustomBookType& type);

    /// @brief The type of @p types named @p name, ignoring case and the spaces at its ends, or
    /// nullptr
    static const CustomBookType* named(const QList<CustomBookType>& types, const QString& name);
};

}  // namespace kalahari::core
