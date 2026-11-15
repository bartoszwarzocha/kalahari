// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_sources.cpp
/// @brief The materials that are sources: their descriptions, bookmarks and quotes in the book
/// file, their entries in the styles of a bibliography, the references in the text and the
/// descriptions pasted as BibTeX and RIS

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/bibliography.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/source_description.h>

#include <QJsonDocument>

using namespace kalahari::core;

namespace {

QJsonObject jsonOf(const QByteArray& text) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    INFO(error.errorString().toStdString());
    REQUIRE(document.isObject());
    return document.object();
}

/// A project with the Workshop elements @p elements (JSON) and the fields @p book of its book
QByteArray projectWith(const QByteArray& elements, const QByteArray& book = {}) {
    return R"({ "format": 2, "id": "p",
                "books": [ { "id": "b", "folder": "book")" +
           book + R"( } ],
                "workshop": { "grouped": false, "elements": [ )" +
           elements + "] } }";
}

SourcePerson person(const char* name, const char* role = "") {
    return SourcePerson{QString::fromUtf8(name), QString::fromLatin1(role), {}};
}

/// The book of the examples: "Atlas pustyń świata"
SourceDescription atlas() {
    SourceDescription source;
    source.type = QString::fromLatin1(SourceDescription::BOOK);
    source.people << person("Anna Wiśniewska", SourceDescription::AUTHOR);
    source.setValue(QStringLiteral("title"), QStringLiteral("Atlas pustyń świata"));
    source.setValue(QStringLiteral("subtitle"), QStringLiteral("krajobrazy, ludzie, szlaki"));
    source.setValue(QStringLiteral("edition"), QStringLiteral("2 popr."));
    source.setValue(QStringLiteral("place"), QStringLiteral("Warszawa"));
    source.setValue(QStringLiteral("publisher"), QStringLiteral("Wydawnictwo Piasek"));
    source.setValue(QStringLiteral("year"), QStringLiteral("2011"));
    source.setValue(QStringLiteral("pages"), QStringLiteral("212"));
    source.setValue(QStringLiteral("isbn"), QStringLiteral("978-83-0000-000-0"));
    return source;
}

QString entry(const SourceDescription& source, const char* style, const char* language = "pl",
              bool rich = false) {
    return Bibliography::entry(source, QStringLiteral("Material"), QString::fromLatin1(style),
                               QString::fromLatin1(language), rich);
}

} // namespace

TEST_CASE("Sources: the description, bookmarks and quotes in the book file", "[core][sources]") {
    const QByteArray elements = R"(
        { "id": "m12", "kind": "kalahari.base:publication", "title": "Atlas",
          "file": "workshop/atlas.pdf",
          "source": { "type": "book",
                      "people": [ { "name": "Anna Wiśniewska", "role": "author", "orcid": "x" },
                                  { "name": "Jan Nowak", "role": "translator" } ],
                      "title": "Atlas pustyń świata", "year": "2011",
                      "isbn": "978-83-0000-000-0", "bibliography": "always",
                      "callNumber": "II 1234" },
          "bookmarks": [ { "page": 45, "note": "Przysłowie o karawanie" },
                         { "page": 210, "color": "green" } ],
          "lastPage": 45 },
        { "id": "m31", "kind": "kalahari.base:quote", "title": "Karawana",
          "description": "Karawana idzie tak szybko, jak jej najwolniejszy wielbłąd.",
          "quote": { "source": "m12", "page": "45", "line": 3 } },
        { "id": "m32", "kind": "kalahari.base:quote", "title": "Przysłowie",
          "quote": { "author": "Tau", "work": "Rozmowa w Ghanzi" } })";
    const QByteArray book = R"(, "bibliographyStyle": "apa")";
    QStringList problems;
    const std::optional<BookProject> project =
        BookProject::fromJson(jsonOf(projectWith(elements, book)), problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    REQUIRE(problems.isEmpty());

    const ProjectElement& publication = project->workshop.elements.at(0);
    const SourceDescription& source = publication.source;
    CHECK(source.type == QStringLiteral("book"));
    REQUIRE(source.people.size() == 2);
    CHECK(source.people.at(0).name == QStringLiteral("Anna Wiśniewska"));
    CHECK(source.people.at(0).role == QStringLiteral("author"));
    CHECK(source.people.at(0).extra.keys() == QStringList{QStringLiteral("orcid")});
    CHECK(source.people.at(1).role == QStringLiteral("translator"));
    CHECK(source.value(SourceDescription::TITLE) == QStringLiteral("Atlas pustyń świata"));
    CHECK(source.value(SourceDescription::YEAR) == QStringLiteral("2011"));
    CHECK(source.value(SourceDescription::ISBN) == QStringLiteral("978-83-0000-000-0"));
    CHECK(source.bibliography == QStringLiteral("always"));
    CHECK(source.extra.keys() == QStringList{QStringLiteral("callNumber")});
    REQUIRE(publication.bookmarks.size() == 2);
    CHECK(publication.bookmarks.at(0).page == 45);
    CHECK(publication.bookmarks.at(0).note == QStringLiteral("Przysłowie o karawanie"));
    CHECK(publication.bookmarks.at(1).note.isEmpty());
    CHECK(publication.bookmarks.at(1).extra.keys() == QStringList{QStringLiteral("color")});
    CHECK(publication.lastPage == 45);
    CHECK(publication.quote.isEmpty());
    CHECK(publication.extra.isEmpty());

    const QuoteSource& quote = project->workshop.elements.at(1).quote;
    CHECK(quote.source == QStringLiteral("m12"));
    CHECK(quote.page == QStringLiteral("45"));
    CHECK(quote.extra.keys() == QStringList{QStringLiteral("line")});
    CHECK(project->workshop.elements.at(1).source.isEmpty());
    CHECK(project->workshop.elements.at(2).quote.author == QStringLiteral("Tau"));
    CHECK(project->workshop.elements.at(2).quote.work == QStringLiteral("Rozmowa w Ghanzi"));
    CHECK(project->books.at(0).bibliographyStyle == QStringLiteral("apa"));

    // The Workshop is saved as it was read, the book with its style
    const QJsonObject read = jsonOf(projectWith(elements, book));
    const QJsonObject saved = project->toJson();
    CHECK(saved[QStringLiteral("workshop")] == read[QStringLiteral("workshop")]);
    CHECK(saved[QStringLiteral("books")][0][QStringLiteral("bibliographyStyle")].toString() ==
          QStringLiteral("apa"));

    // A change changes the project; emptied, nothing is saved
    BookProject changed = *project;
    changed.workshop.elements[0].source = SourceDescription{};
    changed.workshop.elements[0].bookmarks.clear();
    changed.workshop.elements[0].lastPage = 0;
    changed.workshop.elements[1].quote = QuoteSource{};
    changed.books[0].bibliographyStyle.clear();
    CHECK_FALSE(changed == *project);
    const QJsonObject emptied = changed.toJson();
    const QJsonObject first =
        emptied[QStringLiteral("workshop")][QStringLiteral("elements")][0].toObject();
    for (const char* key : {"source", "bookmarks", "lastPage"}) {
        CHECK_FALSE(first.contains(QLatin1String(key)));
    }
    CHECK_FALSE(emptied[QStringLiteral("workshop")][QStringLiteral("elements")][1]
                    .toObject()
                    .contains(QLatin1String("quote")));
    CHECK_FALSE(emptied[QStringLiteral("books")][0].toObject().contains(
        QLatin1String("bibliographyStyle")));
}

TEST_CASE("Sources: wrong values in the book file are problems", "[core][sources]") {
    const auto problemsOf = [](const QByteArray& element) {
        QStringList problems;
        (void)BookProject::fromJson(jsonOf(projectWith(element)), problems);
        return problems;
    };
    CHECK_FALSE(problemsOf(R"({ "id": "m1", "kind": "kalahari.base:publication",
                                "title": "A", "source": "book" })")
                    .isEmpty());
    CHECK_FALSE(problemsOf(R"({ "id": "m1", "kind": "kalahari.base:publication",
                                "title": "A", "bookmarks": [ { "page": 0 } ] })")
                    .isEmpty());
    CHECK_FALSE(problemsOf(R"({ "id": "m1", "kind": "kalahari.base:publication",
                                "title": "A", "lastPage": -2 })")
                    .isEmpty());
    CHECK_FALSE(problemsOf(R"({ "id": "m1", "kind": "kalahari.base:quote",
                                "title": "A", "quote": [ "m2" ] })")
                    .isEmpty());
}

TEST_CASE("Sources: the kinds, their fields and the names of people", "[core][sources]") {
    CHECK(SourceDescription::kinds().size() == 9);
    CHECK(SourceDescription::kinds().first() == QStringLiteral("book"));
    CHECK(SourceDescription::fieldsOf(QStringLiteral("article"))
              .contains(QStringLiteral("doi")));
    CHECK_FALSE(SourceDescription::fieldsOf(QStringLiteral("webPage"))
                    .contains(QStringLiteral("isbn")));
    for (const QString& kind : SourceDescription::kinds()) {
        for (const QString& field : SourceDescription::fieldsOf(kind)) {
            INFO(kind.toStdString() << " " << field.toStdString());
            CHECK(SourceDescription::allFields().contains(field));
        }
    }
    CHECK(SourceDescription::canDescribe(QStringLiteral("publication")));
    CHECK(SourceDescription::canDescribe(QStringLiteral("recording")));
    CHECK_FALSE(SourceDescription::canDescribe(QStringLiteral("picture")));
    CHECK(SourceDescription::kindFor(QStringLiteral("web_page")) == QStringLiteral("webPage"));

    SourceDescription source;
    CHECK(source.isEmpty());
    CHECK(source.kind() == QStringLiteral("book"));
    source.setValue(QStringLiteral("title"), QStringLiteral("  Atlas  "));
    CHECK(source.value(SourceDescription::TITLE) == QStringLiteral("Atlas"));
    source.setValue(QStringLiteral("title"), QStringLiteral(" "));
    CHECK(source.isEmpty());
    CHECK(source.inBibliography(true));
    CHECK_FALSE(source.inBibliography(false));
    source.bibliography = QString::fromLatin1(SourceDescription::NEVER);
    CHECK_FALSE(source.inBibliography(true));
    source.bibliography = QString::fromLatin1(SourceDescription::ALWAYS);
    CHECK(source.inBibliography(false));

    source.people << person("Anna Wiśniewska") << person("Ewa Lis", "editor") << person("  ");
    CHECK(source.peopleOf(SourceDescription::AUTHOR).size() == 1);
    CHECK(source.peopleOf(SourceDescription::EDITOR).size() == 1);

    CHECK(PersonName::of(QStringLiteral("Anna Wiśniewska")).family == QStringLiteral("Wiśniewska"));
    CHECK(PersonName::of(QStringLiteral("Anna Maria Wiśniewska")).given ==
          QStringLiteral("Anna Maria"));
    CHECK(PersonName::of(QStringLiteral("Wiśniewska, Anna")).given == QStringLiteral("Anna"));
    CHECK(PersonName::of(QStringLiteral("Tau")).family == QStringLiteral("Tau"));
    CHECK(PersonName::of(QStringLiteral("Atlas Przyrody"), true).family ==
          QStringLiteral("Atlas Przyrody"));
    CHECK(PersonName::of(QStringLiteral("Jean-Pierre Anna Dupont")).initials() ==
          QStringLiteral("J.-P. A."));
}

TEST_CASE("Sources: entries in PN-ISO 690", "[core][sources][bibliography]") {
    CHECK(entry(atlas(), Bibliography::PN_ISO_690) ==
          QStringLiteral("WIŚNIEWSKA, Anna. Atlas pustyń świata: krajobrazy, ludzie, szlaki. "
                         "Wyd. 2 popr. Warszawa: Wydawnictwo Piasek, 2011. "
                         "ISBN 978-83-0000-000-0."));
    // Rich: the title in italics, the text escaped
    SourceDescription escaped = atlas();
    escaped.setValue(QStringLiteral("publisher"), QStringLiteral("A & B"));
    const QString rich = entry(escaped, Bibliography::PN_ISO_690, "pl", true);
    CHECK(rich.contains(QStringLiteral("<i>Atlas pustyń świata: krajobrazy, ludzie, "
                                       "szlaki</i>")));
    CHECK(rich.contains(QStringLiteral("A &amp; B")));

    SourceDescription chapter;
    chapter.type = QString::fromLatin1(SourceDescription::CHAPTER);
    chapter.people << person("Piotr Nowicki") << person("Ewa Lis", "editor");
    chapter.setValue(QStringLiteral("title"), QStringLiteral("Wielbłąd w kulturze ludów pustyni"));
    chapter.setValue(QStringLiteral("container"), QStringLiteral("Zwierzęta i ludzie"));
    chapter.setValue(QStringLiteral("place"), QStringLiteral("Kraków"));
    chapter.setValue(QStringLiteral("publisher"), QStringLiteral("Oficyna Oaza"));
    chapter.setValue(QStringLiteral("year"), QStringLiteral("2016"));
    chapter.setValue(QStringLiteral("pages"), QStringLiteral("77–102"));
    CHECK(entry(chapter, Bibliography::PN_ISO_690) ==
          QStringLiteral("NOWICKI, Piotr. Wielbłąd w kulturze ludów pustyni. W: LIS, Ewa (red.). "
                         "Zwierzęta i ludzie. Kraków: Oficyna Oaza, 2016, s. 77–102."));

    SourceDescription article;
    article.type = QString::fromLatin1(SourceDescription::ARTICLE);
    article.people << person("Marta Zielińska");
    article.setValue(QStringLiteral("title"), QStringLiteral("Studnie na szlakach solnych"));
    article.setValue(QStringLiteral("container"), QStringLiteral("Kwartalnik Pustynny"));
    article.setValue(QStringLiteral("year"), QStringLiteral("2019"));
    article.setValue(QStringLiteral("volume"), QStringLiteral("12"));
    article.setValue(QStringLiteral("issue"), QStringLiteral("3"));
    article.setValue(QStringLiteral("pages"), QStringLiteral("41–58"));
    article.setValue(QStringLiteral("doi"), QStringLiteral("10.0000/kp.2019.3.4"));
    CHECK(entry(article, Bibliography::PN_ISO_690) ==
          QStringLiteral("ZIELIŃSKA, Marta. Studnie na szlakach solnych. Kwartalnik Pustynny. "
                         "2019, t. 12, nr 3, s. 41–58. DOI 10.0000/kp.2019.3.4."));

    SourceDescription page;
    page.type = QString::fromLatin1(SourceDescription::WEB_PAGE);
    page.people << person("Atlas Przyrody", "institution");
    page.setValue(QStringLiteral("title"),
                  QStringLiteral("Kalahari – pustynia w Afryce Południowej"));
    page.setValue(QStringLiteral("url"), QStringLiteral("https://atlas-przyrody.example/kalahari"));
    page.setValue(QStringLiteral("accessed"), QStringLiteral("21.10.2026"));
    CHECK(entry(page, Bibliography::PN_ISO_690) ==
          QStringLiteral("Kalahari – pustynia w Afryce Południowej [online]. Atlas Przyrody "
                         "[dostęp 21.10.2026]. Dostępne w: "
                         "https://atlas-przyrody.example/kalahari."));

    SourceDescription archive;
    archive.type = QString::fromLatin1(SourceDescription::ARCHIVE);
    archive.people << person("Jan Kowalski");
    archive.setValue(QStringLiteral("title"), QStringLiteral("Dziennik wyprawy"));
    archive.setValue(QStringLiteral("form"), QStringLiteral("rękopis"));
    archive.setValue(QStringLiteral("year"), QStringLiteral("1931"));
    archive.setValue(QStringLiteral("container"), QStringLiteral("Archiwum Wypraw"));
    archive.setValue(QStringLiteral("fonds"), QStringLiteral("4"));
    archive.setValue(QStringLiteral("signature"), QStringLiteral("112"));
    CHECK(entry(archive, Bibliography::PN_ISO_690) ==
          QStringLiteral("KOWALSKI, Jan. Dziennik wyprawy [rękopis]. 1931. Archiwum Wypraw, "
                         "zespół 4, sygn. 112."));

    SourceDescription talk;
    talk.type = QString::fromLatin1(SourceDescription::INTERVIEW);
    talk.people << person("Tau", "interviewee") << person("Anna Nowak", "interviewer");
    talk.setValue(QStringLiteral("title"), QStringLiteral("Rozmowa w Ghanzi"));
    talk.setValue(QStringLiteral("form"), QStringLiteral("nagranie"));
    talk.setValue(QStringLiteral("place"), QStringLiteral("Ghanzi"));
    talk.setValue(QStringLiteral("year"), QStringLiteral("12.03.2025"));
    CHECK(entry(talk, Bibliography::PN_ISO_690) ==
          QStringLiteral("TAU. Rozmowa w Ghanzi [nagranie]. Rozmowę prowadzi: Anna Nowak. "
                         "Ghanzi, 12.03.2025."));

    // Without a title of its own, the title of the material; without anything else, it alone
    CHECK(entry(SourceDescription{}, Bibliography::PN_ISO_690) == QStringLiteral("Material."));
    // More than three authors: the first and "i in."
    SourceDescription many = atlas();
    many.people << person("B B") << person("C C") << person("D D");
    CHECK(entry(many, Bibliography::PN_ISO_690)
              .startsWith(QStringLiteral("WIŚNIEWSKA, Anna i in. ")));
}

TEST_CASE("Sources: entries in APA, Chicago, MLA and Harvard", "[core][sources][bibliography]") {
    CHECK(entry(atlas(), Bibliography::APA) ==
          QStringLiteral("Wiśniewska, A. (2011). Atlas pustyń świata: Krajobrazy, ludzie, szlaki "
                         "(wyd. 2 popr.). Wydawnictwo Piasek."));
    CHECK(entry(atlas(), Bibliography::CHICAGO) ==
          QStringLiteral("Wiśniewska, Anna. Atlas pustyń świata: Krajobrazy, ludzie, szlaki. "
                         "Wyd. 2 popr. Warszawa: Wydawnictwo Piasek, 2011."));
    CHECK(entry(atlas(), Bibliography::MLA) ==
          QStringLiteral("Wiśniewska, Anna. Atlas pustyń świata: Krajobrazy, ludzie, szlaki. "
                         "Wyd. 2 popr., Wydawnictwo Piasek, 2011."));
    CHECK(entry(atlas(), Bibliography::HARVARD) ==
          QStringLiteral("Wiśniewska, A. (2011) Atlas pustyń świata: krajobrazy, ludzie, szlaki. "
                         "Wyd. 2 popr. Warszawa: Wydawnictwo Piasek."));
    // In English
    CHECK(entry(atlas(), Bibliography::APA, "en") ==
          QStringLiteral("Wiśniewska, A. (2011). Atlas pustyń świata: Krajobrazy, ludzie, szlaki "
                         "(2 popr. ed.). Wydawnictwo Piasek."));
    // Two authors in APA: "i" in Polish, "&" in English
    SourceDescription two = atlas();
    two.people << person("Jan Nowak");
    CHECK(entry(two, Bibliography::APA)
              .startsWith(QStringLiteral("Wiśniewska, A. i Nowak, J. (2011).")));
    CHECK(entry(two, Bibliography::APA, "en")
              .startsWith(QStringLiteral("Wiśniewska, A. & Nowak, J. (2011).")));
    // A style the program does not know: the one of the settings
    CHECK(Bibliography::styles().contains(Bibliography::knownStyle(QStringLiteral("vancouver"))));
    CHECK(Bibliography::styleName(QStringLiteral("pn-iso-690")) == QStringLiteral("PN-ISO 690"));
}

TEST_CASE("Sources: references in the text", "[core][sources][bibliography]") {
    const QString pl = QStringLiteral("pl");
    CHECK(Bibliography::reference(atlas(), QString(), QStringLiteral("45"),
                                  QString::fromLatin1(Bibliography::AUTHOR_YEAR), 1, pl) ==
          QStringLiteral("(Wiśniewska 2011, s. 45)"));
    CHECK(Bibliography::reference(atlas(), QString(), QStringLiteral("45"),
                                  QString::fromLatin1(Bibliography::AUTHOR_TITLE), 1, pl) ==
          QStringLiteral("Wiśniewska, Atlas pustyń świata, s. 45"));
    CHECK(Bibliography::reference(atlas(), QString(), QStringLiteral("45"),
                                  QString::fromLatin1(Bibliography::NUMBER), 3, pl) ==
          QStringLiteral("[3, s. 45]"));
    CHECK(Bibliography::reference(atlas(), QString(), QString(),
                                  QString::fromLatin1(Bibliography::AUTHOR_YEAR), 1, pl) ==
          QStringLiteral("(Wiśniewska 2011)"));
    SourceDescription two = atlas();
    two.people << person("Jan Nowak");
    CHECK(Bibliography::reference(two, QString(), QStringLiteral("9"),
                                  QString::fromLatin1(Bibliography::AUTHOR_YEAR), 1,
                                  QStringLiteral("en")) ==
          QStringLiteral("(Wiśniewska and Nowak 2011, p. 9)"));
    // Without an author: the title; without a year: "b.r."
    SourceDescription untitled;
    CHECK(Bibliography::reference(untitled, QStringLiteral("Mapa"), QString(),
                                  QString::fromLatin1(Bibliography::AUTHOR_YEAR), 1, pl) ==
          QStringLiteral("(Mapa b.r.)"));
    // A quote without a source
    QuoteSource quote;
    quote.author = QStringLiteral("Tau");
    quote.work = QStringLiteral("Rozmowa w Ghanzi");
    quote.page = QStringLiteral("2");
    CHECK(Bibliography::reference(quote, pl) == QStringLiteral("Tau, Rozmowa w Ghanzi, s. 2"));
}

TEST_CASE("Sources: descriptions pasted as BibTeX and RIS", "[core][sources][bibliography]") {
    const std::optional<SourceDescription> bibtex = Bibliography::parse(QStringLiteral(R"(
        @Article{zielinska2019,
          author  = {Zieli{\'n}ska, Marta and Nowak, Jan},
          title   = {Studnie na szlakach {solnych}},
          journal = "Kwartalnik Pustynny",
          year    = 2019,
          volume  = {12}, number = {3},
          pages   = {41--58},
          doi     = {10.0000/kp.2019.3.4}
        })"));
    REQUIRE(bibtex.has_value());
    CHECK(bibtex->type == QStringLiteral("article"));
    REQUIRE(bibtex->people.size() == 2);
    CHECK(bibtex->people.at(1).name == QStringLiteral("Nowak, Jan"));
    CHECK(bibtex->value(SourceDescription::TITLE) == QStringLiteral("Studnie na szlakach solnych"));
    CHECK(bibtex->value(SourceDescription::CONTAINER) == QStringLiteral("Kwartalnik Pustynny"));
    CHECK(bibtex->value(SourceDescription::YEAR) == QStringLiteral("2019"));
    CHECK(bibtex->value(SourceDescription::ISSUE) == QStringLiteral("3"));
    CHECK(bibtex->value(SourceDescription::PAGES) == QStringLiteral("41–58"));
    CHECK(bibtex->value(SourceDescription::DOI) == QStringLiteral("10.0000/kp.2019.3.4"));

    const std::optional<SourceDescription> ris = Bibliography::parse(QStringLiteral(
        "TY  - BOOK\r\nAU  - Wiśniewska, Anna\r\nTI  - Atlas pustyń świata\r\n"
        "PY  - 2011/05/01/\r\nCY  - Warszawa\r\nPB  - Wydawnictwo Piasek\r\n"
        "SN  - 978-83-0000-000-0\r\nSP  - 1\r\nEP  - 212\r\nER  - \r\n"
        "TY  - JOUR\r\nTI  - Another\r\nER  -\r\n"));
    REQUIRE(ris.has_value());
    CHECK(ris->type == QStringLiteral("book"));
    REQUIRE(ris->people.size() == 1);
    CHECK(ris->value(SourceDescription::TITLE) == QStringLiteral("Atlas pustyń świata"));
    CHECK(ris->value(SourceDescription::YEAR) == QStringLiteral("2011"));
    CHECK(ris->value(SourceDescription::PLACE) == QStringLiteral("Warszawa"));
    CHECK(ris->value(SourceDescription::ISBN) == QStringLiteral("978-83-0000-000-0"));
    CHECK(ris->value(SourceDescription::PAGES) == QStringLiteral("1–212"));

    CHECK_FALSE(Bibliography::parse(QStringLiteral("Atlas pustyń świata, 2011")).has_value());
    CHECK_FALSE(Bibliography::parse(QStringLiteral("@book{empty,}")).has_value());
}
