// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file source_names.h
/// @brief The sources in the program's words: the names of their kinds, of what their people
/// did and of their fields, and which materials can be sources or quotes

#ifndef KALAHARI_GUI_SOURCE_NAMES_H
#define KALAHARI_GUI_SOURCE_NAMES_H

#include <QCoreApplication>
#include <QString>

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
struct SourceDescription;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The words of the sources (core::SourceDescription) in the program's language
class SourceNames {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::SourceNames)

public:
    /// @brief The id of the kind of @p element of the base package (core::MaterialKinds::QUOTE...),
    /// else empty
    [[nodiscard]] static QString baseKindOf(const core::ProjectManager& book,
                                            const core::ProjectElement& element);

    /// @brief Whether @p element can be a source: a publication, a document, a web page or a
    /// recording (core::SourceDescription::canDescribe())
    [[nodiscard]] static bool canDescribe(const core::ProjectManager& book,
                                          const core::ProjectElement& element);

    /// @brief Whether @p element is a quote
    [[nodiscard]] static bool isQuote(const core::ProjectManager& book,
                                      const core::ProjectElement& element);

    /// @brief Name of kind @p kind: "Book", "Chapter in a book"...; a kind this version does
    /// not know as it is written
    [[nodiscard]] static QString kindName(const QString& kind);

    /// @brief Name of what a person did: "author", "editor"...; a role this version does not
    /// know as it is written
    [[nodiscard]] static QString roleName(const QString& role);

    /// @brief Label of field @p field on the card of a source of kind @p kind: "Title",
    /// "Journal", "Site"...
    [[nodiscard]] static QString fieldLabel(const QString& field, const QString& kind);

    /// @brief Name of whether a bibliography has a source (core::SourceDescription::LINKED...)
    [[nodiscard]] static QString inclusionName(const QString& inclusion);

    /// @brief An example of reference form @p form (core::Bibliography::AUTHOR_YEAR...), in the
    /// program's language: "(Wiśniewska 2011, s. 45)"
    [[nodiscard]] static QString referenceExample(const QString& form);

    /// @brief "Atlas pustyń świata – Wiśniewska, 2011": the title of @p source (or @p title),
    /// its first author's family name and its year, to choose it in a list
    [[nodiscard]] static QString shortName(const core::SourceDescription& source,
                                           const QString& title);

    /// @brief "Wiśniewska Anna; Nowak Jan": the authors of @p source, family name first, as a
    /// list of sources shows and sorts them
    [[nodiscard]] static QString authorsOf(const core::SourceDescription& source);

    /// @brief The reference of the text to page @p page of source @p sourceId of @p book, in
    /// the form of the settings and the language of the book: "(Wiśniewska 2011, s. 45)";
    /// empty when the book has no such material
    [[nodiscard]] static QString referenceTo(const core::ProjectManager& book,
                                             const QString& sourceId, const QString& page);

    /// @brief @p text in the quotation marks of the language of @p book, with the reference
    /// to page @p page of source @p sourceId: „Karawana idzie…” (Wiśniewska 2011, s. 45)
    [[nodiscard]] static QString quotedWithReference(const core::ProjectManager& book,
                                                     const QString& sourceId,
                                                     const QString& text, const QString& page);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_SOURCE_NAMES_H
