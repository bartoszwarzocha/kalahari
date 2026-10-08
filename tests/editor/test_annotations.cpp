/// @file test_annotations.cpp
/// @brief Annotations (comments, TODOs and notes): the model, their KML and the editing
/// that keeps them where the writer put them

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/search_engine.h>
#include "editor_test_utils.h"

#include <QDateTime>
#include <QMimeData>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimeZone>

#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// An <annotation> element of the KML section
QString record(const QString& id, const QString& kind = QStringLiteral("comment"),
               const QString& text = QStringLiteral("Text")) {
    return QStringLiteral("<annotation id=\"%1\" kind=\"%2\">%3</annotation>").arg(id, kind, text);
}

/// KML with these annotations and paragraphs
QString kmlWith(const QString& annotations, const QStringList& paragraphs) {
    QString kml = QStringLiteral("<kml><annotations>") + annotations + QStringLiteral("</annotations>");
    for (const QString& paragraph : paragraphs) {
        kml += QStringLiteral("<p>") + paragraph + QStringLiteral("</p>");
    }
    return kml + QStringLiteral("</kml>");
}

/// An editor with KML loaded
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    editor->fromKml(kml);
    return editor;
}

/// Where the annotations of an editor are, in text order: "id start-end" for a fragment,
/// "id @place" for a place
QString places(const BookEditor& editor) {
    QStringList list;
    for (const AnnotationPlace& place : editor.annotations()) {
        list << (place.annotation.point
                     ? QStringLiteral("%1 @%2").arg(place.annotation.id).arg(place.start)
                     : QStringLiteral("%1 %2-%3").arg(place.annotation.id).arg(place.start).arg(place.end));
    }
    return list.join(QStringLiteral(", "));
}

/// The annotation with this id
Annotation annotationOf(const BookEditor& editor, const QString& id) {
    for (const AnnotationPlace& place : editor.annotations()) {
        if (place.annotation.id == id) {
            return place.annotation;
        }
    }
    return {};
}

/// The KML of copied data
QString kmlData(const QMimeData& data) {
    return QString::fromUtf8(data.data(QString::fromLatin1(MIME_KML)));
}

}  // anonymous namespace

// =============================================================================
// The model
// =============================================================================

TEST_CASE("Annotations: kinds have their KML names, ids are unique", "[editor][annotations]") {
    for (const AnnotationKind kind :
         {AnnotationKind::Comment, AnnotationKind::Todo, AnnotationKind::Note}) {
        CHECK(annotationKindFromName(annotationKindName(kind)) == kind);
    }
    CHECK(annotationKindName(AnnotationKind::Todo) == QStringLiteral("todo"));
    CHECK_FALSE(annotationKindFromName(u"remark").has_value());

    const QString id = newAnnotationId();
    CHECK_FALSE(id.isEmpty());
    CHECK(id != newAnnotationId());
}

TEST_CASE("Annotations: a format carries a list of them", "[editor][annotations]") {
    Annotation a;
    a.id = QStringLiteral("a");
    Annotation b;
    b.id = QStringLiteral("b");
    Annotation changed = a;
    changed.text = QStringLiteral("Changed");

    QTextCharFormat format;
    CHECK(annotationsOf(format).isEmpty());
    setAnnotations(format, {a, b});
    CHECK(annotationsOf(format) == AnnotationList{a, b});
    setAnnotations(format, {});
    CHECK_FALSE(format.hasProperty(KmlPropAnnotations));

    // An annotation with an id the list has takes its place
    CHECK(withAnnotation({a, b}, changed) == AnnotationList{changed, b});
    CHECK(withAnnotation({a}, b) == AnnotationList{a, b});

    // Formats with the same annotations are the same format, so the document keeps one
    // fragment for their text
    QTextCharFormat first;
    QTextCharFormat second;
    setAnnotations(first, {a});
    setAnnotations(second, {a});
    CHECK(first == second);
    setAnnotations(second, {changed});
    CHECK(first != second);
}

// =============================================================================
// Adding, changing, removing, going to
// =============================================================================

TEST_CASE("Annotations: added on the selection, or on the cursor's place without one",
          "[editor][annotations]") {
    // "The quick fox" 0-13, the paragraph break 13, "Second line" 14-25
    auto editor = editorWith(kmlOf({QStringLiteral("The quick fox"), QStringLiteral("Second line")}));
    const QString text = editor->plainText();

    SECTION("on a fragment, the selection stays") {
        editor->setSelection({{0, 4}, {0, 9}});
        const Annotation added =
            editor->addAnnotation(AnnotationKind::Comment, QStringLiteral("Why quick?"),
                                  QStringLiteral("Ann"));
        CHECK_FALSE(added.id.isEmpty());
        CHECK_FALSE(added.point);
        CHECK(added.kind == AnnotationKind::Comment);
        CHECK(added.text == QStringLiteral("Why quick?"));
        CHECK(added.author == QStringLiteral("Ann"));
        CHECK_FALSE(added.done);
        REQUIRE(added.created.isValid());
        CHECK(added.created.time().msec() == 0);  // the chapter file keeps whole seconds
        CHECK(added.created.offsetFromUtc() == 0);
        CHECK(places(*editor) == added.id + QStringLiteral(" 4-9"));
        CHECK(annotationOf(*editor, added.id) == added);
        CHECK(editor->hasSelection());
        CHECK(editor->plainText() == text);
    }

    SECTION("on the cursor's place") {
        editor->setCursorPosition({0, 3});
        const Annotation added =
            editor->addAnnotation(AnnotationKind::Todo, QStringLiteral("Check"), QString());
        CHECK(added.point);
        CHECK(places(*editor) == added.id + QStringLiteral(" @3"));
    }

    SECTION("on the start of a paragraph: on the paragraph itself") {
        editor->setCursorPosition({1, 0});
        const Annotation added =
            editor->addAnnotation(AnnotationKind::Note, QStringLiteral("Mine"), QString());
        CHECK(places(*editor) == added.id + QStringLiteral(" @14"));
        CHECK(annotationIds(editor->textDocument()->findBlockByNumber(1).charFormat()) ==
              QStringList{added.id});
    }

    SECTION("on a selection of a paragraph break only: on its place") {
        editor->setSelection({{0, 13}, {1, 0}});
        const Annotation added =
            editor->addAnnotation(AnnotationKind::Note, QStringLiteral("Break"), QString());
        CHECK(added.point);
        CHECK(places(*editor) == added.id + QStringLiteral(" @13"));
    }

    SECTION("on a fragment over two paragraphs; the second keeps a clean format of its own") {
        editor->setSelection({{0, 10}, {1, 6}});
        const Annotation added =
            editor->addAnnotation(AnnotationKind::Comment, QStringLiteral("Long"), QString());
        CHECK(places(*editor) == added.id + QStringLiteral(" 10-20"));
        CHECK(annotationIds(editor->textDocument()->findBlockByNumber(1).charFormat()).isEmpty());
    }

    SECTION("one undo step") {
        editor->setSelection({{0, 4}, {0, 9}});
        editor->addAnnotation(AnnotationKind::Comment, QStringLiteral("Undone"), QString());
        editor->undo();
        CHECK(editor->annotations().empty());
        CHECK(editor->plainText() == text);
    }
}

TEST_CASE("Annotations: changed and taken off wherever they are", "[editor][annotations]") {
    // "One two three" 0-13, the paragraph break 13, "four five" 14-23
    const QStringList paragraphs = {
        QStringLiteral("One <anchor ref=\"c1\">two </anchor><anchor ref=\"c1\"><b>three</b></anchor>"),
        QStringLiteral("<anchor ref=\"c1\">four</anchor> five<anchor ref=\"t1\"/>")};
    auto editor = editorWith(kmlWith(record(QStringLiteral("c1")) +
                                         record(QStringLiteral("t1"), QStringLiteral("todo")),
                                     paragraphs));
    REQUIRE(places(*editor) == QStringLiteral("c1 4-18, t1 @23"));

    Annotation comment = annotationOf(*editor, QStringLiteral("c1"));
    comment.text = QStringLiteral("Changed");
    comment.done = true;
    comment.point = true;  // ignored: the anchor stays as it is
    REQUIRE(editor->updateAnnotation(comment));
    CHECK(places(*editor) == QStringLiteral("c1 4-18, t1 @23"));
    CHECK(annotationOf(*editor, QStringLiteral("c1")).text == QStringLiteral("Changed"));
    CHECK(annotationOf(*editor, QStringLiteral("c1")).done);

    // Every piece changed, the bold one and the one in the next paragraph too
    CHECK(editor->toKml() ==
          kmlWith(QStringLiteral("<annotation id=\"c1\" kind=\"comment\" done=\"true\">Changed"
                                 "</annotation>") +
                      record(QStringLiteral("t1"), QStringLiteral("todo")),
                  paragraphs));

    REQUIRE(editor->removeAnnotation(QStringLiteral("c1")));
    CHECK(places(*editor) == QStringLiteral("t1 @23"));
    CHECK(editor->toKml() ==
          kmlWith(record(QStringLiteral("t1"), QStringLiteral("todo")),
                  {QStringLiteral("One two <b>three</b>"),
                   QStringLiteral("four five<anchor ref=\"t1\"/>")}));
    CHECK_FALSE(editor->removeAnnotation(QStringLiteral("c1")));
    CHECK_FALSE(editor->updateAnnotation(comment));

    editor->undo();  // the removal
    CHECK(places(*editor) == QStringLiteral("c1 4-18, t1 @23"));
    CHECK(annotationOf(*editor, QStringLiteral("c1")).text == QStringLiteral("Changed"));
    editor->undo();  // the change
    CHECK(annotationOf(*editor, QStringLiteral("c1")).text == QStringLiteral("Text"));
}

TEST_CASE("Annotations: undone and redone, a step of them brings back its cursor and selection",
          "[editor][annotations]") {
    // "First paragraph" 0-15, the paragraph break 15, "One two three" 16-29
    auto editor = editorWith(kmlWith(record(QStringLiteral("c1")),
                                     {QStringLiteral("First paragraph"),
                                      QStringLiteral("One <anchor ref=\"c1\">two</anchor> three")}));
    REQUIRE(places(*editor) == QStringLiteral("c1 20-23"));

    SECTION("a change") {
        editor->setCursorPosition({1, 9});
        Annotation comment = annotationOf(*editor, QStringLiteral("c1"));
        comment.text = QStringLiteral("New");
        REQUIRE(editor->updateAnnotation(comment));

        editor->setCursorPosition({0, 2});  // the writer went on elsewhere
        editor->undo();
        CHECK(annotationOf(*editor, QStringLiteral("c1")).text == QStringLiteral("Text"));
        CHECK(editor->cursorPosition() == CursorPosition{1, 9});
        CHECK_FALSE(editor->hasSelection());
        editor->setCursorPosition({0, 2});
        editor->redo();
        CHECK(annotationOf(*editor, QStringLiteral("c1")).text == QStringLiteral("New"));
        CHECK(editor->cursorPosition() == CursorPosition{1, 9});
    }

    SECTION("a new one with its text typed after it, in one step") {
        editor->setSelection({{1, 8}, {1, 13}});
        Annotation typed = editor->addAnnotation(AnnotationKind::Todo, QString(), QString());
        typed.text = QStringLiteral("Check");
        REQUIRE(editor->updateAnnotation(typed, true));

        editor->setCursorPosition({0, 2});
        editor->undo();
        CHECK(places(*editor) == QStringLiteral("c1 20-23"));
        CHECK(editor->selection().normalized().start == CursorPosition{1, 8});
        CHECK(editor->selection().normalized().end == CursorPosition{1, 13});
    }

    SECTION("a removal") {
        editor->setCursorPosition({1, 2});
        REQUIRE(editor->removeAnnotation(QStringLiteral("c1")));
        editor->setCursorPosition({0, 2});
        editor->undo();
        CHECK(places(*editor) == QStringLiteral("c1 20-23"));
        CHECK(editor->cursorPosition() == CursorPosition{1, 2});
    }

    SECTION("nothing to change, no step") {
        const QString text = editor->plainText();
        editor->setCursorPosition({0, 5});
        editor->insertText(QStringLiteral("x"));
        Annotation none;
        none.id = QStringLiteral("none");
        CHECK_FALSE(editor->updateAnnotation(none, true));
        CHECK_FALSE(editor->removeAnnotation(QStringLiteral("none")));

        editor->undo();  // the typing, no more
        CHECK(editor->plainText() == text);
        CHECK_FALSE(editor->canUndo());
    }
}

TEST_CASE("Annotations: going to one, to the next and to the previous TODO",
          "[editor][annotations]") {
    // "One two three four five"
    auto editor = editorWith(kmlWith(
        record(QStringLiteral("t1"), QStringLiteral("todo")) + record(QStringLiteral("c1")) +
            QStringLiteral("<annotation id=\"t2\" kind=\"todo\" done=\"true\">Done</annotation>") +
            record(QStringLiteral("t3"), QStringLiteral("todo")),
        {QStringLiteral("<anchor ref=\"t1\">One</anchor> two<anchor ref=\"c1\"/> three"
                        "<anchor ref=\"t2\"/> four <anchor ref=\"t3\">five</anchor>")}));
    REQUIRE(places(*editor) == QStringLiteral("t1 0-3, c1 @7, t2 @13, t3 19-23"));

    SECTION("a fragment is selected, with the cursor at its end") {
        REQUIRE(editor->goToAnnotation(QStringLiteral("t3")));
        CHECK(editor->selection().normalized().start == CursorPosition{0, 19});
        CHECK(editor->selection().normalized().end == CursorPosition{0, 23});
        CHECK(editor->cursorPosition() == CursorPosition{0, 23});
    }

    SECTION("on a place the cursor goes there") {
        editor->setSelection({{0, 0}, {0, 3}});
        REQUIRE(editor->goToAnnotation(QStringLiteral("c1")));
        CHECK_FALSE(editor->hasSelection());
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});
        CHECK_FALSE(editor->goToAnnotation(QStringLiteral("none")));
    }

    SECTION("TODOs not done yet, in both directions") {
        editor->setCursorPosition({0, 2});
        CHECK(editor->goToNextTodo() == QStringLiteral("t3"));  // past the comment and the TODO done
        CHECK(editor->cursorPosition() == CursorPosition{0, 23});
        CHECK(editor->goToNextTodo().isEmpty());
        CHECK(editor->goToPreviousTodo() == QStringLiteral("t1"));  // before the selected one
        CHECK(editor->cursorPosition() == CursorPosition{0, 3});
        CHECK(editor->selection().normalized().start == CursorPosition{0, 0});
        CHECK(editor->goToPreviousTodo().isEmpty());
    }
}

// =============================================================================
// KML
// =============================================================================

TEST_CASE("Annotations KML: places at paragraph starts and in empty paragraphs",
          "[editor][annotations][kml]") {
    // "Start" 0-5, an empty paragraph at 6, "Bold" 7-11
    const QString kml = kmlWith(record(QStringLiteral("a")) +
                                    record(QStringLiteral("b"), QStringLiteral("todo")) +
                                    record(QStringLiteral("c"), QStringLiteral("note")),
                                {QStringLiteral("<anchor ref=\"a\"/>Start"),
                                 QStringLiteral("<anchor ref=\"b\"/>"),
                                 QStringLiteral("<anchor ref=\"c\"/><b>Bold</b>")});
    auto editor = editorWith(kml);
    CHECK(places(*editor) == QStringLiteral("a @0, b @6, c @7"));
    CHECK(editor->plainText() == QStringLiteral("Start\n\nBold"));
    CHECK(editor->toKml() == kml);
}

TEST_CASE("Annotations KML: a fragment over paragraphs, nested anchors and several places "
          "after one character",
          "[editor][annotations][kml]") {
    // As the editor writes it: an anchor for each run of formatting, the places after a
    // character inside the run
    const QString kml = kmlWith(
        record(QStringLiteral("a")) + record(QStringLiteral("b"), QStringLiteral("todo")) +
            record(QStringLiteral("p"), QStringLiteral("note")) +
            record(QStringLiteral("q"), QStringLiteral("note")),
        {QStringLiteral("One <anchor ref=\"a\">two </anchor><anchor ref=\"a\"><anchor ref=\"b\">"
                        "three</anchor></anchor>"),
         QStringLiteral("<anchor ref=\"a\">four<anchor ref=\"p\"/><anchor ref=\"q\"/></anchor> five")});
    auto editor = editorWith(kml);
    CHECK(places(*editor) == QStringLiteral("b 8-13, a 4-18, p @18, q @18"));
    CHECK(editor->toKml() == kml);
}

TEST_CASE("Annotations KML: what the reader leaves out", "[editor][annotations][kml]") {
    auto editor = editorWith(
        QStringLiteral(
        "<kml><annotations>"
        "<annotation kind=\"comment\">No id</annotation>"
        "<annotation id=\"k\" kind=\"remark\">An unknown kind</annotation>"
        "<annotation id=\"a\" kind=\"comment\">First</annotation>"
        "<annotation id=\"a\" kind=\"todo\">A repeated id</annotation>"
        "<annotation id=\"o\" kind=\"note\">No anchor</annotation>"
        "<other>Not an annotation</other>"
        "</annotations>"
        "<p><anchor ref=\"a\">kept</anchor> <anchor ref=\"k\">plain</anchor> "
        "<anchor ref=\"x\">unknown</anchor></p>"
        "<p><anchor ref=\"late\">too late</anchor></p>") +
        QStringLiteral("<annotations>") + record(QStringLiteral("late")) +
        QStringLiteral("</annotations></kml>"));

    // A section after the anchors comes too late for them
    CHECK(places(*editor) == QStringLiteral("a 0-4"));
    CHECK(editor->plainText() == QStringLiteral("kept plain unknown\ntoo late"));
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><annotations><annotation id=\"a\" kind=\"comment\">First</annotation>"
                         "</annotations><p><anchor ref=\"a\">kept</anchor> plain unknown</p>"
                         "<p>too late</p></kml>"));
}

TEST_CASE("Annotations KML: text of several lines and special characters survive",
          "[editor][annotations][kml]") {
    auto editor = editorWith(kmlOf({QStringLiteral("Text")}));
    editor->setSelection({{0, 0}, {0, 4}});
    const Annotation added = editor->addAnnotation(
        AnnotationKind::Comment, QStringLiteral(" Line one\nLine \"two\" & <three> "),
        QStringLiteral("O'Brien & \"Co\""));

    auto reloaded = editorWith(editor->toKml());
    REQUIRE(reloaded->annotations().size() == 1);
    CHECK(reloaded->annotations().front().annotation == added);
}

TEST_CASE("Annotations KML: a time without a time zone is in UTC", "[editor][annotations][kml]") {
    auto editor = editorWith(kmlWith(
        QStringLiteral("<annotation id=\"a\" kind=\"note\" created=\"2026-01-02T03:04:05\">x</annotation>"
                       "<annotation id=\"b\" kind=\"note\" created=\"yesterday\">y</annotation>"),
        {QStringLiteral("<anchor ref=\"a\">One</anchor><anchor ref=\"b\">Two</anchor>")}));
    CHECK(annotationOf(*editor, QStringLiteral("a")).created ==
          QDateTime(QDate(2026, 1, 2), QTime(3, 4, 5), QTimeZone::utc()));

    // A date the editor does not understand stays as it is
    CHECK_FALSE(annotationOf(*editor, QStringLiteral("b")).created.isValid());
    const QString saved = editor->toKml();
    CHECK(saved.contains(QStringLiteral("created=\"2026-01-02T03:04:05Z\"")));
    CHECK(saved.contains(QStringLiteral("created=\"yesterday\"")));
}

TEST_CASE("Annotations KML: a copied range holds the annotations anchored in it",
          "[editor][annotations][kml]") {
    // "One two" 0-7, the paragraph break 7, "Three" 8-13
    auto editor = editorWith(kmlWith(record(QStringLiteral("a")) +
                                         record(QStringLiteral("b"), QStringLiteral("todo")) +
                                         record(QStringLiteral("c"), QStringLiteral("note")),
                                     {QStringLiteral("<anchor ref=\"a\">One</anchor> two<anchor ref=\"b\"/>"),
                                      QStringLiteral("<anchor ref=\"c\"/>Three")}));

    SECTION("part of a fragment") {
        editor->setSelection({{0, 1}, {0, 5}});
        const auto data = editor->createMimeDataFromSelection();
        REQUIRE(data);
        CHECK(kmlData(*data) == kmlWith(record(QStringLiteral("a")),
                                        {QStringLiteral("<anchor ref=\"a\">ne</anchor> t")}));
        CHECK(data->text() == QStringLiteral("ne t"));
        CHECK(data->html() == QStringLiteral("<p>ne t</p>"));
    }

    SECTION("a place at its end and the start of a paragraph; other programs get only the text") {
        editor->setSelection({{0, 4}, {1, 2}});
        const auto data = editor->createMimeDataFromSelection();
        REQUIRE(data);
        CHECK(kmlData(*data) == kmlWith(record(QStringLiteral("b"), QStringLiteral("todo")) +
                                            record(QStringLiteral("c"), QStringLiteral("note")),
                                        {QStringLiteral("two<anchor ref=\"b\"/>"),
                                         QStringLiteral("<anchor ref=\"c\"/>Th")}));
        CHECK(data->text() == QStringLiteral("two\nTh"));
        CHECK(data->html() == QStringLiteral("<p>two</p><p>Th</p>"));
        CHECK(ClipboardHandler::kmlToText(kmlData(*data)) == QStringLiteral("two\nTh"));
    }
}

TEST_CASE("Annotations KML: a place after a character of a surrogate pair takes the whole pair",
          "[editor][annotations][kml]") {
    // "a" + U+1F600 (two UTF-16 units) + "b"
    const QString pair = QString(QChar(0xD83D)) + QChar(0xDE00);
    auto editor = editorWith(kmlWith(record(QStringLiteral("p"), QStringLiteral("todo")),
                                     {QStringLiteral("a") + pair + QStringLiteral("<anchor ref=\"p\"/>b")}));
    REQUIRE(places(*editor) == QStringLiteral("p @3"));
    QTextCursor cursor(editor->textDocument());
    cursor.setPosition(2);
    const QTextCharFormat high = cursor.charFormat();
    cursor.setPosition(3);
    CHECK(cursor.charFormat() == high);  // both halves, one format

    SECTION("Backspace takes both halves, the place stays") {
        editor->setCursorPosition({0, 3});
        editor->deleteBackward();
        CHECK(editor->plainText() == QStringLiteral("ab"));
        CHECK(places(*editor) == QStringLiteral("p @1"));
    }

    SECTION("an annotation added after the pair") {
        editor->setCursorPosition({0, 3});
        const Annotation added =
            editor->addAnnotation(AnnotationKind::Note, QStringLiteral("x"), QString());
        CHECK(places(*editor) == QStringLiteral("p @3, ") + added.id + QStringLiteral(" @3"));
        cursor.setPosition(2);
        const QTextCharFormat first = cursor.charFormat();
        cursor.setPosition(3);
        CHECK(cursor.charFormat() == first);
    }
}

// =============================================================================
// Editing keeps the annotations where the writer put them
// =============================================================================

TEST_CASE("Annotations editing: text typed inside a fragment joins it, next to it does not",
          "[editor][annotations][editing]") {
    // "The quick fox": "quick" 4-9, a place after "fox"
    auto editor = editorWith(kmlWith(record(QStringLiteral("a")) +
                                         record(QStringLiteral("p"), QStringLiteral("todo")),
                                     {QStringLiteral("The <anchor ref=\"a\">quick</anchor> fox"
                                                     "<anchor ref=\"p\"/>")}));
    REQUIRE(places(*editor) == QStringLiteral("a 4-9, p @13"));

    SECTION("inside") {
        editor->setCursorPosition({0, 6});
        editor->insertText(QStringLiteral("X"));
        CHECK(editor->plainText() == QStringLiteral("The quXick fox"));
        CHECK(places(*editor) == QStringLiteral("a 4-10, p @14"));
    }

    SECTION("before its first character") {
        editor->setCursorPosition({0, 4});
        editor->insertText(QStringLiteral("X"));
        CHECK(places(*editor) == QStringLiteral("a 5-10, p @14"));
    }

    SECTION("after its last character") {
        editor->setCursorPosition({0, 9});
        editor->insertText(QStringLiteral("X"));
        CHECK(places(*editor) == QStringLiteral("a 4-9, p @14"));
    }

    SECTION("at a place: the text goes after it") {
        editor->setCursorPosition({0, 13});
        editor->insertText(QStringLiteral("X"));
        CHECK(editor->plainText() == QStringLiteral("The quick foxX"));
        CHECK(places(*editor) == QStringLiteral("a 4-9, p @13"));
    }

    SECTION("lines: new paragraphs of their own, the text inside the fragment") {
        editor->setCursorPosition({0, 6});
        editor->insertText(QStringLiteral("1\n2"));
        // "The qu1" 0-7, the paragraph break 7, "2ick fox" 8-16
        CHECK(editor->plainText() == QStringLiteral("The qu1\n2ick fox"));
        CHECK(places(*editor) == QStringLiteral("a 4-12, p @16"));
        CHECK(annotationIds(editor->textDocument()->findBlockByNumber(1).charFormat()).isEmpty());
    }

    SECTION("typed text is one undo step") {
        editor->setCursorPosition({0, 6});
        editor->insertText(QStringLiteral("X"));
        editor->insertText(QStringLiteral("Y"));
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("The quick fox"));
        CHECK(places(*editor) == QStringLiteral("a 4-9, p @13"));
    }
}

TEST_CASE("Annotations editing: deleted characters leave their annotations on their place",
          "[editor][annotations][editing]") {
    // "The ab fox" 0-10: "ab" 4-6, a place after "fox"; the paragraph break 10; "Next" 11-15
    // with a place at its start
    const QString kml = kmlWith(record(QStringLiteral("a")) +
                                    record(QStringLiteral("p"), QStringLiteral("todo")) +
                                    record(QStringLiteral("s"), QStringLiteral("note")),
                                {QStringLiteral("The <anchor ref=\"a\">ab</anchor> fox<anchor ref=\"p\"/>"),
                                 QStringLiteral("<anchor ref=\"s\"/>Next")});
    auto editor = editorWith(kml);
    REQUIRE(places(*editor) == QStringLiteral("a 4-6, p @10, s @11"));

    SECTION("Backspace over a whole fragment") {
        editor->setCursorPosition({0, 6});
        editor->deleteBackward();
        CHECK(places(*editor) == QStringLiteral("a 4-5, p @9, s @10"));
        editor->deleteBackward();
        CHECK(editor->plainText() == QStringLiteral("The  fox\nNext"));
        CHECK(places(*editor) == QStringLiteral("a @4, p @8, s @9"));
    }

    SECTION("Delete over a whole fragment") {
        editor->setCursorPosition({0, 4});
        editor->deleteForward();
        editor->deleteForward();
        CHECK(places(*editor) == QStringLiteral("a @4, p @8, s @9"));
    }

    SECTION("Backspace or Delete on the character of a place") {
        editor->setCursorPosition({0, 10});
        editor->deleteBackward();
        CHECK(places(*editor) == QStringLiteral("a 4-6, p @9, s @10"));
        editor->setCursorPosition({0, 8});
        editor->deleteForward();
        CHECK(editor->plainText() == QStringLiteral("The ab f\nNext"));
        CHECK(places(*editor) == QStringLiteral("a 4-6, p @8, s @9"));
    }

    SECTION("Backspace joining paragraphs keeps the place of the paragraph start") {
        editor->setCursorPosition({1, 0});
        editor->deleteBackward();
        CHECK(editor->plainText() == QStringLiteral("The ab foxNext"));
        CHECK(places(*editor) == QStringLiteral("a 4-6, p @10, s @10"));
    }

    SECTION("Delete joining paragraphs too") {
        editor->setCursorPosition({0, 10});
        editor->deleteForward();
        CHECK(places(*editor) == QStringLiteral("a 4-6, p @10, s @10"));
    }

    SECTION("an annotation deleted with its character comes back with undo") {
        editor->setCursorPosition({0, 10});
        editor->deleteBackward();
        editor->undo();
        CHECK(editor->toKml() == kml);
    }

    SECTION("deleting plain text is one undo step") {
        editor->setCursorPosition({1, 4});
        editor->deleteBackward();
        editor->deleteBackward();
        CHECK(editor->plainText() == QStringLiteral("The ab fox\nNe"));
        editor->undo();
        CHECK(editor->toKml() == kml);
    }
}

TEST_CASE("Annotations editing: a deleted selection leaves its annotations on its place",
          "[editor][annotations][editing]") {
    const QString kml = kmlWith(record(QStringLiteral("a")) +
                                    record(QStringLiteral("p"), QStringLiteral("todo")) +
                                    record(QStringLiteral("s"), QStringLiteral("note")),
                                {QStringLiteral("The <anchor ref=\"a\">ab</anchor> fox<anchor ref=\"p\"/>"),
                                 QStringLiteral("<anchor ref=\"s\"/>Next")});
    auto editor = editorWith(kml);

    SECTION("a whole fragment and a place") {
        editor->setSelection({{0, 3}, {0, 10}});
        REQUIRE(editor->deleteSelectedText());
        CHECK(editor->plainText() == QStringLiteral("The\nNext"));
        CHECK(places(*editor) == QStringLiteral("a @3, p @3, s @4"));
        editor->undo();
        CHECK(editor->toKml() == kml);
    }

    SECTION("part of a fragment: it stays on the rest") {
        editor->setSelection({{0, 5}, {0, 8}});
        REQUIRE(editor->deleteSelectedText());
        CHECK(editor->plainText() == QStringLiteral("The aox\nNext"));
        CHECK(places(*editor) == QStringLiteral("a 4-5, p @7, s @8"));
    }

    SECTION("over the start of a paragraph with a place") {
        editor->setSelection({{0, 8}, {1, 2}});
        REQUIRE(editor->deleteSelectedText());
        CHECK(editor->plainText() == QStringLiteral("The ab fxt"));
        CHECK(places(*editor) == QStringLiteral("a 4-6, p @8, s @8"));
    }
}

TEST_CASE("Annotations editing: text that replaces a selection", "[editor][annotations][editing]") {
    // "The quick brown fox": "quick brown" 4-15, a place after "fox"
    const QString kml = kmlWith(record(QStringLiteral("a")) +
                                    record(QStringLiteral("p"), QStringLiteral("todo")),
                                {QStringLiteral("The <anchor ref=\"a\">quick brown</anchor> fox"
                                                "<anchor ref=\"p\"/>")});
    auto editor = editorWith(kml);
    REQUIRE(places(*editor) == QStringLiteral("a 4-15, p @19"));

    SECTION("inside a fragment joins it") {
        editor->setSelection({{0, 10}, {0, 15}});
        editor->insertText(QStringLiteral("red"));
        CHECK(editor->plainText() == QStringLiteral("The quick red fox"));
        CHECK(places(*editor) == QStringLiteral("a 4-13, p @17"));
    }

    SECTION("a whole fragment: the new text takes its place in it") {
        editor->setSelection({{0, 4}, {0, 15}});
        editor->insertText(QStringLiteral("slow"));
        CHECK(places(*editor) == QStringLiteral("a 4-8, p @12"));
    }

    SECTION("over the end of a fragment and a place") {
        editor->setSelection({{0, 12}, {0, 19}});
        editor->insertText(QStringLiteral("X"));
        CHECK(editor->plainText() == QStringLiteral("The quick brX"));
        CHECK(places(*editor) == QStringLiteral("a 4-12, p @12"));
    }

    SECTION("one undo step") {
        editor->setSelection({{0, 12}, {0, 19}});
        editor->insertText(QStringLiteral("X"));
        editor->undo();
        CHECK(editor->toKml() == kml);
    }
}

TEST_CASE("Annotations editing: a new paragraph does not take the annotations of its place",
          "[editor][annotations][editing]") {
    auto editor = editorWith(kmlWith(record(QStringLiteral("a")) +
                                         record(QStringLiteral("p"), QStringLiteral("todo")),
                                     {QStringLiteral("The <anchor ref=\"a\">quick</anchor> fox"
                                                     "<anchor ref=\"p\"/>")}));

    SECTION("inside a fragment: both paragraphs keep their part of it") {
        editor->setCursorPosition({0, 6});
        editor->insertNewline();
        // "The qu" 0-6, the paragraph break 6, "ick fox" 7-14
        CHECK(places(*editor) == QStringLiteral("a 4-10, p @14"));
        CHECK(annotationIds(editor->textDocument()->findBlockByNumber(1).charFormat()).isEmpty());
    }

    SECTION("at a place") {
        editor->setCursorPosition({0, 13});
        editor->insertNewline();
        CHECK(editor->plainText() == QStringLiteral("The quick fox\n"));
        CHECK(places(*editor) == QStringLiteral("a 4-9, p @13"));
        CHECK(annotationIds(editor->textDocument()->findBlockByNumber(1).charFormat()).isEmpty());
    }

    SECTION("over a selection with a place") {
        editor->setSelection({{0, 10}, {0, 13}});
        editor->insertNewline();
        CHECK(editor->plainText() == QStringLiteral("The quick \n"));
        CHECK(places(*editor) == QStringLiteral("a 4-9, p @10"));
    }
}

TEST_CASE("Annotations editing: pasted and dropped text brings its annotations",
          "[editor][annotations][editing]") {
    // "The quick fox" 0-13: "quick" 4-9, a place after "fox"; the paragraph break 13;
    // "End" 14-17 with a place at its start
    const QString kml = kmlWith(record(QStringLiteral("a")) +
                                    record(QStringLiteral("p"), QStringLiteral("todo")) +
                                    record(QStringLiteral("s"), QStringLiteral("note")),
                                {QStringLiteral("The <anchor ref=\"a\">quick</anchor> fox<anchor ref=\"p\"/>"),
                                 QStringLiteral("<anchor ref=\"s\"/>End")});
    auto editor = editorWith(kml);
    REQUIRE(places(*editor) == QStringLiteral("a 4-9, p @13, s @14"));

    SECTION("a copy gets a new id") {
        editor->setSelection({{0, 4}, {0, 9}});
        const auto data = editor->createMimeDataFromSelection();
        editor->clearSelection();
        editor->setCursorPosition({1, 3});
        editor->insertFromMimeData(data.get());
        REQUIRE(editor->annotations().size() == 4);
        const Annotation copy = editor->annotations().back().annotation;
        CHECK(copy.id != QStringLiteral("a"));
        CHECK(places(*editor) == QStringLiteral("a 4-9, p @13, s @14, ") + copy.id +
                                     QStringLiteral(" 17-22"));
        Annotation original = annotationOf(*editor, QStringLiteral("a"));
        original.id = copy.id;
        CHECK(copy == original);
    }

    SECTION("pasted into its own fragment, it joins it") {
        editor->setSelection({{0, 5}, {0, 7}});
        const auto data = editor->createMimeDataFromSelection();
        editor->clearSelection();
        editor->setCursorPosition({0, 8});
        editor->insertFromMimeData(data.get());
        CHECK(editor->plainText() == QStringLiteral("The quicuik fox\nEnd"));
        CHECK(places(*editor) == QStringLiteral("a 4-11, p @15, s @16"));
    }

    SECTION("plain text pasted inside a fragment joins it") {
        QMimeData data;
        data.setText(QStringLiteral("XY"));
        editor->setCursorPosition({0, 6});
        editor->insertFromMimeData(&data);
        CHECK(places(*editor) == QStringLiteral("a 4-11, p @15, s @16"));
    }

    SECTION("the place of a copied paragraph start goes to the place of the text") {
        editor->setSelection({{1, 0}, {1, 3}});
        const auto data = editor->createMimeDataFromSelection();
        editor->clearSelection();
        editor->setCursorPosition({0, 4});
        editor->insertFromMimeData(data.get());
        // "The Endquick fox": the copy of the place before "End"
        REQUIRE(editor->annotations().size() == 4);
        const QString copy = editor->annotations().front().annotation.id;
        CHECK(copy != QStringLiteral("s"));
        CHECK(places(*editor) == copy + QStringLiteral(" @4, a 7-12, p @16, s @17"));
    }

    SECTION("dropped text moves with them, as one undo step") {
        editor->setSelection({{0, 4}, {0, 10}});
        const auto data = editor->createMimeDataFromSelection();
        REQUIRE(editor->dropMimeData(data.get(), {1, 3}, true));
        // "The fox" 0-7, the paragraph break 7, "Endquick " 8-17
        CHECK(editor->plainText() == QStringLiteral("The fox\nEndquick "));
        CHECK(places(*editor) == QStringLiteral("p @7, s @8, a 11-16"));
        editor->undo();
        CHECK(editor->toKml() == kml);
    }
}

TEST_CASE("Annotations editing: cut text takes its annotations to the clipboard",
          "[editor][annotations][editing][clipboard]") {
    auto editor = editorWith(kmlWith(record(QStringLiteral("a")) +
                                         record(QStringLiteral("p"), QStringLiteral("todo")),
                                     {QStringLiteral("The <anchor ref=\"a\">quick</anchor> fox"
                                                     "<anchor ref=\"p\"/>"),
                                      QStringLiteral("End")}));
    editor->setSelection({{0, 4}, {0, 10}});
    editor->cut();
    CHECK(editor->plainText() == QStringLiteral("The fox\nEnd"));
    CHECK(places(*editor) == QStringLiteral("p @7"));

    editor->clearSelection();
    editor->setCursorPosition({1, 3});
    editor->paste();
    CHECK(editor->plainText() == QStringLiteral("The fox\nEndquick "));
    CHECK(places(*editor) == QStringLiteral("p @7, a 11-16"));  // the same annotation
}

TEST_CASE("Annotations editing: replacing found text keeps the annotations",
          "[editor][annotations][editing][search]") {
    // "The quick fox quick": "quick" 4-9, a place after "fox"
    const QString kml = kmlWith(record(QStringLiteral("a")) +
                                    record(QStringLiteral("p"), QStringLiteral("todo")),
                                {QStringLiteral("The <anchor ref=\"a\">quick</anchor> fox"
                                                "<anchor ref=\"p\"/> quick")});
    auto editor = editorWith(kml);
    SearchEngine engine;
    engine.setDocument(editor->textDocument());

    SECTION("a replaced fragment keeps its annotation") {
        engine.setSearchText(QStringLiteral("quick"));
        engine.setReplaceText(QStringLiteral("slow"));
        engine.findAll();
        CHECK(engine.replaceAll() == 2);
        CHECK(editor->plainText() == QStringLiteral("The slow fox slow"));
        CHECK(places(*editor) == QStringLiteral("a 4-8, p @12"));
        editor->undo();
        CHECK(editor->toKml() == kml);
    }

    SECTION("a replaced character of a place leaves it on the place") {
        engine.setSearchText(QStringLiteral("fox"));
        engine.setReplaceText(QStringLiteral("cat"));
        engine.findAll();
        REQUIRE(engine.setCurrentMatchIndex(0));
        REQUIRE(engine.replaceCurrent());
        CHECK(editor->plainText() == QStringLiteral("The quick cat quick"));
        CHECK(places(*editor) == QStringLiteral("a 4-9, p @10"));
    }
}
