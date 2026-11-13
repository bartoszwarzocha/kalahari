/// @file test_text_places.cpp
/// @brief Edit > Go To... in the editor: the places of a text, by number, on or back, the next
///        and the previous one

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_places.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QTextBlock>
#include <QTextDocument>
#include <memory>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(700, 500));
    EditorAppearance appearance;
    appearance.cursor.blinking = false;
    editor->setAppearance(appearance);
    editor->fromKml(kml);
    return editor;
}

/// The paragraph of the cursor
int paragraphOf(const BookEditor& editor) {
    return editor.cursorPosition().paragraph;
}

/// The text of the paragraph of the cursor
std::string paragraphText(const BookEditor& editor) {
    return editor.textDocument()->findBlockByNumber(paragraphOf(editor)).text().toStdString();
}

GoToOutcome done(int ordinal, int count) {
    GoToOutcome outcome;
    outcome.ordinal = ordinal;
    outcome.count = count;
    return outcome;
}

GoToOutcome failed(GoToResult result, int count) {
    GoToOutcome outcome;
    outcome.result = result;
    outcome.count = count;
    return outcome;
}

}  // namespace

TEST_CASE("Go To: the numbers it reads", "[editor][goto]") {
    const auto read = [](const char* text) {
        const PlaceNumber number = placeNumberOf(QString::fromUtf8(text));
        return std::vector<int>{number.valid ? 1 : 0, number.relative ? 1 : 0, number.value};
    };
    CHECK(read("5") == std::vector<int>{1, 0, 5});
    CHECK(read(" 12 ") == std::vector<int>{1, 0, 12});
    CHECK(read("+2") == std::vector<int>{1, 1, 2});
    CHECK(read("- 1") == std::vector<int>{1, 1, -1});
    CHECK(read("0") == std::vector<int>{0, 0, 0});
    CHECK(read("+0") == std::vector<int>{0, 0, 0});
    CHECK(read("") == std::vector<int>{0, 0, 0});
    CHECK(read("abc") == std::vector<int>{0, 0, 0});
    CHECK(read("2a") == std::vector<int>{0, 0, 0});
    CHECK(read("1.5") == std::vector<int>{0, 0, 0});
}

TEST_CASE("Go To: the headings by number, on and back, next and previous",
          "[editor][goto]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><h1>One</h1><p>a</p><h2>Two</h2><p>b</p><p>c</p><h3>Three</h3><p>d</p>"
        "<h1>Four</h1><p>e</p></kml>"));
    REQUIRE(editor->placePositions(PlaceKind::Heading).size() == 4);

    SECTION("A number: the heading of that number, the cursor at its start") {
        CHECK(editor->goToPlace(PlaceKind::Heading, QStringLiteral("3")) == done(3, 4));
        CHECK(paragraphText(*editor) == "Three");
        CHECK(editor->cursorPosition().offset == 0);
        CHECK_FALSE(editor->hasSelection());
        CHECK(editor->goToPlace(PlaceKind::Heading, QStringLiteral("5")) ==
              failed(GoToResult::NoNumber, 4));
        CHECK(paragraphText(*editor) == "Three");  // it stays
        CHECK(editor->goToPlace(PlaceKind::Heading, QStringLiteral("x")) ==
              failed(GoToResult::Invalid, 4));
    }

    SECTION("On and back from the heading the cursor is under") {
        editor->setCursorPosition({4, 1});  // c, under Two
        CHECK(editor->goToPlace(PlaceKind::Heading, QStringLiteral("+2")) == done(4, 4));
        CHECK(paragraphText(*editor) == "Four");
        CHECK(editor->goToPlace(PlaceKind::Heading, QStringLiteral("-3")) == done(1, 4));
        CHECK(editor->goToPlace(PlaceKind::Heading, QStringLiteral("-1")) ==
              failed(GoToResult::NoNumber, 4));
        // Before the first heading: "+1" goes to it
        auto before = editorWith(QStringLiteral("<kml><p>a</p><h1>One</h1></kml>"));
        CHECK(before->goToPlace(PlaceKind::Heading, QStringLiteral("+1")) == done(1, 1));
        CHECK(paragraphText(*before) == "One");
    }

    SECTION("Next and Previous: the heading after the cursor, and the one before") {
        editor->setCursorPosition({1, 0});  // a
        CHECK(editor->goToAdjacentPlace(PlaceKind::Heading, true) == done(2, 4));
        CHECK(paragraphText(*editor) == "Two");
        CHECK(editor->goToAdjacentPlace(PlaceKind::Heading, true) == done(3, 4));
        CHECK(editor->goToAdjacentPlace(PlaceKind::Heading, true) == done(4, 4));
        CHECK(editor->goToAdjacentPlace(PlaceKind::Heading, true) ==
              failed(GoToResult::NoFurther, 4));
        CHECK(paragraphText(*editor) == "Four");
        // From the middle of a section, Previous goes to its heading
        editor->setCursorPosition({4, 1});  // c, under Two
        CHECK(editor->goToAdjacentPlace(PlaceKind::Heading, false) == done(2, 4));
        CHECK(editor->goToAdjacentPlace(PlaceKind::Heading, false) == done(1, 4));
        CHECK(editor->goToAdjacentPlace(PlaceKind::Heading, false) ==
              failed(GoToResult::NoEarlier, 4));
    }

    SECTION("A text without headings") {
        auto plain = editorWith(QStringLiteral("<kml><p>a</p></kml>"));
        CHECK(plain->goToAdjacentPlace(PlaceKind::Heading, true) ==
              failed(GoToResult::NoPlaces, 0));
        CHECK(plain->goToPlace(PlaceKind::Heading, QStringLiteral("1")) ==
              failed(GoToResult::NoPlaces, 0));
    }
}

TEST_CASE("Go To: the notes by the numbers they show", "[editor][goto]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>A<footnote id=\"f1\">One</footnote> b<endnote id=\"e1\">End</endnote>"
        " c<footnote id=\"f2\">Two</footnote></p><p>d<footnote id=\"f3\">Three</footnote></p>"
        "</kml>"));
    const std::vector<int> places = editor->placePositions(PlaceKind::Note);
    REQUIRE(places.size() == 4);

    // The footnotes 1, 2, 3 and the endnote i: the cursor after the number
    GoToOutcome second = done(3, 4);
    second.number = QStringLiteral("2");
    CHECK(editor->goToPlace(PlaceKind::Note, QStringLiteral("2")) == second);
    CHECK(editor->textDocument()->characterAt(
              editor->textDocument()->findBlockByNumber(0).position() +
              editor->cursorPosition().offset - 1) == QChar::ObjectReplacementCharacter);
    GoToOutcome endnote = done(2, 4);
    endnote.number = QStringLiteral("i");
    CHECK(editor->goToPlace(PlaceKind::Note, QStringLiteral("I")) == endnote);
    CHECK(editor->goToPlace(PlaceKind::Note, QStringLiteral("7")) ==
          failed(GoToResult::NoNumber, 4));

    // On, next and previous in text order, the cursor after a number counting as on it
    GoToOutcome third = done(4, 4);
    third.number = QStringLiteral("3");
    CHECK(editor->goToPlace(PlaceKind::Note, QStringLiteral("+2")) == third);
    CHECK(paragraphOf(*editor) == 1);
    CHECK(editor->goToAdjacentPlace(PlaceKind::Note, false) == second);
    CHECK(editor->goToAdjacentPlace(PlaceKind::Note, false) == endnote);
    CHECK(editor->goToAdjacentPlace(PlaceKind::Note, true) == second);
}

TEST_CASE("Go To: the images selected, the tables at their first cell", "[editor][goto]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>a</p><p><img src=\"images/one.png\" alt=\"One\" width=\"2cm\"/></p>"
        "<table><tr><td><p>x</p></td><td><p>y</p></td></tr></table><p>b</p>"
        "<p>c <img src=\"images/two.png\" alt=\"Two\" width=\"1cm\"/> d</p>"
        "<table><tr><td><p>z</p></td></tr></table><p>e</p></kml>"));
    REQUIRE(editor->placePositions(PlaceKind::Image).size() == 2);
    REQUIRE(editor->placePositions(PlaceKind::Table).size() == 2);

    CHECK(editor->goToPlace(PlaceKind::Image, QStringLiteral("2")) == done(2, 2));
    REQUIRE(editor->selectedImage().has_value());
    CHECK(*editor->selectedImage() == editor->placePositions(PlaceKind::Image)[1]);
    CHECK(editor->goToAdjacentPlace(PlaceKind::Image, false) == done(1, 2));
    CHECK(*editor->selectedImage() == editor->placePositions(PlaceKind::Image)[0]);
    CHECK(editor->goToAdjacentPlace(PlaceKind::Image, false) ==
          failed(GoToResult::NoEarlier, 2));
    // The cursor before an image, which is not selected: Next selects that image
    editor->clearSelection();
    editor->setCursorPosition({1, 0});
    REQUIRE_FALSE(editor->selectedImage().has_value());
    CHECK(editor->goToAdjacentPlace(PlaceKind::Image, true) == done(1, 2));
    CHECK(*editor->selectedImage() == editor->placePositions(PlaceKind::Image)[0]);
    // and "+1" too
    editor->clearSelection();
    editor->setCursorPosition({1, 0});
    CHECK(editor->goToPlace(PlaceKind::Image, QStringLiteral("+1")) == done(1, 2));

    CHECK(editor->goToAdjacentPlace(PlaceKind::Table, true) == done(1, 2));
    CHECK(editor->isInTable());
    CHECK(paragraphText(*editor) == "x");
    CHECK(editor->goToAdjacentPlace(PlaceKind::Table, true) == done(2, 2));
    CHECK(paragraphText(*editor) == "z");
    CHECK(editor->goToPlace(PlaceKind::Table, QStringLiteral("-1")) == done(1, 2));
    CHECK(paragraphText(*editor) == "x");
}

TEST_CASE("Go To: the annotations, also two at one place", "[editor][goto]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><annotations><annotation id=\"a1\" kind=\"comment\">C</annotation>"
        "<annotation id=\"a2\" kind=\"todo\">T</annotation>"
        "<annotation id=\"a3\" kind=\"note\">N</annotation></annotations>"
        "<p><anchor ref=\"a1\">one</anchor> two <anchor ref=\"a2\"/></p>"
        "<p><anchor ref=\"a3\">three</anchor></p></kml>"));
    REQUIRE(editor->placePositions(PlaceKind::Annotation).size() == 3);

    CHECK(editor->goToAdjacentPlace(PlaceKind::Annotation, true) == done(1, 3));
    REQUIRE(editor->hasSelection());
    CHECK(editor->selectedText() == QStringLiteral("one"));
    CHECK(editor->goToAdjacentPlace(PlaceKind::Annotation, true) == done(2, 3));
    CHECK_FALSE(editor->hasSelection());  // the place of a2
    CHECK(editor->goToAdjacentPlace(PlaceKind::Annotation, true) == done(3, 3));
    CHECK(editor->selectedText() == QStringLiteral("three"));
    CHECK(editor->goToPlace(PlaceKind::Annotation, QStringLiteral("1")) == done(1, 3));

    SECTION("Two annotations of one fragment: Next goes from one to the other") {
        auto same = editorWith(QStringLiteral(
            "<kml><annotations><annotation id=\"a1\" kind=\"comment\">C</annotation>"
            "<annotation id=\"a2\" kind=\"todo\">T</annotation></annotations>"
            "<p>x <anchor ref=\"a1\"><anchor ref=\"a2\">word</anchor></anchor> y</p></kml>"));
        REQUIRE(same->placePositions(PlaceKind::Annotation).size() == 2);
        CHECK(same->goToAdjacentPlace(PlaceKind::Annotation, true) == done(1, 2));
        CHECK(same->goToAdjacentPlace(PlaceKind::Annotation, true) == done(2, 2));
        CHECK(same->goToAdjacentPlace(PlaceKind::Annotation, true) ==
              failed(GoToResult::NoFurther, 2));
        CHECK(same->goToAdjacentPlace(PlaceKind::Annotation, false) == done(1, 2));
        CHECK(same->selectedText() == QStringLiteral("word"));
    }
}

TEST_CASE("Go To: the pages of the page view, none in the continuous one", "[editor][goto]") {
    QStringList paragraphs;
    for (int i = 0; i < 160; ++i) {
        paragraphs << QStringLiteral("Line %1 of the text that fills the pages one after another.")
                          .arg(i);
    }
    auto editor = editorWith(kmlOf(paragraphs));
    CHECK(editor->goToAdjacentPlace(PlaceKind::Page, true) == failed(GoToResult::NoPlaces, 0));
    CHECK(editor->goToPlace(PlaceKind::Page, QStringLiteral("2")) ==
          failed(GoToResult::NoPlaces, 0));

    editor->setViewMode(ViewMode::Page);
    qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout())
        ->layoutPendingBlocks();
    QCoreApplication::processEvents();
    const int pages = editor->totalPages();
    REQUIRE(pages >= 3);
    CHECK(editor->goToPlace(PlaceKind::Page, QStringLiteral("2")) == done(2, pages));
    CHECK(editor->currentPage() == 2);
    CHECK(editor->goToAdjacentPlace(PlaceKind::Page, true) == done(3, pages));
    CHECK(editor->currentPage() == 3);
    CHECK(editor->goToPlace(PlaceKind::Page, QStringLiteral("-2")) == done(1, pages));
    CHECK(editor->goToAdjacentPlace(PlaceKind::Page, false) ==
          failed(GoToResult::NoEarlier, pages));
    CHECK(editor->goToPlace(PlaceKind::Page, QString::number(pages + 1)) ==
          failed(GoToResult::NoNumber, pages));
    CHECK(editor->goToPlace(PlaceKind::Page, QString::number(pages)) == done(pages, pages));
    CHECK(editor->goToAdjacentPlace(PlaceKind::Page, true) == failed(GoToResult::NoFurther, pages));
}
