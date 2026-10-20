/// @file test_styles_view.cpp
/// @brief How the editor shows the paragraph styles, the objects of the text and the
///        formatting marks
///
/// Normal follows the settings and the other styles keep the proportions of their package; a
/// page break starts a new page, a heading never stays alone at the bottom of a page; a scene
/// break looks as the settings say; the formatting marks show the spaces, the breaks and the
/// ends of the paragraphs over the text, without moving it.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_package.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/formatting_marks.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_styles.h>
#include "editor_test_utils.h"

#include <QFontMetricsF>
#include <QImage>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

namespace {

/// A style of a package
kalahari::core::PackageParagraphStyle packageStyle(const QString& id,
                                                   const QVariantMap& properties = {}) {
    kalahari::core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), id);
    style.properties = properties;
    return style;
}

/// Normal of 12 pt and a heading of 20 pt, centred, kept with the paragraph after it (or not)
ParagraphStyleSet bookStyles(bool keepWithNext = true) {
    return ParagraphStyleSet::fromPackage(
        {packageStyle(QStringLiteral("normal"), {{QStringLiteral("fontSize"), 12}}),
         packageStyle(QStringLiteral("heading_1"),
                      {{QStringLiteral("fontSize"), 20},
                       {QStringLiteral("alignment"), QStringLiteral("center")},
                       {QStringLiteral("keepWithNext"), keepWithNext}})},
        QStringLiteral("en"));
}

/// Light paper, black text, the marks in a color of their own
EditorAppearance lightAppearance() {
    EditorAppearance appearance;
    appearance.colorMode = EditorColorMode::Light;
    appearance.colors.continuous.backgroundLight = QColor(255, 255, 255);
    appearance.colors.continuous.textLight = QColor(0, 0, 0);
    appearance.colors.formattingMarks.light = QColor(0, 200, 0);
    appearance.cursor.blinking = false;
    return appearance;
}

/// An editor with the book's styles, light paper and KML loaded
std::unique_ptr<BookEditor> editorWith(const QString& content, bool keepWithNext = true) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->setAppearance(lightAppearance());
    editor->setParagraphStyles(bookStyles(keepWithNext));
    editor->fromKml(QStringLiteral("<kml>") + content + QStringLiteral("</kml>"));
    return editor;
}

KalahariTextDocumentLayout* layoutOf(const BookEditor& editor) {
    return qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
}

/// The block of a paragraph, laid out
QTextBlock blockOf(const BookEditor& editor, int paragraph) {
    const QTextBlock block = editor.textDocument()->findBlockByNumber(paragraph);
    KalahariTextDocumentLayout::blockLayout(block);
    return block;
}

/// Top of a line of a paragraph, in document coordinates
qreal lineTop(const BookEditor& editor, int paragraph, int line) {
    const QTextBlock block = blockOf(editor, paragraph);
    return layoutOf(editor)->blockY(paragraph) + block.layout()->lineAt(line).y();
}

/// The page a document y is on in the page view
int pageOf(const BookEditor& editor, qreal y) {
    return static_cast<int>(std::floor((y + 0.01) / layoutOf(editor)->pageFlow().pitch));
}

/// What the editor shows, one pixel per logical pixel
QImage editorImage(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// The band of the widget a paragraph's first line takes (the caret before it)
QRect bandOf(BookEditor& editor, int paragraph) {
    editor.setCursorPosition({paragraph, 0});
    const QRect caret = editor.inputMethodQuery(Qt::ImCursorRectangle).toRect();
    return QRect(0, caret.top(), editor.width(), caret.height());
}

/// Pixels of a band that are dark: the ink of the text's color
int inkIn(const QImage& image, const QRect& band) {
    int count = 0;
    for (int y = std::max(0, band.top()); y <= std::min(image.height() - 1, band.bottom()); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qGray(image.pixel(x, y)) < 128) {
                ++count;
            }
        }
    }
    return count;
}

/// A pixel of the marks' color (green, as lightAppearance() gives them) over white paper,
/// also where a thin stroke covers it only in part (a pale green)
bool isMarkPixel(QRgb pixel) {
    return qGreen(pixel) - std::max(qRed(pixel), qBlue(pixel)) > 40;
}

/// Pixels of the marks' color
int marksIn(const QImage& image) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (isMarkPixel(image.pixel(x, y))) {
                ++count;
            }
        }
    }
    return count;
}

/// Put a text in place of the whole document
void setText(BookEditor& editor, const QString& text) {
    QTextCursor cursor(editor.textDocument());
    cursor.select(QTextCursor::Document);
    cursor.insertText(text);
}

/// The glyphs of marks, in their order
QString glyphsOf(const std::vector<FormattingMark>& marks) {
    QString glyphs;
    for (const FormattingMark& mark : marks) {
        glyphs.append(mark.glyph);
    }
    return glyphs;
}

}  // namespace

// =============================================================================
// Styles
// =============================================================================

TEST_CASE("Styles view: Normal follows the settings, the other styles keep their proportions",
          "[editor][styles][view]") {
    auto editor = editorWith(QStringLiteral("<h1>Title</h1><p>Text</p>"));
    EditorAppearance appearance = editor->appearance();
    appearance.typography.textFont.setPointSizeF(15.0);
    editor->setAppearance(appearance);

    // Heading 1 is 20 pt at Normal's 12 pt in the package: 20/12 of the text's 15 pt
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    CHECK(layout->blockMetrics(blockOf(*editor, 0)).charFormat.fontPointSize() ==
          Approx(15.0 * 20.0 / 12.0));
    CHECK(layout->blockMetrics(blockOf(*editor, 1)).charFormat.propertyCount() == 0);
    CHECK(layout->shownAlignment(blockOf(*editor, 0)) == Qt::AlignHCenter);
    CHECK(blockOf(*editor, 0).layout()->lineAt(0).height() >
          blockOf(*editor, 1).layout()->lineAt(0).height());
}

TEST_CASE("Styles view: a page break starts a new page in the page view only",
          "[editor][objects][view]") {
    auto editor = editorWith(QStringLiteral("<p>Before</p><page-break/><p>After</p>"));

    // The endless page: the text goes on after the break
    const qreal continuous = lineTop(*editor, 2, 0);
    CHECK(continuous < 200.0);

    // The pages: at the top of the next page's text area
    editor->setViewMode(ViewMode::Page);
    layoutOf(*editor)->layoutPendingBlocks();
    const qreal pitch = layoutOf(*editor)->pageFlow().pitch;
    REQUIRE(pitch > 0.0);
    CHECK(pageOf(*editor, lineTop(*editor, 0, 0)) == 0);
    CHECK(lineTop(*editor, 2, 0) == Approx(pitch + lineTop(*editor, 0, 0)).margin(0.5));
}

TEST_CASE("Styles view: a heading never stays alone at the bottom of a page",
          "[editor][styles][view]") {
    // However many paragraphs come before it, the heading is on the page of the paragraph
    // after it; without keepWithNext some count leaves it alone at the bottom
    const auto pagesApart = [](bool keepWithNext) {
        int apart = 0;
        for (int count = 10; count < 60; ++count) {
            QString content;
            for (int i = 0; i < count; ++i) {
                content += QStringLiteral("<p>Paragraph %1</p>").arg(i);
            }
            content += QStringLiteral("<h1>Heading</h1><p>After the heading</p>");
            auto editor = editorWith(content, keepWithNext);
            editor->setViewMode(ViewMode::Page);
            layoutOf(*editor)->layoutPendingBlocks();
            if (pageOf(*editor, lineTop(*editor, count, 0)) !=
                pageOf(*editor, lineTop(*editor, count + 1, 0))) {
                ++apart;
            }
        }
        return apart;
    };
    CHECK(pagesApart(true) == 0);
    CHECK(pagesApart(false) > 0);
}

// =============================================================================
// Scene and page breaks
// =============================================================================

TEST_CASE("Styles view: a scene break looks as the settings say", "[editor][objects][view]") {
    auto editor = editorWith(QStringLiteral("<p>Text</p><scene-break/><p>More</p>"));
    const QRect band = bandOf(*editor, 1);
    editor->setCursorPosition({0, 0});

    const auto look = [&editor, &band](SceneBreakLook sceneBreakLook) {
        EditorAppearance appearance = editor->appearance();
        appearance.sceneBreakLook = sceneBreakLook;
        editor->setAppearance(appearance);
        return editorImage(*editor).copy(band);
    };
    const QImage asterisks = look(SceneBreakLook::Asterisks);
    const QImage asterism = look(SceneBreakLook::Asterism);
    const QImage fleuron = look(SceneBreakLook::Fleuron);
    const QImage line = look(SceneBreakLook::Line);
    const QImage blank = look(SceneBreakLook::Blank);
    CHECK(layoutOf(*editor)->sceneBreakLook() == SceneBreakLook::Blank);

    // Qt (6.5 and later) draws an object only through the handler of its kind
    for (const int objectType : {HORIZONTAL_RULE_OBJECT, SCENE_BREAK_OBJECT, PAGE_BREAK_OBJECT}) {
        CHECK(layoutOf(*editor)->handlerForObject(objectType) != nullptr);
    }

    // Each look its own; a blank line nothing at all
    const QRect whole(QPoint(0, 0), band.size());
    CHECK(inkIn(asterisks, whole) > 0);
    CHECK(inkIn(line, whole) > 0);
    CHECK(inkIn(blank, whole) == 0);
    CHECK(asterisks != asterism);
    CHECK(asterism != fleuron);
    CHECK(fleuron != line);

    // The text keeps the break
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p>Text</p><scene-break/><p>More</p></kml>"));
}

TEST_CASE("Styles view: a page break shows in the page view with the formatting marks",
          "[editor][objects][view][marks]") {
    auto editor = editorWith(QStringLiteral("<p>Text</p><page-break/><p>More</p>"));
    const auto shown = [&editor](ViewMode mode, bool marks) {
        editor->setViewMode(mode);
        EditorAppearance appearance = editor->appearance();
        appearance.showFormattingMarks = marks;
        editor->setAppearance(appearance);
        const QRect band = bandOf(*editor, 1);
        editor->setCursorPosition({0, 0});
        return inkIn(editorImage(*editor), band.adjusted(0, 1, 0, -1)) > 0;
    };

    // A dashed line with "Page Break" on the endless page; the pages show it themselves
    CHECK(shown(ViewMode::Continuous, false));
    CHECK_FALSE(shown(ViewMode::Page, false));
    CHECK(shown(ViewMode::Page, true));
    CHECK(layoutOf(*editor)->formattingMarksShown());
}

// =============================================================================
// Formatting marks
// =============================================================================

TEST_CASE("Formatting marks: the spaces, the breaks and the end of a paragraph",
          "[editor][view][marks]") {
    const QString text = QStringLiteral("a b") + QChar(QChar::Nbsp) + QStringLiteral("c\td") +
                         QChar(QChar::LineSeparator) + QStringLiteral("e");
    auto editor = editorWith(QStringLiteral("<p>x</p>"));
    setText(*editor, text);

    const QTextBlock block = blockOf(*editor, 0);
    const std::vector<FormattingMark> marks = formattingMarksOf(block);
    const char16_t expected[] = {SPACE_MARK, NO_BREAK_SPACE_MARK, TAB_MARK, LINE_BREAK_MARK,
                                 PARAGRAPH_MARK};
    REQUIRE(glyphsOf(marks) == QString::fromUtf16(expected, 5));

    // · in the middle of its space, → of its tab
    const QTextLayout* layout = block.layout();
    const QTextLine first = layout->lineAt(0);
    const auto middleOf = [](const FormattingMark& mark) {
        return mark.position.x() + QFontMetricsF(mark.font).horizontalAdvance(mark.glyph) / 2.0;
    };
    CHECK(middleOf(marks.at(0)) == Approx((first.cursorToX(1) + first.cursorToX(2)) / 2.0));
    CHECK(marks.at(0).position.y() == Approx(first.y() + first.ascent()));
    CHECK(middleOf(marks.at(2)) == Approx((first.cursorToX(5) + first.cursorToX(6)) / 2.0));

    // ↵ where the line ends, ¶ after the last letter, on the second line
    CHECK(marks.at(3).position.x() == Approx(first.cursorToX(7)));
    REQUIRE(layout->lineCount() == 2);
    const QTextLine second = layout->lineAt(1);
    CHECK(marks.at(4).position.x() == Approx(second.cursorToX(text.size())));
    CHECK(marks.at(4).position.y() == Approx(second.y() + second.ascent()));
    CHECK(marks.at(4).font.pointSizeF() == Approx(editor->textDocument()->defaultFont().pointSizeF()));
}

TEST_CASE("Formatting marks: headings, empty paragraphs and the objects",
          "[editor][view][marks][objects]") {
    auto editor = editorWith(
        QStringLiteral("<h1>Title</h1><h1></h1><p></p><scene-break/><hr/><page-break/>"));
    const qreal textPoints = editor->textDocument()->defaultFont().pointSizeF();
    const qreal headingPoints = textPoints * 20.0 / 12.0;

    // ¶ of a heading in its size, after its centred text
    const std::vector<FormattingMark> heading = formattingMarksOf(blockOf(*editor, 0));
    REQUIRE(glyphsOf(heading) == QString(QChar(PARAGRAPH_MARK)));
    CHECK(heading.at(0).font.pointSizeF() == Approx(headingPoints));
    CHECK(heading.at(0).position.x() ==
          Approx(blockOf(*editor, 0).layout()->lineAt(0).cursorToX(5)));

    // An empty heading and an empty paragraph: ¶ in their style's size
    const std::vector<FormattingMark> emptyHeading = formattingMarksOf(blockOf(*editor, 1));
    REQUIRE(emptyHeading.size() == 1);
    CHECK(emptyHeading.at(0).font.pointSizeF() == Approx(headingPoints));
    const std::vector<FormattingMark> empty = formattingMarksOf(blockOf(*editor, 2));
    REQUIRE(empty.size() == 1);
    CHECK(empty.at(0).font.pointSizeF() == Approx(textPoints));
    CHECK(empty.at(0).position.x() == Approx(blockOf(*editor, 2).layout()->lineAt(0).cursorToX(0)));

    // A scene break: ¶ right after the asterisks in the middle; a blank one: in the middle
    const QTextLine sceneLine = blockOf(*editor, 3).layout()->lineAt(0);
    const qreal middle = (sceneLine.cursorToX(0) + sceneLine.cursorToX(1)) / 2.0;
    const qreal half = QFontMetricsF(editor->textDocument()->defaultFont())
                           .horizontalAdvance(sceneBreakText(SceneBreakLook::Asterisks)) / 2.0;
    std::vector<FormattingMark> scene = formattingMarksOf(blockOf(*editor, 3));
    REQUIRE(glyphsOf(scene) == QString(QChar(PARAGRAPH_MARK)));
    CHECK(scene.at(0).position.x() == Approx(middle + half));
    EditorAppearance appearance = editor->appearance();
    appearance.sceneBreakLook = SceneBreakLook::Blank;
    editor->setAppearance(appearance);
    scene = formattingMarksOf(blockOf(*editor, 3));
    REQUIRE(scene.size() == 1);
    CHECK(scene.at(0).position.x() == Approx(middle));

    // A horizontal line and a page break are as wide as the text: none
    CHECK(formattingMarksOf(blockOf(*editor, 4)).empty());
    CHECK(formattingMarksOf(blockOf(*editor, 5)).empty());
}

TEST_CASE("Formatting marks: an optional hyphen", "[editor][view][marks]") {
    auto editor = editorWith(QStringLiteral("<p>x</p>"));
    const QChar hyphen(QChar::SoftHyphen);
    const QFont textFont = editor->textDocument()->defaultFont();
    const auto hyphenMarks = [&editor]() {
        std::vector<FormattingMark> found;
        for (const FormattingMark& mark : formattingMarksOf(blockOf(*editor, 0))) {
            if (mark.glyph == QChar(OPTIONAL_HYPHEN_MARK)) {
                found.push_back(mark);
            }
        }
        return found;
    };

    SECTION("inside a line: small, above the middle of the letters, where the hyphen is") {
        setText(*editor, QStringLiteral("wo") + hyphen + QStringLiteral("rd and more"));
        const std::vector<FormattingMark> marks = hyphenMarks();
        REQUIRE(marks.size() == 1);
        const QTextLine line = blockOf(*editor, 0).layout()->lineAt(0);
        CHECK(marks.at(0).font.pointSizeF() ==
              Approx(textFont.pointSizeF() * INLINE_HYPHEN_MARK_SCALE));
        CHECK(marks.at(0).position.y() < line.y() + line.ascent());
        CHECK(marks.at(0).position.x() < line.cursorToX(2));
    }

    SECTION("at the end of a line: after the hyphen there, in the text's size") {
        // As many words before a long word as make the line break at its hyphen
        int position = -1;
        for (int words = 1; words < 200 && position < 0; ++words) {
            const QString text = QStringLiteral("word ").repeated(words) +
                                 QStringLiteral("abcdefghij") + hyphen +
                                 QStringLiteral("klmnopqrstuvwxyz end");
            setText(*editor, text);
            const int at = static_cast<int>(text.indexOf(hyphen));
            const QTextLine line = blockOf(*editor, 0).layout()->lineForTextPosition(at);
            if (at == line.textStart() + line.textLength() - 1) {
                position = at;
            }
        }
        REQUIRE(position >= 0);
        const std::vector<FormattingMark> marks = hyphenMarks();
        REQUIRE(marks.size() == 1);
        const QTextLine line = blockOf(*editor, 0).layout()->lineForTextPosition(position);
        CHECK(marks.at(0).font.pointSizeF() == Approx(textFont.pointSizeF()));
        CHECK(marks.at(0).position.x() > line.cursorToX(position));
        CHECK(marks.at(0).position.y() == Approx(line.y() + line.ascent()));
    }
}

TEST_CASE("Formatting marks: drawn over the text in their color, only when shown",
          "[editor][view][marks]") {
    auto editor = editorWith(QStringLiteral("<p>One two three</p><p></p>"));
    editor->setCursorPosition({1, 0});
    CHECK(marksIn(editorImage(*editor)) == 0);

    EditorAppearance appearance = editor->appearance();
    appearance.showFormattingMarks = true;
    editor->setAppearance(appearance);
    const QImage withMarks = editorImage(*editor);
    CHECK(marksIn(withMarks) > 0);

    // In the size of the text: ¶ as large against T as the font makes it
    {
        auto alone = editorWith(QStringLiteral("<p>T</p>"));
        EditorAppearance shown = alone->appearance();
        shown.showFormattingMarks = true;
        alone->setAppearance(shown);
        alone->setCursorPosition({0, 0});
        const QRect caret =
            alone->inputMethodQuery(Qt::ImCursorRectangle).toRect().adjusted(-2, 0, 2, 0);
        const QImage image = editorImage(*alone);
        int letterTop = image.height(), letterBottom = -1, markTop = image.height(), markBottom = -1;
        // The line of the text, from its start
        for (int y = caret.top(); y <= caret.bottom(); ++y) {
            for (int x = caret.left(); x < caret.left() + caret.height() * 4; ++x) {
                if (isMarkPixel(image.pixel(x, y))) {
                    markTop = std::min(markTop, y);
                    markBottom = std::max(markBottom, y);
                } else if (qGray(image.pixel(x, y)) < 100 &&
                           (x < caret.left() || x > caret.right())) {
                    letterTop = std::min(letterTop, y);
                    letterBottom = std::max(letterBottom, y);
                }
            }
        }
        REQUIRE(markBottom >= 0);
        REQUIRE(letterBottom >= 0);
        const QFontMetricsF metrics(alone->textDocument()->defaultFont());
        const qreal scale = (letterBottom - letterTop + 1) /
                            metrics.tightBoundingRect(QStringLiteral("T")).height();
        const qreal expected =
            metrics.tightBoundingRect(QString(QChar(PARAGRAPH_MARK))).height() * scale;
        INFO("mark " << markBottom - markTop + 1 << " expected " << expected << " letter "
                     << letterBottom - letterTop + 1);
        CHECK(std::abs((markBottom - markTop + 1) - expected) <= 2.5);
    }

    // The text stays where it was
    const QTextLine line = blockOf(*editor, 0).layout()->lineAt(0);
    const qreal width = line.naturalTextWidth();
    appearance.showFormattingMarks = false;
    editor->setAppearance(appearance);
    CHECK(blockOf(*editor, 0).layout()->lineAt(0).naturalTextWidth() == width);
    CHECK(marksIn(editorImage(*editor)) == 0);
}
