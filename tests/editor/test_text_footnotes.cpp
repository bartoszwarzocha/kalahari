/// @file test_text_footnotes.cpp
/// @brief Footnotes and endnotes: the model, the numbers, the note's text and KML

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/text_footnotes.h>
#include <kalahari/core/chapter_document.h>

#include <QFont>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QXmlStreamReader>

using namespace kalahari::editor;

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
