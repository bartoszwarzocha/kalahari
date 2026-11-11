/// @file test_paragraph_styles.cpp
/// @brief Paragraph styles and the objects of the text (a horizontal line, the scene and page
///        breaks) as the writer edits them
///
/// Enter at the end of a heading starts the text, a heading split in two stays two headings,
/// a style goes on the paragraphs at once as one undo step. An object is one character in a
/// block of its own: text never joins it, Backspace and Delete take it whole.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_keys.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QTextBlock>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A style of a package
kalahari::core::PackageParagraphStyle packageStyle(const QString& id, const QString& next = {},
                                                   const QVariantMap& properties = {}) {
    kalahari::core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), id);
    style.nextStyle = next;
    style.properties = properties;
    return style;
}

/// The styles of a book: Normal, a heading followed by Normal, Dialogue followed by itself,
/// Code without spell checking
ParagraphStyleSet bookStyles() {
    return ParagraphStyleSet::fromPackage(
        {packageStyle(QStringLiteral("normal")),
         packageStyle(QStringLiteral("heading_1"), QStringLiteral("normal"),
                      {{QStringLiteral("fontSize"), 20},
                       {QStringLiteral("alignment"), QStringLiteral("center")},
                       {QStringLiteral("keepWithNext"), true}}),
         packageStyle(QStringLiteral("heading_2"), QStringLiteral("normal")),
         packageStyle(QStringLiteral("dialogue")),
         packageStyle(QStringLiteral("code"), {}, {{QStringLiteral("spellCheck"), false}})},
        QStringLiteral("en"));
}

/// An editor with the book's styles and KML loaded, the cursor at @p cursor
std::unique_ptr<BookEditor> editorWith(const QString& kml, CursorPosition cursor = {0, 0}) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->setParagraphStyles(bookStyles());
    editor->fromKml(kml);
    editor->setCursorPosition(cursor);
    return editor;
}

/// Select from @p from to @p to
void select(BookEditor& editor, CursorPosition from, CursorPosition to) {
    editor.setCursorPosition(to);
    editor.setSelection({from, to});
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

QString kml(const QString& content) {
    return QStringLiteral("<kml>") + content + QStringLiteral("</kml>");
}

}  // namespace

// =============================================================================
// Styles
// =============================================================================

TEST_CASE("Paragraph styles: the level of a heading from its style", "[editor][styles]") {
    for (int level = 1; level <= HEADING_LEVELS; ++level) {
        CHECK(headingLevelOf(headingStyleId(level)) == level);
    }
    for (const char* id : {"normal", "heading_", "heading_0", "heading_4", "heading_12",
                           "heading_x", "Heading_1", "subheading_1"}) {
        CAPTURE(id);
        CHECK(headingLevelOf(QString::fromLatin1(id)) == 0);
    }
}

TEST_CASE("Paragraph styles: Enter", "[editor][styles]") {
    SECTION("At the end of a heading the text follows it") {
        auto editor = editorWith(kml(QStringLiteral("<h1>Title</h1>")), {0, 5});
        press(*editor, Qt::Key_Return);
        type(*editor, QStringLiteral("Text"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<h1>Title</h1><p>Text</p>")));
        REQUIRE(editor->currentParagraphStyle() == QStringLiteral("normal"));
    }

    SECTION("In the middle of a heading it splits into two headings") {
        auto editor = editorWith(kml(QStringLiteral("<h1>Title</h1>")), {0, 2});
        press(*editor, Qt::Key_Return);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<h1>Ti</h1><h1>tle</h1>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("A style without a style after it goes on") {
        auto editor = editorWith(kml(QStringLiteral("<p style=\"dialogue\">A</p>")), {0, 1});
        press(*editor, Qt::Key_Return);
        type(*editor, QStringLiteral("B"));
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p style=\"dialogue\">A</p><p style=\"dialogue\">B</p>")));
    }

    SECTION("The text after a heading has no alignment of the heading's") {
        auto editor = editorWith(kml(QStringLiteral("<h1 align=\"right\">Title</h1>")), {0, 5});
        press(*editor, Qt::Key_Return);
        type(*editor, QStringLiteral("Text"));
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<h1 align=\"right\">Title</h1><p>Text</p>")));
    }

    SECTION("One undo step takes the new paragraph back") {
        auto editor = editorWith(kml(QStringLiteral("<h1>Title</h1>")), {0, 5});
        press(*editor, Qt::Key_Return);
        editor->undo();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<h1>Title</h1>")));
    }
}

TEST_CASE("Paragraph styles: putting paragraphs in a style", "[editor][styles]") {
    SECTION("The paragraph at the cursor") {
        auto editor = editorWith(kml(QStringLiteral("<p>A</p><p>B</p>")), {1, 0});
        editor->applyParagraphStyle(QStringLiteral("heading_2"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>A</p><h2>B</h2>")));
        REQUIRE(editor->currentParagraphStyle() == QStringLiteral("heading_2"));
    }

    SECTION("The selected paragraphs, the objects among them staying, in one undo step") {
        auto editor = editorWith(kml(QStringLiteral("<p>A</p><hr/><p>B</p><p>C</p>")));
        select(*editor, {0, 0}, {2, 1});
        editor->applyParagraphStyle(QStringLiteral("dialogue"));
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p style=\"dialogue\">A</p><hr/>"
                                   "<p style=\"dialogue\">B</p><p>C</p>")));
        editor->undo();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>A</p><hr/><p>B</p><p>C</p>")));
    }

    SECTION("Normal takes the style off") {
        auto editor = editorWith(kml(QStringLiteral("<p style=\"dialogue\">A</p>")));
        editor->applyParagraphStyle(QStringLiteral("normal"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>A</p>")));
    }

    SECTION("A paragraph already in the style: no undo step") {
        auto editor = editorWith(kml(QStringLiteral("<h1>A</h1>")));
        editor->applyParagraphStyle(QStringLiteral("heading_1"));
        REQUIRE_FALSE(editor->canUndo());
    }

    SECTION("The style at the cursor: also one the book does not have, none at an object") {
        auto editor = editorWith(kml(QStringLiteral("<p style=\"poem\">A</p><hr/>")));
        REQUIRE(editor->currentParagraphStyle() == QStringLiteral("poem"));
        editor->setCursorPosition({1, 0});
        REQUIRE(editor->currentParagraphStyle().isEmpty());
    }

    SECTION("The alignment of a heading's style is the paragraph's alignment") {
        auto editor = editorWith(kml(QStringLiteral("<h1>A</h1><p>B</p>")));
        REQUIRE(editor->currentAlignment() == Qt::AlignHCenter);
        editor->setAlignRight();
        REQUIRE(editor->currentAlignment() == Qt::AlignRight);
    }
}

TEST_CASE("Paragraph styles: a style the book does not have shows as Normal",
          "[editor][styles]") {
    auto editor = editorWith(kml(QStringLiteral("<p>A</p><p style=\"poem\">B</p>")));
    const auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    const QTextBlock normal = editor->textDocument()->findBlockByNumber(0);
    const QTextBlock unknown = editor->textDocument()->findBlockByNumber(1);
    REQUIRE(layout->blockMetrics(unknown).charFormat == layout->blockMetrics(normal).charFormat);
    REQUIRE(layout->blockMetrics(unknown).spaceBefore == layout->blockMetrics(normal).spaceBefore);
    REQUIRE(editor->toKml() == kml(QStringLiteral("<p>A</p><p style=\"poem\">B</p>")));
}

TEST_CASE("Paragraph styles: the spelling of Code is not checked", "[editor][styles]") {
    const ParagraphStyleSet styles = bookStyles();
    REQUIRE(styles.isChecked(QStringLiteral("normal")));
    REQUIRE(styles.isChecked(QStringLiteral("poem")));
    REQUIRE_FALSE(styles.isChecked(QStringLiteral("code")));
}

TEST_CASE("Paragraph styles: the book's own styles and the styles of its type it changes",
          "[editor][styles]") {
    using kalahari::core::BookParagraphStyle;
    const QList<kalahari::core::PackageParagraphStyle> type = {
        packageStyle(QStringLiteral("normal"), {},
                     {{QStringLiteral("fontSize"), 12}, {QStringLiteral("firstLineIndent"), 18}}),
        [] {
            auto quote = packageStyle(QStringLiteral("quote"), {},
                                      {{QStringLiteral("italic"), true},
                                       {QStringLiteral("leftMargin"), 36},
                                       {QStringLiteral("spaceBefore"), 6}});
            quote.baseStyle = QStringLiteral("normal");
            return quote;
        }(),
        packageStyle(QStringLiteral("code"), {}, {{QStringLiteral("spellCheck"), false}})};

    // Without entries of the book: the type's styles, as fromPackage() gives them
    const ParagraphStyleSet plain = ParagraphStyleSet::forBook(type, {}, QStringLiteral("en"));
    CHECK(plain == ParagraphStyleSet::fromPackage(type, QStringLiteral("en")));
    REQUIRE(plain.find(QStringLiteral("quote")) != nullptr);
    CHECK(plain.find(QStringLiteral("quote"))->origin == StyleOrigin::Type);
    CHECK(plain.find(QStringLiteral("quote"))->baseStyle == QStringLiteral("normal"));
    CHECK(plain.find(QStringLiteral("quote"))->ownProperties.size() == 3);
    CHECK(plain.property(QStringLiteral("quote"), QStringLiteral("fontSize")).toInt() == 12);

    // Quote changed in the book: its own properties and next style are the book's, its name
    // and base the type's
    BookParagraphStyle quote;
    quote.id = QStringLiteral("quote");
    quote.name = QStringLiteral("Ignored");
    quote.nextStyle = QStringLiteral("normal");
    quote.properties = {{QStringLiteral("italic"), true}, {QStringLiteral("spaceBefore"), 12}};
    // The writer's own style on top of Quote, and one whose base the book does not have
    BookParagraphStyle memory;
    memory.id = QStringLiteral("wspomnienie");
    memory.name = QStringLiteral("Wspomnienie");
    memory.baseStyle = QStringLiteral("quote");
    memory.properties = {{QStringLiteral("leftMargin"), 18}};
    BookParagraphStyle orphan;
    orphan.id = QStringLiteral("sen");
    orphan.name = QStringLiteral("Sen");
    orphan.baseStyle = QStringLiteral("poem");
    orphan.properties = {{QStringLiteral("alignment"), QStringLiteral("center")}};

    const ParagraphStyleSet set =
        ParagraphStyleSet::forBook(type, {quote, memory, orphan}, QStringLiteral("pl"));
    REQUIRE(set.styles().size() == 5);
    CHECK(set.styles().at(3).id == QStringLiteral("wspomnienie"));  // after the type's
    CHECK(set.styles().at(4).id == QStringLiteral("sen"));

    const ParagraphStyle* changed = set.find(QStringLiteral("quote"));
    REQUIRE(changed != nullptr);
    CHECK(changed->origin == StyleOrigin::Changed);
    CHECK(changed->name == QStringLiteral("quote"));
    CHECK(changed->baseStyle == QStringLiteral("normal"));
    CHECK(changed->ownProperties == quote.properties);
    CHECK(changed->properties.value(QStringLiteral("spaceBefore")).toInt() == 12);
    CHECK_FALSE(changed->properties.contains(QStringLiteral("leftMargin")));  // replaced
    CHECK(changed->properties.value(QStringLiteral("fontSize")).toInt() == 12);  // from Normal
    CHECK(set.nextStyleOf(QStringLiteral("quote")) == QStringLiteral("normal"));

    const ParagraphStyle* own = set.find(QStringLiteral("wspomnienie"));
    REQUIRE(own != nullptr);
    CHECK(own->origin == StyleOrigin::Own);
    CHECK(own->name == QStringLiteral("Wspomnienie"));  // the writer's name in any language
    CHECK(own->baseStyle == QStringLiteral("quote"));
    CHECK(own->properties.value(QStringLiteral("leftMargin")).toInt() == 18);
    CHECK(own->properties.value(QStringLiteral("spaceBefore")).toInt() == 12);  // the book's Quote
    CHECK(own->properties.value(QStringLiteral("italic")).toBool());
    CHECK(set.nextStyleOf(QStringLiteral("wspomnienie")) == QStringLiteral("wspomnienie"));

    const ParagraphStyle* lost = set.find(QStringLiteral("sen"));
    REQUIRE(lost != nullptr);
    CHECK(lost->baseStyle.isEmpty());
    CHECK(lost->properties == orphan.properties);
    CHECK(set.find(QStringLiteral("code"))->origin == StyleOrigin::Type);
    CHECK(set.normal().origin == StyleOrigin::Type);
}

TEST_CASE("Paragraph styles: an editor shows a style of the book changed at once",
          "[editor][styles]") {
    auto editor = editorWith(kml(QStringLiteral("<p>A</p><p style=\"heading_1\">B</p>")));
    const auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    const QTextBlock heading = editor->textDocument()->findBlockByNumber(1);
    const qreal before = layout->blockMetrics(heading).charFormat.fontPointSize();

    int changes = 0;
    QObject::connect(editor.get(), &BookEditor::paragraphStylesChanged, [&changes]() { ++changes; });
    kalahari::core::BookParagraphStyle bigger;
    bigger.id = QStringLiteral("heading_1");
    bigger.nextStyle = QStringLiteral("normal");
    bigger.properties = {{QStringLiteral("fontSize"), 30}};
    const QList<kalahari::core::PackageParagraphStyle> type = {
        packageStyle(QStringLiteral("normal")),
        packageStyle(QStringLiteral("heading_1"), QStringLiteral("normal"),
                     {{QStringLiteral("fontSize"), 20}})};
    editor->setParagraphStyles(ParagraphStyleSet::forBook(type, {bigger}, QStringLiteral("en")));
    CHECK(changes == 1);
    CHECK(layout->blockMetrics(heading).charFormat.fontPointSize() > before);

    // The same styles again change nothing
    editor->setParagraphStyles(ParagraphStyleSet::forBook(type, {bigger}, QStringLiteral("en")));
    CHECK(changes == 1);
}

TEST_CASE("Paragraph styles: the paragraphs of a style go to another, open or saved",
          "[editor][styles]") {
    // Written by the serializer, as a saved chapter has it
    const QString source = editorWith(kml(QStringLiteral(
        "<p>A</p><p style=\"sen\">B &amp; b</p><p style=\"sen\" align=\"center\">C</p>"
        "<h1>D</h1><p style=\"quote\">E</p><ul><li style=\"sen\">F</li><li>G</li></ul>"
        "<p style=\"sen\"><b>H</b></p><hr/>")))->toKml();

    const QHash<QString, int> counts = paragraphStyleCounts(source);
    CHECK(counts.value(QStringLiteral("sen")) == 4);
    CHECK(counts.value(QStringLiteral("normal")) == 2);  // A and G; the line is no paragraph
    CHECK(counts.value(QStringLiteral("heading_1")) == 1);
    CHECK(counts.value(QStringLiteral("quote")) == 1);

    // A saved text changes as an open one does, character for character
    const auto check = [&source](const QString& from, const QString& to, int expected) {
        INFO(from.toStdString() << " > " << to.toStdString());
        auto editor = editorWith(source);
        CHECK(editor->replaceParagraphStyle(from, to) == expected);
        int count = -1;
        const QString saved = replaceParagraphStyleInKml(source, from, to, &count);
        CHECK(count == expected);
        CHECK(saved == editor->toKml());
        CHECK(paragraphStyleCounts(saved).value(from) == 0);
    };
    check(QStringLiteral("sen"), QStringLiteral("quote"), 4);
    check(QStringLiteral("sen"), QStringLiteral("normal"), 4);
    check(QStringLiteral("sen"), QString(), 4);  // no style: Normal
    check(QStringLiteral("sen"), QStringLiteral("heading_2"), 4);
    check(QStringLiteral("heading_1"), QStringLiteral("quote"), 1);
    check(QStringLiteral("heading_1"), QStringLiteral("heading_3"), 1);

    // Nothing to change: the text as it was, and no undo step
    CHECK(replaceParagraphStyleInKml(source, QStringLiteral("poem"), QStringLiteral("quote")) == source);
    CHECK(replaceParagraphStyleInKml(source, QStringLiteral("sen"), QStringLiteral("sen")) == source);
    auto editor = editorWith(source);
    CHECK(editor->replaceParagraphStyle(QStringLiteral("poem"), QStringLiteral("quote")) == 0);
    CHECK_FALSE(editor->canUndo());

    // In an editor one undo step brings the paragraphs back
    REQUIRE(editor->replaceParagraphStyle(QStringLiteral("sen"), QStringLiteral("quote")) == 4);
    editor->undo();
    CHECK(editor->toKml() == source);
}

TEST_CASE("Paragraph styles: a style of the book deleted, restored or saved",
          "[editor][styles]") {
    using kalahari::core::BookParagraphStyle;
    auto quote = packageStyle(QStringLiteral("quote"), {},
                              {{QStringLiteral("italic"), true},
                               {QStringLiteral("leftMargin"), 36}});
    quote.baseStyle = QStringLiteral("normal");
    const QList<kalahari::core::PackageParagraphStyle> type = {
        packageStyle(QStringLiteral("normal"), {}, {{QStringLiteral("fontSize"), 12}}), quote};

    // Memory on Quote; Dream and Echo on Memory, Echo after it; Quote changed to go on to it
    BookParagraphStyle changedQuote;
    changedQuote.id = QStringLiteral("quote");
    changedQuote.nextStyle = QStringLiteral("memory");
    changedQuote.properties = {{QStringLiteral("italic"), true}};
    BookParagraphStyle memory;
    memory.id = QStringLiteral("memory");
    memory.name = QStringLiteral("Memory");
    memory.baseStyle = QStringLiteral("quote");
    memory.properties = {{QStringLiteral("leftMargin"), 18}, {QStringLiteral("spaceAfter"), 6}};
    BookParagraphStyle dream;
    dream.id = QStringLiteral("dream");
    dream.name = QStringLiteral("Dream");
    dream.baseStyle = QStringLiteral("memory");
    dream.nextStyle = QStringLiteral("memory");
    dream.properties = {{QStringLiteral("spaceAfter"), 12}};
    BookParagraphStyle echo;
    echo.id = QStringLiteral("echo");
    echo.name = QStringLiteral("Echo");
    echo.baseStyle = QStringLiteral("dream");
    const QList<BookParagraphStyle> entries = {changedQuote, memory, dream, echo};
    const ParagraphStyleSet before = ParagraphStyleSet::forBook(type, entries, QStringLiteral("en"));

    // The paragraphs of a deleted style go to its base, Normal without one
    CHECK(successorOf(before, QStringLiteral("memory")) == QStringLiteral("quote"));
    CHECK(successorOf(before, QStringLiteral("quote")) == QStringLiteral("normal"));
    CHECK(successorOf(before, QStringLiteral("none")) == QStringLiteral("normal"));

    // Memory deleted: Dream is on Quote with Memory's properties under its own, and looks as
    // it did; Echo, on Dream, too; what went on to Memory goes on to Quote
    const QList<BookParagraphStyle> without = withoutOwnStyle(entries, before, QStringLiteral("memory"));
    REQUIRE(without.size() == 3);
    CHECK(without.at(0).nextStyle == QStringLiteral("quote"));
    CHECK(without.at(1).id == QStringLiteral("dream"));
    CHECK(without.at(1).baseStyle == QStringLiteral("quote"));
    CHECK(without.at(1).nextStyle == QStringLiteral("quote"));
    CHECK(without.at(1).properties.value(QStringLiteral("leftMargin")).toInt() == 18);
    CHECK(without.at(1).properties.value(QStringLiteral("spaceAfter")).toInt() == 12);
    CHECK(without.at(2) == echo);
    const ParagraphStyleSet after = ParagraphStyleSet::forBook(type, without, QStringLiteral("en"));
    CHECK_FALSE(after.contains(QStringLiteral("memory")));
    CHECK(after.find(QStringLiteral("dream"))->properties ==
          before.find(QStringLiteral("dream"))->properties);
    CHECK(after.find(QStringLiteral("echo"))->properties ==
          before.find(QStringLiteral("echo"))->properties);
    CHECK(withoutOwnStyle(entries, before, QStringLiteral("none")) == entries);

    // Quote restored: its entry goes, the rest stays
    const QList<BookParagraphStyle> restored = withoutStyleEntry(entries, QStringLiteral("quote"));
    CHECK(restored == QList<BookParagraphStyle>{memory, dream, echo});
    CHECK(ParagraphStyleSet::forBook(type, restored, QStringLiteral("en"))
              .find(QStringLiteral("quote"))
              ->origin == StyleOrigin::Type);

    // An entry saved takes the place of its id, or goes after the others
    BookParagraphStyle bigger = dream;
    bigger.properties = {{QStringLiteral("fontSize"), 14}};
    const QList<BookParagraphStyle> saved = withStyleEntry(entries, bigger);
    CHECK(saved == QList<BookParagraphStyle>{changedQuote, memory, bigger, echo});
    BookParagraphStyle fresh;
    fresh.id = QStringLiteral("fresh");
    CHECK(withStyleEntry(entries, fresh).constLast() == fresh);

    // A style is based on none of itself and the styles on it, at any depth
    CHECK(possibleBaseStyles(before, QStringLiteral("memory")) ==
          QStringList{QStringLiteral("normal"), QStringLiteral("quote")});
    CHECK(possibleBaseStyles(before, QStringLiteral("echo")).size() == 4);
    CHECK(possibleBaseStyles(before, QString()).size() == 5);
}

TEST_CASE("Paragraph styles: the id of a new style of the writer's", "[editor][styles]") {
    CHECK(paragraphStyleIdFor(QStringLiteral("Wiadomość tekstowa"), {}) ==
          QStringLiteral("wiadomosc_tekstowa"));
    CHECK(paragraphStyleIdFor(QStringLiteral("  Łódź – Żółw!  "), {}) == QStringLiteral("lodz_zolw"));
    CHECK(paragraphStyleIdFor(QStringLiteral("Straße 2"), {}) == QStringLiteral("strasse_2"));
    CHECK(paragraphStyleIdFor(QStringLiteral("…"), {}) == QStringLiteral("style"));
    CHECK(paragraphStyleIdFor(QStringLiteral("Quote"), {QStringLiteral("quote")}) ==
          QStringLiteral("quote_2"));
    CHECK(paragraphStyleIdFor(QStringLiteral("Quote"),
                              {QStringLiteral("quote"), QStringLiteral("quote_2")}) ==
          QStringLiteral("quote_3"));
}

TEST_CASE("Paragraph styles: a new style takes the formatting of the paragraph",
          "[editor][styles]") {
    auto editor = editorWith(kml(QStringLiteral(
        "<p indent-left=\"18\" space-after=\"6\" line-height=\"1.2\" align=\"center\">"
        "<i>All of it in italics, <b>part in bold</b></i></p><p>B</p>")));
    const QVariantMap own = editor->ownStyleProperties();
    CHECK(own.value(QStringLiteral("leftMargin")).toDouble() == 18.0);
    CHECK(own.value(QStringLiteral("spaceAfter")).toDouble() == 6.0);
    CHECK(own.value(QStringLiteral("lineHeight")).toDouble() == 1.2);
    CHECK(own.value(QStringLiteral("alignment")).toString() == QStringLiteral("center"));
    CHECK(own.value(QStringLiteral("italic")).toBool());
    CHECK_FALSE(own.contains(QStringLiteral("bold")));  // only a part has it
    CHECK_FALSE(own.contains(QStringLiteral("rightMargin")));
    editor->setCursorPosition({1, 0});
    CHECK(editor->ownStyleProperties().isEmpty());

    // The paragraph in the new style loses what the style took: one undo step
    editor->setCursorPosition({0, 0});
    const QString before = editor->toKml();
    editor->applyNewParagraphStyle(QStringLiteral("dialogue"),
                                   {QStringLiteral("leftMargin"), QStringLiteral("italic"),
                                    QStringLiteral("alignment")});
    CHECK(editor->currentParagraphStyle() == QStringLiteral("dialogue"));
    const QVariantMap left = editor->ownStyleProperties();
    CHECK_FALSE(left.contains(QStringLiteral("leftMargin")));
    CHECK_FALSE(left.contains(QStringLiteral("italic")));
    CHECK_FALSE(left.contains(QStringLiteral("alignment")));
    CHECK(left.value(QStringLiteral("spaceAfter")).toDouble() == 6.0);  // not taken
    CHECK(editor->toKml().contains(QStringLiteral("<b>part in bold</b>")));
    CHECK(editor->paragraphStyleCounts().value(QStringLiteral("dialogue")) == 1);
    CHECK(editor->paragraphStyleCounts().value(QStringLiteral("normal")) == 1);
    editor->undo();
    CHECK(editor->toKml() == before);
}

// =============================================================================
// Objects
// =============================================================================

TEST_CASE("Objects: inserting", "[editor][objects]") {
    SECTION("In the middle of a paragraph: it splits, the cursor goes to the text after") {
        auto editor = editorWith(kml(QStringLiteral("<p align=\"center\">ab<b>cd</b></p>")), {0, 2});
        editor->insertTextObject(TextObject::HorizontalRule);
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p align=\"center\">ab</p><hr/>"
                                   "<p align=\"center\"><b>cd</b></p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("At the end of a paragraph: a new one follows, in the style after it") {
        auto editor = editorWith(kml(QStringLiteral("<h1>Title</h1>")), {0, 5});
        editor->insertTextObject(TextObject::PageBreak);
        type(*editor, QStringLiteral("Text"));
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<h1>Title</h1><page-break/><p>Text</p>")));
    }

    SECTION("At the start of a paragraph: before it") {
        auto editor = editorWith(kml(QStringLiteral("<h2>abcd</h2>")));
        editor->insertTextObject(TextObject::SceneBreak);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<scene-break/><h2>abcd</h2>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("In an empty paragraph: it stays after the object, with the cursor") {
        auto editor = editorWith(kml(QStringLiteral("<p>a</p><p></p><p>b</p>")), {1, 0});
        editor->insertTextObject(TextObject::SceneBreak);
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p>a</p><scene-break/><p></p><p>b</p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("Before and after another object") {
        auto editor = editorWith(kml(QStringLiteral("<hr/>")));
        editor->insertTextObject(TextObject::SceneBreak);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<scene-break/><hr/>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
        editor->setCursorPosition({1, 1});
        editor->insertTextObject(TextObject::PageBreak);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<scene-break/><hr/><page-break/>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 1});
    }

    SECTION("In place of the selection, one undo step") {
        auto editor = editorWith(kml(QStringLiteral("<p>abcdef</p>")));
        select(*editor, {0, 2}, {0, 4});
        editor->insertTextObject(TextObject::HorizontalRule);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><hr/><p>ef</p>")));
        editor->undo();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>abcdef</p>")));
    }
}

TEST_CASE("Objects: text never joins them", "[editor][objects]") {
    SECTION("Typed after an object: a new paragraph") {
        auto editor = editorWith(kml(QStringLiteral("<p>a</p><hr/>")), {1, 1});
        type(*editor, QStringLiteral("xy"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><hr/><p>xy</p>")));
    }

    SECTION("Typed before an object: a new paragraph before it") {
        auto editor = editorWith(kml(QStringLiteral("<h1>T</h1><hr/><p>b</p>")), {1, 0});
        type(*editor, QStringLiteral("x"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<h1>T</h1><p>x</p><hr/><p>b</p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 1});
    }

    SECTION("Typed over a selection that ends before an object") {
        auto editor = editorWith(kml(QStringLiteral("<p>ab</p><hr/>")));
        select(*editor, {0, 0}, {1, 0});
        type(*editor, QStringLiteral("x"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>x</p><hr/>")));
    }

    SECTION("Enter before an object: an empty paragraph before it, the cursor stays") {
        auto editor = editorWith(kml(QStringLiteral("<p>a</p><hr/>")), {1, 0});
        press(*editor, Qt::Key_Return);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><p></p><hr/>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("Enter or Shift+Enter after an object: a new paragraph after it") {
        const auto modifiers = GENERATE(Qt::NoModifier, Qt::ShiftModifier);
        auto editor = editorWith(kml(QStringLiteral("<p>a</p><hr/>")), {1, 1});
        press(*editor, Qt::Key_Return, modifiers);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><hr/><p></p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("An object pasted into a paragraph gets a block of its own") {
        auto editor = editorWith(kml(QStringLiteral("<p>abcd</p>")), {0, 2});
        QMimeData data;
        data.setData(QString::fromLatin1(MIME_KML),
                     kml(QStringLiteral("<p>x</p><hr/><p>y</p>")).toUtf8());
        editor->insertFromMimeData(&data);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>abx</p><hr/><p>ycd</p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 1});

        QMimeData object;
        object.setData(QString::fromLatin1(MIME_KML), kml(QStringLiteral("<hr/>")).toUtf8());
        editor->setCursorPosition({0, 1});
        editor->insertFromMimeData(&object);
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p>a</p><hr/><p>bx</p><hr/><p>ycd</p>")));
        editor->undo();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>abx</p><hr/><p>ycd</p>")));
    }

    SECTION("Pasted next to an object: a new paragraph") {
        auto editor = editorWith(kml(QStringLiteral("<hr/>")), {0, 1});
        QMimeData data;
        data.setText(QStringLiteral("text"));
        editor->insertFromMimeData(&data);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<hr/><p>text</p>")));
    }
}

TEST_CASE("Objects: deleting them as one character", "[editor][objects]") {
    const QString text = kml(QStringLiteral("<p>a</p><hr/><p>b</p>"));
    const QString without = kml(QStringLiteral("<p>a</p><p>b</p>"));

    SECTION("Backspace after the object") {
        auto editor = editorWith(text, {1, 1});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == without);
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
        editor->undo();
        REQUIRE(editor->toKml() == text);
    }

    SECTION("Backspace at the start of the paragraph after it") {
        auto editor = editorWith(text, {2, 0});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == without);
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("Delete before the object") {
        auto editor = editorWith(text, {1, 0});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == without);
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("Delete at the end of the paragraph before it") {
        auto editor = editorWith(text, {0, 1});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == without);
        REQUIRE(editor->cursorPosition() == CursorPosition{0, 1});
    }

    SECTION("Between an object and text the keys go over the paragraph break") {
        auto editor = editorWith(text, {1, 0});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == text);
        REQUIRE(editor->cursorPosition() == CursorPosition{0, 1});
        editor->setCursorPosition({1, 1});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == text);
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("An empty paragraph next to an object goes before the object") {
        auto after = editorWith(kml(QStringLiteral("<hr/><p></p>")), {1, 0});
        press(*after, Qt::Key_Backspace);
        REQUIRE(after->toKml() == kml(QStringLiteral("<hr/>")));
        REQUIRE(after->cursorPosition() == CursorPosition{0, 1});

        auto before = editorWith(kml(QStringLiteral("<p></p><hr/>")));
        press(*before, Qt::Key_Delete);
        REQUIRE(before->toKml() == kml(QStringLiteral("<hr/>")));
        REQUIRE(before->cursorPosition() == CursorPosition{0, 0});

        auto fromObject = editorWith(kml(QStringLiteral("<p></p><hr/>")), {1, 0});
        press(*fromObject, Qt::Key_Backspace);
        REQUIRE(fromObject->toKml() == kml(QStringLiteral("<hr/>")));
        REQUIRE(fromObject->cursorPosition() == CursorPosition{0, 0});
    }

    SECTION("The only object of the text leaves an empty paragraph") {
        auto editor = editorWith(kml(QStringLiteral("<page-break/>")), {0, 1});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p></p>")));
    }
}

TEST_CASE("Objects: the arrows pass them as one character", "[editor][objects]") {
    auto editor = editorWith(kml(QStringLiteral("<p>ab</p><hr/><p>cd</p>")), {0, 2});
    press(*editor, Qt::Key_Right);
    REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    press(*editor, Qt::Key_Right);
    REQUIRE(editor->cursorPosition() == CursorPosition{1, 1});
    press(*editor, Qt::Key_Right);
    REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    press(*editor, Qt::Key_Left);
    press(*editor, Qt::Key_Left);
    REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});

    // Shift with an arrow selects it; Delete takes it whole
    press(*editor, Qt::Key_Right, Qt::ShiftModifier);
    REQUIRE(editor->hasSelection());
    REQUIRE(editor->selection().normalized().start == CursorPosition{1, 0});
    REQUIRE(editor->selection().normalized().end == CursorPosition{1, 1});
    press(*editor, Qt::Key_Delete);
    REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><p>cd</p>")));
    editor->undo();
    REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><hr/><p>cd</p>")));
}

TEST_CASE("Objects: a selection of whole objects", "[editor][objects]") {
    const QString text = kml(QStringLiteral("<p>ab</p><hr/><page-break/><h2>cd</h2>"));

    SECTION("Deleted: their paragraphs go too, the cursor goes to the text after them") {
        auto editor = editorWith(text);
        select(*editor, {1, 0}, {2, 1});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><h2>cd</h2>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("Deleted at the end of the text: the cursor goes to the text before them") {
        auto editor = editorWith(kml(QStringLiteral("<p>ab</p><scene-break/>")));
        select(*editor, {1, 0}, {1, 1});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{0, 2});
    }

    SECTION("Text typed over them takes their place") {
        auto editor = editorWith(text);
        select(*editor, {1, 0}, {2, 1});
        type(*editor, QStringLiteral("x"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><p>x</p><h2>cd</h2>")));
    }
}

TEST_CASE("Objects: selections that end at them", "[editor][objects]") {
    const QString text = kml(QStringLiteral("<p>ab</p><hr/><h2>cd</h2>"));

    SECTION("Deleted from text to before the object: the object stays") {
        auto editor = editorWith(text);
        select(*editor, {0, 1}, {1, 0});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><hr/><h2>cd</h2>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{0, 1});
    }

    SECTION("Deleted from after the object into text: the object stays") {
        auto editor = editorWith(text);
        select(*editor, {1, 1}, {2, 1});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><hr/><h2>d</h2>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("Deleted from before the object: the text keeps its paragraph's style") {
        auto editor = editorWith(text);
        select(*editor, {1, 0}, {2, 1});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><h2>d</h2>")));
    }

    SECTION("Cut from text to before the object: the object stays") {
        auto editor = editorWith(text);
        select(*editor, {0, 1}, {1, 0});
        editor->cut();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><hr/><h2>cd</h2>")));
    }
}

TEST_CASE("Objects: they have no text", "[editor][objects]") {
    SECTION("Copied: the KML keeps the object, the plain text a line for it, not its mark") {
        auto editor = editorWith(kml(QStringLiteral("<p>ab</p><hr/><p>cd</p>")));
        select(*editor, {0, 0}, {2, 2});
        const std::unique_ptr<QMimeData> data = editor->createMimeDataFromSelection();
        REQUIRE(data != nullptr);
        REQUIRE(data->text() ==
                QStringLiteral("ab\n") + QString(10, QChar(0x2500)) + QStringLiteral("\ncd"));
        REQUIRE(QString::fromUtf8(data->data(QString::fromLatin1(MIME_KML))) ==
                kml(QStringLiteral("<p>ab</p><hr/><p>cd</p>")));
    }

    SECTION("The mark of an object in plain text is left out") {
        auto editor = editorWith(kml(QStringLiteral("<p>ab</p><hr/>")), {0, 1});
        editor->insertText(QStringLiteral("x\uFFFCy"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>axyb</p><hr/>")));
        REQUIRE(editor->paragraphPlainText(0) == QStringLiteral("axyb"));

        QMimeData data;
        data.setText(QString(QChar(QChar::ObjectReplacementCharacter)));
        editor->setCursorPosition({1, 1});
        editor->insertFromMimeData(&data);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>axyb</p><hr/>")));
        REQUIRE(editor->paragraphCount() == 2);
    }

    SECTION("Killed with Control+K and typed back with Control+Y (macOS)") {
        auto editor = editorWith(kml(QStringLiteral("<p>ab</p><hr/><p>cd</p>")), {1, 0});
        editor->performTextKey(TextKeyAction::KillToParagraphEnd);
        const QString killed = editor->toKml();
        editor->setCursorPosition({0, 1});
        editor->performTextKey(TextKeyAction::Yank);
        INFO(killed.toStdString() + " / " + editor->toKml().toStdString());
        REQUIRE_FALSE(editor->plainText().contains(QChar(QChar::ObjectReplacementCharacter)));
        REQUIRE(editor->toKml() == killed);
    }
}

TEST_CASE("Objects: selecting a word and a paragraph", "[editor][objects]") {
    auto editor = editorWith(kml(QStringLiteral("<p>one two</p>")), {0, 5});
    editor->selectWord();
    REQUIRE(editor->selectedText() == QStringLiteral("two"));
    editor->selectParagraph();
    REQUIRE(editor->selectedText() == QStringLiteral("one two"));
}
