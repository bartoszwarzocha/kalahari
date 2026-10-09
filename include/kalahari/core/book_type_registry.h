/// @file book_type_registry.h
/// @brief Book type packages loaded from folders, and what a book of each type offers

#pragma once

#include <kalahari/core/book_type_package.h>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

namespace kalahari::core {

/// @brief Kind together with the package that defines it
struct KindRef {
    const BookTypePackage* package = nullptr;  ///< Package that defines the kind
    const ElementKind* kind = nullptr;         ///< The kind; nullptr when there is none

    explicit operator bool() const { return kind != nullptr; }
};

/// @brief Element that a new book of a type starts with
struct StartElement {
    KindRef kind;                        ///< Kind of the element
    BookPlace place = BookPlace::Main;   ///< The first place whose list has the kind
};

/// @brief Problem with a package; a package with problems is not loaded
struct BookTypeProblem {
    QString directory;  ///< Folder of the package
    QString message;    ///< "<field>: <what is wrong>"
};

/// @brief Book type packages loaded from folders
///
/// Each package is a folder with booktype.json (see book_type_package.h). The registry checks
/// what a package takes from the packages it uses and answers the questions of the New Book
/// and Add Element windows: which types there are, which kinds a type offers in each place
/// and inside each group, what a new book starts with and which styles it gets.
///
/// @code
/// BookTypeRegistry registry;
/// registry.load({resourcesDir + "/booktypes"});
/// for (const BookTypePackage* type : registry.bookTypes()) { ... }
/// const QList<KindRef> mainKinds = registry.kindsIn("kalahari.novel", BookPlace::Main);
/// @endcode
class BookTypeRegistry {
public:
    /// @brief Load the packages in @p directories, replacing the packages loaded before
    ///
    /// Not loaded, with their problems in problems() and in the log: a package with problems,
    /// a package that uses a package which is missing or has problems, and a package with the
    /// id of a package loaded before it. Pointers to packages and kinds stay valid until the
    /// next load().
    void load(const QStringList& directories);

    /// @brief Problems found by the last load()
    const QList<BookTypeProblem>& problems() const { return m_problems; }

    /// @brief Loaded packages: in the order of @p directories, in each by folder name
    QList<const BookTypePackage*> packages() const;

    /// @brief Loaded book types (packages with role "type"), in the order of packages()
    QList<const BookTypePackage*> bookTypes() const;

    /// @brief Package @p id, or nullptr
    const BookTypePackage* package(const QString& id) const;

    /// @brief Kind @p kindId as package @p packageId sees it
    ///
    /// The package's own kind, else one of the packages it uses. A package comes before the
    /// packages it uses, and of two packages in "uses" the later one comes first.
    KindRef findKind(const QString& packageId, const QString& kindId) const;

    /// @brief Kinds that package @p packageId offers in @p place, in its order
    QList<KindRef> kindsIn(const QString& packageId, BookPlace place) const;

    /// @brief Kinds of package @p packageId that can be inside a group of kind @p groupKindId
    ///
    /// Kinds of the package's lists, in the order main, front, back, Workshop.
    QList<KindRef> kindsInside(const QString& packageId, const QString& groupKindId) const;

    /// @brief Elements that a new book of type @p packageId starts with
    QList<StartElement> startElements(const QString& packageId) const;

    /// @brief Every kind of every package, which a book without a type offers
    ///
    /// Kinds of the same id from different packages are different kinds. In the order of
    /// packages(); in a package, in the order of its lists, then the kinds it does not list.
    QList<KindRef> allKinds() const;

    /// @brief Paragraph styles of package @p packageId and of the packages it uses
    ///
    /// Styles of used packages first; a style of the package replaces a used one of its id.
    QList<PackageParagraphStyle> paragraphStyles(const QString& packageId) const;

    /// @brief Character styles of package @p packageId and of the packages it uses
    QList<PackageCharacterStyle> characterStyles(const QString& packageId) const;

private:
    enum class CheckState { Unchecked, Checking, Valid, Invalid };

    /// Package and the packages it uses, each before the packages it uses
    QList<const BookTypePackage*> lineage(const BookTypePackage& package) const;
    KindRef findKind(const BookTypePackage& package, const QString& kindId) const;
    QList<KindRef> kindsInside(const BookTypePackage& package, const QString& groupKindId) const;
    bool validate(const BookTypePackage& package, QHash<QString, CheckState>& states);
    QStringList check(const BookTypePackage& package) const;

    std::vector<std::unique_ptr<BookTypePackage>> m_packages;
    QHash<QString, const BookTypePackage*> m_packagesById;
    QList<BookTypeProblem> m_problems;
};

}  // namespace kalahari::core
