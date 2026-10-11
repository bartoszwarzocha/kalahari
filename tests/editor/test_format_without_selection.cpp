/// @file test_format_without_selection.cpp
/// @brief Character formats given without a selection: on the word with the cursor, or on
///        the text typed next
///
/// As in Word and LibreOffice: with the cursor inside a word, bold, italic, underline,
/// strikethrough, the font and its size go on the whole word. Elsewhere they go on the text
/// typed next at the cursor, until the cursor moves.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <kalahari/editor/book_editor.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QFont>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFragment>
#include <memory>
#include <optional>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A format command, how the text shows it and its KML tag
struct Command {
    const char* name;
    void (BookEditor::*give)();
    bool (BookEditor::*isOn)() const;
    bool (*has)(const QTextCharFormat&);
    const char* tag;
};

const Command BOLD{"bold", &BookEditor::toggleBold, &BookEditor::isBold,
                   [](const QTextCharFormat& f) { return f.fontWeight() >= QFont::Bold; }, "b"};
const Command ITALIC{"italic", &BookEditor::toggleItalic, &BookEditor::isItalic,
                     [](const QTextCharFormat& f) { return f.fontItalic(); }, "i"};
const Command UNDERLINE{"underline", &BookEditor::toggleUnderline, &BookEditor::isUnderline,
                        [](const QTextCharFormat& f) { return f.fontUnderline(); }, "u"};
const Command STRIKETHROUGH{"strikethrough", &BookEditor::toggleStrikethrough,
                            &BookEditor::isStrikethrough,
                            [](const QTextCharFormat& f) { return f.fontStrikeOut(); }, "s"};

const QString FAMILY = QStringLiteral("Kalahari Test Mono");
constexpr int SIZE = 21;

/// Editor holding one paragraph for each string (KML inside a paragraph)
std::unique_ptr<BookEditor> editorWith(const QStringList& paragraphs) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(kmlOf(paragraphs));
    return editor;
}

void give(BookEditor& editor, const Command& command) {
    (editor.*command.give)();
}

bool isOn(const BookEditor& editor, const Command& command) {
    return (editor.*command.isOn)();
}

void press(BookEditor& editor, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(&editor, &event);
}

/// Type the text as the keyboard does, a character at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(&editor, &event);
    }
}

/// The text of a paragraph that has a format, in text order
QString textWith(const BookEditor& editor, int paragraph, bool (*has)(const QTextCharFormat&)) {
    QString text;
    const QTextBlock block = editor.textDocument()->findBlockByNumber(paragraph);
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (fragment.isValid() && has(fragment.charFormat())) {
            text += fragment.text();
        }
    }
    return text;
}

QString textWith(const BookEditor& editor, const Command& command) {
    return textWith(editor, 0, command.has);
}

bool hasFamily(const QTextCharFormat& format) {
    return format.fontFamilies().toStringList().value(0) == FAMILY;
}

bool hasSize(const QTextCharFormat& format) {
    return format.fontPointSize() == SIZE;
}

}  // anonymous namespace

TEST_CASE("Without a selection a format goes on the text typed next", "[editor][format]") {
    const Command command = GENERATE(BOLD, ITALIC, UNDERLINE, STRIKETHROUGH);
    CAPTURE(command.name);
    const QString tag = QString::fromLatin1(command.tag);

    SECTION("Between words: at the start of a word") {
        auto editor = editorWith({QStringLiteral("One two")});
        editor->setCursorPosition({0, 4});
        give(*editor, command);

        CHECK(isOn(*editor, command));
        CHECK(textWith(*editor, command).isEmpty());  // nothing has it yet
        type(*editor, QStringLiteral("big "));
        CHECK(textWith(*editor, command) == QStringLiteral("big "));
        CHECK(isOn(*editor, command));
        CHECK(editor->toKml().contains(QStringLiteral("One <%1>big </%1>two").arg(tag)));
    }

    SECTION("At the end of a word and of the paragraph") {
        auto editor = editorWith({QStringLiteral("One two")});
        editor->setCursorPosition({0, 7});
        give(*editor, command);
        type(*editor, QStringLiteral(" three"));
        CHECK(textWith(*editor, command) == QStringLiteral(" three"));
    }

    SECTION("In an empty paragraph") {
        auto editor = editorWith({QString()});
        give(*editor, command);
        CHECK(isOn(*editor, command));
        type(*editor, QStringLiteral("New"));
        CHECK(textWith(*editor, command) == QStringLiteral("New"));
    }

    SECTION("Given where the text has it, the text typed next is without it") {
        auto editor = editorWith({QStringLiteral("<%1>Word</%1> end").arg(tag)});
        editor->setCursorPosition({0, 4});
        REQUIRE(isOn(*editor, command));
        give(*editor, command);

        CHECK_FALSE(isOn(*editor, command));
        type(*editor, QStringLiteral("er"));
        CHECK(textWith(*editor, command) == QStringLiteral("Word"));
        CHECK(editor->plainText() == QStringLiteral("Worder end"));
    }

    SECTION("Given twice, the text typed next is as it would be without it") {
        auto editor = editorWith({QStringLiteral("One two")});
        editor->setCursorPosition({0, 4});
        give(*editor, command);
        give(*editor, command);

        CHECK_FALSE(isOn(*editor, command));
        type(*editor, QStringLiteral("big "));
        CHECK(textWith(*editor, command).isEmpty());

        // Exactly as the text around it: the paragraph is one run of one format
        const QTextBlock block = editor->textDocument()->findBlockByNumber(0);
        CHECK(block.begin().fragment().text() == QStringLiteral("One big two"));
    }

    SECTION("Several formats go on the same text") {
        const Command other = command.give == BOLD.give ? ITALIC : BOLD;
        auto editor = editorWith({QStringLiteral("One two")});
        editor->setCursorPosition({0, 4});
        give(*editor, command);
        give(*editor, other);
        type(*editor, QStringLiteral("big "));
        CHECK(textWith(*editor, command) == QStringLiteral("big "));
        CHECK(textWith(*editor, other) == QStringLiteral("big "));
    }
}

TEST_CASE("Moving the cursor drops a format given without a selection", "[editor][format]") {
    const Command command = GENERATE(BOLD, ITALIC, UNDERLINE, STRIKETHROUGH);
    CAPTURE(command.name);
    auto editor = editorWith({QStringLiteral("One two")});
    editor->setCursorPosition({0, 4});
    give(*editor, command);
    REQUIRE(isOn(*editor, command));

    SECTION("Away and back with the arrow keys") {
        press(*editor, Qt::Key_Right);
        CHECK_FALSE(isOn(*editor, command));
        press(*editor, Qt::Key_Left);
    }

    SECTION("Away and back by a click") {
        editor->setCursorPosition({0, 0});
        editor->setCursorPosition({0, 4});
    }

    SECTION("Selecting text there") {
        press(*editor, Qt::Key_Right, Qt::ShiftModifier);
        CHECK_FALSE(isOn(*editor, command));  // the selection does not have it
        press(*editor, Qt::Key_Left);
    }

    REQUIRE(editor->cursorPosition() == CursorPosition{0, 4});
    CHECK_FALSE(isOn(*editor, command));
    type(*editor, QStringLiteral("big "));
    CHECK(textWith(*editor, command).isEmpty());
}

TEST_CASE("Told of a move or a selection, one sees the format there without the one given",
          "[editor][format]") {
    // The toolbar follows the cursor and the selection this way
    auto editor = editorWith({QStringLiteral("One two")});
    editor->setCursorPosition({0, 4});
    editor->toggleBold();
    editor->setSelectionFontFamily(FAMILY);

    std::optional<bool> bold;
    QString family;
    const auto look = [&] {
        bold = editor->isBold();
        family = editor->currentFontFamily();
    };

    SECTION("The cursor moved") {
        QObject::connect(editor.get(), &BookEditor::cursorPositionChanged, editor.get(),
                         [&](const CursorPosition&) { look(); });
        editor->setCursorPosition({0, 0});
    }

    SECTION("Text selected up to the cursor, which stays") {
        QObject::connect(editor.get(), &BookEditor::selectionChanged, editor.get(), look);
        editor->setSelection(SelectionRange{{0, 0}, {0, 4}});
        REQUIRE(editor->cursorPosition() == CursorPosition{0, 4});
    }

    REQUIRE(bold.has_value());
    CHECK_FALSE(*bold);
    CHECK(family != FAMILY);
}

TEST_CASE("Typing keeps a format given without a selection", "[editor][format]") {
    SECTION("Enter: the new paragraph's text has it") {
        auto editor = editorWith({QStringLiteral("One")});
        editor->setCursorPosition({0, 3});
        editor->toggleItalic();
        press(*editor, Qt::Key_Return);

        CHECK(editor->isItalic());
        type(*editor, QStringLiteral("Two"));
        CHECK(textWith(*editor, 1, ITALIC.has) == QStringLiteral("Two"));
        CHECK(textWith(*editor, 0, ITALIC.has).isEmpty());
    }

    SECTION("Text composed with an input method") {
        auto editor = editorWith({QStringLiteral("One two")});
        editor->setCursorPosition({0, 4});
        editor->toggleBold();

        QInputMethodEvent composing(QStringLiteral("ż"), {});
        QCoreApplication::sendEvent(editor.get(), &composing);
        CHECK(textWith(*editor, BOLD) == QStringLiteral("ż"));  // shown as it will be typed
        QInputMethodEvent composingMore(QStringLiteral("żó"), {});
        QCoreApplication::sendEvent(editor.get(), &composingMore);
        CHECK(textWith(*editor, BOLD) == QStringLiteral("żó"));

        QInputMethodEvent committed;
        committed.setCommitString(QStringLiteral("żółw "));
        QCoreApplication::sendEvent(editor.get(), &committed);
        CHECK(editor->plainText() == QStringLiteral("One żółw two"));
        CHECK(textWith(*editor, BOLD) == QStringLiteral("żółw "));
    }

    SECTION("Plain text pasted from another program") {
        auto editor = editorWith({QStringLiteral("One two")});
        editor->setCursorPosition({0, 4});
        editor->toggleBold();

        QMimeData pasted;
        pasted.setText(QStringLiteral("big "));
        editor->insertFromMimeData(&pasted);
        CHECK(editor->plainText() == QStringLiteral("One big two"));
        CHECK(textWith(*editor, BOLD) == QStringLiteral("big "));
    }
}

TEST_CASE("Undo, redo and another text drop a format given without a selection",
          "[editor][format]") {
    auto editor = editorWith({QStringLiteral("One two")});
    CursorPosition place{0, 4};

    SECTION("Undo of a step made at the same place") {
        editor->setCursorPosition(place);
        editor->setAlignCenter();
        editor->toggleBold();
        editor->undo();
    }

    SECTION("Redo of a step made at the same place") {
        editor->setCursorPosition(place);
        editor->setAlignCenter();
        editor->undo();
        editor->toggleBold();
        editor->redo();
    }

    SECTION("Another text, with the cursor at the same place") {
        place = {0, 0};
        editor->toggleBold();
        editor->fromKml(kmlOf({QStringLiteral("One two")}));
    }

    SECTION("Another text put in place of this one, with the cursor at the same place") {
        editor->setCursorPosition(place);
        editor->toggleBold();
        editor->replaceWithKml(kmlOf({QStringLiteral("One two")}));
    }

    REQUIRE(editor->cursorPosition() == place);
    CHECK_FALSE(editor->isBold());
    type(*editor, QStringLiteral("big "));
    CHECK(textWith(*editor, BOLD).isEmpty());
}

TEST_CASE("With the cursor inside a word a format goes on the whole word", "[editor][format]") {
    const Command command = GENERATE(BOLD, ITALIC, UNDERLINE, STRIKETHROUGH);
    CAPTURE(command.name);
    const QString tag = QString::fromLatin1(command.tag);

    SECTION("On and off again") {
        auto editor = editorWith({QStringLiteral("One two three")});
        editor->setCursorPosition({0, 5});
        give(*editor, command);

        CHECK(textWith(*editor, command) == QStringLiteral("two"));
        CHECK(editor->toKml().contains(QStringLiteral("One <%1>two</%1> three").arg(tag)));
        CHECK(editor->cursorPosition() == CursorPosition{0, 5});
        CHECK_FALSE(editor->hasSelection());
        CHECK(isOn(*editor, command));

        give(*editor, command);
        CHECK(textWith(*editor, command).isEmpty());
        CHECK_FALSE(isOn(*editor, command));
    }

    SECTION("A word that has it in part gets it whole") {
        auto editor = editorWith({QStringLiteral("One t<%1>w</%1>o three").arg(tag)});
        editor->setCursorPosition({0, 6});
        give(*editor, command);
        CHECK(textWith(*editor, command) == QStringLiteral("two"));
    }

    SECTION("A word with apostrophes and one with Polish letters") {
        auto editor = editorWith({QStringLiteral("Rock’n’roll, zażółć gęślą")});
        editor->setCursorPosition({0, 2});
        give(*editor, command);
        CHECK(textWith(*editor, command) == QStringLiteral("Rock’n’roll"));

        editor->setCursorPosition({0, 16});
        give(*editor, command);
        CHECK(textWith(*editor, command) == QStringLiteral("Rock’n’rollzażółć"));
    }

    SECTION("Between a word and a punctuation mark the text typed next gets it") {
        auto editor = editorWith({QStringLiteral("One, two")});
        editor->setCursorPosition({0, 3});
        give(*editor, command);
        CHECK(textWith(*editor, command).isEmpty());
        CHECK(isOn(*editor, command));
    }

    SECTION("Between two spaces the text typed next gets it") {
        auto editor = editorWith({QStringLiteral("One  two")});
        REQUIRE(editor->plainText() == QStringLiteral("One  two"));
        editor->setCursorPosition({0, 4});
        give(*editor, command);
        CHECK(textWith(*editor, command).isEmpty());
        CHECK(isOn(*editor, command));
    }
}

TEST_CASE("Without a selection the font and its size go on the word or the text typed next",
          "[editor][format]") {
    SECTION("Inside a word: the whole word") {
        auto editor = editorWith({QStringLiteral("One two three")});
        editor->setCursorPosition({0, 5});
        editor->setSelectionFontFamily(FAMILY);
        editor->setSelectionFontSize(SIZE);

        CHECK(textWith(*editor, 0, hasFamily) == QStringLiteral("two"));
        CHECK(textWith(*editor, 0, hasSize) == QStringLiteral("two"));
        CHECK(editor->currentFontFamily() == FAMILY);
        CHECK(editor->currentFontSize() == SIZE);
        CHECK(editor->cursorPosition() == CursorPosition{0, 5});
        CHECK_FALSE(editor->hasSelection());
    }

    SECTION("Elsewhere: the text typed next, until the cursor moves") {
        auto editor = editorWith({QStringLiteral("One two")});
        const QString defaultFamily = editor->currentFontFamily();
        const int defaultSize = editor->currentFontSize();
        REQUIRE(defaultFamily != FAMILY);
        REQUIRE(defaultSize != SIZE);

        editor->setCursorPosition({0, 4});
        editor->setSelectionFontFamily(FAMILY);
        editor->setSelectionFontSize(SIZE);
        CHECK(editor->currentFontFamily() == FAMILY);
        CHECK(editor->currentFontSize() == SIZE);
        CHECK(textWith(*editor, 0, hasFamily).isEmpty());

        SECTION("Typed") {
            type(*editor, QStringLiteral("big "));
            CHECK(textWith(*editor, 0, hasFamily) == QStringLiteral("big "));
            CHECK(textWith(*editor, 0, hasSize) == QStringLiteral("big "));
            CHECK(editor->toKml().contains(
                QStringLiteral("<span font=\"%1\" size=\"%2\">big </span>").arg(FAMILY).arg(SIZE)));
        }

        SECTION("The cursor moved") {
            press(*editor, Qt::Key_Right);
            press(*editor, Qt::Key_Left);
            CHECK(editor->currentFontFamily() == defaultFamily);
            CHECK(editor->currentFontSize() == defaultSize);
            type(*editor, QStringLiteral("big "));
            CHECK(textWith(*editor, 0, hasFamily).isEmpty());
        }

        // The editor's own font is the settings' one
        CHECK(editor->appearance().typography.textFont.family() == defaultFamily);
        CHECK(editor->appearance().typography.textFont.pointSize() == defaultSize);
    }

    SECTION("The font and size of the place given again: the text typed next is as it would be") {
        auto editor = editorWith({QStringLiteral("One two")});
        editor->setCursorPosition({0, 4});
        const QString family = editor->currentFontFamily();
        const int size = editor->currentFontSize();
        editor->setSelectionFontFamily(FAMILY);
        editor->setSelectionFontFamily(family);
        editor->setSelectionFontSize(SIZE);
        editor->setSelectionFontSize(size);
        type(*editor, QStringLiteral("big "));
        CHECK(textWith(*editor, 0, [](const QTextCharFormat& f) {
                  return f.hasProperty(QTextFormat::FontFamilies) ||
                         f.hasProperty(QTextFormat::FontPointSize);
              }).isEmpty());
    }
}

TEST_CASE("Undo of a format brings back the cursor and the selection it was given with",
          "[editor][format]") {
    auto editor = editorWith({QStringLiteral("One two three")});

    SECTION("On the word with the cursor") {
        editor->setCursorPosition({0, 5});
        editor->toggleBold();
        editor->setCursorPosition({0, 0});

        editor->undo();
        CHECK(textWith(*editor, BOLD).isEmpty());
        CHECK(editor->cursorPosition() == CursorPosition{0, 5});
        CHECK_FALSE(editor->hasSelection());

        editor->setCursorPosition({0, 0});
        editor->redo();
        CHECK(textWith(*editor, BOLD) == QStringLiteral("two"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 5});
    }

    SECTION("On a selection") {
        editor->setCursorPosition({0, 4});
        press(*editor, Qt::Key_Right, Qt::ShiftModifier);
        press(*editor, Qt::Key_Right, Qt::ShiftModifier);
        press(*editor, Qt::Key_Right, Qt::ShiftModifier);
        editor->toggleItalic();
        editor->setSelectionFontSize(SIZE);
        // Elsewhere, without the selection (on macOS Home would only scroll the view)
        editor->clearSelection();
        editor->setCursorPosition({0, 0});
        REQUIRE_FALSE(editor->hasSelection());

        editor->undo();
        CHECK(textWith(*editor, 0, hasSize).isEmpty());
        CHECK(editor->selectedText() == QStringLiteral("two"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});

        editor->undo();
        CHECK(textWith(*editor, ITALIC).isEmpty());
        CHECK(editor->selectedText() == QStringLiteral("two"));
    }
}
