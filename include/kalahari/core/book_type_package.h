/// @file book_type_package.h
/// @brief Book type packages: element kinds, structure and styles of a book type, as data
///
/// A book type (novel, screenplay, poetry collection...) is a package: a folder with a
/// manifest, booktype.json, and optional files next to it (styles, templates). The program
/// knows only three forms of elements and four places in a project; the names of types and
/// kinds and their rules come from packages. Built-in packages and add-ons have one format.
///
/// booktype.json:
/// @code{.json}
/// {
///   "format": 1,
///   "id": "kalahari.novel",
///   "version": "1.0",
///   "role": "type",
///   "name": { "pl": "Powieść", "en": "Novel" },
///   "description": { "pl": "...", "en": "..." },
///   "icon": "template.novel",
///   "uses": ["kalahari.base"],
///   "kinds": {
///     "prologue": { "form": "text", "places": ["main"], "limit": 1,
///                   "name": { "pl": "Prolog", "en": "Prologue" },
///                   "plural": { "pl": "Prologi", "en": "Prologues" } }
///   },
///   "front":    ["title_page", "copyright", "dedication", "motto", "toc", "preface"],
///   "main":     ["prologue", "part", "chapter", "epilogue"],
///   "back":     ["afterword", "acknowledgments", "glossary", "about_author", "toc"],
///   "workshop": ["work_note", "mindmap", "timeline", "character", "location", "item", "material"],
///   "primary":  "chapter",
///   "start":    ["title_page", "chapter"],
///   "styles":   "styles.json"
/// }
/// @endcode
///
/// - "role": "type" is a book type of the New Book window; "shared" is a package of kinds and
///   styles that types use ("kalahari.base").
/// - "uses": packages whose kinds and styles this package uses. A kind or style of the package
///   itself replaces one with the same id from a package it uses.
/// - "front", "main", "back", "workshop": kinds offered in the front, main and back part of the
///   book and in the Workshop, in this order.
/// - "primary": the main text kind (chapter, story, poem), "start": kinds a new book starts with.
/// - "partsLayer" (types only, true when missing): whether a new book of the type shows its
///   front, main and back parts; without them, the elements of all three are right in the book.
/// - The lists, "primary" and "start" can name a kind of another package without using it:
///   "kalahari.nonfiction:bibliography". Such a reference takes the kind alone; the styles of
///   its package do not become styles of this package.
///
/// A kind (rodzaj): "form" ("text", "group" or "window"), "name" and "plural", "icon",
/// "places" (front, main, back, workshop or ids of group kinds it can be inside), "limit"
/// (most elements of the kind in a book), "title" (default title, "%n" is the number) with
/// "numbering" ("arabic" or "roman"), "template" (text kinds: a starting .kchapter file),
/// "editor", "generated" and "settings" (window kinds), "workshopGroup" (kinds of the
/// Workshop: "libraries" or "resources", the group they go to when the Workshop is grouped).
///
/// styles.json has the fields of the paragraph_styles and character_styles tables of
/// project.db, with names in several languages and the style of the next paragraph:
/// @code{.json}
/// {
///   "paragraph_styles": [
///     { "id": "heading_1", "name": { "pl": "Nagłówek 1", "en": "Heading 1" },
///       "base_style": "normal", "next_style": "normal", "properties": { "fontSize": 20 } }
///   ],
///   "character_styles": [
///     { "id": "emphasis", "name": { "pl": "Wyróżnienie", "en": "Emphasis" },
///       "properties": { "italic": true } }
///   ]
/// }
/// @endcode

#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <optional>

namespace kalahari::core {

/// @brief Text in several languages
struct LocalizedText {
    QMap<QString, QString> values;  ///< Language code ("en", "pl") -> text

    /// @brief Text in @p language: its exact code ("pl_PL"), then the language alone ("pl"),
    /// then English
    QString text(const QString& language) const;

    bool isEmpty() const { return values.isEmpty(); }
};

/// @brief Place of an element in a book project
enum class BookPlace {
    Front,    ///< Front part of the book (title page, dedication...)
    Main,     ///< Main part of the book (chapters, parts...)
    Back,     ///< Back part of the book (afterword, acknowledgments...)
    Workshop  ///< Workshop: files that go with the book but not into it (notes, mind maps...)
};

/// @brief Name of @p place in packages: "front", "main", "back" or "workshop"
QString bookPlaceName(BookPlace place);

/// @brief Place named @p name in packages, or nullopt when it is not a place
std::optional<BookPlace> bookPlaceFromName(const QString& name);

/// @brief Form of an element: what the program does with it
enum class ElementForm {
    Text,   ///< A .kchapter file opened in the text editor (chapter, poem, working note)
    Group,  ///< Elements grouped without a file of their own (part, cycle, episode)
    Window  ///< A file or settings opened in a window of its own (mind map, card, contents)
};

/// @brief Format of the number "%n" in a default title
enum class TitleNumbering {
    Arabic,  ///< "Chapter 3"
    Roman    ///< "Act III"
};

/// @brief Group of the Workshop that a kind goes to when the Workshop is grouped
enum class WorkshopGroup {
    None,       ///< First level of the Workshop: working notes, mind maps, timelines...
    Libraries,  ///< Cards: characters, locations, items, sources
    Resources   ///< Materials
};

/// @brief Name of @p group in packages: "libraries" or "resources"; empty for None
QString workshopGroupName(WorkshopGroup group);

/// @brief Group named @p name in packages, or nullopt when it is not a group of the Workshop
std::optional<WorkshopGroup> workshopGroupFromName(const QString& name);

/// @brief Kind named in a package: by its id ("chapter"), or with the package whose kind it
/// is ("kalahari.nonfiction:bibliography")
struct KindReference {
    QString packageId;  ///< Empty for a kind named by its id alone
    QString kindId;     ///< Id of the kind, e.g. "bibliography"

    /// @brief Reference written as @p text, or nullopt when @p text is not one
    static std::optional<KindReference> parse(const QString& text);

    /// @brief "kalahari.nonfiction:bibliography", or the kind id alone without a package
    QString toString() const;

    bool operator==(const KindReference&) const = default;
};

/// @brief Kind of element (rodzaj): chapter, dedication, act, poem, mind map...
struct ElementKind {
    QString id;                          ///< Unique in its package, e.g. "chapter"
    ElementForm form = ElementForm::Text;
    LocalizedText name;                  ///< Singular name: "Chapter"
    LocalizedText plural;                ///< Plural name: "Chapters"
    QString icon;                        ///< Icon id of ArtProvider; may be empty
    QStringList places;                  ///< "front", "main", "back", "workshop" or group kind ids
    int limit = 0;                       ///< Most elements of this kind in a book; 0: no limit
    LocalizedText title;                 ///< Default title, "%n" is the number; empty: the name
    TitleNumbering numbering = TitleNumbering::Arabic;
    QString templateFile;                ///< Text kinds: starting .kchapter, in the package
    QString editor;                      ///< Window kinds: id of the window that opens the element
    bool generated = false;              ///< Window kinds: content made by a tool of the type
    QJsonObject settings;                ///< Window kinds: settings of the window
    WorkshopGroup workshopGroup = WorkshopGroup::None;  ///< Workshop kinds: group when grouped

    /// @brief Whether an element of this kind can be in @p place, outside groups
    bool allows(BookPlace place) const;

    /// @brief Whether an element of this kind can be inside a group of kind @p groupKindId
    bool allowsInside(const QString& groupKindId) const;

    /// @brief Default title of the @p number-th element of this kind in @p language
    QString defaultTitle(const QString& language, int number) const;
};

/// @brief Paragraph style of a package (a row of the paragraph_styles table of project.db)
struct PackageParagraphStyle {
    QString id;              ///< Unique among the package's paragraph styles
    LocalizedText name;      ///< Display name
    QString baseStyle;       ///< Paragraph style it inherits from; may be empty
    QString nextStyle;       ///< Style of the paragraph after it; empty: the same style
    QVariantMap properties;  ///< Properties of StyleResolver: fontSize, alignment...
};

/// @brief Character style of a package (a row of the character_styles table of project.db)
struct PackageCharacterStyle {
    QString id;              ///< Unique among the package's character styles
    LocalizedText name;      ///< Display name
    QVariantMap properties;  ///< Properties of StyleResolver: bold, italic...
};

/// @brief What a package is for
enum class PackageRole {
    Type,   ///< A book type, offered in the New Book window
    Shared  ///< Kinds and styles that types use; not a type itself
};

/// @brief Book type package read from its folder
///
/// read() checks everything that does not depend on other packages; BookTypeRegistry checks
/// the rest (kinds and styles of used packages).
struct BookTypePackage {
    static constexpr int FORMAT = 1;  ///< Version of the package format this program reads
    static constexpr const char* MANIFEST_FILE = "booktype.json";

    QString id;                       ///< Unique id, e.g. "kalahari.novel"
    QString version;                  ///< Version of the package, e.g. "1.0"
    PackageRole role = PackageRole::Type;
    LocalizedText name;               ///< Name, e.g. "Novel"
    LocalizedText description;        ///< One sentence about the type
    QString icon;                     ///< Icon id of ArtProvider; may be empty
    QStringList uses;                 ///< Ids of packages whose kinds and styles it uses
    QList<ElementKind> kinds;         ///< Kinds defined by this package, ordered by id
    QStringList frontKinds;           ///< Kinds offered in the front part, in order
    QStringList mainKinds;            ///< Kinds offered in the main part, in order
    QStringList backKinds;            ///< Kinds offered in the back part, in order
    QStringList workshopKinds;        ///< Kinds offered in the Workshop, in order
    QString primaryKind;              ///< Main text kind; required for a type
    QStringList startKinds;           ///< Kinds a new book of the type starts with
    bool partsLayer = true;           ///< Whether a new book shows its front, main and back parts
    QString stylesFile;               ///< Styles file, relative to the package folder
    QList<PackageParagraphStyle> paragraphStyles;  ///< Paragraph styles of the styles file
    QList<PackageCharacterStyle> characterStyles;  ///< Character styles of the styles file
    QString directory;                ///< Absolute path of the package folder

    /// @brief Kind defined by this package (not by the packages it uses), or nullptr
    const ElementKind* ownKind(const QString& kindId) const;

    /// @brief Kinds offered in @p place
    const QStringList& kindsIn(BookPlace place) const;

    /// @brief Read the package in folder @p directory
    /// @param problems Gets one line per problem: "<field>: <what is wrong>"
    /// @return The package, or nullopt when it has problems
    static std::optional<BookTypePackage> read(const QString& directory, QStringList& problems);

    /// @brief Whether @p text can be the id of a package: lowercase words joined by dots
    static bool isId(const QString& text);
};

}  // namespace kalahari::core
