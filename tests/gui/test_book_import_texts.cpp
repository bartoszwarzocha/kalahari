// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_book_import_texts.cpp
/// @brief The texts of an imported file added to a book at the writer's place and at their
///        kinds' places, as files outside the text, the images of the file, and Undo import

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/editor/image_store.h>
#include <kalahari/gui/book_import_texts.h>
#include "test_support/window_parts.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <algorithm>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// A novel "Szkło" open in the book of the tests: a title page and a chapter
class Novel {
public:
    Novel() {
        REQUIRE(m_folder.isValid());
        test::book().loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        REQUIRE(test::book().createProject(m_folder.path(), QStringLiteral("Szkło"), QStringLiteral("Anna"),
                                           QStringLiteral("pl"), true, QStringLiteral("kalahari.novel")));
    }
    ~Novel() { test::book().closeProject(false); }
    Novel(const Novel&) = delete;
    Novel& operator=(const Novel&) = delete;

    core::ProjectManager& book() { return test::book(); }

    /// The kind of this id
    core::KindRef kind(const char* id) const {
        for (const core::KindRef& kind : test::book().bookTypes().allKinds()) {
            if (kind.kind->id == QLatin1String(id)) {
                return kind;
            }
        }
        FAIL("No kind " << id);
        return {};
    }

    /// The titles of a list of the book
    static QStringList titles(const QList<core::ProjectElement>& elements) {
        QStringList result;
        for (const core::ProjectElement& element : elements) {
            result << element.title;
        }
        return result;
    }

private:
    QTemporaryDir m_folder;
};

ImportedText text(const core::KindRef& kind, const QString& title, const QString& kml = QString()) {
    ImportedText made;
    made.kind = kind;
    made.title = title;
    made.kml = kml;
    return made;
}

}  // namespace

TEST_CASE("Import into a book: the names of the kinds of the titles", "[import][gui]") {
    Novel novel;
    const std::vector<editor::KindTitle> titles = importKindTitles(novel.book());
    const auto of = [&titles](const char* kind) -> QStringList {
        for (const editor::KindTitle& title : titles) {
            if (title.kind.endsWith(QLatin1Char(':') + QLatin1String(kind))) {
                return title.titles;
            }
        }
        return {};
    };
    CHECK(of("prologue").contains(QStringLiteral("Prolog")));
    CHECK(of("prologue").contains(QStringLiteral("Prologue")));
    CHECK(of("acknowledgments").contains(QStringLiteral("Podziękowania")));
    CHECK(of("title_page").contains(QStringLiteral("Strona tytułowa")));
    CHECK(of("chapter").isEmpty());  // the main text kind: any title
    CHECK(editor::kindOfTitle(QStringLiteral("Prolog. Noc"), titles).endsWith(QStringLiteral(":prologue")));
    CHECK(editor::kindOfTitle(QStringLiteral("Rozdział 1"), titles).isEmpty());
}

TEST_CASE("Import into a book: the texts at the place and at their kinds' places, and Undo import", "[import][gui]") {
    Novel novel;
    core::ProjectManager& book = novel.book();
    const core::ProjectBook* before = book.book();
    REQUIRE(before != nullptr);
    const QStringList mainBefore = Novel::titles(before->mainElements);
    const QStringList backBefore = Novel::titles(before->backElements);
    REQUIRE(mainBefore.size() == 1);

    ImportedText part = text(novel.kind("part"), QStringLiteral("Część pierwsza"));
    part.texts = {text(novel.kind("chapter"), QStringLiteral("Rozdział 1"), QStringLiteral("<kml><p>Raz.</p></kml>")),
                  text(novel.kind("chapter"), QStringLiteral("Rozdział 2"), QStringLiteral("<kml><p>Dwa.</p></kml>"))};
    const std::vector<ImportedText> texts{
        text(novel.kind("prologue"), QStringLiteral("Prolog"), QStringLiteral("<kml><p>Noc.</p></kml>")),
        part,
        text(novel.kind("chapter"), QStringLiteral("Rozdział 3"), QStringLiteral("<kml><p>Trzy.</p></kml>")),
        text(novel.kind("epilogue"), QStringLiteral("Epilog"), QStringLiteral("<kml><p>Koniec.</p></kml>")),
        text(novel.kind("acknowledgments"), QStringLiteral("Podziękowania"), QStringLiteral("<kml><p>Dzięki.</p></kml>"))};

    ImportPlace place;
    place.at = {core::BookPlace::Main, QString(), 1};  // after the chapter of the new book
    ImportedElements added;
    REQUIRE(addImportedTexts(book, texts, place, &added));
    CHECK(added.elements.size() == 5);

    const core::ProjectBook* after = book.book();
    // The prologue opens the body, the part and the chapter go to the place, the epilogue ends
    // the body, and the acknowledgments go to the back matter
    CHECK(Novel::titles(after->mainElements) == QStringList{QStringLiteral("Prolog"), mainBefore.front(),
                                                            QStringLiteral("Część pierwsza"), QStringLiteral("Rozdział 3"),
                                                            QStringLiteral("Epilog")});
    REQUIRE(after->mainElements.size() == 5);
    CHECK(Novel::titles(after->mainElements[2].elements) == QStringList{QStringLiteral("Rozdział 1"), QStringLiteral("Rozdział 2")});
    CHECK(Novel::titles(after->backElements) == backBefore + QStringList{QStringLiteral("Podziękowania")});
    const QString chapter = after->mainElements[2].elements[1].id;
    CHECK(book.chapterText(chapter) == QStringLiteral("<kml><p>Dwa.</p></kml>"));
    const QString file = QDir(book.getProjectPath()).filePath(after->mainElements[2].elements[1].file);
    CHECK(QFileInfo::exists(file));

    // Undo import: the book as it was, without the files of the texts
    REQUIRE(removeImported(book, added));
    CHECK(Novel::titles(book.book()->mainElements) == mainBefore);
    CHECK(Novel::titles(book.book()->backElements) == backBefore);
    CHECK_FALSE(QFileInfo::exists(file));
}

TEST_CASE("Import into a book: a kind the book cannot have stops the import", "[import][gui]") {
    Novel novel;
    core::ProjectManager& book = novel.book();
    const std::vector<ImportedText> texts{
        text(novel.kind("chapter"), QStringLiteral("Rozdział A"), QStringLiteral("<kml><p>A.</p></kml>")),
        text(novel.kind("title_page"), QStringLiteral("Druga strona tytułowa")),  // the book has one
        text(novel.kind("chapter"), QStringLiteral("Rozdział B"))};
    ImportPlace place;
    place.at = {core::BookPlace::Main, QString(), 0};
    ImportedElements added;
    CHECK_FALSE(addImportedTexts(book, texts, place, &added));
    REQUIRE(added.elements.size() == 1);  // what it added before
    CHECK(Novel::titles(book.book()->mainElements).front() == QStringLiteral("Rozdział A"));
    CHECK(removeImported(book, added));
    CHECK_FALSE(Novel::titles(book.book()->mainElements).contains(QStringLiteral("Rozdział A")));
}

TEST_CASE("Import into a book: the texts as other files of the book", "[import][gui]") {
    Novel novel;
    core::ProjectManager& book = novel.book();
    ImportedText part = text(novel.kind("part"), QStringLiteral("Część pierwsza"));
    part.texts = {text(novel.kind("chapter"), QStringLiteral("Rozdział 1"), QStringLiteral("<kml><p>Raz.</p></kml>"))};
    const std::vector<ImportedText> texts{part, text(novel.kind("prologue"), QStringLiteral("Prolog"),
                                                     QStringLiteral("<kml><p>Noc.</p></kml>"))};
    ImportPlace place;
    place.otherFiles = true;
    ImportedElements added;
    REQUIRE(addImportedTexts(book, texts, place, &added));
    const QList<core::ProjectElement>& files = book.book()->files;
    CHECK(Novel::titles(files) == QStringList{QStringLiteral("Rozdział 1"), QStringLiteral("Prolog")});  // no part
    REQUIRE(files.size() == 2);
    CHECK(book.chapterText(files[0].id) == QStringLiteral("<kml><p>Raz.</p></kml>"));
    CHECK(book.isBookFile(files[1].id));
    REQUIRE(removeImported(book, added));
    CHECK(book.book()->files.isEmpty());
}

TEST_CASE("Import into a book: the images of the file, and the styles it added", "[import][gui]") {
    Novel novel;
    core::ProjectManager& book = novel.book();
    editor::ImportDocument document;
    editor::ImportImage first;
    first.data = QByteArray("not checked: the reader read it");
    first.fileName = QStringLiteral("word/media/image1.png");
    editor::ImportImage missing;
    missing.missing = true;
    missing.fileName = QStringLiteral("brak.png");
    document.images = {first, missing, first};

    QStringList written;
    const std::vector<QString> sources = saveImportedImages(book, document, &written);
    REQUIRE(sources.size() == 3);
    CHECK(sources[0] == QStringLiteral("images/image1.png"));
    CHECK(sources[1].isEmpty());
    CHECK(sources[2] == sources[0]);  // the same file
    CHECK(written == QStringList{sources[0]});
    const QString image = QDir(book.imagesBaseFolder()).filePath(sources[0]);
    CHECK(QFileInfo::exists(image));

    // Imported again: the book has the file, so the second import wrote none
    QStringList again;
    CHECK(saveImportedImages(book, document, &again)[0] == sources[0]);
    CHECK(again.isEmpty());

    QList<core::BookParagraphStyle> styles = book.paragraphStyles();
    core::BookParagraphStyle own;
    own.id = QStringLiteral("imported_motto");
    own.name = QStringLiteral("Motto z pliku");
    own.baseStyle = QStringLiteral("normal");
    styles.append(own);
    REQUIRE(book.setParagraphStyles(styles));

    ImportedElements added;
    added.images = written;
    added.styles = {own.id};
    REQUIRE(removeImported(book, added));
    CHECK_FALSE(QFileInfo::exists(image));
    const QList<core::BookParagraphStyle> left = book.paragraphStyles();
    CHECK(std::none_of(left.begin(), left.end(), [](const core::BookParagraphStyle& style) {
        return style.id == QStringLiteral("imported_motto");
    }));
}
