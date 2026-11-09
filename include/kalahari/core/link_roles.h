/// @file link_roles.h
/// @brief How a material is linked with an element: the roles of links, and the paragraph a
/// link names

#pragma once

#include "kalahari/core/book_project.h"

#include <QString>
#include <QStringList>

namespace kalahari {
namespace core {

/// @brief Ids of the roles of links (ElementLink::role), the role a new link of a material
/// gets, and the paragraph of a text that a link names
struct LinkRoles {
    static constexpr const char* QUOTED = "quoted";              ///< Quoted in the text
    static constexpr const char* SOURCE = "source";              ///< A source of information
    static constexpr const char* INSPIRATION = "inspiration";    ///< An inspiration
    static constexpr const char* ILLUSTRATION = "illustration";  ///< Shows what the text tells
    static constexpr const char* TO_CHECK = "to_check";          ///< Something to check

    /// @brief The setting of whether a link may name a paragraph of a text (true, the usual
    /// one); without it, the program shows no paragraphs of links, and keeps them
    static constexpr const char* PARAGRAPHS_SETTING = "materials.linkParagraphs";

    /// @brief Words of a paragraph its excerpt keeps at most (excerptOf())
    static constexpr int EXCERPT_WORDS = 8;

    /// @brief The roles, in the order the program offers them
    static const QStringList& all();

    /// @brief The role of a new link of a material of kind @p kindId (MaterialKinds): a quote
    /// is quoted in the text, a picture is an illustration, a web page, a document and a
    /// publication are sources of information; empty for the other kinds
    static QString suggestedFor(const QString& kindId);

    /// @brief Whether a link may name a paragraph (PARAGRAPHS_SETTING)
    static bool paragraphsShown();

    /// @brief The words that find @p paragraph again: its first EXCERPT_WORDS words, with the
    /// spaces between them made single
    static QString excerptOf(const QString& paragraph);

    /// @brief The paragraph of @p paragraphs (from 1) that @p link names
    ///
    /// The one of its number while it starts with its excerpt; else, after paragraphs were
    /// added or taken out before it, the one nearest to its number that starts with the
    /// excerpt; else the one of its number, which was rewritten. 0 when the link names no
    /// paragraph or the text has fewer paragraphs.
    static int paragraphOf(const QStringList& paragraphs, const ElementLink& link);
};

} // namespace core
} // namespace kalahari
