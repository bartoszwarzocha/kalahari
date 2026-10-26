/// @file test_list_editing.cpp
/// @brief Lists in the editor: Numbering and Bullets, Enter, Backspace, Tab, the indents, the
///        numbering of an item, autocorrect and the context menu of an item

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_lists.h>
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

/// A key pressed as the keyboard does it, with the text it types
void press(BookEditor& editor, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QCoreApplication::sendEvent(&editor, &event);
}

/// Type the text as the keyboard does, a character at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        press(editor, 0, Qt::NoModifier, QString(ch));
    }
}

/// The marks of the items, by paragraph
std::vector<QString> markersOf(const BookEditor& editor) {
    return listMarkers(*editor.textDocument());
}

/// The list item a paragraph is
std::optional<ListItem> itemOf(const BookEditor& editor, int paragraph) {
    return listItemOf(editor.textDocument()->findBlockByNumber(paragraph).blockFormat());
}

}  // anonymous namespace

TEST_CASE("Lists: Numbering and Bullets make the paragraphs a list and take them out of it",
          "[editor][lists]") {
    const QString kml = QStringLiteral("<kml><p>One</p><p>Two</p><p>Three</p></kml>");
    auto editor = editorWith(kml);
    editor->setSelection({{0, 1}, {2, 1}});

    editor->toggleList(ListKind::Numbered);
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><ol><li>One</li><li>Two</li><li>Three</li></ol></kml>"));
    CHECK(markersOf(*editor) ==
          std::vector<QString>{QStringLiteral("1."), QStringLiteral("2."), QStringLiteral("3.")});
    CHECK(editor->currentListKind() == ListKind::Numbered);

    // Bullets for the numbered items: the same paragraphs with bullets
    editor->toggleList(ListKind::Bulleted);
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><ul><li>One</li><li>Two</li><li>Three</li></ul></kml>"));
    CHECK(editor->currentListKind() == ListKind::Bulleted);

    // Bullets again: out of the list
    editor->toggleList(ListKind::Bulleted);
    CHECK(editor->toKml() == kml);
    CHECK_FALSE(editor->currentListKind().has_value());

    // Each change one step of the undo history, with the selection it was made with
    editor->undo();
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><ul><li>One</li><li>Two</li><li>Three</li></ul></kml>"));
    CHECK(editor->hasSelection());
    editor->undo();
    editor->undo();
    CHECK(editor->toKml() == kml);
}

TEST_CASE("Lists: a paragraph right before or after a list of the kind joins it",
          "[editor][lists]") {
    auto editor = editorWith(
        QStringLiteral("<kml><ol><li>One</li><li>Two</li></ol><p>Three</p><p>Apart</p></kml>"));
    editor->setCursorPosition({2, 0});
    editor->toggleList(ListKind::Numbered);
    CHECK(editor->toKml() == QStringLiteral(
                                 "<kml><ol><li>One</li><li>Two</li><li>Three</li></ol>"
                                 "<p>Apart</p></kml>"));
    CHECK(itemOf(*editor, 2)->id == itemOf(*editor, 0)->id);

    // Bullets: a new list, not the numbered one
    editor->setCursorPosition({3, 0});
    editor->toggleList(ListKind::Bulleted);
    CHECK(markersOf(*editor).back() == QString(QChar(BULLET_DISC)));
    CHECK(itemOf(*editor, 3)->id != itemOf(*editor, 0)->id);
}

TEST_CASE("Lists: Enter starts the next item, in an empty item it goes a level up and out",
          "[editor][lists]") {
    auto editor = editorWith(QStringLiteral("<kml><ol><li>One<ol type=\"a\" suffix=\")\">"
                                            "<li>One a</li></ol></li></ol></kml>"));
    editor->setCursorPosition({1, 5});
    press(*editor, Qt::Key_Return);
    type(*editor, QStringLiteral("One b"));
    CHECK(markersOf(*editor) ==
          std::vector<QString>{QStringLiteral("1."), QStringLiteral("a)"), QStringLiteral("b)")});

    // Enter in the empty item: a level up, then out of the list
    press(*editor, Qt::Key_Return);
    CHECK(itemOf(*editor, 3)->level == 1);
    press(*editor, Qt::Key_Return);
    CHECK(itemOf(*editor, 3)->level == 0);
    CHECK(markersOf(*editor)[3] == QStringLiteral("2."));
    press(*editor, Qt::Key_Return);
    CHECK_FALSE(itemOf(*editor, 3).has_value());
    CHECK(editor->textDocument()->blockCount() == 4);
    type(*editor, QStringLiteral("After"));
    CHECK(editor->toKml() == QStringLiteral("<kml><ol><li>One<ol type=\"a\" suffix=\")\">"
                                            "<li>One a</li><li>One b</li></ol></li></ol>"
                                            "<p>After</p></kml>"));
}

TEST_CASE("Lists: the item after Enter does not keep the number of its own", "[editor][lists]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><ol><li>One</li><li value=\"5\">Five</li></ol><p page-break-before=\"true\">"
        "Page</p></kml>"));
    editor->setCursorPosition({1, 4});
    press(*editor, Qt::Key_Return);
    type(*editor, QStringLiteral("Six"));
    CHECK(markersOf(*editor)[2] == QStringLiteral("6."));
    CHECK_FALSE(itemOf(*editor, 2)->value.has_value());

    // A paragraph split in two: the new one does not start a page
    editor->setCursorPosition({3, 2});
    press(*editor, Qt::Key_Return);
    CHECK(editor->toKml().endsWith(
        QStringLiteral("<p page-break-before=\"true\">Pa</p><p>ge</p></kml>")));
}

TEST_CASE("Lists: Backspace at the start of an item takes its number, then the item",
          "[editor][lists]") {
    const QString kml = QStringLiteral("<kml><ul><li>One</li><li>Two</li></ul></kml>");
    auto editor = editorWith(kml);
    editor->setCursorPosition({1, 0});
    press(*editor, Qt::Key_Backspace);
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><ul><li>One</li><li numbered=\"false\">Two</li></ul></kml>"));
    press(*editor, Qt::Key_Backspace);
    CHECK(editor->toKml() == QStringLiteral("<kml><ul><li>One</li></ul><p>Two</p></kml>"));
    // Then it joins the paragraph before it, as any paragraph does
    press(*editor, Qt::Key_Backspace);
    CHECK(editor->plainText() == QStringLiteral("OneTwo"));

    // The first item of the text too
    editor->fromKml(kml);
    editor->setCursorPosition({0, 0});
    press(*editor, Qt::Key_Backspace);
    CHECK_FALSE(itemOf(*editor, 0)->numbered);
}

TEST_CASE("Lists: Tab and Shift+Tab at the start of an item change its level",
          "[editor][lists]") {
    auto editor = editorWith(
        QStringLiteral("<kml><ol><li>One</li><li>Two</li><li>Three</li></ol><p>Text</p></kml>"));
    editor->setCursorPosition({1, 0});
    press(*editor, Qt::Key_Tab);
    CHECK(itemOf(*editor, 1)->level == 1);
    CHECK(markersOf(*editor)[1] == QStringLiteral("a)"));
    CHECK(markersOf(*editor)[2] == QStringLiteral("2."));
    press(*editor, Qt::Key_Backtab, Qt::ShiftModifier);
    CHECK(itemOf(*editor, 1)->level == 0);
    // At the first level Shift+Tab changes nothing, and stays in the text
    press(*editor, Qt::Key_Backtab, Qt::ShiftModifier);
    CHECK(itemOf(*editor, 1)->level == 0);

    // A selection of items: all of them
    editor->setSelection({{1, 1}, {2, 2}});
    press(*editor, Qt::Key_Tab);
    CHECK(itemOf(*editor, 1)->level == 1);
    CHECK(itemOf(*editor, 2)->level == 1);
    CHECK(itemOf(*editor, 0)->level == 0);
    CHECK(editor->plainText() == QStringLiteral("One\nTwo\nThree\nText"));

    // Inside an item, or outside a list, Tab is not the list's
    editor->setCursorPosition({0, 2});
    press(*editor, Qt::Key_Tab);
    CHECK(itemOf(*editor, 0)->level == 0);
    editor->setCursorPosition({3, 0});
    press(*editor, Qt::Key_Tab);
    CHECK(editor->toKml().endsWith(QStringLiteral("<p>Text</p></kml>")));
}

TEST_CASE("Lists: Increase and Decrease Indent", "[editor][lists][indent]") {
    auto editor = editorWith(QStringLiteral("<kml><p>Text</p><ul><li>Item</li></ul></kml>"));
    editor->setCursorPosition({0, 0});

    // A paragraph outside a list: a step of 1.25 cm at a time, never past the edge
    editor->increaseIndent();
    CHECK(editor->toKml().startsWith(QStringLiteral("<kml><p indent-left=\"35.43\">Text</p>")));
    editor->increaseIndent();
    CHECK(editor->toKml().startsWith(QStringLiteral("<kml><p indent-left=\"70.87\">Text</p>")));
    editor->decreaseIndent();
    editor->decreaseIndent();
    CHECK(editor->toKml().startsWith(QStringLiteral("<kml><p>Text</p>")));
    editor->decreaseIndent();
    CHECK(editor->toKml().startsWith(QStringLiteral("<kml><p>Text</p>")));
    CHECK(editor->shownParagraphFormat().leftIndent.value_or(-1) == 0.0);

    // A list item: a level down and up
    editor->setCursorPosition({1, 0});
    editor->increaseIndent();
    CHECK(itemOf(*editor, 1)->level == 1);
    CHECK(markersOf(*editor)[1] == QString(QChar(BULLET_CIRCLE)));
    editor->decreaseIndent();
    CHECK(itemOf(*editor, 1)->level == 0);

    // The indents a list item is shown with: those of its level
    const ParagraphFormat shown = editor->shownParagraphFormat();
    CHECK(shown.leftIndent == 2.0 * LIST_INDENT_STEP);
    CHECK(shown.firstLineIndent == -LIST_INDENT_STEP);
}

TEST_CASE("Lists: Restart at 1, Continue Numbering and Set Numbering Value",
          "[editor][lists]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><ol><li>One</li><li>Two</li><li>Three</li></ol><p>Text</p>"
        "<ol><li>New one</li><li>New two</li></ol></kml>"));

    // Restart: the item gets the first number, the next ones go on from it
    editor->setCursorPosition({1, 0});
    editor->restartNumbering();
    CHECK(markersOf(*editor)[1] == QStringLiteral("1."));
    CHECK(markersOf(*editor)[2] == QStringLiteral("2."));
    CHECK(editor->canContinueNumbering());
    editor->continueNumbering();
    CHECK(markersOf(*editor)[1] == QStringLiteral("2."));
    CHECK_FALSE(editor->canContinueNumbering());

    // The first item of the second list: Continue Numbering joins it with the list before it
    editor->setCursorPosition({4, 0});
    CHECK(editor->canContinueNumbering());
    editor->continueNumbering();
    CHECK(markersOf(*editor)[4] == QStringLiteral("4."));
    CHECK(markersOf(*editor)[5] == QStringLiteral("5."));
    CHECK(itemOf(*editor, 5)->id == itemOf(*editor, 0)->id);
    CHECK(editor->toKml() == QStringLiteral(
                                 "<kml><ol><li>One</li><li>Two</li><li>Three</li></ol><p>Text</p>"
                                 "<ol continue=\"true\"><li>New one</li><li>New two</li></ol>"
                                 "</kml>"));

    // Set Numbering Value
    editor->setNumberingValue(10);
    CHECK(markersOf(*editor)[4] == QStringLiteral("10."));
    CHECK(markersOf(*editor)[5] == QStringLiteral("11."));
    CHECK(editor->currentListNumber() == 10);

    // One step each
    editor->undo();
    CHECK(markersOf(*editor)[4] == QStringLiteral("4."));
}

TEST_CASE("Lists: Bullets and Numbering gives the lists of the selected paragraphs the formats",
          "[editor][lists]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><ol><li>One</li><li>Two</li></ol><p>Text</p></kml>"));
    editor->setCursorPosition({0, 0});
    ListDefinition roman = ListDefinition::numbered();
    roman.levels[0].format = ListNumberFormat::UpperRoman;
    editor->applyListDefinition(roman);
    CHECK(markersOf(*editor)[1] == QStringLiteral("II."));
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><ol type=\"I\"><li>One</li><li>Two</li></ol><p>Text</p></kml>"));

    // A paragraph outside a list joins the first list of the selection
    editor->setSelection({{1, 0}, {2, 2}});
    editor->applyListDefinition(ListDefinition::bulleted());
    CHECK(markersOf(*editor) == std::vector<QString>{QString(QChar(BULLET_DISC)),
                                                     QString(QChar(BULLET_DISC)),
                                                     QString(QChar(BULLET_DISC))});
}

TEST_CASE("Lists: autocorrect makes \"* \" and \"1. \" starting a paragraph a list",
          "[editor][lists][autocorrect]") {
    auto editor = editorWith(kmlOf({QString()}));
    type(*editor, QStringLiteral("* Coat"));
    CHECK(editor->toKml() == QStringLiteral("<kml><ul><li>Coat</li></ul></kml>"));

    // Undone right after: what was typed comes back
    editor->fromKml(kmlOf({QString()}));
    type(*editor, QStringLiteral("1. "));
    CHECK(editor->toKml() == QStringLiteral("<kml><ol><li></li></ol></kml>"));
    editor->undo();
    CHECK(editor->toKml() == QStringLiteral("<kml><p>1. </p></kml>"));
    CHECK(editor->cursorPosition() == CursorPosition{0, 3});

    // From the number typed, with its suffix; the next one goes on with the list
    editor->fromKml(kmlOf({QString()}));
    type(*editor, QStringLiteral("3) Three"));
    press(*editor, Qt::Key_Return);
    type(*editor, QStringLiteral("Four"));
    CHECK(editor->toKml() == QStringLiteral(
                                 "<kml><ol suffix=\")\" start=\"3\"><li>Three</li><li>Four</li>"
                                 "</ol></kml>"));

    // Not in the middle of a paragraph, nor with the option off
    editor->fromKml(kmlOf({QStringLiteral("Text")}));
    editor->setCursorPosition({0, 4});
    type(*editor, QStringLiteral(" 1. "));
    CHECK(editor->toKml() == QStringLiteral("<kml><p>Text 1. </p></kml>"));
    AutoCorrectOptions options;
    options.bulletedLists = false;
    editor->setAutoCorrect(options);
    editor->fromKml(kmlOf({QString()}));
    type(*editor, QStringLiteral("* "));
    CHECK(editor->toKml() == QStringLiteral("<kml><p>* </p></kml>"));
}

TEST_CASE("Lists: the context menu of an item", "[editor][lists][menu]") {
    auto editor = editorWith(
        QStringLiteral("<kml><ol><li>One</li><li>Two</li></ol><ul><li>Bullet</li></ul></kml>"));
    editor->setCursorPosition({1, 0});

    QStringList texts;
    {
        PopupMenuReader reader(texts, QStringLiteral("&Restart at 1"));
        editor->showContextMenuAtCursor();
    }
    for (const char* text : {"&Restart at 1", "Contin&ue Numbering", "Set Numbering &Value...",
                             "Incr&ease Indent", "Decrea&se Indent", "Bullets and Numberin&g..."}) {
        INFO(text);
        CHECK(texts.contains(QString::fromLatin1(text)));
    }
    CHECK(markersOf(*editor)[1] == QStringLiteral("1."));

    // The windows are asked for
    int valueAsked = 0;
    int formatsAsked = 0;
    QObject::connect(editor.get(), &BookEditor::numberingValueAsked, [&valueAsked]() { ++valueAsked; });
    QObject::connect(editor.get(), &BookEditor::bulletsAndNumberingAsked,
                     [&formatsAsked]() { ++formatsAsked; });
    {
        PopupMenuReader reader(texts, QStringLiteral("Set Numbering &Value..."));
        editor->showContextMenuAtCursor();
    }
    {
        PopupMenuReader reader(texts, QStringLiteral("Bullets and Numberin&g..."));
        editor->showContextMenuAtCursor();
    }
    CHECK(valueAsked == 1);
    CHECK(formatsAsked == 1);

    // A bulleted item has no numbering
    texts.clear();
    editor->setCursorPosition({2, 0});
    {
        PopupMenuReader reader(texts, QString());
        editor->showContextMenuAtCursor();
    }
    CHECK_FALSE(texts.contains(QStringLiteral("Contin&ue Numbering")));
    CHECK(texts.contains(QStringLiteral("Incr&ease Indent")));
}

TEST_CASE("Paragraph: its own formatting from the Paragraph window", "[editor][paragraph]") {
    auto editor = editorWith(kmlOf({QStringLiteral("One"), QStringLiteral("Two")}));
    editor->selectAll();
    ParagraphFormat changes;
    changes.spaceBefore = 12.0;
    changes.keepTogether = true;
    editor->applyParagraphFormat(changes, Qt::AlignRight);
    CHECK(editor->toKml() == QStringLiteral(
                                 "<kml><p align=\"right\" space-before=\"12\" keep-together=\"true\">"
                                 "One</p><p align=\"right\" space-before=\"12\" "
                                 "keep-together=\"true\">Two</p></kml>"));
    CHECK(editor->shownParagraphFormat().spaceBefore == 12.0);

    // The value the style gives is not kept as the paragraph's own
    changes = ParagraphFormat{};
    changes.spaceBefore = 0.0;
    changes.keepTogether = false;
    editor->applyParagraphFormat(changes);
    CHECK(editor->toKml() == QStringLiteral(
                                 "<kml><p align=\"right\">One</p><p align=\"right\">Two</p></kml>"));
}

TEST_CASE("Lists: a number written in the format of its level", "[editor][lists]") {
    CHECK(listNumberFromText(QStringLiteral("12"), ListNumberFormat::Decimal) == 12);
    CHECK(listNumberFromText(QStringLiteral("c"), ListNumberFormat::LowerAlpha) == 3);
    CHECK(listNumberFromText(QStringLiteral("AA"), ListNumberFormat::UpperAlpha) == 27);
    CHECK(listNumberFromText(QStringLiteral("xiv"), ListNumberFormat::LowerRoman) == 14);
    CHECK(listNumberFromText(QStringLiteral("MCMXCIV"), ListNumberFormat::UpperRoman) == 1994);
    CHECK(listNumberFromText(QStringLiteral("7"), ListNumberFormat::LowerRoman) == 7);
    CHECK_FALSE(listNumberFromText(QStringLiteral("iiii"), ListNumberFormat::LowerRoman));
    CHECK_FALSE(listNumberFromText(QStringLiteral("ab"), ListNumberFormat::LowerAlpha));
    CHECK_FALSE(listNumberFromText(QStringLiteral("x"), ListNumberFormat::Decimal));
    CHECK_FALSE(listNumberFromText(QStringLiteral("40000"), ListNumberFormat::Decimal));
    CHECK_FALSE(listNumberFromText(QString(), ListNumberFormat::Decimal));
}
