// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file bibliography.h
/// @brief How a source shows in a bibliography and in a reference in the text, by the style the
/// book follows, and the descriptions pasted from a library catalog (BibTeX, RIS)

#ifndef KALAHARI_CORE_BIBLIOGRAPHY_H
#define KALAHARI_CORE_BIBLIOGRAPHY_H

#include "kalahari/core/source_description.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace kalahari::core {

/// @brief The entries of the sources in a bibliography and the references to them in the text
///
/// An entry follows one of the styles (PN_ISO_690...) in the words of the language of the book:
/// Polish ones for "pl", else English ones. A rich entry is HTML with the titles in italics; a
/// plain one is the same text without them. The examples of the styles are in the tests; the
/// program does not change what the writer typed, other than the case of the family names in
/// PN-ISO 690 and the initials of the given names where a style has them.
class Bibliography {
public:
    /// @name Styles of a bibliography
    /// @{
    static constexpr const char* PN_ISO_690 = "pn-iso-690";  ///< The default
    static constexpr const char* APA = "apa";  ///< The American Psychological Association
    static constexpr const char* CHICAGO = "chicago";  ///< Chicago, notes and bibliography
    static constexpr const char* MLA = "mla";          ///< The Modern Language Association
    static constexpr const char* HARVARD = "harvard";  ///< Harvard, author and date
    /// @}

    /// @name Forms of a reference in the text
    /// @{
    static constexpr const char* AUTHOR_YEAR = "authorYear";    ///< (Wiśniewska 2011, s. 45)
    static constexpr const char* AUTHOR_TITLE = "authorTitle";  ///< Wiśniewska, Atlas..., s. 45
    static constexpr const char* NUMBER = "number";             ///< [1, s. 45]
    /// @}

    /// @brief The setting of the style of new books, and of books without one of their own
    static constexpr const char* STYLE_SETTING = "materials.sources.style";

    /// @brief The setting of the form of a reference in the text
    static constexpr const char* REFERENCE_SETTING = "materials.sources.reference";

    /// @brief The styles, in the order the program offers them
    [[nodiscard]] static const QStringList& styles();

    /// @brief The name of style @p style: "PN-ISO 690", "APA"...; empty for none
    [[nodiscard]] static QString styleName(const QString& style);

    /// @brief The style of the settings (STYLE_SETTING), PN_ISO_690 for one it does not know
    [[nodiscard]] static QString settingsStyle();

    /// @brief @p style when it is one of styles(), else the style of the settings
    [[nodiscard]] static QString knownStyle(const QString& style);

    /// @brief The forms of a reference, in the order the program offers them
    [[nodiscard]] static const QStringList& referenceForms();

    /// @brief The form of the settings (REFERENCE_SETTING), AUTHOR_YEAR for one it does not know
    [[nodiscard]] static QString settingsReferenceForm();

    /// @brief The entry of @p source in a bibliography of style @p style
    /// @param title The title of the material, for a description without one
    /// @param language The language of the book: "pl" for the Polish words
    /// @param rich HTML with the titles in italics, else plain text
    [[nodiscard]] static QString entry(const SourceDescription& source, const QString& title,
                                       const QString& style, const QString& language,
                                       bool rich);

    /// @brief The reference to page @p page of @p source in the text, in form @p form
    /// @param title The title of the material, for a description without one
    /// @param number The number of the source in the bibliography (NUMBER)
    /// @param language The language of the book: "pl" for the Polish words
    [[nodiscard]] static QString reference(const SourceDescription& source, const QString& title,
                                           const QString& page, const QString& form, int number,
                                           const QString& language);

    /// @brief The reference to a quote without a source: "Author, Work"; empty when it has
    /// neither
    [[nodiscard]] static QString reference(const QuoteSource& quote, const QString& language);

    /// @brief The description in @p text: a record of BibTeX ("@book{...}") or of RIS ("TY  -
    /// BOOK"), as a library catalog or a bibliography manager copies it; nullopt for any other
    /// text. The first record of several.
    [[nodiscard]] static std::optional<SourceDescription> parse(const QString& text);
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_BIBLIOGRAPHY_H
