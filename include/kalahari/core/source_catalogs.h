// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file source_catalogs.h
/// @brief The catalogs on the web that give the description of a source by its ISBN or DOI,
/// and what their answers say

#ifndef KALAHARI_CORE_SOURCE_CATALOGS_H
#define KALAHARI_CORE_SOURCE_CATALOGS_H

#include "kalahari/core/source_description.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace kalahari::core {

/// @brief Where the description of a source comes from when the writer gives its number
///
/// Only on the writer's wish (LOOKUP_SETTING, off at first) and only the number goes out: a
/// DOI to doi.org, whose registration agency answers with a RIS record, an ISBN to the catalog
/// of ISBN_CATALOG_SETTING (Google Books at first, else Open Library), which answers in JSON.
class SourceCatalogs {
public:
    /// @brief Whether the card of a source has From the Web by ISBN or DOI...; false at first
    static constexpr const char* LOOKUP_SETTING = "materials.sources.lookup";

    /// @brief The catalog an ISBN goes to: GOOGLE_BOOKS (at first) or OPEN_LIBRARY
    static constexpr const char* ISBN_CATALOG_SETTING = "materials.sources.isbnCatalog";

    /// @name Catalogs
    /// @{
    static constexpr const char* GOOGLE_BOOKS = "googleBooks";  ///< ISBN: Google Books
    static constexpr const char* OPEN_LIBRARY = "openLibrary";  ///< ISBN: Open Library
    static constexpr const char* DOI_ORG = "doi";               ///< DOI: doi.org
    /// @}

    /// @brief Whether the setting allows the lookup
    [[nodiscard]] static bool lookupAllowed();

    /// @brief The catalogs of an ISBN, in the order the Settings offer them
    [[nodiscard]] static const QStringList& isbnCatalogs();

    /// @brief The catalog of an ISBN the setting says; GOOGLE_BOOKS for an unknown one
    [[nodiscard]] static QString settingsIsbnCatalog();

    /// @brief The name of catalog @p catalog as the program shows it ("Google Books")
    [[nodiscard]] static QString catalogName(const QString& catalog);

    /// @brief The ISBN @p text has, as 10 or 13 digits ("978830000000X" without hyphens or
    /// spaces, an "ISBN" in front of it allowed); empty when its last digit does not check
    [[nodiscard]] static QString isbn(const QString& text);

    /// @brief The DOI @p text has ("10.1038/nphys1170"), also written as its address
    /// ("https://doi.org/10...") or with "doi:" in front of it; empty for none
    [[nodiscard]] static QString doi(const QString& text);

    /// @brief The address catalog @p catalog answers number @p number at: %1 in its pattern
    /// is the number
    [[nodiscard]] static QString addressPattern(const QString& catalog);

    /// @brief The description Google Books gives in @p json (an answer to "q=isbn:..."); the
    /// ISBN @p isbn when it gives none; nullopt when it found nothing
    [[nodiscard]] static std::optional<SourceDescription> fromGoogleBooks(const QByteArray& json,
                                                                         const QString& isbn);

    /// @brief The description Open Library gives in @p json (an answer to
    /// "bibkeys=ISBN:...&jscmd=data"); nullopt when it found nothing
    [[nodiscard]] static std::optional<SourceDescription> fromOpenLibrary(const QByteArray& json,
                                                                         const QString& isbn);

    /// @brief The description of DOI @p doi in RIS record @p ris, as doi.org gives it; the
    /// address that is only the DOI's own is left out; nullopt for no record
    [[nodiscard]] static std::optional<SourceDescription> fromDoiRecord(const QByteArray& ris,
                                                                       const QString& doi);

    /// @brief @p found over @p shown: the fields and the people @p found has take the place of
    /// those @p shown has, the others stay, as do whether the bibliography has it and what
    /// this version does not know
    [[nodiscard]] static SourceDescription merged(const SourceDescription& shown,
                                                  const SourceDescription& found);
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_SOURCE_CATALOGS_H
