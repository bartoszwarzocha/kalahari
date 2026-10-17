/// @file test_read_only_text.cpp
/// @brief A read-only text, a chapter of a book open read-only: it is read, selected, copied
/// and searched, and nothing changes it

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/find_replace_bar.h>
#include "editor_test_utils.h"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMimeData>

#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A chapter with a comment on "quick"
const QString kChapter = QStringLiteral(
    "<kml><annotations><annotation id=\"a\" kind=\"comment\">Why?</annotation></annotations>"
    "<p>The <anchor ref=\"a\">quick</anchor> fox.</p><p>The last word.</p></kml>");

/// A key pressed in @p editor, as the keyboard does it
void press(BookEditor& editor, Qt::Key key, const QString& text = QString(),
           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(&editor, &event);
}

}  // namespace

TEST_CASE("Read-only text: nothing changes it, and each change says it was refused",
          "[editor][readonly]") {
    auto editor = std::make_unique<BookEditor>();
    REQUIRE(editor->fromKml(kChapter));
    CHECK_FALSE(editor->isReadOnly());
    editor->setReadOnly(true);
    CHECK(editor->isReadOnly());
    const QString kml = editor->toKml();
    int refused = 0;
    QObject::connect(editor.get(), &BookEditor::changeRefused, [&refused]() { ++refused; });

    SECTION("typing and deleting") {
        editor->setCursorPosition({0, 3});
        editor->typeText(QStringLiteral("x"));
        editor->insertText(QStringLiteral("y"));
        editor->insertNewline();
        editor->deleteBackward();
        press(*editor, Qt::Key_A, QStringLiteral("a"));
        press(*editor, Qt::Key_Backspace);
        press(*editor, Qt::Key_Delete);
        press(*editor, Qt::Key_Return);
        CHECK(editor->toKml() == kml);
        CHECK(refused == 8);
    }

    SECTION("cutting, deleting and pasting a selection") {
        QGuiApplication::clipboard()->setText(QStringLiteral("Pasted"));
        editor->setSelection({{0, 0}, {0, 3}});
        editor->cut();
        CHECK_FALSE(editor->deleteSelectedText());
        editor->paste();
        auto data = std::make_unique<QMimeData>();
        data->setText(QStringLiteral("Dropped"));
        CHECK_FALSE(editor->dropMimeData(data.get(), {1, 0}, false));
        CHECK(editor->toKml() == kml);
        CHECK(editor->selectedText() == QStringLiteral("The"));
        CHECK(refused >= 3);
        CHECK_FALSE(editor->canPaste());
    }

    SECTION("the formats, the alignments and the font") {
        editor->setSelection({{0, 4}, {0, 9}});
        editor->toggleBold();
        editor->toggleItalic();
        editor->toggleUnderline();
        editor->toggleStrikethrough();
        editor->setAlignCenter();
        editor->setAlignJustify();
        editor->setSelectionFontSize(30);
        CHECK(editor->toKml() == kml);
        CHECK(refused == 7);
    }

    SECTION("the annotations") {
        editor->setSelection({{1, 4}, {1, 8}});
        CHECK(editor->addAnnotation(AnnotationKind::Todo, QStringLiteral("Check"), QString())
                  .id.isEmpty());
        CHECK(editor->addAnnotation(0, 0, AnnotationKind::Note, QStringLiteral("Mine"), QString())
                  .id.isEmpty());
        const std::vector<AnnotationPlace> places = editor->annotations();
        REQUIRE(places.size() == 1);
        Annotation done = places.front().annotation;
        done.done = true;
        CHECK_FALSE(editor->updateAnnotation(done));
        CHECK_FALSE(editor->removeAnnotation(done.id));
        CHECK(editor->toKml() == kml);
        // The text keeps its annotations: a plain text file is another thing
        CHECK(editor->annotationsAllowed());
    }

    SECTION("undo and redo of what was done before") {
        editor->setReadOnly(false);
        editor->setCursorPosition({1, 0});
        editor->typeText(QStringLiteral("So "));
        const QString typed = editor->toKml();
        REQUIRE(editor->canUndo());
        editor->setReadOnly(true);
        CHECK_FALSE(editor->canUndo());
        editor->undo();
        CHECK(editor->toKml() == typed);
        editor->setReadOnly(false);
        CHECK(editor->canUndo());
        editor->undo();
        CHECK(editor->toKml() == kml);
    }

    SECTION("changed again once it may be changed") {
        editor->setReadOnly(false);
        CHECK_FALSE(editor->isReadOnly());
        editor->setCursorPosition({0, 0});
        editor->typeText(QStringLiteral("So "));
        CHECK(editor->plainText().startsWith(QStringLiteral("So The quick")));
        CHECK(refused == 0);
    }
}

TEST_CASE("Read-only text: it is selected, copied and searched", "[editor][readonly]") {
    auto editor = std::make_unique<BookEditor>();
    REQUIRE(editor->fromKml(kChapter));
    editor->setReadOnly(true);
    int refused = 0;
    QObject::connect(editor.get(), &BookEditor::changeRefused, [&refused]() { ++refused; });

    // Selected and copied
    editor->setSelection({{0, 4}, {0, 9}});
    QGuiApplication::clipboard()->clear();
    editor->copy();
    CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("quick"));
    editor->selectAll();
    CHECK(editor->selectedText().startsWith(QStringLiteral("The quick fox.")));

    // Find and Replace opens as Find: the replacements would change it
    editor->clearSelection();
    editor->showFindReplace();
    auto* bar = editor->findChild<FindReplaceBar*>();
    REQUIRE(bar != nullptr);
    CHECK_FALSE(bar->isHidden());
    CHECK_FALSE(bar->isReplaceMode());

    // Made read-only while Find and Replace is open, it only finds
    editor->setReadOnly(false);
    editor->showFindReplace();
    REQUIRE(bar->isReplaceMode());
    editor->setReadOnly(true);
    CHECK_FALSE(bar->isReplaceMode());

    // No input method composes in it, and the screen readers hear that it is read-only
    CHECK_FALSE(editor->testAttribute(Qt::WA_InputMethodEnabled));
    CHECK_FALSE(editor->inputMethodQuery(Qt::ImEnabled).toBool());
    CHECK(refused == 0);
    editor->setReadOnly(false);
    CHECK(editor->testAttribute(Qt::WA_InputMethodEnabled));
    CHECK(editor->inputMethodQuery(Qt::ImEnabled).toBool());
}
