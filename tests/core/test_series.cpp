// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_series.cpp
/// @brief A series: several books of one project with one Workshop – the name of the series,
/// the current book, new books, their order and removal, and elements moved between them

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/project_manager.h>

#include "kalahari_tests/test_support/window_parts.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <optional>

using kalahari::core::BookPlace;
using kalahari::core::BookProject;
using kalahari::core::ChapterDocument;
using kalahari::core::ElementPlace;
using kalahari::core::KindReference;
using kalahari::core::NewBook;
using kalahari::core::ProjectBook;
using kalahari::core::ProjectElement;
using kalahari::core::ProjectManager;

namespace {

ProjectManager& projects() {
    auto& pm = kalahari::test::book();
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.bookTypes().problems().isEmpty());
    return pm;
}

/// Kind @p kindId of the base package
kalahari::core::KindRef base(const ProjectManager& pm, const char* kindId) {
    return pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QString::fromLatin1(kindId));
}

/// The titles of @p elements, separated by commas
QString titles(const QList<ProjectElement>& elements) {
    QStringList list;
    for (const ProjectElement& element : elements) {
        list.append(element.title);
    }
    return list.join(QStringLiteral(", "));
}

/// The titles of the books of the open project, separated by commas
QString bookTitles(const ProjectManager& pm) {
    QStringList list;
    for (const ProjectBook& book : pm.project()->books) {
        list.append(book.title);
    }
    return list.join(QStringLiteral(", "));
}

/// The text of the chapter file of element @p elementId
QString textOf(const ProjectManager& pm, const QString& elementId) {
    const ProjectElement* element = pm.findElement(elementId);
    REQUIRE(element != nullptr);
    const std::optional<ChapterDocument> chapter =
        ChapterDocument::load(QDir(pm.getProjectPath()).filePath(element->file));
    REQUIRE(chapter.has_value());
    return chapter->kml();
}

}  // namespace

TEST_CASE("Series: the name of the series in the book file, left out when it has none",
          "[core][series]") {
    const QByteArray json = R"({
        "format": 2, "id": "p1", "series": "Piaski",
        "books": [
            { "id": "b1", "title": "Piasek", "folder": "book", "front": [], "main": [],
              "back": [] },
            { "id": "b2", "title": "Wydmy", "folder": "book2", "front": [], "main": [],
              "back": [] }
        ],
        "workshop": { "elements": [] }
    })";
    QStringList problems;
    const std::optional<BookProject> project =
        BookProject::fromJson(QJsonDocument::fromJson(json).object(), problems);
    REQUIRE(problems.isEmpty());
    REQUIRE(project.has_value());
    CHECK(project->series == QStringLiteral("Piaski"));
    CHECK(project->books.size() == 2);
    CHECK(project->bookIndex(QStringLiteral("b2")) == 1);
    CHECK(project->bookIndex(QStringLiteral("b3")) == -1);
    CHECK(project->bookIndex(QString()) == -1);
    CHECK(project->toJson().value(QStringLiteral("series")).toString() ==
          QStringLiteral("Piaski"));
    CHECK_FALSE(project->toJson().contains(QStringLiteral("extra")));

    BookProject without = *project;
    without.series.clear();
    CHECK_FALSE(without.toJson().contains(QStringLiteral("series")));
}

TEST_CASE("Series: a new book after the current one, with its first chapter, in a folder of its "
          "own",
          "[core][series]") {
    const QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Piasek", "Ewa Lis", "pl", true, "kalahari.novel"));
    const QString first = pm.book()->id;
    CHECK_FALSE(pm.isSeries());
    CHECK(pm.seriesName() == QStringLiteral("Piasek"));
    CHECK(pm.currentBookIndex() == 0);
    int changed = 0;
    const QMetaObject::Connection counting =
        QObject::connect(&pm, &ProjectManager::currentBookChanged, [&changed]() { ++changed; });

    // Without a title: no book
    CHECK(pm.addBook(NewBook{QStringLiteral("  "), QString(), QString()}).isEmpty());

    const QString second = pm.addBook(NewBook{QStringLiteral(" Wydmy "), QStringLiteral("Ewa Lis"),
                                              QString()});
    REQUIRE_FALSE(second.isEmpty());
    CHECK(pm.isSeries());
    CHECK(bookTitles(pm) == QStringLiteral("Piasek, Wydmy"));
    // The current book stays the current one
    CHECK(pm.book()->id == first);
    CHECK(changed == 0);
    QObject::disconnect(counting);

    const BookProject* project = pm.project();
    const ProjectBook& book = project->books.at(1);
    CHECK(book.author == QStringLiteral("Ewa Lis"));
    CHECK(book.language == QStringLiteral("pl"));
    CHECK(book.folder == QStringLiteral("book2"));
    CHECK(book.partsLayer == project->books.at(0).partsLayer);
    // Its first chapter, without the pages of the book before it
    CHECK(book.frontElements.isEmpty());
    CHECK(book.backElements.isEmpty());
    REQUIRE(book.mainElements.size() == 1);
    const ProjectElement& chapter = book.mainElements.first();
    CHECK(chapter.title == QStringLiteral("Rozdział 1"));
    CHECK(chapter.kind == KindReference{"kalahari.base", "chapter"});
    CHECK(chapter.file == QStringLiteral("book2/chapter_001.kchapter"));
    CHECK(QFileInfo::exists(QDir(pm.getProjectPath()).filePath(chapter.file)));
    CHECK(pm.bookOf(chapter.id) == &book);
    CHECK(pm.bookOf(project->books.at(0).mainElements.first().id) == &project->books.at(0));

    // Before the first book, in another language, and the name of the series
    const QString zero = pm.addBook(NewBook{QStringLiteral("Prequel"), QStringLiteral("Ewa Lis"),
                                            QStringLiteral("en"), 0});
    REQUIRE_FALSE(zero.isEmpty());
    CHECK(bookTitles(pm) == QStringLiteral("Prequel, Piasek, Wydmy"));
    CHECK(pm.project()->books.first().folder == QStringLiteral("book3"));
    CHECK(titles(pm.project()->books.first().mainElements) == QStringLiteral("Chapter 1"));
    CHECK(pm.book()->id == first);
    CHECK(pm.currentBookIndex() == 1);
    CHECK(pm.seriesName() == QStringLiteral("Prequel"));
    REQUIRE(pm.setSeriesName(QStringLiteral("  Piaski ")));
    CHECK(pm.seriesName() == QStringLiteral("Piaski"));

    // All of it in the book file
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
    CHECK(bookTitles(pm) == QStringLiteral("Prequel, Piasek, Wydmy"));
    CHECK(pm.seriesName() == QStringLiteral("Piaski"));
    // A book opened again starts with its first book
    CHECK(pm.currentBookIndex() == 0);
    CHECK(pm.book()->id == zero);
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Series: a new book with the pages of the book before it, the title page anew",
          "[core][series]") {
    const QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Piasek", "Ewa Lis", "pl", true, "kalahari.novel"));
    // The book has its title page and a page about the author, with the writer's text
    const QString about =
        pm.addElement(base(pm, "about_author"), QStringLiteral("O autorce"), BookPlace::Back);
    REQUIRE_FALSE(about.isEmpty());
    pm.setChapterContent(about, QStringLiteral("<kml><p>Ewa Lis mieszka w Gdyni.</p></kml>"));
    REQUIRE(pm.saveChapterContent(about));

    NewBook request{QStringLiteral("Wydmy"), QStringLiteral("Ewa Lis"), QString()};
    request.pagesOfPrevious = true;
    const QString second = pm.addBook(request);
    REQUIRE_FALSE(second.isEmpty());
    const ProjectBook& book = pm.project()->books.at(1);
    CHECK(titles(book.frontElements) == QStringLiteral("Strona tytułowa"));
    CHECK(titles(book.mainElements) == QStringLiteral("Rozdział 1"));
    CHECK(titles(book.backElements) == QStringLiteral("O autorce"));
    // The title page has the new title, the page about the author the text of the first book
    CHECK(textOf(pm, book.frontElements.first().id).contains(QStringLiteral("Wydmy")));
    CHECK_FALSE(textOf(pm, book.frontElements.first().id).contains(QStringLiteral("Piasek")));
    CHECK(textOf(pm, book.backElements.first().id).contains(QStringLiteral("mieszka w Gdyni")));
    CHECK(book.backElements.first().file == QStringLiteral("book2/about_author_001.kchapter"));
    CHECK(book.backElements.first().id != about);
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Series: the current book takes the new elements, its order and its removal",
          "[core][series]") {
    const QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Piasek", "Ewa Lis", "pl", true, "kalahari.novel"));
    const QString first = pm.book()->id;
    const QString second =
        pm.addBook(NewBook{QStringLiteral("Wydmy"), QStringLiteral("Ewa Lis"), QString()});
    const QString third =
        pm.addBook(NewBook{QStringLiteral("Oaza"), QStringLiteral("Ewa Lis"), QString(), 2});
    REQUIRE_FALSE(third.isEmpty());
    CHECK(bookTitles(pm) == QStringLiteral("Piasek, Wydmy, Oaza"));

    // Another book becomes the current one, and a new chapter goes to it
    int changed = 0;
    const QMetaObject::Connection counting =
        QObject::connect(&pm, &ProjectManager::currentBookChanged, [&changed]() { ++changed; });
    CHECK_FALSE(pm.setCurrentBook(QStringLiteral("none")));
    REQUIRE(pm.setCurrentBook(second));
    CHECK(changed == 1);
    REQUIRE(pm.setCurrentBook(second));
    CHECK(changed == 1);
    CHECK(pm.book()->title == QStringLiteral("Wydmy"));
    const QString chapter =
        pm.addElement(base(pm, "chapter"), QStringLiteral("Rozdział 2"), BookPlace::Main);
    REQUIRE_FALSE(chapter.isEmpty());
    CHECK(titles(pm.project()->books.at(1).mainElements) ==
          QStringLiteral("Rozdział 1, Rozdział 2"));
    CHECK(titles(pm.project()->books.at(0).mainElements) == QStringLiteral("Rozdział 1"));
    CHECK(pm.findElement(chapter)->file == QStringLiteral("book2/chapter_002.kchapter"));
    CHECK(pm.bookOf(chapter)->id == second);

    // The order: the current book stays the current one
    CHECK_FALSE(pm.moveBook(second, 3));
    CHECK_FALSE(pm.moveBook(QStringLiteral("none"), 0));
    REQUIRE(pm.moveBook(second, 0));
    CHECK(bookTitles(pm) == QStringLiteral("Wydmy, Piasek, Oaza"));
    CHECK(pm.book()->id == second);
    CHECK(pm.currentBookIndex() == 0);

    // A card of the Workshop linked with a chapter and a section of a book loses its links
    // with the book, and keeps the link with a chapter of another book
    const QString card =
        pm.addElement(base(pm, "character"), QStringLiteral("Lejla"), BookPlace::Workshop);
    REQUIRE_FALSE(card.isEmpty());
    const QString firstChapter = pm.project()->books.at(1).mainElements.first().id;
    const QString section = pm.project()->sectionLink(BookPlace::Back, 0);
    REQUIRE(pm.setLinks(card, QStringList{chapter, section, firstChapter}));
    REQUIRE(pm.findElement(card)->links.size() == 3);
    int links = 0;
    const QMetaObject::Connection linking =
        QObject::connect(&pm, &ProjectManager::linksChanged, [&links]() { ++links; });

    // The current book removed: the first one is the current book then
    const std::optional<ProjectBook> removed = pm.removeBook(second);
    REQUIRE(removed.has_value());
    CHECK(removed->title == QStringLiteral("Wydmy"));
    CHECK(bookTitles(pm) == QStringLiteral("Piasek, Oaza"));
    CHECK(pm.book()->id == first);
    CHECK(changed == 2);
    CHECK(pm.findElement(chapter) == nullptr);
    REQUIRE(pm.findElement(card) != nullptr);
    CHECK(pm.findElement(card)->links.size() == 1);
    CHECK(pm.findElement(card)->links.first().to == firstChapter);
    CHECK(links == 1);
    QObject::disconnect(linking);
    QObject::disconnect(counting);
    // Its files stay on the disk
    CHECK(QFileInfo::exists(QDir(pm.getProjectPath()).filePath("book2/chapter_002.kchapter")));

    // The last book stays
    REQUIRE(pm.removeBook(third).has_value());
    CHECK_FALSE(pm.removeBook(first).has_value());
    CHECK_FALSE(pm.isSeries());
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Series: a chapter moved to another book, with the elements inside it",
          "[core][series]") {
    const QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Piasek", "Ewa Lis", "pl", true, "kalahari.novel"));
    const QString chapter = pm.book()->mainElements.first().id;
    const QString second =
        pm.addBook(NewBook{QStringLiteral("Wydmy"), QStringLiteral("Ewa Lis"), QString()});
    REQUIRE_FALSE(second.isEmpty());
    const QString file = pm.findElement(chapter)->file;

    // The places of the other book: in its sections
    const QList<ElementPlace> places = pm.movePlacesIn(chapter, second);
    REQUIRE_FALSE(places.isEmpty());
    for (const ElementPlace& place : places) {
        CHECK(place.place != BookPlace::Workshop);
    }
    const ElementPlace start{BookPlace::Main, QString(), 0};
    const ElementPlace end{BookPlace::Main, QString(), 1};
    CHECK(places.contains(end));
    CHECK(pm.canMoveTo(chapter, end, second));
    // Its own book has one chapter, so no place after the second one
    CHECK_FALSE(pm.canMoveTo(chapter, end));
    // In its own book, the places of movePlacesOf(), also when the book is named
    CHECK(pm.movePlacesIn(chapter, pm.book()->id) == pm.movePlacesOf(chapter));
    CHECK(pm.canMoveTo(chapter, start, pm.book()->id));
    CHECK(pm.movePlacesIn(chapter, QStringLiteral("none")).isEmpty());
    // Its place now, in its own book, changes nothing
    REQUIRE(pm.moveElementTo(chapter, start, pm.book()->id));
    CHECK(pm.bookOf(chapter) == pm.book());

    REQUIRE(pm.moveElementTo(chapter, end, second));
    CHECK(pm.book()->mainElements.isEmpty());
    CHECK(titles(pm.project()->books.at(1).mainElements) ==
          QStringLiteral("Rozdział 1, Rozdział 1"));
    CHECK(pm.bookOf(chapter)->id == second);
    // Its file stays where it is
    CHECK(pm.findElement(chapter)->file == file);
    // A place of a book the project does not have
    CHECK_FALSE(pm.moveElementTo(chapter, start, QStringLiteral("none")));

    // An element of the Workshop does not go to a book
    const QString note =
        pm.addElement(base(pm, "work_note"), QStringLiteral("Notatka"), BookPlace::Workshop);
    REQUIRE_FALSE(note.isEmpty());
    CHECK(pm.movePlacesIn(note, second).isEmpty());
    CHECK_FALSE(pm.moveElementTo(note, start, second));
    CHECK(pm.bookOf(note) == nullptr);
    REQUIRE(pm.closeProject(false));
}
