/// @file test_clear_formatting.cpp
/// @brief Format > Clear Formatting: the formatting given by hand goes, the styles of the
///        paragraphs, the annotations and the objects stay

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// An editor with KML loaded
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(kml);
    return editor;
}

/// Type the text as the keyboard does, a character at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(&editor, &event);
    }
}

}  // anonymous namespace

TEST_CASE("Clear Formatting: the selection loses the formatting given by hand, in one step",
          "[editor][format]") {
    const QString kml = QStringLiteral(
        "<kml><p align=\"center\"><b>Bold</b> <i>it</i> <u>un</u> <s>st</s> x<sub>2</sub> "
        "<span color=\"#aa0000\">red</span></p><p align=\"right\">Next <b>one</b></p>"
        "<p>Last <b>word</b></p></kml>");
    auto editor = editorWith(kml);
    editor->setSelection({{0, 2}, {1, 6}});

    editor->clearFormatting();

    // The selected text up to "Next o", the alignments of both paragraphs; the rest keeps its
    // own
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p><b>Bo</b>ld it un st x2 red</p><p>Next o<b>ne</b></p>"
                         "<p>Last <b>word</b></p></kml>"));
    CHECK(editor->selection().normalized().start == CursorPosition{0, 2});

    // One step of the undo history, with the selection it was made with
    editor->undo();
    CHECK(editor->toKml() == kml);
    CHECK(editor->hasSelection());
    editor->redo();
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p><b>Bo</b>ld it un st x2 red</p><p>Next o<b>ne</b></p>"
                         "<p>Last <b>word</b></p></kml>"));
}

TEST_CASE("Clear Formatting: the styles, the annotations and the objects stay",
          "[editor][format]") {
    const QString kml = QStringLiteral(
        "<kml><annotations><annotation id=\"c1\" kind=\"comment\">Note</annotation>"
        "</annotations><h1>Title <i>one</i></h1>"
        "<p style=\"quote\">A <anchor ref=\"c1\"><b>quoted</b></anchor> line</p><hr/></kml>");
    auto editor = editorWith(kml);
    editor->selectAll();

    editor->clearFormatting();

    CHECK(editor->toKml() ==
          QStringLiteral("<kml><annotations><annotation id=\"c1\" kind=\"comment\">Note"
                         "</annotation></annotations><h1>Title one</h1>"
                         "<p style=\"quote\">A <anchor ref=\"c1\">quoted</anchor> line</p>"
                         "<hr/></kml>"));
}

TEST_CASE("Clear Formatting: without a selection the word with the cursor, or the text typed "
          "next",
          "[editor][format]") {
    SECTION("inside a word: the word and the paragraph's alignment") {
        auto editor = editorWith(QStringLiteral(
            "<kml><p align=\"center\"><b>One</b> <i>two</i></p></kml>"));
        editor->setCursorPosition({0, 1});
        editor->clearFormatting();
        CHECK(editor->toKml() == QStringLiteral("<kml><p>One <i>two</i></p></kml>"));
    }

    SECTION("after bold text: the text typed next is not bold, the text before stays") {
        auto editor = editorWith(QStringLiteral("<kml><p><b>One</b></p></kml>"));
        editor->setCursorPosition({0, 3});
        REQUIRE(editor->isBold());
        editor->clearFormatting();
        CHECK_FALSE(editor->isBold());
        type(*editor, QStringLiteral(" two"));
        CHECK(editor->toKml() == QStringLiteral("<kml><p><b>One</b> two</p></kml>"));
    }

    SECTION("nothing to take off: no step of the undo history") {
        auto editor = editorWith(QStringLiteral("<kml><p>Plain</p></kml>"));
        editor->setCursorPosition({0, 2});
        const bool couldUndo = editor->canUndo();
        editor->clearFormatting();
        CHECK(editor->canUndo() == couldUndo);
        CHECK(editor->toKml() == QStringLiteral("<kml><p>Plain</p></kml>"));
    }
}

TEST_CASE("Clear Formatting: a read-only text keeps its formatting", "[editor][format]") {
    const QString kml = QStringLiteral("<kml><p align=\"center\"><b>One</b></p></kml>");
    auto editor = editorWith(kml);
    editor->setReadOnly(true);
    editor->selectAll();
    editor->clearFormatting();
    CHECK(editor->toKml() == kml);
}
