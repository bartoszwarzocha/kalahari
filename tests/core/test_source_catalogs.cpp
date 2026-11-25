// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_source_catalogs.cpp
/// @brief The numbers of the sources (ISBN, DOI), the answers of the catalogs on the web and
/// the description they give over the one a card has

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/source_catalogs.h>
#include <kalahari/core/source_description.h>

#include <QByteArray>
#include <QString>

#include <cstdio>
#include <optional>
#include <string>

using kalahari::core::SettingsManager;
using kalahari::core::SourceCatalogs;
using kalahari::core::SourceDescription;
using kalahari::core::SourcePerson;

namespace {

/// The value of @p field of @p source
QString valueOf(const SourceDescription& source, const char* field) {
    return source.value(field);
}

/// An answer of Google Books to "q=isbn:9780306406157"
constexpr const char* GOOGLE_ANSWER = R"({
  "kind": "books#volumes", "totalItems": 1,
  "items": [ { "kind": "books#volume", "id": "x",
    "volumeInfo": {
      "title": "Atlas pustyń świata", "subtitle": "krajobrazy, ludzie, szlaki",
      "authors": [ "Anna Wiśniewska", "Jan Nowak" ],
      "publisher": "Wydawnictwo Piasek", "publishedDate": "2011-05-01",
      "industryIdentifiers": [ { "type": "ISBN_10", "identifier": "0306406152" },
                               { "type": "ISBN_13", "identifier": "9780306406157" } ],
      "pageCount": 212, "printType": "BOOK", "language": "pl" } } ] })";

/// An answer of Open Library to "bibkeys=ISBN:9780306406157&jscmd=data"
constexpr const char* OPEN_LIBRARY_ANSWER = R"({ "ISBN:9780306406157": {
    "url": "https://openlibrary.org/books/OL1M/x", "key": "/books/OL1M",
    "title": "Atlas pustyń świata", "subtitle": "krajobrazy, ludzie, szlaki",
    "authors": [ { "url": "https://openlibrary.org/authors/OL1A", "name": "Anna Wiśniewska" } ],
    "number_of_pages": 212, "pagination": "212 s.",
    "identifiers": { "isbn_10": [ "0306406152" ], "isbn_13": [ "9780306406157" ] },
    "publishers": [ { "name": "Wydawnictwo Piasek" } ],
    "publish_places": [ { "name": "Warszawa" } ],
    "publish_date": "May 1, 2011" } })";

/// A RIS record of doi.org for an article, as Crossref gives it
constexpr const char* DOI_RECORD = "TY  - JOUR\r\n"
                                  "DO  - 10.1038/nphys1170\r\n"
                                  "UR  - http://dx.doi.org/10.1038/nphys1170\r\n"
                                  "TI  - Measured measurement\r\n"
                                  "T2  - Nature Physics\r\n"
                                  "AU  - Aspelmeyer, Markus\r\n"
                                  "PY  - 2009\r\n"
                                  "DA  - 2009/01\r\n"
                                  "PB  - Springer Science and Business Media LLC\r\n"
                                  "SP  - 9\r\n"
                                  "EP  - 11\r\n"
                                  "IS  - 1\r\n"
                                  "VL  - 5\r\n"
                                  "SN  - 1745-2473\r\n"
                                  "ER  - \r\n";

/// Keeps the settings of the lookup and gives them back
class LookupSettings {
public:
    LookupSettings()
        : m_lookup(SettingsManager::getInstance().get<bool>(SourceCatalogs::LOOKUP_SETTING,
                                                            false)),
          m_catalog(SettingsManager::getInstance().get<std::string>(
              SourceCatalogs::ISBN_CATALOG_SETTING, SourceCatalogs::GOOGLE_BOOKS)) {}
    ~LookupSettings() {
        try {
            SettingsManager::getInstance().set<bool>(SourceCatalogs::LOOKUP_SETTING, m_lookup);
            SettingsManager::getInstance().set<std::string>(SourceCatalogs::ISBN_CATALOG_SETTING,
                                                            m_catalog);
        } catch (...) {
            // A destructor may not throw: the later tests then find the settings of this one
            static_cast<void>(
                std::fputs("LookupSettings: the settings of the test stay\n", stderr));
        }
    }
    LookupSettings(const LookupSettings&) = delete;
    LookupSettings& operator=(const LookupSettings&) = delete;
    LookupSettings(LookupSettings&&) = delete;
    LookupSettings& operator=(LookupSettings&&) = delete;

private:
    bool m_lookup;
    std::string m_catalog;
};

}  // namespace

TEST_CASE("Source catalogs: ISBN and DOI as the writer types them", "[core][sources][lookup]") {
    // An ISBN of 13 or 10 digits, with hyphens, spaces and "ISBN"; its last digit checks
    CHECK(SourceCatalogs::isbn(QStringLiteral("978-0-306-40615-7")) ==
          QStringLiteral("9780306406157"));
    CHECK(SourceCatalogs::isbn(QStringLiteral("ISBN 978 0 306 40615 7")) ==
          QStringLiteral("9780306406157"));
    CHECK(SourceCatalogs::isbn(QStringLiteral("ISBN-10: 0-306-40615-2")) ==
          QStringLiteral("0306406152"));
    CHECK(SourceCatalogs::isbn(QStringLiteral("0-8044-2957-x")) == QStringLiteral("080442957X"));
    CHECK(SourceCatalogs::isbn(QStringLiteral("978-0-306-40615-8")).isEmpty());
    CHECK(SourceCatalogs::isbn(QStringLiteral("0-306-40615-3")).isEmpty());
    CHECK(SourceCatalogs::isbn(QStringLiteral("12345")).isEmpty());
    CHECK(SourceCatalogs::isbn(QStringLiteral("97X0306406157")).isEmpty());

    // A DOI, also as its address or with "doi:"
    CHECK(SourceCatalogs::doi(QStringLiteral(" 10.1038/nphys1170 ")) ==
          QStringLiteral("10.1038/nphys1170"));
    CHECK(SourceCatalogs::doi(QStringLiteral("https://doi.org/10.1038/nphys1170")) ==
          QStringLiteral("10.1038/nphys1170"));
    CHECK(SourceCatalogs::doi(QStringLiteral("http://dx.doi.org/10.1000/a%2Fb")) ==
          QStringLiteral("10.1000/a/b"));
    CHECK(SourceCatalogs::doi(QStringLiteral("doi:10.1000/xyz")) == QStringLiteral("10.1000/xyz"));
    CHECK(SourceCatalogs::doi(QStringLiteral("DOI 10.1000/xyz")) == QStringLiteral("10.1000/xyz"));
    CHECK(SourceCatalogs::doi(QStringLiteral("11.1000/xyz")).isEmpty());
    CHECK(SourceCatalogs::doi(QStringLiteral("10.1000")).isEmpty());
    CHECK(SourceCatalogs::doi(QStringLiteral("978-0-306-40615-7")).isEmpty());

    // The addresses the numbers go to
    CHECK(SourceCatalogs::addressPattern(QString::fromLatin1(SourceCatalogs::DOI_ORG)) ==
          QStringLiteral("https://doi.org/%1"));
    CHECK(SourceCatalogs::addressPattern(QString::fromLatin1(SourceCatalogs::GOOGLE_BOOKS))
              .startsWith(QStringLiteral("https://www.googleapis.com/books/v1/volumes?q=isbn:")));
    CHECK(SourceCatalogs::addressPattern(QString::fromLatin1(SourceCatalogs::OPEN_LIBRARY))
              .startsWith(QStringLiteral("https://openlibrary.org/api/books?bibkeys=ISBN:%1")));
    CHECK(SourceCatalogs::catalogName(QString::fromLatin1(SourceCatalogs::OPEN_LIBRARY)) ==
          QStringLiteral("Open Library"));
}

TEST_CASE("Source catalogs: the settings of the lookup", "[core][sources][lookup]") {
    const LookupSettings saved;
    auto& settings = SettingsManager::getInstance();
    settings.set<bool>(SourceCatalogs::LOOKUP_SETTING, false);
    CHECK_FALSE(SourceCatalogs::lookupAllowed());
    settings.set<bool>(SourceCatalogs::LOOKUP_SETTING, true);
    CHECK(SourceCatalogs::lookupAllowed());

    settings.set<std::string>(SourceCatalogs::ISBN_CATALOG_SETTING, SourceCatalogs::OPEN_LIBRARY);
    CHECK(SourceCatalogs::settingsIsbnCatalog() == QLatin1String(SourceCatalogs::OPEN_LIBRARY));
    settings.set<std::string>(SourceCatalogs::ISBN_CATALOG_SETTING, "card catalog");
    CHECK(SourceCatalogs::settingsIsbnCatalog() == QLatin1String(SourceCatalogs::GOOGLE_BOOKS));
    CHECK(SourceCatalogs::isbnCatalogs().size() == 2);
}

TEST_CASE("Source catalogs: the answers of Google Books and Open Library",
          "[core][sources][lookup]") {
    const QString isbn = QStringLiteral("9780306406157");
    const std::optional<SourceDescription> google = SourceCatalogs::fromGoogleBooks(GOOGLE_ANSWER,
                                                                                    isbn);
    REQUIRE(google.has_value());
    CHECK(google->type == QLatin1String(SourceDescription::BOOK));
    REQUIRE(google->people.size() == 2);
    CHECK(google->people.at(0).name == QStringLiteral("Anna Wiśniewska"));
    CHECK(google->people.at(0).role.isEmpty());
    CHECK(valueOf(*google, SourceDescription::TITLE) == QStringLiteral("Atlas pustyń świata"));
    CHECK(valueOf(*google, SourceDescription::SUBTITLE) ==
          QStringLiteral("krajobrazy, ludzie, szlaki"));
    CHECK(valueOf(*google, SourceDescription::PUBLISHER) == QStringLiteral("Wydawnictwo Piasek"));
    CHECK(valueOf(*google, SourceDescription::YEAR) == QStringLiteral("2011"));
    CHECK(valueOf(*google, SourceDescription::PAGES) == QStringLiteral("212"));
    CHECK(valueOf(*google, SourceDescription::ISBN) == isbn);
    CHECK(valueOf(*google, SourceDescription::PLACE).isEmpty());

    // Nothing found, or no answer of the catalog
    CHECK_FALSE(
        SourceCatalogs::fromGoogleBooks(R"({"kind":"books#volumes","totalItems":0})", isbn));
    CHECK_FALSE(SourceCatalogs::fromGoogleBooks("<html>", isbn));
    CHECK_FALSE(SourceCatalogs::fromGoogleBooks(R"({"items":[{"volumeInfo":{"pageCount":5}}]})",
                                                isbn));

    const std::optional<SourceDescription> open = SourceCatalogs::fromOpenLibrary(
        OPEN_LIBRARY_ANSWER, isbn);
    REQUIRE(open.has_value());
    REQUIRE(open->people.size() == 1);
    CHECK(open->people.at(0).name == QStringLiteral("Anna Wiśniewska"));
    CHECK(valueOf(*open, SourceDescription::TITLE) == QStringLiteral("Atlas pustyń świata"));
    CHECK(valueOf(*open, SourceDescription::PLACE) == QStringLiteral("Warszawa"));
    CHECK(valueOf(*open, SourceDescription::PUBLISHER) == QStringLiteral("Wydawnictwo Piasek"));
    CHECK(valueOf(*open, SourceDescription::YEAR) == QStringLiteral("2011"));
    CHECK(valueOf(*open, SourceDescription::PAGES) == QStringLiteral("212"));
    CHECK(valueOf(*open, SourceDescription::ISBN) == isbn);
    // Asked by the old ISBN of the same book
    CHECK(SourceCatalogs::fromOpenLibrary(OPEN_LIBRARY_ANSWER, QStringLiteral("0306406152"))
              .has_value());
    CHECK_FALSE(SourceCatalogs::fromOpenLibrary("{}", isbn));
}

TEST_CASE("Source catalogs: the record of a DOI and the description over the card's",
          "[core][sources][lookup]") {
    const QString doi = QStringLiteral("10.1038/nphys1170");
    const std::optional<SourceDescription> article = SourceCatalogs::fromDoiRecord(DOI_RECORD,
                                                                                   doi);
    REQUIRE(article.has_value());
    CHECK(article->type == QLatin1String(SourceDescription::ARTICLE));
    CHECK(valueOf(*article, SourceDescription::TITLE) == QStringLiteral("Measured measurement"));
    CHECK(valueOf(*article, SourceDescription::CONTAINER) == QStringLiteral("Nature Physics"));
    CHECK(valueOf(*article, SourceDescription::YEAR) == QStringLiteral("2009"));
    CHECK(valueOf(*article, SourceDescription::PAGES) == QStringLiteral("9–11"));
    CHECK(valueOf(*article, SourceDescription::DOI) == doi);
    // The address that is only the DOI's own is left out
    CHECK(valueOf(*article, SourceDescription::URL).isEmpty());

    // A record without the DOI gets the one asked; another address stays
    const QByteArray other = "\xEF\xBB\xBFTY  - BOOK\nTI  - Atlas\n"
                             "UR  - https://example.com/atlas\nER  - \n";
    const std::optional<SourceDescription> book = SourceCatalogs::fromDoiRecord(other, doi);
    REQUIRE(book.has_value());
    CHECK(valueOf(*book, SourceDescription::DOI) == doi);
    CHECK(valueOf(*book, SourceDescription::URL) == QStringLiteral("https://example.com/atlas"));
    // A page of HTML, or BibTeX, is no record of doi.org
    CHECK_FALSE(SourceCatalogs::fromDoiRecord("<html><style>@media (x) {}</style></html>", doi));
    CHECK_FALSE(SourceCatalogs::fromDoiRecord("@article{x, title={A}}", doi));

    // What the catalog gives takes the place of the card's; the rest stays
    SourceDescription shown;
    shown.type = QString::fromLatin1(SourceDescription::BOOK);
    shown.people << SourcePerson{.name = QStringLiteral("A. Nobody"), .role = {}, .extra = {}};
    shown.setValue(QString::fromLatin1(SourceDescription::TITLE), QStringLiteral("Typed"));
    shown.setValue(QString::fromLatin1(SourceDescription::LANGUAGE), QStringLiteral("polski"));
    shown.bibliography = QString::fromLatin1(SourceDescription::ALWAYS);
    const SourceDescription merged = SourceCatalogs::merged(shown, *article);
    CHECK(merged.type == QLatin1String(SourceDescription::ARTICLE));
    REQUIRE(merged.people.size() == 1);
    CHECK(merged.people.at(0).name == QStringLiteral("Aspelmeyer, Markus"));
    CHECK(valueOf(merged, SourceDescription::TITLE) == QStringLiteral("Measured measurement"));
    CHECK(valueOf(merged, SourceDescription::LANGUAGE) == QStringLiteral("polski"));
    CHECK(merged.bibliography == QLatin1String(SourceDescription::ALWAYS));
    // A description without people keeps the card's
    SourceDescription titleOnly;
    titleOnly.setValue(QString::fromLatin1(SourceDescription::TITLE), QStringLiteral("New"));
    CHECK(SourceCatalogs::merged(shown, titleOnly).people == shown.people);
    CHECK(SourceCatalogs::merged(shown, titleOnly).type == shown.type);
}
