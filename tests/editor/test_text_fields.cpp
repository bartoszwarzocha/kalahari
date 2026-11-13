/// @file test_text_fields.cpp
/// @brief The fields of the book's title, its author, the text's title, the page, the words and
///        the date: values, formats, KML, the layout, editing and the clipboard

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_fields.h>
#include "editor_test_utils.h"

#include <QMimeData>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QXmlStreamReader>
#include <memory>

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

KalahariTextDocumentLayout* layoutOf(BookEditor& editor) {
    return qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
}

/// The field of the attributes of an element written as KML
std::optional<TextField> fieldOfElement(const QString& element) {
    QXmlStreamReader reader(element);
    while (!reader.atEnd() && reader.readNext() != QXmlStreamReader::StartElement) {
    }
    return textFieldOfKml(reader.attributes());
}

/// The texts every field of the editor shows, in text order, parted by "|"
std::string shownFields(BookEditor& editor) {
    const QTextDocument& document = *editor.textDocument();
    QStringList texts;
    for (int position = 0; position + 1 < document.characterCount(); ++position) {
        if (const QString text = editor.fieldTextAt(position); !text.isEmpty()) {
            texts << text;
        }
    }
    return texts.join(QLatin1Char('|')).toStdString();
}

/// The width of the field at a position, as the layout made its line
qreal fieldWidth(BookEditor& editor, int position) {
    KalahariTextDocumentLayout* layout = layoutOf(editor);
    layout->layoutPendingBlocks();
    const QTextBlock block = editor.textDocument()->findBlock(position);
    const QTextLayout* text = block.layout();
    REQUIRE(text != nullptr);
    const QTextLine line = text->lineForTextPosition(position - block.position());
    REQUIRE(line.isValid());
    return line.cursorToX(position - block.position() + 1) -
           line.cursorToX(position - block.position());
}

/// A book's context: a title, an author, the text's title and the time
FieldContext bookContext(const QString& language = QStringLiteral("pl")) {
    FieldContext context;
    context.bookTitle = QStringLiteral("Mgła nad Narwią");
    context.author = QStringLiteral("Anna Nowak");
    context.textTitle = QStringLiteral("Rozdział pierwszy");
    context.language = language;
    context.now = QDateTime(QDate(2026, 10, 11), QTime(14, 35));
    return context;
}

}  // namespace

// =============================================================================
// Values and formats
// =============================================================================

TEST_CASE("Fields: the date in the language of the book", "[editor][fields]") {
    const QDateTime when(QDate(2026, 10, 11), QTime(14, 35));
    const QString pl = QStringLiteral("pl");
    CHECK(dateText(when, QStringLiteral("long"), pl) == QStringLiteral("11 października 2026"));
    CHECK(dateText(when, QStringLiteral("short"), pl) == QStringLiteral("11.10.2026"));
    CHECK(dateText(when, QStringLiteral("full"), pl) ==
          QStringLiteral("niedziela, 11 października 2026"));
    CHECK(dateText(when, QStringLiteral("iso"), pl) == QStringLiteral("2026-10-11"));
    CHECK(dateText(when, QStringLiteral("time"), pl) == QStringLiteral("14:35"));
    CHECK(dateText(when, QStringLiteral("date-time"), pl) == QStringLiteral("11.10.2026, 14:35"));

    // In English, and in German with its own short year turned into four digits
    const QString en = QStringLiteral("en-US");
    CHECK(dateText(when, QStringLiteral("long"), en) == QStringLiteral("October 11, 2026"));
    CHECK(dateText(when, QStringLiteral("short"), en) == QStringLiteral("10/11/2026"));
    CHECK(dateText(when, QStringLiteral("time"), en) == QStringLiteral("2:35 PM"));
    CHECK(datePattern(QStringLiteral("short"), QStringLiteral("de")) ==
          QStringLiteral("dd.MM.yyyy"));
    CHECK(dateText(when, QStringLiteral("long"), QStringLiteral("de")) ==
          QStringLiteral("11. Oktober 2026"));
}

TEST_CASE("Fields: each type shows its value in its format", "[editor][fields]") {
    const FieldContext context = bookContext();
    FieldCounts counts;
    counts.words = 12345;
    const auto text = [&](FieldType type, const QString& format) {
        return textFieldText({type, format}, context, counts);
    };
    CHECK(text(FieldType::BookTitle, QString()) == QStringLiteral("Mgła nad Narwią"));
    CHECK(text(FieldType::BookTitle, QStringLiteral("upper")) == QStringLiteral("MGŁA NAD NARWIĄ"));
    CHECK(text(FieldType::Author, QString()) == QStringLiteral("Anna Nowak"));
    CHECK(text(FieldType::TextTitle, QString()) == QStringLiteral("Rozdział pierwszy"));
    CHECK(text(FieldType::Words, QString()) == QStringLiteral("12 345"));
    CHECK(text(FieldType::Words, QStringLiteral("plain")) == QStringLiteral("12345"));
    CHECK(text(FieldType::Date, QString()) == QStringLiteral("11 października 2026"));

    // The page and the pages: "#" without pages, else the numbers in their format
    CHECK(text(FieldType::Page, QString()) == QStringLiteral("#"));
    CHECK(text(FieldType::Pages, QStringLiteral("I")) == QStringLiteral("#"));
    counts.page = 4;
    counts.pages = 12;
    CHECK(text(FieldType::Page, QString()) == QStringLiteral("4"));
    CHECK(text(FieldType::Page, QStringLiteral("i")) == QStringLiteral("iv"));
    CHECK(text(FieldType::Pages, QStringLiteral("I")) == QStringLiteral("XII"));

    // A value the book does not have: the field's name
    FieldContext empty = context;
    empty.author.clear();
    CHECK(textFieldText({FieldType::Author, QString()}, empty, counts) ==
          QStringLiteral("[Author]"));
    CHECK(isTextFieldEmpty({FieldType::Author, QString()}, empty));
    CHECK_FALSE(isTextFieldEmpty({FieldType::Author, QString()}, context));

    // A title of two lines is one line
    FieldContext twoLines = context;
    twoLines.bookTitle = QStringLiteral("Mgła\nnad  Narwią");
    CHECK(textFieldText({FieldType::BookTitle, QString()}, twoLines, counts) ==
          QStringLiteral("Mgła nad Narwią"));
}

TEST_CASE("Fields: the formats a type has", "[editor][fields]") {
    CHECK(fieldFormats(FieldType::Date).first() == QStringLiteral("long"));
    CHECK(normalizedFieldFormat(FieldType::Date, QStringLiteral("ISO")) == QStringLiteral("iso"));
    CHECK(normalizedFieldFormat(FieldType::Date, QStringLiteral("upper")) == QStringLiteral("long"));
    CHECK(normalizedFieldFormat(FieldType::Page, QStringLiteral("I")) == QStringLiteral("I"));
    CHECK(normalizedFieldFormat(FieldType::Page, QStringLiteral("i")) == QStringLiteral("i"));
    CHECK(normalizedFieldFormat(FieldType::Page, QStringLiteral("x")) == QStringLiteral("1"));
    CHECK(normalizedFieldFormat(FieldType::Author, QStringLiteral("Upper")) ==
          QStringLiteral("upper"));
    CHECK(normalizedFieldFormat(FieldType::Author, QStringLiteral("long")).isEmpty());
    for (const FieldType type : FIELD_TYPES) {
        INFO(fieldTypeName(type).toStdString());
        CHECK(fieldTypeOfName(fieldTypeName(type)) == type);
        CHECK_FALSE(fieldTypeTitle(type).isEmpty());
    }
    CHECK_FALSE(fieldTypeOfName(QStringLiteral("weather")));
}

// =============================================================================
// KML
// =============================================================================

TEST_CASE("Fields: a field in KML, without the format when it is the type's first",
          "[editor][fields][kml]") {
    CHECK(textFieldKml({FieldType::Page, QString()}) == QStringLiteral("<field type=\"page\"/>"));
    CHECK(textFieldKml({FieldType::Page, QStringLiteral("i")}) ==
          QStringLiteral("<field type=\"page\" format=\"i\"/>"));
    CHECK(textFieldKml({FieldType::Date, QStringLiteral("long")}) ==
          QStringLiteral("<field type=\"date\"/>"));
    CHECK(textFieldKml({FieldType::BookTitle, QStringLiteral("upper")}) ==
          QStringLiteral("<field type=\"book-title\" format=\"upper\"/>"));
    const std::optional<TextField> read =
        fieldOfElement(QStringLiteral("<field type=\"date\" format=\"date-time\"/>"));
    REQUIRE(read);
    CHECK(read->type == FieldType::Date);
    CHECK(read->format == QStringLiteral("date-time"));
    CHECK_FALSE(fieldOfElement(QStringLiteral("<field type=\"weather\"/>")));
    CHECK_FALSE(fieldOfElement(QStringLiteral("<field/>")));

    // In a text: they stay as they are; a field of a type the program does not know is nothing
    const QString kml = QStringLiteral(
        "<kml><p>Page <field type=\"page\"/> of <b><field type=\"pages\" format=\"I\"/></b>, "
        "<field type=\"date\" format=\"iso\"/></p><p><field type=\"book-title\"/></p></kml>");
    auto editor = editorWith(kml);
    CHECK(editor->toKml() == kml);
    auto unknown = editorWith(
        QStringLiteral("<kml><p>A<field type=\"weather\"/>B</p></kml>"));
    CHECK(unknown->toKml() == QStringLiteral("<kml><p>AB</p></kml>"));
}

// =============================================================================
// The editor
// =============================================================================

TEST_CASE("Fields: the editor shows the book's values and follows them", "[editor][fields]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p><field type=\"book-title\"/> by <field type=\"author\" format=\"upper\"/></p>"
        "<p><field type=\"title\"/>, <field type=\"date\" format=\"short\"/></p></kml>"));
    // Without a context yet: the names of the book's values, today's date
    CHECK(shownFields(*editor) == "[Book Title]|[Author]|[Text Title]|" +
                                      dateText(QDateTime::currentDateTime(),
                                               QStringLiteral("short"), QString())
                                          .toStdString());
    const qreal before = fieldWidth(*editor, 0);
    editor->setFieldContext(bookContext());
    CHECK(shownFields(*editor) ==
          "Mgła nad Narwią|ANNA NOWAK|Rozdział pierwszy|11.10.2026");
    CHECK(fieldWidth(*editor, 0) != before);  // laid out with its new text

    // A context without a time gets the time it is set
    FieldContext now = bookContext();
    now.now = QDateTime();
    editor->setFieldContext(now);
    CHECK(editor->fieldContext().now.isValid());
}

TEST_CASE("Fields: the word count follows the text when typing pauses", "[editor][fields]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>One two three <field type=\"words\"/> words.</p></kml>"));
    CHECK(shownFields(*editor) == "4");  // the field is no word
    editor->setCursorPosition({0, 0});
    editor->insertText(QStringLiteral("Zero "));
    CHECK(shownFields(*editor) == "4");  // not while typing
    CHECK(waitUntil([&editor] { return shownFields(*editor) == "5"; }));
}

TEST_CASE("Fields: the page in the page view, \"#\" in the continuous one", "[editor][fields]") {
    QStringList paragraphs;
    for (int i = 0; i < 120; ++i) {
        paragraphs << QStringLiteral("A line of the text that fills the pages one after another.");
    }
    paragraphs << QStringLiteral("Page <field type=\"page\"/> of <field type=\"pages\"/>");
    auto editor = editorWith(kmlOf(paragraphs));
    const QTextDocument& document = *editor->textDocument();
    const int field = document.lastBlock().position() + 5;
    CHECK(editor->fieldTextAt(field) == QStringLiteral("#"));
    CHECK(editor->fieldTextAt(field + 5) == QStringLiteral("#"));

    editor->setViewMode(ViewMode::Page);
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    layout->layoutPendingBlocks();
    REQUIRE(editor->totalPages() > 1);
    QCoreApplication::processEvents();  // the page fields count the pages after the layout
    const QString pages = QString::number(editor->totalPages());
    CHECK(editor->fieldTextAt(field + 5) == pages);
    CHECK(editor->fieldTextAt(field) == pages);  // the last page
    // The page field has the room of the widest number of the pages
    CHECK(fieldWidth(*editor, field) >= fieldWidth(*editor, field + 5) - 0.5);

    // A field on the first page
    editor->setCursorPosition({0, 0});
    REQUIRE(editor->insertField({FieldType::Page, QStringLiteral("I")}));
    CHECK(editor->fieldTextAt(0) == QStringLiteral("I"));

    editor->setViewMode(ViewMode::Continuous);
    CHECK(editor->fieldTextAt(0) == QStringLiteral("#"));
}

TEST_CASE("Fields: Insert, Edit Field and Convert to Text, each one undo step",
          "[editor][fields]") {
    auto editor = editorWith(QStringLiteral("<kml><p>Written on .</p></kml>"));
    editor->setFieldContext(bookContext());
    editor->setCursorPosition({0, 11});
    REQUIRE(editor->insertField({FieldType::Date, QStringLiteral("iso")}));
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p>Written on <field type=\"date\" format=\"iso\"/>.</p></kml>"));
    CHECK(editor->cursorPosition().offset == 12);
    CHECK(editor->fieldAtCursor() == 11);
    CHECK(editor->textFieldAt(11) == TextField{FieldType::Date, QStringLiteral("iso")});
    CHECK_FALSE(editor->textFieldAt(10));

    // Typed after it: plain text, not a field
    editor->insertText(QStringLiteral("!"));
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p>Written on <field type=\"date\" format=\"iso\"/>!.</p></kml>"));
    editor->undo();

    // Another format
    REQUIRE(editor->setTextFieldAt(11, {FieldType::Date, QStringLiteral("long")}));
    CHECK(editor->fieldTextAt(11) == QStringLiteral("11 października 2026"));
    CHECK_FALSE(editor->setTextFieldAt(11, {FieldType::Date, QStringLiteral("long")}));
    editor->undo();
    CHECK(editor->fieldTextAt(11) == QStringLiteral("2026-10-11"));

    // The text in its place, in its formatting
    REQUIRE(editor->convertFieldToText(11));
    CHECK(editor->toKml() == QStringLiteral("<kml><p>Written on 2026-10-11.</p></kml>"));
    CHECK_FALSE(editor->fieldAtCursor());
    editor->undo();
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p>Written on <field type=\"date\" format=\"iso\"/>.</p></kml>"));
    editor->undo();
    CHECK(editor->toKml() == QStringLiteral("<kml><p>Written on .</p></kml>"));

    // Not in a read-only text
    editor->setReadOnly(true);
    CHECK_FALSE(editor->insertField({FieldType::Author, QString()}));
}

TEST_CASE("Fields: in place of the selection, in bold text", "[editor][fields]") {
    auto editor = editorWith(QStringLiteral("<kml><p><b>By someone</b></p></kml>"));
    editor->setFieldContext(bookContext());
    editor->setSelection({{0, 3}, {0, 10}});
    REQUIRE(editor->insertField({FieldType::Author, QString()}));
    // An object is a run of its own, in the formatting of its place, as a note's number is
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p><b>By </b><b><field type=\"author\"/></b></p></kml>"));
    REQUIRE(editor->convertFieldToText(3));
    CHECK(editor->toKml() == QStringLiteral("<kml><p><b>By Anna Nowak</b></p></kml>"));
}

TEST_CASE("Fields: copied as the text they show", "[editor][fields][clipboard]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>The <field type=\"book-title\"/> on <field type=\"date\" format=\"iso\"/>, "
        "figure <caption-number label=\"Figure\" id=\"f1\"/>.</p></kml>"));
    editor->setFieldContext(bookContext());
    editor->selectAll();
    std::unique_ptr<QMimeData> data = editor->createMimeDataFromSelection();
    REQUIRE(data);
    CHECK(data->text() == QStringLiteral("The Mgła nad Narwią on 2026-10-11, figure 1."));
    CHECK(data->html().contains(QStringLiteral("Mgła nad Narwią")));

    // Pasted in a text: fields again
    editor->clearSelection();
    editor->setCursorPosition({0, 0});
    editor->insertFromMimeData(data.get());
    CHECK(editor->toKml().count(QStringLiteral("<field type=\"book-title\"/>")) == 2);

    // As KML alone: the values without the book
    CHECK(ClipboardHandler::kmlToText(QStringLiteral(
              "<kml><p>Page <field type=\"page\"/> of <field type=\"weather\"/>"
              "<field type=\"pages\"/></p></kml>")) == QStringLiteral("Page # of #"));
}

TEST_CASE("Fields: the context menu of a field", "[editor][fields][menu]") {
    auto editor = editorWith(QStringLiteral("<kml><p>By <field type=\"author\"/> now</p></kml>"));
    editor->setFieldContext(bookContext());
    editor->setCursorPosition({0, 4});  // after the field
    QStringList texts;
    {
        PopupMenuReader reader(texts, QString());
        editor->showContextMenuAtCursor();
    }
    CHECK(texts.contains(QStringLiteral("&Edit Field...")));
    CHECK(texts.contains(QStringLiteral("Con&vert to Text")));

    // Edit asks for the window of the field
    QList<int> asked;
    QObject::connect(editor.get(), &BookEditor::fieldEditRequested,
                     [&asked](int position) { asked << position; });
    {
        PopupMenuReader reader(texts, QStringLiteral("&Edit Field..."));
        editor->showContextMenuAtCursor();
    }
    CHECK(asked == QList<int>{3});

    // Before it too; not elsewhere
    editor->setCursorPosition({0, 3});
    CHECK(editor->fieldAtCursor() == 3);
    {
        PopupMenuReader reader(texts, QStringLiteral("Con&vert to Text"));
        editor->showContextMenuAtCursor();
    }
    CHECK(editor->toKml() == QStringLiteral("<kml><p>By Anna Nowak now</p></kml>"));
    texts.clear();
    {
        PopupMenuReader reader(texts, QString());
        editor->showContextMenuAtCursor();
    }
    CHECK_FALSE(texts.contains(QStringLiteral("&Edit Field...")));
}

TEST_CASE("Fields: no gray background without the setting", "[editor][fields]") {
    auto editor = editorWith(QStringLiteral("<kml><p><field type=\"author\"/></p></kml>"));
    EditorAppearance appearance = editor->appearance();
    CHECK(appearance.fieldShading);
    appearance.fieldShading = false;
    editor->setAppearance(appearance);
    CHECK_FALSE(editor->appearance().fieldShading);
}
