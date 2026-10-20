/// @file package_card.h
/// @brief What a book keeps of each book type package it uses: the name, the version and the
/// icons of the type and of its kinds, to show them where the program does not have the package
///
/// A book works with the packages the program has. In its folder types/ it keeps a card of
/// each package it names (its type, the packages of the kinds it took and of its elements) and
/// of the packages these use, whose kinds it can take later:
/// the id, the version, the role, the name and the icon of the package, and the names and the
/// icons of its kinds, with the SVG files of these icons in each icon theme of the program. The
/// rules of the package, its styles and its templates are not on the card, so the program does
/// not need to know a type beforehand to show it. Where the program does not have a package of
/// the book, the book opens read-only and its type and kinds are shown after the cards, greyed
/// out (ProjectManager::isReadOnly()).
///
/// @code
/// Moja powieść/
/// └─ types/
///    ├─ kalahari.base/    card.json, icons/twotone/template.chapter.svg, icons/filled/...
///    └─ kalahari.novel/   card.json, icons/twotone/template.novel.svg, ...
/// @endcode
///
/// card.json:
/// @code{.json}
/// {
///   "format": 1,
///   "id": "kalahari.novel",
///   "version": "1.0",
///   "role": "type",
///   "name": { "pl": "Powieść", "en": "Novel" },
///   "icon": "template.novel",
///   "kinds": {
///     "prologue": { "name": { "pl": "Prolog", "en": "Prologue" },
///                   "plural": { "pl": "Prologi", "en": "Prologues" },
///                   "words": { "pl": { "gender": "masculine", "singular": "prolog" } },
///                   "icon": "template.chapter" }
///   }
/// }
/// @endcode

#pragma once

#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace kalahari::core {

/// @brief The card of a book type package, as a book keeps it
struct PackageCard {
    static constexpr int FORMAT = 1;                       ///< Version of the format of card.json
    static constexpr const char* FILE_NAME = "card.json";  ///< The card in its folder
    static constexpr const char* ICONS_FOLDER = "icons";   ///< icons/<icon theme>/<icon id>.svg

    QString id;                        ///< Id of the package, e.g. "kalahari.novel"
    QString version;                   ///< Version of the package
    PackageRole role = PackageRole::Type;
    LocalizedText name;                ///< Name of the package: "Novel"
    QString icon;                      ///< Icon id of the package; may be empty
    QList<ElementKind> kinds;          ///< Kinds of the package: only their ids, names, plural
                                       ///< names, forms of the names and icons
    QString directory;                 ///< Folder of the card; empty for a card not read

    /// @brief Kind @p kindId of the card, or nullptr
    const ElementKind* kind(const QString& kindId) const;

    /// @brief The SVG file of icon @p iconId in icon theme @p theme on the card ("twotone")
    /// @return Its path, or empty when the card has no such file
    QString iconFile(const QString& iconId, const QString& theme) const;

    /// @brief The card of @p package
    static PackageCard of(const BookTypePackage& package);

    /// @brief The card as the JSON object of its card.json
    QJsonObject toJson() const;

    /// @brief The card of the JSON object of a card.json
    /// @return The card, or nullopt when it is not one: of another format, or without a valid
    /// package id or version. Fields it cannot read are left out.
    static std::optional<PackageCard> fromJson(const QJsonObject& json);

    /// @brief The card in folder @p directory, or nullopt when it has none that can be read
    static std::optional<PackageCard> read(const QString& directory);
};

/// @brief What PackageCards::update() did
struct PackageCardsUpdate {
    QStringList made;       ///< Ids of the packages whose cards were written for the first time
    QStringList refreshed;  ///< Ids of the packages whose cards were written for a newer version
    QStringList problems;   ///< "<package>: <what is wrong>": cards that cannot be written
};

/// @brief The cards of the packages a book uses, in the folder types/ of the book
struct PackageCards {
    /// @brief Folder of the cards of the book in @p projectDir
    static QString folderOf(const QString& projectDir);

    /// @brief Folder of the card of package @p packageId in the book in @p projectDir
    static QString folderOf(const QString& projectDir, const QString& packageId);

    /// @brief The card of package @p packageId in the book in @p projectDir, or nullopt when
    /// the book has none or it cannot be read
    static std::optional<PackageCard> read(const QString& projectDir, const QString& packageId);

    /// @brief The cards that the book in @p projectDir has of the packages @p project names,
    /// by the ids of the packages
    static QHash<QString, PackageCard> readAll(const QString& projectDir,
                                               const BookProject& project);

    /// @brief Write the card of each package @p project names, and of the packages these use,
    /// that @p installed has, where the book has no card of it or a card of an older version
    ///
    /// A card of the version installed, or of a newer one, stays; so does the card of a
    /// package that is not installed.
    /// @param projectDir Folder of the book
    static PackageCardsUpdate update(const QString& projectDir, const BookProject& project,
                                     const BookTypeRegistry& installed);

    /// @brief Write the card of @p package, with the SVG files of its icons that the program
    /// has, to folder @p destination, replacing what is there
    ///
    /// The new card is written beside the folder and takes its place when it is complete, so
    /// a card that cannot be written leaves the folder as it was.
    /// @param problem Gets what is wrong when the card cannot be written
    static bool write(const BookTypePackage& package, const QString& destination,
                      QString* problem = nullptr);

    /// @brief Whether version @p version, e.g. "1.10", is newer than @p than, e.g. "1.9"
    static bool isNewer(const QString& version, const QString& than);
};

}  // namespace kalahari::core
