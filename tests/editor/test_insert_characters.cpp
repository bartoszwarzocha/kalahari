/// @file test_insert_characters.cpp
/// @brief Characters inserted by a command or a window (Insert > Special Character...,
///        Insert > Formatting Character): an undo step of their own, without autocorrect

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/paragraph_styles.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QTextBlock>

#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// An editor with the paragraphs loaded
std::unique_ptr<BookEditor> editorWith(const QStringList& paragraphs) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(kmlOf(paragraphs));
    return editor;
}

/// Type the text as the keyboard does, a character at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(&editor, &event);
    }
}

}  // namespace

TEST_CASE("Inserted characters: at the cursor, an undo step of their own",
          "[editor][characters]") {
    auto editor = editorWith({QStringLiteral("Ala")});
    editor->setCursorPosition({0, 3});
    type(*editor, QStringLiteral(" ma"));

    editor->insertCharacters(QStringLiteral("\u00A0"));
    CHECK(editor->plainText() == QStringLiteral("Ala ma\u00A0"));
    CHECK(editor->cursorPosition() == CursorPosition{0, 7});

    type(*editor, QStringLiteral("kota"));
    CHECK(editor->plainText() == QStringLiteral("Ala ma\u00A0kota"));

    // Undo takes the character with the text typed after it, and leaves what was typed
    // before it
    editor->undo();
    CHECK(editor->plainText() == QStringLiteral("Ala ma"));
    editor->undo();
    CHECK(editor->plainText() == QStringLiteral("Ala"));
}

TEST_CASE("Inserted characters: autocorrect does not change them", "[editor][characters]") {
    auto editor = editorWith({QStringLiteral("1939")});
    editor->setCursorPosition({0, 4});

    // Typed, two hyphens between numbers become an en dash; inserted, they stay
    editor->insertCharacters(QStringLiteral("--"));
    editor->insertCharacters(QStringLiteral("1945"));
    editor->insertCharacters(QStringLiteral(" "));
    CHECK(editor->plainText() == QStringLiteral("1939--1945 "));

    // Nor does the next word typed change them (the change comes only after the word)
    editor->insertCharacters(QStringLiteral("\""));
    CHECK(editor->plainText() == QStringLiteral("1939--1945 \""));
}

TEST_CASE("Inserted characters: in place of the selection", "[editor][characters]") {
    auto editor = editorWith({QStringLiteral("Ala - kot")});
    editor->setSelection(SelectionRange{{0, 4}, {0, 5}});

    editor->insertCharacters(QStringLiteral("–"));
    CHECK(editor->plainText() == QStringLiteral("Ala – kot"));
    CHECK_FALSE(editor->hasSelection());
    CHECK(editor->cursorPosition() == CursorPosition{0, 5});

    editor->undo();
    CHECK(editor->plainText() == QStringLiteral("Ala - kot"));
}

TEST_CASE("Inserted characters: next to an object they get a paragraph of their own",
          "[editor][characters]") {
    auto editor = editorWith({QStringLiteral("Ala")});
    editor->setCursorPosition({0, 3});
    editor->insertTextObject(TextObject::SceneBreak);
    const QTextBlock sceneBreak = editor->textDocument()->findBlockByNumber(1);
    REQUIRE(blockObject(sceneBreak) == TextObject::SceneBreak);
    editor->setCursorPosition({1, 0});

    editor->insertCharacters(QStringLiteral("—"));
    const QTextBlock inserted =
        editor->textDocument()->findBlockByNumber(editor->cursorPosition().paragraph);
    CHECK(blockObject(inserted) == TextObject::None);
    CHECK(inserted.text() == QStringLiteral("—"));
}

TEST_CASE("Inserted characters: a read-only text refuses them", "[editor][characters]") {
    auto editor = editorWith({QStringLiteral("Ala")});
    editor->setCursorPosition({0, 3});
    editor->setReadOnly(true);
    int refused = 0;
    QObject::connect(editor.get(), &BookEditor::changeRefused, [&refused]() { ++refused; });

    editor->insertCharacters(QStringLiteral("—"));
    CHECK(editor->plainText() == QStringLiteral("Ala"));
    CHECK(refused == 1);
}
