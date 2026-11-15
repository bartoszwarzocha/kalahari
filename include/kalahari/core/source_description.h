// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file source_description.h
/// @brief What a material that is a source says of itself: its bibliographic description, its
/// bookmarks and, of a quote, the source and the page it comes from

#ifndef KALAHARI_CORE_SOURCE_DESCRIPTION_H
#define KALAHARI_CORE_SOURCE_DESCRIPTION_H

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace kalahari::core {

/// @brief A person or an institution of a source, with what they did
struct SourcePerson {
    QString name;       ///< "Anna Wiśniewska", "Wiśniewska, Anna" or an institution
    QString role;       ///< SourceDescription::AUTHOR...; empty: the author
    QJsonObject extra;  ///< Fields this version does not know, saved back

    bool operator==(const SourcePerson&) const = default;
};

/// @brief The bibliographic description of a material: what a bibliography and a reference in
/// the text say of it
///
/// Saved with the material as "source":
/// @code
/// "source": { "type": "book",
///             "people": [{ "name": "Anna Wiśniewska", "role": "author" }],
///             "title": "Atlas pustyń świata", "year": "2011", "isbn": "978-83-0000-000-0",
///             "bibliography": "always" }
/// @endcode
/// Each value is a text as the writer puts it ("2 popr.", "41–58", "12.03.2025"). A material
/// with a description is a source: Materials > Sources lists it. Its title, empty, is the
/// title of the material.
struct SourceDescription {
    /// @name Kinds of sources
    /// @{
    static constexpr const char* BOOK = "book";
    static constexpr const char* CHAPTER = "chapter";      ///< A chapter in a book
    static constexpr const char* ARTICLE = "article";      ///< An article in a journal
    static constexpr const char* WEB_PAGE = "webPage";     ///< A page on the Internet
    static constexpr const char* ARCHIVE = "archive";      ///< A document of an archive
    static constexpr const char* INTERVIEW = "interview";  ///< A talk or an interview
    static constexpr const char* FILM = "film";            ///< A film or a recording
    static constexpr const char* THESIS = "thesis";        ///< A thesis
    static constexpr const char* OTHER = "other";          ///< Any other source
    /// @}

    /// @name What a person of a source did
    /// @{
    static constexpr const char* AUTHOR = "author";
    static constexpr const char* EDITOR = "editor";        ///< Edited the work
    static constexpr const char* TRANSLATOR = "translator";  ///< Translated it
    static constexpr const char* INTERVIEWEE = "interviewee";  ///< Was interviewed
    static constexpr const char* INTERVIEWER = "interviewer";  ///< Asked the questions
    static constexpr const char* DIRECTOR = "director";    ///< Directed a film
    static constexpr const char* SUPERVISOR = "supervisor";    ///< Of a thesis
    static constexpr const char* INSTITUTION = "institution";  ///< The author is one
    /// @}

    /// @name The fields of a description (values)
    /// @{
    static constexpr const char* TITLE = "title";          ///< Of the work: book, chapter, page
    static constexpr const char* SUBTITLE = "subtitle";    ///< The subtitle of the work
    static constexpr const char* CONTAINER = "container";  ///< The book of a chapter, the
                                                           ///< journal of an article, the site
                                                           ///< of a page, the archive
    static constexpr const char* EDITION = "edition";      ///< "2nd edition", "wyd. 3"
    static constexpr const char* PLACE = "place";          ///< Of publishing, of a talk
    static constexpr const char* PUBLISHER = "publisher";  ///< Of a thesis: the university
    static constexpr const char* YEAR = "year";            ///< The year or the date
    static constexpr const char* VOLUME = "volume";        ///< Of a journal or of a work in volumes
    static constexpr const char* ISSUE = "issue";          ///< The number of a journal
    static constexpr const char* PAGES = "pages";          ///< All of a book; "41–58" of a part
    static constexpr const char* SERIES = "series";        ///< The series a book is in
    static constexpr const char* ISBN = "isbn";            ///< Of a book
    static constexpr const char* ISSN = "issn";            ///< Of a journal or a series
    static constexpr const char* DOI = "doi";              ///< Digital Object Identifier
    static constexpr const char* URL = "url";              ///< The address of a page
    static constexpr const char* PUBLISHED = "published";  ///< When a page was published
    static constexpr const char* ACCESSED = "accessed";    ///< When a page was read
    static constexpr const char* FONDS = "fonds";          ///< The fonds of an archive
    static constexpr const char* SIGNATURE = "signature";  ///< The call number in an archive
    static constexpr const char* FORM = "form";            ///< "rękopis", "nagranie, 48 min"
    static constexpr const char* LANGUAGE = "language";    ///< Of a work in another language
    /// @}

    /// @name In the bibliography of the book (bibliography)
    /// @{
    static constexpr const char* LINKED = "linked";  ///< When it is linked with the text
    static constexpr const char* ALWAYS = "always";  ///< Also when it is not linked
    static constexpr const char* NEVER = "never";    ///< Not even when it is linked
    /// @}

    QString type;                   ///< BOOK...; empty: not said (a book)
    QList<SourcePerson> people;     ///< Authors, editors..., in the writer's order
    QMap<QString, QString> values;  ///< TITLE... -> the text; without empty ones
    QString bibliography;           ///< ALWAYS or NEVER; empty: LINKED
    QJsonObject extra;              ///< Fields this version does not know, saved back

    /// @brief Whether it says nothing: the material is no source
    [[nodiscard]] bool isEmpty() const;

    /// @brief Value of @p field (TITLE...); empty when it has none
    [[nodiscard]] QString value(const char* field) const;

    /// @brief Set @p field to @p text, without the spaces at its ends; empty takes it out
    void setValue(const QString& field, const QString& text);

    /// @brief Its kind: type, else BOOK
    [[nodiscard]] QString kind() const;

    /// @brief Whether the bibliography has it: ALWAYS, NEVER, else LINKED and @p linked, whether
    /// the material or a quote of it is linked with the text
    [[nodiscard]] bool inBibliography(bool linked) const;

    /// @brief The people of @p role, in their order; AUTHOR has those without a role too
    [[nodiscard]] QList<SourcePerson> peopleOf(const char* role) const;

    /// @brief The kinds of sources, in the order the card offers them
    [[nodiscard]] static const QStringList& kinds();

    /// @brief What a person of a source can have done, in the order the card offers it
    [[nodiscard]] static const QStringList& roles();

    /// @brief The fields of a source of kind @p kind, in the order of its card
    [[nodiscard]] static QStringList fieldsOf(const QString& kind);

    /// @brief All the fields of a description, saved by their names
    [[nodiscard]] static const QStringList& allFields();

    /// @brief The kind of a new description of a material of base kind @p materialKind
    /// (MaterialKinds::PUBLICATION...): a book, a web page, a film, else other
    [[nodiscard]] static QString kindFor(const QString& materialKind);

    /// @brief Whether a material of base kind @p materialKind can be a source: a publication, a
    /// document, a web page or a recording
    [[nodiscard]] static bool canDescribe(const QString& materialKind);

    bool operator==(const SourceDescription&) const = default;
};

/// @brief A place in the file of a material, a page with a note: "45" "Przysłowie o karawanie"
struct PageBookmark {
    int page = 1;       ///< Page of the file, from 1
    QString note;       ///< The writer's note; empty: none
    QJsonObject extra;  ///< Fields this version does not know, saved back

    bool operator==(const PageBookmark&) const = default;
};

/// @brief Where a quote comes from: a material that is a source and its page, or an author
/// and a work written in
///
/// Saved with the quote as "quote": { "source": "m12", "page": "45" } or { "author": "...",
/// "work": "..." }. The words of the quote are the description of the material.
struct QuoteSource {
    QString source;     ///< The material it comes from; empty: none
    QString page;       ///< Its page, as the writer puts it: "45", "xii", "41–42"
    QString author;     ///< Without a source: who said it
    QString work;       ///< Without a source: where it was said
    QJsonObject extra;  ///< Fields this version does not know, saved back

    /// @brief Whether it says nothing
    [[nodiscard]] bool isEmpty() const;

    bool operator==(const QuoteSource&) const = default;
};

/// @brief A name of a person as a bibliography needs it: "Anna Wiśniewska" or "Wiśniewska,
/// Anna" is family "Wiśniewska" and given "Anna"; "Tau" or an institution is a family name
/// alone
struct PersonName {
    QString family;  ///< The family name, or the whole name
    QString given;   ///< The given names; empty: none

    /// @brief The parts of @p name; @p institution: the whole name is the family name
    [[nodiscard]] static PersonName of(const QString& name, bool institution = false);

    /// @brief The initials of the given names: "A." of "Anna", "J.-P." of "Jean-Pierre"
    [[nodiscard]] QString initials() const;
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_SOURCE_DESCRIPTION_H
