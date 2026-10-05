/// @file test_editor_stage1.cpp
/// @brief Editor Stage 1 (quick fixes): regression tests for the defects it fixed
///
/// Stage 0 tests that turned green in Stage 1 stay in the stage0 files; this file covers
/// behaviour that Stage 1 introduced or fixed without a Stage 0 test.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/editor_render_pipeline.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/kml_parser.h>
#include <kalahari/editor/render_context.h>
#include <kalahari/gui/find_replace_bar.h>
#include "editor_test_utils.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QLineEdit>
#include <QPaintEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QString>
#include <memory>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;
namespace core = kalahari::core;

namespace {

/// Counts of the editor's whole text, computed from scratch - the oracle for the cache.
core::TextCounts countsFromScratch(const BookEditor& editor) {
    return core::countText(editor.plainText());
}

/// A paragraph long enough to wrap onto several lines in a 400-900 px wide editor.
QStringList longParagraphs(int count) {
    QStringList list;
    for (int i = 0; i < count; ++i) {
        list << QStringLiteral("Paragraph %1 has enough words to wrap onto several lines in "
                               "an editor of ordinary width, so that its height depends on "
                               "the wrap width and on the size of the font.").arg(i);
    }
    return list;
}

qreal documentHeight(const BookEditor& editor) {
    return editor.textDocument()->documentLayout()->documentSize().height();
}

/// Records the areas of the paint events a widget receives
class PaintRecorder : public QObject {
public:
    explicit PaintRecorder(QWidget& widget) : m_widget(widget) { widget.installEventFilter(this); }
    ~PaintRecorder() override { m_widget.removeEventFilter(this); }

    std::vector<QRect> rects;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == &m_widget && event->type() == QEvent::Paint) {
            rects.push_back(static_cast<QPaintEvent*>(event)->rect());
        }
        return false;
    }

private:
    QWidget& m_widget;
};

/// The find/replace bar of @p editor, open in replace mode with both texts filled in
kalahari::gui::FindReplaceBar* openReplaceBar(BookEditor& editor, const QString& find,
                                              const QString& replace) {
    editor.showFindReplace();
    auto* bar = editor.findChild<kalahari::gui::FindReplaceBar*>();
    if (bar) {
        for (QLineEdit* input : bar->findChildren<QLineEdit*>()) {
            if (input->placeholderText() == QStringLiteral("Replace...")) {
                input->setText(replace);
            }
        }
        bar->setSearchText(find);
    }
    return bar;
}

/// The push button of @p bar labelled @p text
QPushButton* barButton(QWidget& bar, const QString& text) {
    for (QPushButton* button : bar.findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

}  // anonymous namespace

// =============================================================================
// Word count: one definition, cached per paragraph
// =============================================================================

TEST_CASE("Stage1 word count: cached counts follow every kind of edit",
          "[editor][stage1][statistics]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral(
        "<kml><p>One two three</p><p>– Tak – powiedział.</p><p>Last one</p></kml>"));

    // Dialogue dashes are not words
    REQUIRE(editor.wordCount() == 7);
    REQUIRE(editor.characterCountNoSpaces() ==
            static_cast<size_t>(countsFromScratch(editor).nonSpaceCharacters));

    auto checkAgainstScratch = [&editor]() {
        const core::TextCounts expected = countsFromScratch(editor);
        CHECK(editor.wordCount() == static_cast<size_t>(expected.words));
        CHECK(editor.characterCountNoSpaces() ==
              static_cast<size_t>(expected.nonSpaceCharacters));
    };

    SECTION("typing within a paragraph") {
        editor.setCursorPosition({0, 13});
        editor.insertText(QStringLiteral(" four"));
        CHECK(editor.wordCount() == 8);
        checkAgainstScratch();
    }

    SECTION("pasting several paragraphs") {
        editor.setCursorPosition({1, 0});
        editor.insertText(QStringLiteral("New a\nNew b c\nNew d "));
        CHECK(editor.paragraphCount() == 5);
        CHECK(editor.wordCount() == 14);
        checkAgainstScratch();
    }

    SECTION("deleting across paragraphs merges them") {
        editor.setSelection({{0, 4}, {2, 5}});  // "two three" ... "Last "
        REQUIRE(editor.deleteSelectedText());
        CHECK(editor.plainText() == QStringLiteral("One one"));
        CHECK(editor.wordCount() == 2);
        checkAgainstScratch();
    }

    SECTION("undo and redo") {
        editor.setSelection({{0, 4}, {2, 5}});
        REQUIRE(editor.deleteSelectedText());
        editor.undo();
        CHECK(editor.wordCount() == 7);
        checkAgainstScratch();
        editor.redo();
        CHECK(editor.wordCount() == 2);
        checkAgainstScratch();
    }

    SECTION("joining two words removes one") {
        editor.setSelection({{0, 3}, {0, 4}});  // the space in "One two"
        REQUIRE(editor.deleteSelectedText());
        CHECK(editor.wordCount() == 6);
        checkAgainstScratch();
    }
}

TEST_CASE("Stage1 word count: a saved chapter reports the editor's count",
          "[editor][stage1][statistics]") {
    // Formatting inside a word, entities, metadata anchors and dialogue dashes: the
    // chapter file (ChapterDocument, from KML) and the editor must agree.
    BookEditor editor;
    editor.fromKml(QStringLiteral(
        "<kml><p>Nie<i>zwykle</i> ważne: Tom &amp; Jerry</p>"
        "<p>– Tak – <comment id=\"c1\">powiedział</comment> cicho.</p>"
        "<p></p><p>Przypis<footnote id=\"f1\">1</footnote> i <todo id=\"t1\">zadanie</todo>.</p>"
        "</kml>"));

    const core::ChapterDocument chapter(editor.toKml());
    CHECK(editor.wordCount() == 10);
    CHECK(chapter.wordCount() == static_cast<int>(editor.wordCount()));
    CHECK(chapter.characterCount() == static_cast<int>(editor.characterCountNoSpaces()));
    CHECK(chapter.plainText() == editor.plainText());
    CHECK(chapter.paragraphCount() == 3);  // the empty paragraph does not count
}

TEST_CASE("Stage1 word count: a chapter file counts the text the editor shows",
          "[editor][stage1][statistics]") {
    // The chapter file's statistics (ChapterDocument, also used for snapshots) come from
    // the stored KML, which need not be the editor's own output: it must be read the way
    // the editor reads it (KmlDocumentModel), before any save rewrites it.
    auto checkText = [](const QString& kml, const QString& expectedText) {
        BookEditor editor;
        editor.fromKml(kml);
        const core::ChapterDocument chapter(kml);
        CHECK(editor.plainText() == expectedText);
        CHECK(chapter.plainText() == expectedText);
        CHECK(chapter.wordCount() == static_cast<int>(editor.wordCount()));
    };

    SECTION("paragraphs without a root element") {
        // Regression: the reader stopped at the second top-level element
        checkText(QStringLiteral("<p>One two</p><p>Three</p>\n<p>Four</p>"),
                  QStringLiteral("One two\nThree\nFour"));
    }

    SECTION("an unknown element inside a paragraph") {
        // Regression: its text was counted, though the editor skips it
        checkText(QStringLiteral(
                      "<kml><p>One <future a=\"1\">hidden <b>words</b></future> two</p></kml>"),
                  QStringLiteral("One  two"));
    }

    SECTION("unknown elements and text outside paragraphs") {
        checkText(QStringLiteral(
                      "<kml><p>Kept</p><section><p>Hidden</p></section>stray<p>Last</p></kml>"),
                  QStringLiteral("Kept\nLast"));
    }
}

// =============================================================================
// Find and replace: a replacement is an edit like any other
// =============================================================================

TEST_CASE("Stage1 find and replace: replacing text is reported as a content change",
          "[editor][stage1][search]") {
    // Regression: the bar edits the document directly, and neither Replace nor Replace
    // All emitted contentChanged - a chapter changed only by them was not marked unsaved
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>one two one</p><p>one</p></kml>"));
    int changes = 0;
    QObject::connect(&editor, &BookEditor::contentChanged, [&changes]() { ++changes; });

    auto* bar = openReplaceBar(editor, QStringLiteral("one"), QStringLiteral("1"));
    REQUIRE(bar != nullptr);
    QPushButton* replace = barButton(*bar, QStringLiteral("Replace"));
    QPushButton* replaceAll = barButton(*bar, QStringLiteral("Replace All"));
    REQUIRE(replace != nullptr);
    REQUIRE(replaceAll != nullptr);

    SECTION("Replace, occurrence after occurrence") {
        // The first click selects the first occurrence; each further click replaces the
        // selected one and selects the next (regression: every other one was skipped)
        replace->click();
        CHECK(editor.selectedText() == QStringLiteral("one"));
        CHECK(editor.selection().start == CursorPosition{0, 0});
        CHECK(changes == 0);

        replace->click();
        CHECK(editor.plainText() == QStringLiteral("1 two one\none"));
        CHECK(changes == 1);
        CHECK(editor.selectedText() == QStringLiteral("one"));
        CHECK(editor.selection().start == CursorPosition{0, 6});

        replace->click();
        CHECK(editor.plainText() == QStringLiteral("1 two 1\none"));
        CHECK(editor.selection().start == CursorPosition{1, 0});

        replace->click();
        CHECK(editor.plainText() == QStringLiteral("1 two 1\n1"));
        CHECK(changes == 3);
        CHECK_FALSE(editor.hasSelection());
    }

    SECTION("Replace All") {
        editor.setCursorPosition({1, 3});  // after the last "one", beyond the shorter "1"
        replaceAll->click();
        CHECK(editor.plainText() == QStringLiteral("1 two 1\n1"));
        CHECK(changes == 1);
        CHECK(editor.cursorPosition() == CursorPosition{1, 1});
    }

    SECTION("text typed between two replacements") {
        replace->click();  // selects the first "one"

        // Regression: the matches kept their old positions, so Replace changed whatever
        // text had moved to where the selected occurrence used to be
        editor.clearSelection();
        editor.setCursorPosition({0, 0});
        editor.insertText(QStringLiteral("Zero "));
        replace->click();  // selects an occurrence again ...
        replace->click();  // ... and replaces it
        CHECK(editor.plainText() == QStringLiteral("Zero 1 two one\none"));
    }
}

// =============================================================================
// Scrolling after a zoom
// =============================================================================

TEST_CASE("Stage1 scrolling: the scroll range follows the document height after a zoom",
          "[editor][stage1][layout]") {
    // Regression: the scroll range was refreshed on content changes only, so after a zoom
    // (or a delayed width change) re-wrapped the text, the end of the chapter could not be
    // reached, or the scroll bar ran past it.
    BookEditor editor;
    resizeWidget(editor, QSize(600, 300));
    editor.fromKml(kmlOf(longParagraphs(30)));
    const int maxBefore = editor.verticalScrollBar()->maximum();
    const qreal heightBefore = documentHeight(editor);

    editor.setZoomFactor(2.0);
    REQUIRE(documentHeight(editor) > heightBefore * 1.5);
    CHECK(editor.verticalScrollBar()->maximum() - maxBefore ==
          Approx(documentHeight(editor) - heightBefore).margin(1.0));
}

// =============================================================================
// Fonts: the document default font, never baked into the text
// =============================================================================

TEST_CASE("Stage1 KML: font settings and zoom are not saved into the chapter",
          "[editor][stage1][kml]") {
    // Regression: setAppearance() merged the scaled font family and size into every
    // character (as an undoable edit), and loading baked them in too - so a saved chapter
    // got font="..." size="..." on every run and stopped following the font settings.
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>Plain <b>bold</b> text</p><p>Second</p></kml>"));

    EditorAppearance appearance = editor.appearance();
    appearance.typography.textFont = QFont(QStringLiteral("Arial"), 17);
    editor.setAppearance(appearance);
    editor.setZoomFactor(1.5);
    CHECK_FALSE(editor.canUndo());
    CHECK(editor.textDocument()->defaultFont().pointSizeF() == Approx(17.0 * 1.5));

    editor.setCursorPosition({1, 6});
    editor.insertText(QStringLiteral(" typed"));
    CHECK(editor.toKml() ==
          QStringLiteral("<kml><p>Plain <b>bold</b> text</p><p>Second typed</p></kml>"));
}

// =============================================================================
// KML: what loading used to lose
// =============================================================================

TEST_CASE("Stage1 KML: an unknown inline element does not cut off the rest of the paragraph",
          "[editor][stage1][kml]") {
    // Regression: after skipping an unknown element the parser stood on its end tag and
    // took it for the end of the enclosing element, so the text after it was lost.
    const QString kml = QStringLiteral(
        "<kml><p>Before <future a=\"1\">skipped</future> after <b>bold</b></p>"
        "<p>Next</p></kml>");
    const QString expectedText = QStringLiteral("Before  after bold\nNext");

    SECTION("the editor (KmlDocumentModel)") {
        BookEditor editor;
        editor.fromKml(kml);
        CHECK(editor.plainText() == expectedText);
        CHECK(editor.toKml() ==
              QStringLiteral("<kml><p>Before  after <b>bold</b></p><p>Next</p></kml>"));
    }

    SECTION("KmlParser") {
        KmlParser parser;
        std::unique_ptr<QTextDocument> doc(parser.parseKml(kml));
        REQUIRE(doc != nullptr);
        CHECK(doc->toPlainText() == expectedText);
    }
}

TEST_CASE("Stage1 KML: character and location references survive load and save",
          "[editor][stage1][kml]") {
    // Regression: loading kept the text of <charref>/<locref> but dropped the elements.
    // Unknown attributes of metadata elements are kept too.
    const QString kml = QStringLiteral(
        "<kml><p>Spotkał <charref id=\"r1\" target=\"anna\">Annę</charref> w "
        "<locref id=\"r2\" target=\"krakow\">Krakowie</locref>"
        "<comment id=\"c1\" thread=\"t9\">.</comment></p></kml>");

    BookEditor editor;
    editor.fromKml(kml);
    CHECK(editor.toKml() == kml);
}

TEST_CASE("Stage1 KML: a TODO inside a comment keeps both on save", "[editor][stage1][kml]") {
    // Regression: the serializer wrote only the first metadata element of a run, so a TODO
    // anchored inside a comment lost its <todo> when the chapter was saved.
    BookEditor editor;
    editor.fromKml(QStringLiteral(
        "<kml><p><comment id=\"c1\">abc <todo id=\"t1\">def</todo></comment> ghi</p></kml>"));
    const QString saved = editor.toKml();

    BookEditor reloaded;
    reloaded.fromKml(saved);
    REQUIRE(reloaded.textDocument() != nullptr);
    const QTextBlock block = reloaded.textDocument()->begin();
    const QTextCharFormat inner = formatOfFragmentContaining(block, QStringLiteral("def"));
    CHECK(metadataOf(inner, KmlPropComment).value(QStringLiteral("id")) == QStringLiteral("c1"));
    CHECK(metadataOf(inner, KmlPropTodo).value(QStringLiteral("id")) == QStringLiteral("t1"));

    const QTextCharFormat outer = formatOfFragmentContaining(block, QStringLiteral("abc"));
    CHECK(metadataOf(outer, KmlPropComment).value(QStringLiteral("id")) == QStringLiteral("c1"));
    CHECK_FALSE(outer.hasProperty(KmlPropTodo));
    CHECK_FALSE(formatOfFragmentContaining(block, QStringLiteral("ghi")).hasProperty(KmlPropComment));

    CHECK(reloaded.plainText() == QStringLiteral("abc def ghi"));
    CHECK(reloaded.toKml() == saved);  // stable from the first save on
}

TEST_CASE("Stage1 KML: toggling a TODO keeps its other attributes", "[editor][stage1][kml]") {
    // Regression: a toggled marker was written back with only the attributes TextMarker
    // has fields for, so the next save dropped the rest
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p><todo id=\"t1\" owner=\"Ann\">text</todo> rest</p></kml>"));
    editor.setCursorPosition({0, 0});
    editor.toggleTodoAtCursor();
    CHECK(editor.toKml() ==
          QStringLiteral("<kml><p><todo id=\"t1\" completed=\"true\" owner=\"Ann\">text</todo>"
                         " rest</p></kml>"));
}

// =============================================================================
// Cursor: one blink timer, only while focused, repainting only the cursor
// =============================================================================

TEST_CASE("Stage1 cursor: an editor that never had focus does not blink",
          "[editor][stage1][cursor]") {
    // Regression: the blink timers started when the editor was created
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>Text</p></kml>"));
    editor.setCursorBlinkInterval(100);

    int toggles = 0;
    bool lastState = editor.isCursorVisible();
    runEventLoop(350, [&editor, &toggles, &lastState] {
        if (editor.isCursorVisible() != lastState) {
            lastState = !lastState;
            ++toggles;
        }
    });
    CHECK(toggles == 0);
}

TEST_CASE("Stage1 cursor: blinks only while the editor has focus, repainting just the cursor",
          "[editor][stage1][cursor]") {
    // Regression: two timers blinked the cursor at different rates (an irregular blink,
    // and the whole editor repainted twice per blink), also without focus, and with
    // blinking turned off the cursor could stay hidden.
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs window focus without a window on screen: run with QT_QPA_PLATFORM=offscreen");
    }

    BookEditor editor;
    editor.resize(500, 300);
    editor.fromKml(kmlOf(longParagraphs(5)));
    editor.setCursorBlinkInterval(100);
    editor.show();
    editor.activateWindow();
    editor.setFocus();
    if (!waitUntil([&editor] { return editor.hasFocus(); })) {
        SKIP("the platform did not give the editor focus");
    }
    runEventLoop(150);  // the first full paint

    int toggles = 0;
    bool lastState = editor.isCursorVisible();
    auto countToggles = [&editor, &toggles, &lastState] {
        if (editor.isCursorVisible() != lastState) {
            lastState = !lastState;
            ++toggles;
        }
    };

    SECTION("focused: blinks, repainting only the cursor") {
        PaintRecorder paints(editor);
        runEventLoop(550, countToggles);
        CHECK(toggles >= 3);
        CHECK_FALSE(paints.rects.empty());
        for (const QRect& rect : paints.rects) {
            INFO("repainted " << rect.width() << "x" << rect.height());
            CHECK(rect.width() < 20);
        }
    }

    SECTION("without focus: hidden and not blinking") {
        editor.clearFocus();
        REQUIRE_FALSE(editor.hasFocus());
        lastState = editor.isCursorVisible();
        runEventLoop(350, countToggles);
        CHECK(toggles == 0);
    }

    SECTION("blinking turned off: the cursor stays drawn") {
        editor.setCursorBlinkingEnabled(false);
        CHECK(editor.isCursorVisible());
        lastState = true;
        runEventLoop(350, countToggles);
        CHECK(toggles == 0);

        // The cursor is drawn only while focused, so the two images differ by it
        const QImage focused = editor.grab().toImage();
        editor.clearFocus();
        const QImage unfocused = editor.grab().toImage();
        CHECK(focused != unfocused);
    }
}

// =============================================================================
// DPI: the logical DPI Qt uses for fonts, for pages and margins too
// =============================================================================

TEST_CASE("Stage1 DPI: the text size depends on the font and zoom, not on the screen DPI",
          "[editor][stage1][dpi]") {
    // Regression: the font was scaled by the physical DPI over 96 (1.48 on a laptop
    // screen at 125%), on top of Qt's own conversion of points with the logical DPI.
    EditorRenderPipeline pipeline;
    RenderContext context;
    context.font = QFont(QStringLiteral("Arial"), 12);

    for (double dpi : {72.0, 96.0, 142.4}) {
        context.screenDpi = dpi;
        pipeline.configure(context);
        CHECK(pipeline.context().computed.effectiveFont.pointSizeF() == Approx(12.0));
    }

    context.zoomFactor = 1.5;
    pipeline.configure(context);
    CHECK(pipeline.context().computed.effectiveFont.pointSizeF() == Approx(18.0));

    SECTION("the editor's document gets the font size from the settings") {
        BookEditor editor;
        editor.fromKml(QStringLiteral("<kml><p>Text</p></kml>"));
        CHECK(editor.textDocument()->defaultFont().pointSizeF() ==
              Approx(editor.appearance().typography.textFont.pointSizeF()));
    }
}

TEST_CASE("Stage1 DPI: an A4 page has its true size at the screen DPI", "[editor][stage1][dpi]") {
    // Regression: the page was converted with a different scale than the text, so on a
    // high-DPI screen it was 75% of A4 relative to the text.
    EditorRenderPipeline pipeline;
    RenderContext context;
    context.viewMode = ViewMode::Page;
    context.pageMode.pageSize = QSizeF(595.28, 841.89);  // A4 in points

    context.screenDpi = 96.0;
    pipeline.configure(context);
    CHECK(pipeline.context().computed.pageWidthPixels == Approx(793.7).margin(0.1));
    CHECK(pipeline.context().computed.pageHeightPixels == Approx(1122.5).margin(0.1));
    CHECK(pipeline.context().computed.mmToPixels == Approx(96.0 / 25.4));

    context.screenDpi = 144.0;
    pipeline.configure(context);
    CHECK(pipeline.context().computed.pageWidthPixels == Approx(1190.6).margin(0.1));
    CHECK(pipeline.context().computed.pageHeightPixels == Approx(1683.8).margin(0.1));
}
