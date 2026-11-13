/// @file test_text_footnotes.cpp
/// @brief Footnotes and endnotes: the model, the numbers, the note's text and KML

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_footnotes.h>
#include <kalahari/core/chapter_document.h>
#include "editor_test_utils.h"

#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QMimeData>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QXmlStreamReader>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A document of a paragraph with a note's number after "Text"
void insertNote(QTextCursor& cursor, const Footnote& note) {
    QTextCharFormat format;
    setFootnote(format, note);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), format);
}

/// The note the reader reads from @p kml, and the element the reader is left at
std::pair<Footnote, QString> readFirstNote(const QString& kml) {
    QXmlStreamReader reader(kml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() &&
            (reader.name() == u"footnote" || reader.name() == u"endnote")) {
            Footnote note = readNoteElement(reader);
            const QString at = reader.isEndElement() ? reader.name().toString() : QString();
            return {note, at};
        }
    }
    return {};
}

/// An editor with @p kml loaded, every paragraph laid out
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

/// The editor drawn, one pixel per logical pixel
QImage renderOf(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// The top and the bottom of the line box holding a character (document coordinates)
std::pair<qreal, qreal> lineOf(const KalahariTextDocumentLayout& layout, const QTextDocument& document,
                               int position) {
    const QTextBlock block = document.findBlock(position);
    QTextLayout* lines = KalahariTextDocumentLayout::blockLayout(block);
    const QTextLine line = lines->lineForTextPosition(position - block.position());
    const QRectF box = layout.lineBoxOf(block, line);
    const qreal top = layout.blockY(block.blockNumber());
    return {top + box.top(), top + box.bottom()};
}

/// A paragraph of a few lines of prose
const QString PROSE = QStringLiteral(
    "The mist came down over the river before dawn and stopped at the height of the railing, as "
    "if it could not decide whether to cross to the other bank. Anna stood in the middle of the "
    "bridge with her hands in the pockets of her coat and counted the lights of the town.");

/// A note's text of many lines
QString longNote(int sentences) {
    QString text;
    for (int i = 0; i < sentences; ++i) {
        text += QStringLiteral("A sentence of a long note that goes on for a while. ");
    }
    return text.trimmed();
}

}  // namespace

TEST_CASE("Notes: the numbers in their formats", "[editor][footnotes]") {
    CHECK(noteNumberText(3, NoteNumberFormat::Decimal) == QStringLiteral("3"));
    CHECK(noteNumberText(3, NoteNumberFormat::LowerAlpha) == QStringLiteral("c"));
    CHECK(noteNumberText(3, NoteNumberFormat::UpperAlpha) == QStringLiteral("C"));
    CHECK(noteNumberText(4, NoteNumberFormat::LowerRoman) == QStringLiteral("iv"));
    CHECK(noteNumberText(4, NoteNumberFormat::UpperRoman) == QStringLiteral("IV"));
    CHECK(noteNumberText(1, NoteNumberFormat::Symbols) == QStringLiteral("*"));
    CHECK(noteNumberText(2, NoteNumberFormat::Symbols) == QStringLiteral("†"));
    CHECK(noteNumberText(3, NoteNumberFormat::Symbols) == QStringLiteral("‡"));
    CHECK(noteNumberText(4, NoteNumberFormat::Symbols) == QStringLiteral("§"));
    CHECK(noteNumberText(5, NoteNumberFormat::Symbols) == QStringLiteral("**"));
    CHECK(noteNumberText(10, NoteNumberFormat::Symbols) == QStringLiteral("†††"));
    CHECK(noteNumberText(0, NoteNumberFormat::Decimal).isEmpty());
}

TEST_CASE("Notes: a note in a format", "[editor][footnotes]") {
    const Footnote note{QStringLiteral("f00a1b"), NoteKind::Endnote, QStringLiteral("A <b>b</b>")};
    QTextCharFormat format;
    format.setFontItalic(true);
    setFootnote(format, note);
    CHECK(isFootnoteFormat(format));
    REQUIRE(footnoteOf(format).has_value());
    CHECK(*footnoteOf(format) == note);

    SECTION("A footnote has no endnote property") {
        setFootnote(format, {note.id, NoteKind::Footnote, note.content});
        CHECK_FALSE(format.hasProperty(FOOTNOTE_ENDNOTE_PROPERTY));
        CHECK(footnoteOf(format)->kind == NoteKind::Footnote);
    }

    SECTION("Clearing a note keeps the rest of the format") {
        clearFootnote(format);
        CHECK_FALSE(isFootnoteFormat(format));
        CHECK_FALSE(footnoteOf(format).has_value());
        CHECK_FALSE(format.hasProperty(FOOTNOTE_CONTENT_PROPERTY));
        CHECK(format.fontItalic());
    }

    SECTION("A new id is none of the taken ones") {
        QSet<QString> taken;
        for (int i = 0; i < 200; ++i) {
            const QString id = newFootnoteId(taken);
            CHECK(id.size() == 7);
            CHECK(id.startsWith(QLatin1Char('f')));
            CHECK_FALSE(taken.contains(id));
            taken.insert(id);
        }
    }
}

TEST_CASE("Notes: the notes of a document and their numbers", "[editor][footnotes]") {
    QTextDocument document;
    QTextCursor cursor(&document);
    cursor.insertText(QStringLiteral("One"));
    insertNote(cursor, {QStringLiteral("f1"), NoteKind::Footnote, QStringLiteral("First")});
    cursor.insertText(QStringLiteral(" two"));
    insertNote(cursor, {QStringLiteral("e1"), NoteKind::Endnote, QStringLiteral("End")});
    cursor.insertBlock();
    cursor.insertText(QStringLiteral("Three"));
    insertNote(cursor, {QStringLiteral("f2"), NoteKind::Footnote, QStringLiteral("Second")});
    // The same note twice next to each other (pasted twice): one fragment, two numbers
    insertNote(cursor, {QStringLiteral("f3"), NoteKind::Footnote, QStringLiteral("Third")});
    insertNote(cursor, {QStringLiteral("f3"), NoteKind::Footnote, QStringLiteral("Third")});

    const std::vector<NotePlace> notes = notesIn(document);
    REQUIRE(notes.size() == 5);
    CHECK(notes[0].position == 3);
    CHECK(notes[1].position == 8);
    CHECK(notes[2].position == 15);
    CHECK(notes[3].position == 16);
    CHECK(notes[4].position == 17);
    CHECK(notesIn(document.begin()).size() == 2);
    CHECK(notesIn(document.begin().next()).size() == 3);
    CHECK(hasNotes(document.begin()));
    CHECK(noteIds(document) ==
          QSet<QString>{QStringLiteral("f1"), QStringLiteral("e1"), QStringLiteral("f2"),
                        QStringLiteral("f3")});
    REQUIRE(findNote(document, QStringLiteral("f2")).has_value());
    CHECK(findNote(document, QStringLiteral("f2"))->position == 15);
    CHECK_FALSE(findNote(document, QStringLiteral("nope")).has_value());
    CHECK_FALSE(findNote(document, QString()).has_value());

    SECTION("Each kind is counted on its own, in text order") {
        const FootnoteNumbers numbers(document, FootnoteContext{});
        CHECK(numbers.numberAt(3) == QStringLiteral("1"));
        CHECK(numbers.numberAt(8) == QStringLiteral("i"));
        CHECK(numbers.numberAt(15) == QStringLiteral("2"));
        CHECK(numbers.numberAt(16) == QStringLiteral("3"));
        CHECK(numbers.numberAt(17) == QStringLiteral("4"));
        CHECK(numbers.numberAt(4).isEmpty());
        CHECK(numbers.numberOf(QStringLiteral("e1")) == QStringLiteral("i"));
        CHECK(numbers.numberOf(QStringLiteral("nope")).isEmpty());
        REQUIRE(numbers.noteAt(15) != nullptr);
        CHECK(numbers.noteAt(15)->note.id == QStringLiteral("f2"));
        CHECK(numbers.noteAt(14) == nullptr);
    }

    SECTION("The numbers go on from the texts before, in their formats") {
        FootnoteContext context;
        context.footnoteFormat = NoteNumberFormat::Symbols;
        context.endnoteFormat = NoteNumberFormat::UpperAlpha;
        context.footnotesBefore = 3;
        context.endnotesBefore = 1;
        const FootnoteNumbers numbers(document, context);
        CHECK(numbers.numberAt(3) == QStringLiteral("§"));
        CHECK(numbers.numberAt(8) == QStringLiteral("B"));
        CHECK(numbers.numberAt(15) == QStringLiteral("**"));
    }

    SECTION("A change of the text moves the places, the numbers stay") {
        FootnoteNumbers numbers(document, FootnoteContext{});
        numbers.shift(5, 2);
        CHECK(numbers.numberAt(3) == QStringLiteral("1"));
        CHECK(numbers.numberAt(10) == QStringLiteral("i"));
        CHECK(numbers.numberAt(17) == QStringLiteral("2"));
        CHECK(numbers.numberAt(8).isEmpty());
    }
}

TEST_CASE("Notes: the text of a note", "[editor][footnotes]") {
    SECTION("One paragraph with formatting, a link and a line break") {
        const QString content = QStringLiteral(
            "A <b>bold</b> <i>word</i>, <a href=\"https://example.org\">a link</a><br/>and more");
        const std::vector<NoteParagraph> paragraphs = noteParagraphs(content);
        REQUIRE(paragraphs.size() == 1);
        CHECK(paragraphs[0].text ==
              QStringLiteral("A bold word, a link") + QChar(QChar::LineSeparator) +
                  QStringLiteral("and more"));
        CHECK(notePlainText(content) == QStringLiteral("A bold word, a link and more"));
        CHECK_FALSE(isEmptyNoteContent(content));

        QTextDocument frame;
        setNoteContent(frame, content);
        CHECK(frame.blockCount() == 1);
        CHECK(noteContentOf(frame) == content);
    }

    SECTION("Several paragraphs are <p> elements") {
        const QString content = QStringLiteral("<p>One <u>under</u></p><p>Two</p>");
        const std::vector<NoteParagraph> paragraphs = noteParagraphs(content);
        REQUIRE(paragraphs.size() == 2);
        CHECK(paragraphs[1].text == QStringLiteral("Two"));
        CHECK(notePlainText(content) == QStringLiteral("One under\nTwo"));

        QTextDocument frame;
        setNoteContent(frame, content);
        CHECK(frame.blockCount() == 2);
        CHECK(noteContentOf(frame) == content);
    }

    SECTION("Empty paragraphs at the end go, one paragraph has no <p>") {
        QTextDocument frame;
        QTextCursor cursor(&frame);
        cursor.insertText(QStringLiteral("Only"));
        cursor.insertBlock();
        cursor.insertBlock();
        CHECK(noteContentOf(frame) == QStringLiteral("Only"));
    }

    SECTION("Only the formatting a note keeps") {
        QTextDocument frame;
        QTextCursor cursor(&frame);
        QTextCharFormat format;
        format.setFontWeight(QFont::Bold);
        format.setFontPointSize(30);
        format.setForeground(Qt::red);
        format.setFontFamilies({QStringLiteral("Courier")});
        cursor.insertText(QStringLiteral("Big"), format);
        CHECK(noteContentOf(frame) == QStringLiteral("<b>Big</b>"));

        const QTextCharFormat kept = noteFormatOf(format);
        CHECK(kept.fontWeight() == QFont::Bold);
        CHECK_FALSE(kept.hasProperty(QTextFormat::FontPointSize));
        CHECK_FALSE(kept.hasProperty(QTextFormat::ForegroundBrush));
    }

    SECTION("An empty note") {
        CHECK(isEmptyNoteContent(QString()));
        CHECK(isEmptyNoteContent(QStringLiteral("  <b> </b><br/>")));
        CHECK(noteParagraphs(QString()).size() == 1);
        QTextDocument frame;
        CHECK(noteContentOf(frame).isEmpty());
    }

    SECTION("No objects in a note's text") {
        const QString content = QStringLiteral("Text<footnote id=\"x\">inner</footnote> end");
        CHECK(notePlainText(content) == QStringLiteral("Text end"));
        QTextDocument frame;
        setNoteContent(frame, content);
        CHECK(frame.toPlainText() == QStringLiteral("Text end"));
    }
}

TEST_CASE("Notes: KML of a note", "[editor][footnotes][kml]") {
    SECTION("The element of a note") {
        CHECK(noteKml({QStringLiteral("f1"), NoteKind::Footnote, QStringLiteral("A <i>b</i>")}) ==
              QStringLiteral("<footnote id=\"f1\">A <i>b</i></footnote>"));
        CHECK(noteKml({QStringLiteral("a\"b"), NoteKind::Endnote, QString()}) ==
              QStringLiteral("<endnote id=\"a&quot;b\"></endnote>"));
    }

    SECTION("Reading an element keeps its inside as it is written") {
        const auto [note, at] = readFirstNote(QStringLiteral(
            "<p>x<endnote id=\" e1 \"><p>One &amp; <b>two</b></p><p>Three<br/>four</p></endnote>"
            " after</p>"));
        CHECK(note.id == QStringLiteral("e1"));
        CHECK(note.kind == NoteKind::Endnote);
        CHECK(note.content ==
              QStringLiteral("<p>One &amp; <b>two</b></p><p>Three<br/>four</p>"));
        CHECK(at == QStringLiteral("endnote"));
    }

    SECTION("A note without an id") {
        const auto [note, at] = readFirstNote(QStringLiteral("<p><footnote>Text &lt;x&gt;</footnote></p>"));
        CHECK(note.id.isEmpty());
        CHECK(note.kind == NoteKind::Footnote);
        CHECK(notePlainText(note.content) == QStringLiteral("Text <x>"));
        CHECK(at == QStringLiteral("footnote"));
    }

    SECTION("The old footnote is known by its number") {
        QXmlStreamReader reader(QStringLiteral("<footnote id=\"f1\" number=\"2\">x</footnote>"));
        reader.readNextStartElement();
        CHECK(isOldFootnote(reader.attributes()));
        QXmlStreamReader newer(QStringLiteral("<footnote id=\"f1\">x</footnote>"));
        newer.readNextStartElement();
        CHECK_FALSE(isOldFootnote(newer.attributes()));
    }

    SECTION("A note survives the editor's load and save with what follows it") {
        BookEditor editor;
        const QString kml = QStringLiteral(
            "<kml><p>A<footnote id=\"f1\">One <a href=\"https://example.org\">link</a></footnote>"
            " <b>b</b><endnote id=\"e1\"><p>X</p><p>Y</p></endnote></p><p>Next</p></kml>");
        editor.fromKml(kml);
        CHECK(editor.toKml() == kml);
        CHECK(editor.plainText() == QStringLiteral("A b\nNext"));
    }

    SECTION("The chapter's text and word count leave the notes out") {
        const QString kml = QStringLiteral(
            "<kml><p>Two words<footnote id=\"f1\">Not these three</footnote>.</p></kml>");
        const kalahari::core::ChapterDocument chapter(kml);
        CHECK(chapter.plainText() == QStringLiteral("Two words."));
        CHECK(chapter.wordCount() == 2);
        BookEditor editor;
        editor.fromKml(kml);
        CHECK(editor.wordCount() == 2);
    }
}

// =============================================================================
// The layout
// =============================================================================

TEST_CASE("Notes: a footnote's number is raised in the text, its text under its paragraph",
          "[editor][footnotes][layout]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>One<footnote id=\"f1\">The note</footnote> two</p><p>After</p></kml>"));
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    REQUIRE(layout);
    layout->layoutPendingBlocks();
    const QTextDocument& document = *editor->textDocument();

    // The number: narrower than a digit of the text, one character of its line
    QTextLayout* lines = KalahariTextDocumentLayout::blockLayout(document.begin());
    const QTextLine line = lines->lineAt(0);
    const qreal width = line.cursorToX(4) - line.cursorToX(3);
    CHECK(width > 0.0);
    CHECK(width < QFontMetricsF(document.defaultFont()).horizontalAdvance(QLatin1Char('1')));
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f1")) == QStringLiteral("1"));

    // Its text in the room under the paragraph, above the next one
    CHECK(layout->notesBelow(0) > 0.0);
    CHECK(layout->spaceBelow(0) >= layout->notesBelow(0));
    CHECK(layout->notesBelow(1) == 0.0);
    const std::optional<QRectF> rect = layout->noteRect(QStringLiteral("f1"));
    REQUIRE(rect);
    CHECK(rect->top() >= lineOf(*layout, document, 3).second);
    CHECK(rect->bottom() <= layout->blockY(1) + 0.01);
    const std::optional<KalahariTextDocumentLayout::NoteArea> area =
        layout->noteAreaAt(rect->center());
    REQUIRE(area);
    CHECK(area->id == QStringLiteral("f1"));
    CHECK_FALSE(layout->noteAreaAt(QPointF(5.0, layout->blockY(1) + 2.0)));
    CHECK_FALSE(layout->noteAreaAt(QPointF(5.0, lineOf(*layout, document, 0).first + 1.0)));

    // A longer text takes more room; drawn in both views
    const qreal room = layout->notesBelow(0);
    QTextCursor cursor(editor->textDocument());
    cursor.setPosition(3);
    cursor.setPosition(4, QTextCursor::KeepAnchor);
    QTextCharFormat format = cursor.charFormat();
    setFootnote(format, {QStringLiteral("f1"), NoteKind::Footnote, longNote(8)});
    cursor.setCharFormat(format);
    CHECK(layout->notesBelow(0) > room);
    CHECK_FALSE(renderOf(*editor).isNull());
    editor->setViewMode(ViewMode::Page);
    CHECK_FALSE(renderOf(*editor).isNull());
}

TEST_CASE("Notes: a new note before others moves their numbers", "[editor][footnotes][layout]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>A<footnote id=\"f1\">One</footnote></p><p>B<endnote id=\"e1\">End</endnote>"
        " c<footnote id=\"f2\">Two</footnote></p></kml>"));
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    layout->layoutPendingBlocks();
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f2")) == QStringLiteral("2"));
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("e1")) == QStringLiteral("i"));

    QTextCursor cursor(editor->textDocument());
    cursor.setPosition(0);
    QTextCharFormat format;
    setFootnote(format, {QStringLiteral("f0"), NoteKind::Footnote, QStringLiteral("New")});
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), format);
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f0")) == QStringLiteral("1"));
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f1")) == QStringLiteral("2"));
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f2")) == QStringLiteral("3"));
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("e1")) == QStringLiteral("i"));

    // Typing elsewhere moves the notes after it, their numbers stay
    cursor.setPosition(editor->textDocument()->findBlockByNumber(1).position());
    cursor.insertText(QStringLiteral("xyz"));
    CHECK(layout->footnoteNumbers().noteAt(layout->footnoteNumbers().notes().back().position) != nullptr);
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f2")) == QStringLiteral("3"));
    CHECK(layout->footnoteNumbers().notes().back().position ==
          findNote(*editor->textDocument(), QStringLiteral("f2"))->position);

    // Right before a number and after it: the note stays under its paragraph
    layout->layoutPendingBlocks();
    const qreal room = layout->notesBelow(0);
    REQUIRE(room > 0.0);
    cursor.setPosition(1);  // between the new note's number and "A"
    cursor.insertText(QStringLiteral("Q"));
    cursor.setPosition(3);
    cursor.insertText(QStringLiteral("R"));
    CHECK(layout->notesBelow(0) == room);
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f1")) == QStringLiteral("2"));
    CHECK(layout->footnoteNumbers().notes()[1].position ==
          findNote(*editor->textDocument(), QStringLiteral("f1"))->position);

    // A new paragraph before a number takes its note along
    cursor.setPosition(1);
    cursor.insertBlock();
    layout->layoutPendingBlocks();
    CHECK(layout->notesBelow(0) > 0.0);  // the new note's number stays in the first
    CHECK(layout->notesBelow(1) > 0.0);  // f1 under the new paragraph
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f1")) == QStringLiteral("2"));
    const std::optional<QRectF> moved = layout->noteRect(QStringLiteral("f1"));
    REQUIRE(moved);
    CHECK(moved->top() >= layout->blockY(1));
    CHECK(moved->bottom() <= layout->blockY(2) + 0.01);

    // A number pasted next to another one is counted
    cursor.setPosition(editor->textDocument()->findBlockByNumber(1).position() + 1);
    QTextCharFormat pasted;
    setFootnote(pasted, {QStringLiteral("f9"), NoteKind::Footnote, QStringLiteral("Pasted")});
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), pasted);
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f9")) == QStringLiteral("2"));
    CHECK(layout->footnoteNumbers().numberOf(QStringLiteral("f1")) == QStringLiteral("3"));
}

TEST_CASE("Notes: the endnotes under the last paragraph", "[editor][footnotes][layout]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>A<endnote id=\"e1\">First end</endnote> b<endnote id=\"e2\">Second</endnote></p>"
        "<p>Middle</p><p>Last</p></kml>"));
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    layout->layoutPendingBlocks();
    CHECK(layout->notesBelow(0) == 0.0);
    CHECK(layout->notesBelow(1) == 0.0);
    CHECK(layout->notesBelow(2) > 0.0);
    const std::optional<QRectF> first = layout->noteRect(QStringLiteral("e1"));
    const std::optional<QRectF> second = layout->noteRect(QStringLiteral("e2"));
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->top() >= layout->blockY(2));
    CHECK(second->top() >= first->bottom() - 0.01);
    REQUIRE(layout->noteAreaAt(second->center()));
    CHECK(layout->noteAreaAt(second->center())->id == QStringLiteral("e2"));

    // A paragraph more at the end: the endnotes under it
    QTextCursor cursor(editor->textDocument());
    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
    cursor.insertText(QStringLiteral("New last"));
    CHECK(layout->notesBelow(2) == 0.0);
    CHECK(layout->notesBelow(3) > 0.0);

    // In the page view after the last line
    editor->setViewMode(ViewMode::Page);
    layout->layoutPendingBlocks();
    const std::optional<QRectF> paged = layout->noteRect(QStringLiteral("e1"));
    REQUIRE(paged);
    const QTextDocument& document = *editor->textDocument();
    CHECK(paged->top() >= lineOf(*layout, document, document.lastBlock().position()).second);
    CHECK(layout->notesBelow(3) == 0.0);
    CHECK_FALSE(renderOf(*editor).isNull());
}

TEST_CASE("Notes: in the page view the footnotes are at the bottom of the page of their numbers",
          "[editor][footnotes][layout]") {
    // Paragraphs of prose, every third with a note of a few lines
    QString kml = QStringLiteral("<kml>");
    for (int i = 0; i < 30; ++i) {
        kml += QStringLiteral("<p>") + PROSE;
        if (i % 3 == 1) {
            kml += QStringLiteral("<footnote id=\"n%1\">%2</footnote> more").arg(i).arg(longNote(1 + i % 4));
        }
        kml += QStringLiteral(" words.</p>");
    }
    kml += QStringLiteral("</kml>");
    auto editor = editorWith(kml);
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    constexpr qreal pitch = 500.0;
    constexpr qreal textHeight = 380.0;
    layout->setPageFlow(PageFlow{true, pitch, textHeight});
    layout->layoutPendingBlocks();
    const QTextDocument& document = *editor->textDocument();

    const std::vector<NotePlace> notes = notesIn(document);
    REQUIRE(notes.size() == 10);
    for (const NotePlace& place : notes) {
        INFO(place.note.id.toStdString());
        const std::optional<QRectF> rect = layout->noteRect(place.note.id);
        REQUIRE(rect);
        // On the page of its number, below its line, within the page's text area
        const auto [lineTop, lineBottom] = lineOf(*layout, document, place.position);
        const qreal page = std::floor((lineTop + 0.01) / pitch);
        CHECK(std::floor((rect->top() + 0.01) / pitch) == page);
        CHECK(rect->top() >= lineBottom - 0.01);
        CHECK(rect->bottom() <= page * pitch + textHeight + 0.01);
        // No line of the text on that page goes into the notes
        for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
            QTextLayout* lines = KalahariTextDocumentLayout::blockLayout(block);
            const qreal blockTop = layout->blockY(block.blockNumber());
            for (int i = 0; i < lines->lineCount(); ++i) {
                const QRectF box = layout->lineBoxOf(block, lines->lineAt(i));
                if (std::floor((blockTop + box.top() + 0.01) / pitch) == page) {
                    CHECK(blockTop + box.bottom() <= rect->top() + 0.01);
                }
            }
        }
    }
    CHECK_FALSE(renderOf(*editor).isNull());
}

TEST_CASE("Notes: in the page view a long footnote goes on at the bottom of the next page",
          "[editor][footnotes][layout]") {
    QString kml = QStringLiteral("<kml><p>") + PROSE + QStringLiteral("<footnote id=\"long\">") +
                  longNote(60) + QStringLiteral("</footnote> after.</p>");
    for (int i = 0; i < 12; ++i) {
        kml += QStringLiteral("<p>") + PROSE + QStringLiteral("</p>");
    }
    kml += QStringLiteral("</kml>");
    auto editor = editorWith(kml);
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    constexpr qreal pitch = 500.0;
    constexpr qreal textHeight = 380.0;
    layout->setPageFlow(PageFlow{true, pitch, textHeight});
    layout->layoutPendingBlocks();

    // Its first part on the first page, the rest at the bottom of the second
    const std::optional<QRectF> first = layout->noteRect(QStringLiteral("long"));
    REQUIRE(first);
    CHECK(first->top() < textHeight);
    CHECK(first->bottom() <= textHeight + 0.01);
    const std::optional<KalahariTextDocumentLayout::NoteArea> rest =
        layout->noteAreaAt(QPointF(10.0, pitch + textHeight - 3.0));
    REQUIRE(rest);
    CHECK(rest->id == QStringLiteral("long"));
    CHECK(layout->pageCount() >= 2);
    CHECK_FALSE(renderOf(*editor).isNull());
}

TEST_CASE("Notes: the notes of a table are under it", "[editor][footnotes][layout]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>Before</p><table><tr><td><p>Cell<footnote id=\"t1\">In a cell</footnote></p></td>"
        "<td><p>Other</p></td></tr><tr><td><p>Low</p></td><td><p>Right</p></td></tr></table>"
        "<p>After</p></kml>"));
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    layout->layoutPendingBlocks();
    const QTextDocument& document = *editor->textDocument();
    const std::optional<NotePlace> place = findNote(document, QStringLiteral("t1"));
    REQUIRE(place);
    const QTextBlock cell = document.findBlock(place->position);
    CHECK(layout->notesBelow(cell.blockNumber()) == 0.0);
    // The table's last block: "Right"
    QTextBlock last = cell;
    while (last.isValid() && last.text() != QStringLiteral("Right")) {
        last = last.next();
    }
    REQUIRE(last.isValid());
    CHECK(layout->notesBelow(last.blockNumber()) > 0.0);
    const std::optional<QRectF> rect = layout->noteRect(QStringLiteral("t1"));
    REQUIRE(rect);
    CHECK(rect->top() >= layout->blockY(last.blockNumber()) + 0.01);
    CHECK(rect->bottom() <= layout->blockY(last.next().blockNumber()) + 0.01);
}

// =============================================================================
// Inserting, the text written in the frame, the menu and the mouse
// =============================================================================

namespace {

/// The notes of an editor, as "kind:content" in text order ("f:Text", "e:")
QStringList notesOf(const BookEditor& editor) {
    QStringList notes;
    for (const NotePlace& place : notesIn(*editor.textDocument())) {
        notes << (place.note.kind == NoteKind::Endnote ? QStringLiteral("e:") : QStringLiteral("f:")) +
                     place.note.content;
    }
    return notes;
}

/// A mouse press and release at a point; a double click when @p twice
void clickAt(BookEditor& editor, const QPointF& point, bool twice = false) {
    QMouseEvent pressed(QEvent::MouseButtonPress, point, editor.mapToGlobal(point), Qt::LeftButton,
                        Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &pressed);
    QMouseEvent released(QEvent::MouseButtonRelease, point, editor.mapToGlobal(point),
                         Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &released);
    if (twice) {
        QMouseEvent again(QEvent::MouseButtonDblClick, point, editor.mapToGlobal(point),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&editor, &again);
        QCoreApplication::sendEvent(&editor, &released);
    }
}

}  // namespace

TEST_CASE("Notes: a note inserted with its text is one undo step; Esc on it leaves nothing",
          "[editor][footnotes]") {
    auto editor = editorWith(QStringLiteral("<kml><p>Some text</p></kml>"));
    editor->setCursorPosition({0, 4});

    // After the selection, which stays text
    editor->setSelection({{0, 0}, {0, 4}});
    const QString id = editor->insertNote(NoteKind::Footnote);
    REQUIRE_FALSE(id.isEmpty());
    CHECK(editor->textDocument()->firstBlock().text() ==
          QStringLiteral("Some") + QChar(QChar::ObjectReplacementCharacter) +
              QStringLiteral(" text"));
    CHECK(editor->cursorPosition() == CursorPosition{0, 5});
    CHECK_FALSE(editor->hasSelection());
    CHECK(editor->noteNumber(id) == QStringLiteral("1"));
    REQUIRE(editor->noteAtCursor());
    CHECK(editor->noteAtCursor()->note.id == id);

    // Its text joins the step of its insertion
    CHECK(editor->setNoteContent(id, QStringLiteral("The <b>note</b>"), true));
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("f:The <b>note</b>")});
    CHECK(editor->toKml() == QStringLiteral("<kml><p>Some<footnote id=\"") + id +
                                 QStringLiteral("\">The <b>note</b></footnote> text</p></kml>"));
    editor->undo();
    CHECK(notesOf(*editor).isEmpty());
    CHECK(editor->textDocument()->firstBlock().text() == QStringLiteral("Some text"));
    editor->redo();
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("f:The <b>note</b>")});

    // Edited again: a step of its own
    CHECK(editor->setNoteContent(id, QStringLiteral("Other"), true));
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("f:Other")});
    editor->undo();
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("f:The <b>note</b>")});

    // Esc on a new one: it goes, and cannot be redone
    editor->setCursorPosition({0, 0});
    const QString endnote = editor->insertNote(NoteKind::Endnote);
    REQUIRE_FALSE(endnote.isEmpty());
    CHECK(notesOf(*editor).size() == 2);
    editor->cancelNewNote(endnote);
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("f:The <b>note</b>")});
    CHECK_FALSE(editor->textDocument()->isRedoAvailable());

    // Something done since: the note goes all the same, as a step of its own
    const QString late = editor->insertNote(NoteKind::Footnote);
    editor->insertText(QStringLiteral("x"));
    editor->cancelNewNote(late);
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("f:The <b>note</b>")});
    CHECK(editor->textDocument()->firstBlock().text().startsWith(QStringLiteral("x")));

    // Text typed after a number is not the note
    const QString textAfter = editor->textDocument()->firstBlock().text();
    CHECK(notesIn(*editor->textDocument()).size() == 1);
    CHECK(textAfter.count(QChar(QChar::ObjectReplacementCharacter)) == 1);
}

TEST_CASE("Notes: Convert and Delete, and the context menu of a number", "[editor][footnotes][menu]") {
    auto editor = editorWith(
        QStringLiteral("<kml><p>One<footnote id=\"f1\">First</footnote> two</p></kml>"));
    editor->setCursorPosition({0, 4});  // after the number
    QStringList texts;
    {
        PopupMenuReader reader(texts, QString());
        editor->showContextMenuAtCursor();
    }
    for (const char* text : {"&Edit Footnote", "Con&vert to Endnote", "Delete F&ootnote"}) {
        INFO(text);
        CHECK(texts.contains(QString::fromUtf8(text)));
    }

    // Edit asks for the frame
    QStringList asked;
    QObject::connect(editor.get(), &BookEditor::noteEditRequested,
                     [&asked](const QString& id) { asked << id; });
    {
        PopupMenuReader reader(texts, QStringLiteral("&Edit Footnote"));
        editor->showContextMenuAtCursor();
    }
    CHECK(asked == QStringList{QStringLiteral("f1")});

    // Convert: an endnote, one undo step
    {
        PopupMenuReader reader(texts, QStringLiteral("Con&vert to Endnote"));
        editor->showContextMenuAtCursor();
    }
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("e:First")});
    CHECK(editor->noteNumber(QStringLiteral("f1")) == QStringLiteral("i"));
    texts.clear();
    {
        PopupMenuReader reader(texts, QStringLiteral("Delete Endn&ote"));
        editor->showContextMenuAtCursor();
    }
    CHECK(texts.contains(QStringLiteral("Con&vert to Footnote")));
    CHECK(notesOf(*editor).isEmpty());
    CHECK(editor->textDocument()->firstBlock().text() == QStringLiteral("One two"));
    editor->undo();
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("e:First")});
    editor->undo();
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("f:First")});

    // Before the number: not its menu
    editor->setCursorPosition({0, 3});
    texts.clear();
    {
        PopupMenuReader reader(texts, QString());
        editor->showContextMenuAtCursor();
    }
    CHECK_FALSE(texts.contains(QStringLiteral("&Edit Footnote")));

    // A read-only text: greyed out
    editor->setCursorPosition({0, 4});
    editor->setReadOnly(true);
    CHECK(editor->insertNote(NoteKind::Footnote).isEmpty());
    CHECK_FALSE(editor->setNoteKind(QStringLiteral("f1"), NoteKind::Endnote));
    CHECK_FALSE(editor->removeNote(QStringLiteral("f1")));
    CHECK_FALSE(editor->setNoteContent(QStringLiteral("f1"), QStringLiteral("x")));
    CHECK(notesOf(*editor) == QStringList{QStringLiteral("f:First")});
}

TEST_CASE("Notes: a click on a note's text and a double click on its number open its frame",
          "[editor][footnotes][mouse]") {
    auto editor = editorWith(
        QStringLiteral("<kml><p>One<footnote id=\"f1\">First note</footnote> two</p>"
                       "<p>Next</p></kml>"));
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    layout->layoutPendingBlocks();
    QStringList asked;
    QObject::connect(editor.get(), &BookEditor::noteEditRequested,
                     [&asked](const QString& id) { asked << id; });

    // The note's text, under its paragraph
    const QRectF note = editor->noteRect(QStringLiteral("f1"));
    REQUIRE_FALSE(note.isEmpty());
    REQUIRE(editor->noteAt(note.center()));
    CHECK(editor->noteAt(note.center())->note.id == QStringLiteral("f1"));
    editor->setCursorPosition({1, 2});
    clickAt(*editor, note.center());
    CHECK(asked == QStringList{QStringLiteral("f1")});
    CHECK(editor->cursorPosition() == CursorPosition{1, 2});  // the cursor stays

    // The number, in the text
    const QRectF number = editor->placeRect(4, true);
    const QPointF onNumber(number.left() - 2.0, number.center().y());
    REQUIRE(editor->noteAt(onNumber));
    clickAt(*editor, onNumber, true);
    CHECK(asked == QStringList{QStringLiteral("f1"), QStringLiteral("f1")});

    // The frame's room: the note takes it under its paragraph
    const qreal before = layout->notesBelow(0);
    editor->setNoteFrameRoom(QStringLiteral("f1"), note.height() + 80.0);
    CHECK(layout->notesBelow(0) > before + 40.0);
    CHECK(editor->noteRect(QStringLiteral("f1")).height() >= note.height() + 79.0);
    editor->setNoteFrameRoom(QStringLiteral("f1"), 0.0);
    CHECK(layout->notesBelow(0) == before);
    CHECK(editor->noteFrameFont().family() == editor->textDocument()->defaultFont().family());
}

// =============================================================================
// The clipboard
// =============================================================================

namespace {

/// The notes of KML, as "kind:content" in text order
QStringList notesOfKml(const QString& kml) {
    auto editor = editorWith(kml);
    QStringList notes;
    for (const NotePlace& place : notesIn(*editor->textDocument())) {
        notes << (place.note.kind == NoteKind::Endnote ? QStringLiteral("e:")
                                                       : QStringLiteral("f:")) +
                     place.note.content;
    }
    return notes;
}

/// What LibreOffice writes of a text with notes (Writer 7, HTML (StarWriter))
const QString LIBREOFFICE_HTML = QStringLiteral(
    "<html><head><meta charset=\"utf-8\"/></head><body lang=\"en-US\" dir=\"ltr\">"
    "<p style=\"line-height: 100%; margin-bottom: 0in\">\n"
    "Rain fell.<a class=\"sdfootnoteanc\" name=\"sdfootnote1anc\" href=\"#sdfootnote1sym\">"
    "<sup>1</sup></a>\nThen it stopped.<a class=\"sdendnoteanc\" name=\"sdendnote1anc\" "
    "href=\"#sdendnote1sym\"><sup>i</sup></a></p>\n"
    "<p style=\"line-height: 100%; margin-bottom: 0in\">Next paragraph.<a class=\"sdfootnoteanc\" "
    "name=\"sdfootnote2anc\" href=\"#sdfootnote2sym\"><sup>2</sup></a></p>\n"
    "<div id=\"sdfootnote1\"><p class=\"sdfootnote\"><a class=\"sdfootnotesym\" "
    "name=\"sdfootnote1sym\" href=\"#sdfootnote1anc\">1</a>Cold\n\t<b>rain</b>.</p>\n"
    "\t<p class=\"sdfootnote\">Second para.</p>\n</div>\n"
    "<div id=\"sdfootnote2\"><p class=\"sdfootnote\"><a class=\"sdfootnotesym\" "
    "name=\"sdfootnote2sym\" href=\"#sdfootnote2anc\">2</a>Second\n\tfootnote.</p>\n</div>\n"
    "<div id=\"sdendnote1\"><p class=\"sdendnote\"><a class=\"sdendnotesym\" "
    "name=\"sdendnote1sym\" href=\"#sdendnote1anc\">i</a>An\n\tendnote.</p>\n</div>\n"
    "</body></html>");

/// What Word writes of a text with a footnote and an endnote (its HTML on the clipboard)
const QString WORD_HTML = QStringLiteral(
    "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"><body lang=EN-US>"
    "<!--StartFragment--><p class=MsoNormal>Rain fell.<a style='mso-footnote-id:ftn1' "
    "href=\"#_ftn1\" name=\"_ftnref1\" title=\"\"><span class=MsoFootnoteReference><span "
    "style='mso-special-character:footnote'><![if !supportFootnotes]><span "
    "class=MsoFootnoteReference><span style='font-size:11.0pt'>[1]</span></span><![endif]>"
    "</span></span></a> Then it stopped.<a style='mso-endnote-id:edn1' href=\"#_edn1\" "
    "name=\"_ednref1\" title=\"\"><span class=MsoEndnoteReference><span "
    "style='mso-special-character:footnote'><![if !supportEndnotes]><span "
    "class=MsoEndnoteReference>[i]</span><![endif]></span></span></a></p>"
    "<div style='mso-element:footnote-list'><![if !supportFootnotes]><br clear=all>\n"
    "<hr align=left size=1 width=\"33%\">\n<![endif]>\n"
    "<div style='mso-element:footnote' id=ftn1>\n<p class=MsoFootnoteText><a "
    "style='mso-footnote-id:ftn1' href=\"#_ftnref1\" name=\"_ftn1\" title=\"\"><span "
    "class=MsoFootnoteReference><span style='mso-special-character:footnote'><![if "
    "!supportFootnotes]><span class=MsoFootnoteReference>[1]</span><![endif]></span></span>"
    "</a> Cold <i>rain</i>, <a href=\"https://example.com\">a link</a>.</p>\n</div>\n</div>"
    "<div style='mso-element:endnote-list'><![if !supportEndnotes]><br clear=all>\n"
    "<hr align=left size=1 width=\"33%\">\n<![endif]>\n"
    "<div style='mso-element:endnote' id=edn1>\n<p class=MsoEndnoteText><a "
    "style='mso-endnote-id:edn1' href=\"#_ednref1\" name=\"_edn1\" title=\"\"><span "
    "class=MsoEndnoteReference><span style='mso-special-character:footnote'><![if "
    "!supportEndnotes]><span class=MsoEndnoteReference>[i]</span><![endif]></span></span>"
    "</a> An endnote.</p>\n</div>\n</div><!--EndFragment--></body></html>");

}  // namespace

TEST_CASE("Notes: copied, other programs get the numbers and the notes after the text",
          "[editor][footnotes][clipboard]") {
    const QString kml = QStringLiteral(
        "<kml><p>Rain<footnote id=\"f1\">Cold <b>rain</b></footnote> fell.<endnote "
        "id=\"f2\">End</endnote></p><p>More<footnote id=\"f3\"><p>One</p><p>Two</p></footnote>"
        "</p><p>Old <footnote number=\"7\">kept</footnote> text</p></kml>");

    // HTML: the number a link to the note's text after the text, as LibreOffice writes it
    CHECK(ClipboardHandler::kmlToHtml(kml) ==
          QStringLiteral(
              "<p>Rain<a class=\"sdfootnoteanc\" name=\"sdfootnote1anc\" "
              "href=\"#sdfootnote1sym\"><sup>1</sup></a> fell.<a class=\"sdendnoteanc\" "
              "name=\"sdendnote1anc\" href=\"#sdendnote1sym\"><sup>i</sup></a></p>"
              "<p>More<a class=\"sdfootnoteanc\" name=\"sdfootnote2anc\" "
              "href=\"#sdfootnote2sym\"><sup>2</sup></a></p><p>Old kept text</p>"
              "<div id=\"sdfootnote1\"><p><a class=\"sdfootnotesym\" name=\"sdfootnote1sym\" "
              "href=\"#sdfootnote1anc\">1</a>Cold <b>rain</b></p></div>"
              "<div id=\"sdfootnote2\"><p><a class=\"sdfootnotesym\" name=\"sdfootnote2sym\" "
              "href=\"#sdfootnote2anc\">2</a>One</p><p>Two</p></div>"
              "<div id=\"sdendnote1\"><p><a class=\"sdendnotesym\" name=\"sdendnote1sym\" "
              "href=\"#sdendnote1anc\">i</a>End</p></div>"));

    // Plain text: the number in the text, the notes after an empty line, the footnotes first
    CHECK(ClipboardHandler::kmlToText(kml) ==
          QStringLiteral("Rain1 fell.i\nMore2\nOld kept text\n\n1 Cold rain\n2 One\nTwo\ni End"));

    // Pasted back from the HTML: the same notes
    const QString pasted = ClipboardHandler::htmlToKml(ClipboardHandler::kmlToHtml(kml));
    CHECK(notesOfKml(pasted) == QStringList{QStringLiteral("f:Cold <b>rain</b>"),
                                            QStringLiteral("e:End"),
                                            QStringLiteral("f:<p>One</p><p>Two</p>")});
    CHECK_FALSE(pasted.contains(QStringLiteral("sdfootnote")));

    // The numbers the editor shows: a part of the text goes on from them
    auto editor = editorWith(kml);
    editor->setSelection({{1, 0}, {1, 5}});
    const auto data = editor->createMimeDataFromSelection();
    REQUIRE(data);
    CHECK(data->text() == QStringLiteral("More2\n\n2 One\nTwo"));
    CHECK(data->html().contains(QStringLiteral("<sup>2</sup>")));
    CHECK(data->html().contains(QStringLiteral("href=\"#sdfootnote1anc\">2</a>One")));
}

TEST_CASE("Notes: pasted from LibreOffice and Word, the notes stay notes",
          "[editor][footnotes][clipboard]") {
    SECTION("LibreOffice") {
        const QString kml = ClipboardHandler::htmlToKml(LIBREOFFICE_HTML);
        CHECK(notesOfKml(kml) ==
              QStringList{QStringLiteral("f:<p>Cold <b>rain</b>.</p><p>Second para.</p>"),
                          QStringLiteral("e:An endnote."), QStringLiteral("f:Second footnote.")});
        auto editor = editorWith(kml);
        const QChar number(QChar::ObjectReplacementCharacter);
        CHECK(editor->textDocument()->firstBlock().text() ==
              QStringLiteral("Rain fell.") + number + QStringLiteral(" Then it stopped.") + number);
        CHECK(editor->textDocument()->firstBlock().next().text() ==
              QStringLiteral("Next paragraph.") + number);
        CHECK(editor->textDocument()->blockCount() == 2);  // nothing of the notes after it
    }

    SECTION("Word: also its links in a note, and nothing of its lists of notes") {
        const QString kml = ClipboardHandler::htmlToKml(WORD_HTML, PasteMode::Source);
        CHECK(notesOfKml(kml) ==
              QStringList{QStringLiteral(
                              "f:Cold <i>rain</i>, <a href=\"https://example.com\">a link</a>."),
                          QStringLiteral("e:An endnote.")});
        auto editor = editorWith(kml);
        const QChar number(QChar::ObjectReplacementCharacter);
        CHECK(editor->textDocument()->blockCount() == 1);
        CHECK(editor->textDocument()->firstBlock().text() ==
              QStringLiteral("Rain fell.") + number + QStringLiteral(" Then it stopped.") + number);
    }

    SECTION("Only the text: the numbers in the text, the notes' texts after it") {
        const QString kml = ClipboardHandler::htmlToKml(LIBREOFFICE_HTML, PasteMode::Text);
        CHECK(notesOfKml(kml).isEmpty());
        CHECK(ClipboardHandler::kmlToText(kml) ==
              QStringLiteral("Rain fell.1 Then it stopped.i\nNext paragraph.2\n1 Cold rain.\n"
                             "Second para.\n2 Second footnote.\ni An endnote."));
    }

    SECTION("A number without its note's text stays text") {
        const QString kml = ClipboardHandler::htmlToKml(QStringLiteral(
            "<p>Rain<a class=\"sdfootnoteanc\" name=\"sdfootnote1anc\" "
            "href=\"#sdfootnote1sym\"><sup>1</sup></a></p>"));
        CHECK(notesOfKml(kml).isEmpty());
        CHECK(ClipboardHandler::kmlToText(kml) == QStringLiteral("Rain1"));
    }

    SECTION("Nothing is lost: a note's text without its number, a link without its end") {
        // The text of a note whose number was not copied stays text
        QString kml = ClipboardHandler::htmlToKml(QStringLiteral(
            "<p>Rain fell.</p><div id=\"sdfootnote1\"><p><a class=\"sdfootnotesym\" "
            "name=\"sdfootnote1sym\" href=\"#sdfootnote1anc\">1</a>Cold rain.</p></div>"));
        CHECK(notesOfKml(kml).isEmpty());
        CHECK(ClipboardHandler::kmlToText(kml) == QStringLiteral("Rain fell.\n1Cold rain."));

        // A number's link without its end tag: the text after it stays, and so does the note's
        kml = ClipboardHandler::htmlToKml(QStringLiteral(
            "<p>Rain<a class=\"sdfootnoteanc\" name=\"sdfootnote1anc\" "
            "href=\"#sdfootnote1sym\"><sup>1</sup> fell all day.</p><div id=\"sdfootnote1\"><p>"
            "<a class=\"sdfootnotesym\" name=\"sdfootnote1sym\" href=\"#sdfootnote1anc\">1</a>"
            "Cold rain.</p></div>"));
        CHECK(notesOfKml(kml).isEmpty());
        const QString text = ClipboardHandler::kmlToText(kml);
        CHECK(text.contains(QStringLiteral("fell all day.")));
        CHECK(text.contains(QStringLiteral("Cold rain.")));

        // One of two notes copied without its number: the other is a note, it stays text
        kml = ClipboardHandler::htmlToKml(QStringLiteral(
            "<p>Rain<a class=\"sdfootnoteanc\" name=\"sdfootnote1anc\" "
            "href=\"#sdfootnote1sym\"><sup>1</sup></a> fell.</p>"
            "<div id=\"sdfootnote1\"><p><a class=\"sdfootnotesym\" name=\"sdfootnote1sym\" "
            "href=\"#sdfootnote1anc\">1</a>Cold rain.</p></div>"
            "<div id=\"sdfootnote2\"><p><a class=\"sdfootnotesym\" name=\"sdfootnote2sym\" "
            "href=\"#sdfootnote2anc\">2</a>Kept text.</p></div>"));
        CHECK(notesOfKml(kml) == QStringList{QStringLiteral("f:Cold rain.")});
        CHECK(ClipboardHandler::kmlToText(kml).contains(QStringLiteral("2Kept text.")));
    }
}
