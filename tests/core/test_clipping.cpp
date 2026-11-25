// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_clipping.cpp
/// @brief Clippings: their titles, the places they were cut from found again, and their data
/// in the book file

#include <kalahari/core/book_project.h>
#include <kalahari/core/clipping.h>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>

#include <optional>

using kalahari::core::BookProject;
using kalahari::core::ClippingData;
using kalahari::core::ClippingInsertion;
using kalahari::core::ClippingOrigin;
using kalahari::core::ClippingPlace;
using kalahari::core::Clippings;

namespace {

QJsonObject jsonOf(const QByteArray& text) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    INFO(error.errorString().toStdString());
    REQUIRE(document.isObject());
    return document.object();
}

/// A project with the Workshop elements @p elements (JSON)
QByteArray projectWith(const QByteArray& elements) {
    return R"({ "format": 2, "id": "p",
                "books": [ { "id": "b", "folder": "book" } ],
                "workshop": { "grouped": false, "elements": [ )" +
           elements + "] } }";
}

/// An origin with the words @p before and @p after the place, in paragraph @p paragraph (from 1)
ClippingOrigin originOf(int paragraph, const QString& before, const QString& after) {
    ClippingOrigin origin;
    origin.element = QStringLiteral("t1");
    origin.paragraph = paragraph;
    origin.before = before;
    origin.after = after;
    return origin;
}

/// An object of a text (an image, a footnote): U+FFFC
QString textObject() {
    return {QChar::ObjectReplacementCharacter};
}

/// @brief The problems of a book with @p element in its Workshop, joined
QString problemOf(const QByteArray& element) {
    QStringList problems;
    BookProject::fromJson(jsonOf(projectWith(element)), problems);
    return problems.join(QStringLiteral("; "));
}

}  // namespace

TEST_CASE("Clippings: the title is the first words of the text", "[core][clippings]") {
    // A short text is its own title
    CHECK(Clippings::titleOf(QStringLiteral("Długo patrzył na horyzont.")) ==
          QStringLiteral("Długo patrzył na horyzont."));
    // A longer one: its first words, without the punctuation after the last of them
    CHECK(Clippings::titleOf(QStringLiteral("Jeden dwa trzy cztery pięć sześć, siedem osiem.")) ==
          QStringLiteral("Jeden dwa trzy cztery pięć sześć") + QChar(0x2026));
    // On one line: the ends of lines and of paragraphs are spaces, the objects are left out
    CHECK(Clippings::titleOf(QStringLiteral("Ala") + QChar(QChar::LineSeparator) +
                             QStringLiteral("ma") + textObject() + QStringLiteral("  kota") +
                             QChar(QChar::ParagraphSeparator) + QStringLiteral("i psa")) ==
          QStringLiteral("Ala ma kota i psa"));
    CHECK(Clippings::titleOf(QString()).isEmpty());
    CHECK(Clippings::titleOf(textObject() + QStringLiteral(" ")).isEmpty());
}

TEST_CASE("Clippings: the words around the place", "[core][clippings]") {
    // The last words of the text, with the spaces after them
    CHECK(Clippings::wordsBefore(QStringLiteral("a b c d e f g h i j ")) ==
          QStringLiteral("c d e f g h i j "));
    CHECK(Clippings::wordsBefore(QStringLiteral("Długo patrzył ")) ==
          QStringLiteral("Długo patrzył "));
    CHECK(Clippings::wordsBefore(QStringLiteral("   ")).isEmpty());
    CHECK(Clippings::wordsBefore(QString()).isEmpty());
    // An object of the text ends them
    CHECK(Clippings::wordsBefore(QStringLiteral("x") + textObject() + QStringLiteral("one two")) ==
          QStringLiteral("one two"));
    CHECK(Clippings::wordsBefore(QStringLiteral("x") + textObject()).isEmpty());

    // The first words of the text, with the spaces before them
    CHECK(Clippings::wordsAfter(QStringLiteral(" a b c d e f g h i j")) ==
          QStringLiteral(" a b c d e f g h"));
    CHECK(Clippings::wordsAfter(QStringLiteral("Potem wstał.")) == QStringLiteral("Potem wstał."));
    CHECK(Clippings::wordsAfter(QStringLiteral("one two") + textObject() + QStringLiteral("three")) ==
          QStringLiteral("one two"));
    CHECK(Clippings::wordsAfter(textObject() + QStringLiteral("three")).isEmpty());
    CHECK(Clippings::wordsAfter(QString()).isEmpty());
}

TEST_CASE("Clippings: the origin of a cut", "[core][clippings]") {
    const QDateTime when = QDateTime(QDate(2026, 11, 15), QTime(7, 42), QTimeZone::utc());
    const QStringList paragraphs{QStringLiteral("Pierwszy akapit."),
                                 QStringLiteral("Długo patrzył na horyzont. Potem wstał.")};

    // In a paragraph: its number from 1 and the words on both sides
    const ClippingOrigin origin = Clippings::originAt(QStringLiteral("t1"), paragraphs, 1, 27,
                                                      when);
    CHECK(origin.element == QStringLiteral("t1"));
    CHECK(origin.cut == when);
    CHECK(origin.paragraph == 2);
    CHECK(origin.before == QStringLiteral("Długo patrzył na horyzont. "));
    CHECK(origin.after == QStringLiteral("Potem wstał."));
    CHECK(origin.title.isEmpty());

    // At the start and at the end of a paragraph: nothing on that side
    CHECK(Clippings::originAt(QStringLiteral("t1"), paragraphs, 0, 0, when).before.isEmpty());
    const ClippingOrigin atEnd = Clippings::originAt(QStringLiteral("t1"), paragraphs, 0, 999,
                                                     when);
    CHECK(atEnd.before == QStringLiteral("Pierwszy akapit."));
    CHECK(atEnd.after.isEmpty());

    // No such paragraph: no place, the text and the time only
    const ClippingOrigin none = Clippings::originAt(QStringLiteral("t1"), paragraphs, 5, 0, when);
    CHECK(none.paragraph == 0);
    CHECK(none.before.isEmpty());
    CHECK(none.after.isEmpty());
    CHECK(none.element == QStringLiteral("t1"));
    CHECK_FALSE(isEmpty(none));
    CHECK(isEmpty(ClippingOrigin{}));
}

TEST_CASE("Clippings: the place found again", "[core][clippings]") {
    const ClippingOrigin origin = originOf(2, QStringLiteral("Długo patrzył na horyzont. "),
                                           QStringLiteral("Potem wstał."));

    SECTION("The paragraphs before it changed: the words find it, exact") {
        const QStringList paragraphs{QStringLiteral("Nowy."), QStringLiteral("Pierwszy akapit."),
                                     QStringLiteral("Długo patrzył na horyzont. Potem wstał.")};
        CHECK(Clippings::placeIn(paragraphs, origin) == ClippingPlace{2, 27, true});
    }

    SECTION("The same words twice: the place nearest to its paragraph") {
        const ClippingOrigin twice = originOf(3, QStringLiteral("A b. "), QStringLiteral("C d."));
        const QStringList paragraphs{QStringLiteral("A b. C d."), QStringLiteral("x"),
                                     QStringLiteral("A b. C d.")};
        CHECK(Clippings::placeIn(paragraphs, twice) == ClippingPlace{2, 5, true});
    }

    SECTION("The words before it changed: after the words after it, not exact") {
        const QStringList paragraphs{QStringLiteral("Pierwszy akapit."),
                                     QStringLiteral("Długo patrzył na morze. Potem wstał.")};
        CHECK(Clippings::placeIn(paragraphs, origin) == ClippingPlace{1, 24, false});
    }

    SECTION("The words after it changed: after the words before it, not exact") {
        const QStringList paragraphs{QStringLiteral("Długo patrzył na horyzont. Usiadł.")};
        CHECK(Clippings::placeIn(paragraphs, origin) == ClippingPlace{0, 27, false});
    }

    SECTION("No words left: the start of its paragraph, or of the last one") {
        const QStringList paragraphs{QStringLiteral("Inny."), QStringLiteral("Zupełnie inny."),
                                     QStringLiteral("Trzeci.")};
        CHECK(Clippings::placeIn(paragraphs, origin) == ClippingPlace{1, 0, false});
        CHECK(Clippings::placeIn({QStringLiteral("Inny.")}, origin) ==
              ClippingPlace{0, 0, false});
    }

    SECTION("The start of a paragraph and an empty paragraph") {
        const QStringList paragraphs{QStringLiteral("Inny."), QStringLiteral("Potem wstał."),
                                     QString(), QStringLiteral("Dalej.")};
        CHECK(Clippings::placeIn(paragraphs, originOf(2, QString(), QStringLiteral("Potem"))) ==
              ClippingPlace{1, 0, true});
        CHECK(Clippings::placeIn(paragraphs, originOf(3, QString(), QString())) ==
              ClippingPlace{2, 0, true});
        // After an object of the text is the start of what follows it
        const QStringList withObject{textObject() + QStringLiteral("Potem wstał.")};
        CHECK(Clippings::placeIn(withObject, originOf(1, QString(), QStringLiteral("Potem"))) ==
              ClippingPlace{0, 1, true});
    }

    SECTION("A text without paragraphs has no place") {
        CHECK_FALSE(Clippings::placeIn({}, origin).has_value());
    }
}

TEST_CASE("Clippings: their data in the book file", "[core][clippings]") {
    const QByteArray elements = R"(
        { "id": "e1", "kind": "kalahari.base:clipping", "title": "Długo patrzył",
          "file": "workshop/e1.kml",
          "clipping": {
            "origin": { "element": "c91a44f0", "title": "Rozdział 2", "paragraph": 3,
                        "before": "długo patrzył na horyzont. ", "after": "Potem wstał.",
                        "cut": "2026-11-15T07:42:00Z", "scissors": true },
            "inserted": [ { "element": "c91a44f0", "at": "2026-11-15T07:50:00Z", "by": "x" },
                          { "element": "d22", "at": "2026-11-15T08:00:00Z" } ],
            "marked": "yes" } },
        { "id": "e2", "kind": "kalahari.base:clipping", "title": "Dodany ręcznie",
          "file": "workshop/e2.kml" })";
    QStringList problems;
    const std::optional<BookProject> project =
        BookProject::fromJson(jsonOf(projectWith(elements)), problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    REQUIRE(problems.isEmpty());
    const BookProject read = project.value_or(BookProject());

    const ClippingData& cut = read.workshop.elements.at(0).clipping;
    CHECK(cut.origin.element == QStringLiteral("c91a44f0"));
    CHECK(cut.origin.title == QStringLiteral("Rozdział 2"));
    CHECK(cut.origin.paragraph == 3);
    CHECK(cut.origin.before == QStringLiteral("długo patrzył na horyzont. "));
    CHECK(cut.origin.after == QStringLiteral("Potem wstał."));
    CHECK(cut.origin.cut == QDateTime(QDate(2026, 11, 15), QTime(7, 42), QTimeZone::utc()));
    REQUIRE(cut.inserted.size() == 2);
    CHECK(cut.inserted.at(0).element == QStringLiteral("c91a44f0"));
    CHECK(cut.inserted.at(0).at ==
          QDateTime(QDate(2026, 11, 15), QTime(7, 50), QTimeZone::utc()));
    CHECK(cut.inserted.at(1).element == QStringLiteral("d22"));
    // The fields this version does not know are kept, and saved back
    CHECK(cut.origin.extra.keys() == QStringList{QStringLiteral("scissors")});
    CHECK(cut.inserted.at(0).extra.keys() == QStringList{QStringLiteral("by")});
    CHECK(cut.extra.keys() == QStringList{QStringLiteral("marked")});
    CHECK(read.workshop.elements.at(0).extra.isEmpty());

    // A clipping added by hand has none
    CHECK(isEmpty(read.workshop.elements.at(1).clipping));

    // The book is saved as it was read
    CHECK(read.toJson()[QStringLiteral("workshop")] ==
          jsonOf(projectWith(elements))[QStringLiteral("workshop")]);

    // A change of the data changes the project; data emptied leaves no "clipping"
    BookProject changed = read;
    changed.workshop.elements[0].clipping.inserted.removeLast();
    CHECK_FALSE(changed == read);
    const QJsonObject clipping = changed.toJson()[QStringLiteral("workshop")]
                                     [QStringLiteral("elements")][0][QStringLiteral("clipping")]
                                         .toObject();
    CHECK(clipping.value(QStringLiteral("inserted")).toArray().size() == 1);
    changed.workshop.elements[0].clipping = ClippingData{};
    const QJsonObject written = changed.toJson()[QStringLiteral("workshop")]
                                    [QStringLiteral("elements")][0].toObject();
    CHECK_FALSE(written.contains(QStringLiteral("clipping")));

    // Inserted only, no origin: no "origin"
    changed.workshop.elements[1].clipping.inserted << ClippingInsertion{
        .element = QStringLiteral("t1"),
        .at = QDateTime::fromString(QStringLiteral("2026-11-15T09:00:00Z"), Qt::ISODate),
        .extra = {}};
    const QJsonObject inserted = changed.toJson()[QStringLiteral("workshop")]
                                     [QStringLiteral("elements")][1][QStringLiteral("clipping")]
                                         .toObject();
    CHECK(inserted.keys() == QStringList{QStringLiteral("inserted")});
}

TEST_CASE("Clippings: problems of their data", "[core][clippings]") {
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:clipping", "clipping": "x" })") ==
          QStringLiteral("workshop.elements[0].clipping: must be an object"));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:clipping",
                         "clipping": { "origin": 3 } })") ==
          QStringLiteral("workshop.elements[0].clipping.origin: must be an object"));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:clipping",
                         "clipping": { "origin": { "paragraph": -1 } } })") ==
          QStringLiteral("workshop.elements[0].clipping.origin.paragraph: must be a whole "
                         "number, 0 or more"));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:clipping",
                         "clipping": { "inserted": [ 3 ] } })") ==
          QStringLiteral("workshop.elements[0].clipping.inserted[0]: must be an object"));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:clipping",
                         "clipping": { "origin": { "cut": "yesterday" } } })") ==
          QStringLiteral("workshop.elements[0].clipping.origin.cut: must be a date and time "
                         "such as \"2026-10-09T10:00:00Z\""));
}
