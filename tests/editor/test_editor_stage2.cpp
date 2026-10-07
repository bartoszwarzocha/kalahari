/// @file test_editor_stage2.cpp
/// @brief Editor Stage 2 (variant A): typography, paste and undo, on-demand layout
///
/// Geometry is checked on the layout itself; what the user sees is checked on images of
/// the editor (editorImage()), compared pixel by pixel with the background.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/clipboard_handler.h>
#include "editor_test_utils.h"

#include <QImage>
#include <QMimeData>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

namespace {

constexpr qreal kWidth = 300.0;

QString longParagraph(int index) {
    return QStringLiteral("Paragraph %1 has enough words to wrap onto several lines "
                          "when the layout is narrow, which makes its height depend on "
                          "the line spacing as well as on the width.").arg(index);
}

QFont testFont() {
    QFont font(QStringLiteral("Arial"));
    font.setPointSizeF(12.0);
    return font;
}

/// A document wired like BookEditor::createDocument() wires it
struct TypesetDocument {
    std::unique_ptr<QTextDocument> doc = std::make_unique<QTextDocument>();
    KalahariTextDocumentLayout* layout = nullptr;

    TypesetDocument() {
        layout = new KalahariTextDocumentLayout(doc.get());
        doc->setDocumentLayout(layout);
        doc->setDefaultFont(testFont());
        doc->setDocumentMargin(0);
        doc->setTextWidth(kWidth);
    }

    void load(const QStringList& paragraphs) {
        QTextCursor cursor(doc.get());
        cursor.beginEditBlock();
        for (int i = 0; i < paragraphs.size(); ++i) {
            if (i > 0) cursor.insertBlock();
            cursor.insertText(paragraphs[i]);
        }
        cursor.endEditBlock();
    }

    QTextBlock block(int number) const { return doc->findBlockByNumber(number); }
};

/// Extra leading the layout adds to a line of @p height (whole pixels)
qreal extraFor(qreal height, qreal lineSpacing) {
    return std::round(height * (lineSpacing - 1.0));
}

/// Editor appearance with the given typography (everything else default)
EditorAppearance appearanceWith(qreal lineHeight, qreal paragraphSpacing, bool indent,
                                qreal indentSize) {
    EditorAppearance appearance;
    appearance.typography.lineHeight = lineHeight;
    appearance.typography.paragraphSpacing = paragraphSpacing;
    appearance.typography.firstLineIndent = indent;
    appearance.typography.indentSize = indentSize;
    return appearance;
}

KalahariTextDocumentLayout* layoutOf(const BookEditor& editor) {
    return qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
}

/// Image of the editor with one pixel per logical pixel. QWidget::grab() follows the
/// display scaling (125% gives 1.25 pixels per logical pixel), which the pixel positions
/// computed from the layout do not.
QImage editorImage(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// Widget position of document point (0, 0): the caret at the start of the text, less its
/// place in the first line (at zoom 100% a document pixel is a widget pixel)
QPointF textOrigin(BookEditor& editor) {
    editor.setCursorPosition({0, 0});
    const QRectF caret = editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    const QTextLine line = editor.textDocument()->firstBlock().layout()->lineAt(0);
    return caret.topLeft() - QPointF(line.cursorToX(0), line.y());
}

/// True when the pixel differs visibly from @p background
bool differs(const QImage& image, int x, int y, QColor background) {
    const QColor c = image.pixelColor(x, y);
    return std::abs(c.red() - background.red()) + std::abs(c.green() - background.green()) +
               std::abs(c.blue() - background.blue()) > 24;
}

/// Leftmost column from @p left on, in rows [top, bottom), with a pixel differing from the
/// background (left of the page, the desk differs too)
int leftmostInk(const QImage& image, int left, int top, int bottom, QColor background) {
    for (int x = std::max(0, left); x < image.width(); ++x) {
        for (int y = std::max(0, top); y < std::min(bottom, image.height()); ++y) {
            if (differs(image, x, y, background)) return x;
        }
    }
    return -1;
}

}  // anonymous namespace

// =============================================================================
// Typography in the layout
// =============================================================================

TEST_CASE("Stage2 typography: line spacing, paragraph spacing and first-line indent",
          "[editor][stage2][typography]") {
    TypesetDocument d;
    d.load({longParagraph(0), longParagraph(1), longParagraph(2)});
    d.layout->setTypography({1.5, 10.0, 20.0, 0.0});
    d.layout->layoutPendingBlocks();

    const QTextLayout* first = d.block(0).layout();
    REQUIRE(first->lineCount() >= 3);

    SECTION("lines are spaced by the multiple of their height, half of it above") {
        const QTextLine line0 = first->lineAt(0);
        const QTextLine line1 = first->lineAt(1);
        const qreal extra0 = extraFor(line0.height(), 1.5);
        CHECK(extra0 > 0);
        CHECK(line0.y() == Approx(std::floor(extra0 / 2.0)));
        CHECK(line1.y() - line0.y() ==
              Approx(line0.height() + extra0 / 2.0 + extraFor(line1.height(), 1.5) / 2.0)
                  .margin(1.0));
    }

    SECTION("the block height holds its line boxes and the paragraph spacing") {
        qreal lines = 0;
        for (int i = 0; i < first->lineCount(); ++i) {
            lines += first->lineAt(i).height() + extraFor(first->lineAt(i).height(), 1.5);
        }
        CHECK(d.layout->blockHeight(0) == Approx(lines + 10.0));
        CHECK(d.layout->blockY(1) == Approx(d.layout->blockHeight(0)));
        CHECK(d.layout->documentSize().height() ==
              Approx(d.layout->blockY(2) + d.layout->blockHeight(2)));
    }

    SECTION("line boxes of a block touch") {
        for (int i = 0; i + 1 < first->lineCount(); ++i) {
            const QRectF box = KalahariTextDocumentLayout::lineBox(first->lineAt(i), 1.5);
            const QRectF next = KalahariTextDocumentLayout::lineBox(first->lineAt(i + 1), 1.5);
            CHECK(box.bottom() == Approx(next.top()));
        }
        CHECK(KalahariTextDocumentLayout::lineBox(first->lineAt(0), 1.5).top() == Approx(0.0));
    }

    SECTION("only the first line of a left-aligned paragraph is indented") {
        CHECK(first->lineAt(0).x() == Approx(20.0));
        CHECK(first->lineAt(0).width() == Approx(kWidth - 20.0));
        CHECK(first->lineAt(1).x() == Approx(0.0));
        CHECK(first->lineAt(1).width() == Approx(kWidth));
    }

    SECTION("justified paragraphs are indented, centered and right-aligned ones are not") {
        const auto setAlignment = [&d](int block, Qt::Alignment alignment) {
            QTextCursor cursor(d.block(block));
            QTextBlockFormat format = cursor.blockFormat();
            format.setAlignment(alignment);
            cursor.setBlockFormat(format);
        };
        setAlignment(0, Qt::AlignJustify);
        setAlignment(1, Qt::AlignHCenter);
        setAlignment(2, Qt::AlignRight);

        CHECK(d.block(0).layout()->lineAt(0).x() == Approx(20.0));
        CHECK(d.block(1).layout()->lineAt(0).x() == Approx(0.0));
        CHECK(d.block(1).layout()->lineAt(0).width() == Approx(kWidth));
        CHECK(d.block(2).layout()->lineAt(0).width() == Approx(kWidth));
    }

    SECTION("defaults lay out exactly as without typography") {
        TypesetDocument plain;
        plain.load({longParagraph(0), longParagraph(1), longParagraph(2)});
        d.layout->setTypography({});
        d.layout->layoutPendingBlocks();
        for (int i = 0; i < 3; ++i) {
            CHECK(d.layout->blockHeight(i) == Approx(plain.layout->blockHeight(i)));
            CHECK(d.layout->blockHeight(i) ==
                  Approx(d.block(i).layout()->boundingRect().height()));
        }
    }
}

TEST_CASE("Stage2 typography: lengths follow the document font", "[editor][stage2][typography]") {
    TypesetDocument d;
    d.load({longParagraph(0)});
    d.layout->setTypography({1.0, 10.0, 20.0, 12.0});
    CHECK(d.layout->paragraphSpacing() == Approx(10.0));
    CHECK(d.layout->firstLineIndent() == Approx(20.0));

    // A font twice the size (from the settings): spacing and indent double in the same
    // relayout
    QFont zoomed = testFont();
    zoomed.setPointSizeF(24.0);
    d.doc->setDefaultFont(zoomed);
    d.layout->layoutPendingBlocks();
    CHECK(d.layout->paragraphSpacing() == Approx(20.0));
    CHECK(d.layout->firstLineIndent() == Approx(40.0));
    CHECK(d.block(0).layout()->lineAt(0).x() == Approx(40.0));
}

TEST_CASE("Stage2 typography: hit test between lines and in the paragraph spacing",
          "[editor][stage2][typography]") {
    TypesetDocument d;
    d.load({longParagraph(0), longParagraph(1)});
    d.layout->setTypography({2.0, 16.0, 0.0, 0.0});
    d.layout->layoutPendingBlocks();

    const QTextBlock block = d.block(0);
    const QTextLayout* layout = block.layout();
    REQUIRE(layout->lineCount() >= 2);
    const QTextLine line0 = layout->lineAt(0);
    const QTextLine line1 = layout->lineAt(1);
    const auto lineOf = [&](qreal y) {
        const int position = d.layout->hitTest(QPointF(5.0, y), Qt::FuzzyHit) - block.position();
        return layout->lineForTextPosition(position).lineNumber();
    };

    // Just below the glyphs of line 0 (its lower half-leading) and just above line 1
    CHECK(lineOf(line0.y() + line0.height() + 1.0) == 0);
    CHECK(lineOf(line1.y() - 1.0) == 1);

    // The paragraph spacing below the block belongs to the block's last line
    const qreal spacingY = d.layout->blockY(0) + d.layout->blockHeight(0) - 2.0;
    const int position = d.layout->hitTest(QPointF(5.0, spacingY), Qt::FuzzyHit);
    CHECK(d.doc->findBlock(position).blockNumber() == 0);
    CHECK(layout->lineForTextPosition(position - block.position()).lineNumber() ==
          layout->lineCount() - 1);
}

// =============================================================================
// Typography in the editor
// =============================================================================

TEST_CASE("Stage2 typography: the editor lays out with its appearance settings",
          "[editor][stage2][typography]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf({longParagraph(0), longParagraph(1), longParagraph(2)}));
    auto* layout = layoutOf(editor);
    REQUIRE(layout != nullptr);

    // Defaults: 1.6 line spacing, 12 px after each paragraph, 24 px first-line indent
    const EditorTypography defaults;
    CHECK(layout->typography().lineSpacing == Approx(defaults.lineHeight));
    CHECK(layout->paragraphSpacing() == Approx(defaults.paragraphSpacing));
    CHECK(layout->firstLineIndent() == Approx(defaults.indentSize));
    const qreal spacedHeight = layout->documentSize().height();

    editor.setAppearance(appearanceWith(1.0, 0.0, false, 24.0));
    layout->layoutPendingBlocks();
    CHECK(layout->typography().lineSpacing == Approx(1.0));
    CHECK(layout->paragraphSpacing() == Approx(0.0));
    CHECK(layout->firstLineIndent() == Approx(0.0));
    CHECK(layout->documentSize().height() < spacedHeight);

    SECTION("zoom scales the spacing with the text, laying out nothing again") {
        // The painter scales the page: the layout keeps its spacing, indent and lines
        editor.setAppearance(appearanceWith(1.5, 10.0, true, 20.0));
        layout->layoutPendingBlocks();
        const qreal height = layout->documentSize().height();
        LaidOutBlockCounter laidOut(editor);
        editor.setZoomFactor(2.0);
        layout->layoutPendingBlocks();
        CHECK(laidOut.count() == 0);
        CHECK(layout->paragraphSpacing() == Approx(10.0));
        CHECK(layout->firstLineIndent() == Approx(20.0));
        CHECK(layout->documentSize().height() == Approx(height));
    }
}

TEST_CASE("Stage2 typography: going to the first line scrolls to the document top",
          "[editor][stage2][typography]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.setAppearance(appearanceWith(2.0, 10.0, false, 0.0));
    QStringList paragraphs;
    for (int i = 0; i < 20; ++i) paragraphs << longParagraph(i);
    editor.fromKml(kmlOf(paragraphs));

    editor.setCursorPosition({19, 0});
    REQUIRE(editor.scrollOffset() > 0.0);
    editor.setCursorPosition({0, 0});
    CHECK(editor.scrollOffset() == Approx(0.0));
}

TEST_CASE("Stage2 typography: changing it leaves the document and its history alone",
          "[editor][stage2][typography]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(QStringLiteral("<kml><p>Plain <b>bold</b> text.</p>"
                                  "<p align=\"center\">Centered</p></kml>"));
    const QString kmlBefore = editor.toKml();
    const bool modifiedBefore = editor.textDocument()->isModified();
    REQUIRE_FALSE(editor.textDocument()->isUndoAvailable());
    int contentChanges = 0;
    QObject::connect(&editor, &BookEditor::contentChanged, [&contentChanges] { ++contentChanges; });

    editor.setAppearance(appearanceWith(2.0, 30.0, true, 40.0));

    CHECK(editor.toKml() == kmlBefore);
    CHECK(contentChanges == 0);
    CHECK_FALSE(editor.textDocument()->isUndoAvailable());
    CHECK(editor.textDocument()->isModified() == modifiedBefore);
    for (QTextBlock b = editor.textDocument()->begin(); b.isValid(); b = b.next()) {
        CHECK(b.blockFormat().lineHeight() == Approx(0.0));
        CHECK(b.blockFormat().textIndent() == Approx(0.0));
        CHECK(b.blockFormat().bottomMargin() == Approx(0.0));
    }
}

TEST_CASE("Stage2 typography: the image shows the indent and a joined-up selection",
          "[editor][stage2][typography][render]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.setAppearance(appearanceWith(2.0, 0.0, true, 40.0));
    editor.fromKml(kmlOf({longParagraph(0)}));

    const EditorAppearance& appearance = editor.appearance();
    const QColor background = appearance.colors.background(appearance.colorMode);
    const QPointF origin = textOrigin(editor);
    const int marginLeft = static_cast<int>(std::round(origin.x()));
    const int marginTop = static_cast<int>(std::round(origin.y()));

    const QTextLayout* layout = editor.textDocument()->firstBlock().layout();
    REQUIRE(layout->lineCount() >= 3);
    const QTextLine line0 = layout->lineAt(0);
    const QTextLine line1 = layout->lineAt(1);

    SECTION("the first line starts at the indent, the next ones at the margin") {
        // From within the page's left margin
        const QImage image = editorImage(editor);
        const int ink0 = leftmostInk(image, marginLeft - 8, marginTop + static_cast<int>(line0.y()),
                                     marginTop + static_cast<int>(line0.y() + line0.height()),
                                     background);
        const int ink1 = leftmostInk(image, marginLeft - 8, marginTop + static_cast<int>(line1.y()),
                                     marginTop + static_cast<int>(line1.y() + line1.height()),
                                     background);
        REQUIRE(ink0 >= 0);
        REQUIRE(ink1 >= 0);
        CHECK(ink0 >= marginLeft + 40 - 2);
        CHECK(ink1 < marginLeft + 12);
    }

    SECTION("a selection also covers the space between its lines") {
        // A row in the lower half-leading of line 0: no glyphs there, only the selection
        const int gapRow = marginTop + static_cast<int>(
            line0.y() + line0.height() + extraFor(line0.height(), 2.0) / 4.0);
        const int column = marginLeft + 60;

        const QImage unselected = editorImage(editor);
        CHECK_FALSE(differs(unselected, column, gapRow, background));

        editor.selectAll();
        const QImage selected = editorImage(editor);
        CHECK(differs(selected, column, gapRow, background));
    }
}

// =============================================================================
// Copy, paste and undo
// =============================================================================

namespace {

/// Editor holding @p kml, sized like a small window
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(kml);
    return editor;
}

/// Clipboard content of a range of @p kml, as BookEditor::copy() would put it there
std::unique_ptr<QMimeData> copied(const QString& kml, const SelectionRange& range) {
    auto source = editorWith(kml);
    source->setSelection(range);
    return source->createMimeDataFromSelection();
}

QString kmlData(const QMimeData& mimeData) {
    return QString::fromUtf8(mimeData.data(QString::fromLatin1(MIME_KML)));
}

/// Plain text as another program puts it on the clipboard
std::unique_ptr<QMimeData> plainText(const QString& text) {
    auto mimeData = std::make_unique<QMimeData>();
    mimeData->setText(text);
    return mimeData;
}

const QString kSource = QStringLiteral(
    "<kml><p>Plain <b>bold</b> text.</p><p align=\"right\"><i>Right</i> side</p>"
    "<p>Last</p></kml>");

}  // anonymous namespace

TEST_CASE("Stage2 paste: copy puts KML, HTML and plain text on the clipboard",
          "[editor][stage2][paste]") {
    auto editor = editorWith(kSource);

    SECTION("a range across paragraphs") {
        editor->setSelection({{0, 6}, {1, 5}});
        const auto mimeData = editor->createMimeDataFromSelection();
        REQUIRE(mimeData);
        CHECK(mimeData->text() == QStringLiteral("bold text.\nRight"));
        CHECK(kmlData(*mimeData) ==
              QStringLiteral("<kml><p><b>bold</b> text.</p><p align=\"right\"><i>Right</i></p></kml>"));
        CHECK(mimeData->html() == QStringLiteral("<p><b>bold</b> text.</p>"
                                                 "<p style=\"text-align:right\"><i>Right</i></p>"));
    }

    SECTION("whole paragraphs end with an empty one") {
        editor->setSelection({{0, 0}, {1, 0}});
        const auto mimeData = editor->createMimeDataFromSelection();
        REQUIRE(mimeData);
        CHECK(mimeData->text() == QStringLiteral("Plain bold text.\n"));
        CHECK(kmlData(*mimeData) ==
              QStringLiteral("<kml><p>Plain <b>bold</b> text.</p><p align=\"right\"></p></kml>"));
    }

    SECTION("nothing without a selection") {
        editor->clearSelection();
        CHECK_FALSE(editor->createMimeDataFromSelection());
    }
}

TEST_CASE("Stage2 paste: pasting over a selection is one undo step", "[editor][stage2][paste]") {
    auto editor = editorWith(kSource);
    const QString original = editor->toKml();
    const auto mimeData = copied(kSource, {{0, 6}, {1, 5}});  // "bold text.¶Right"

    editor->setSelection({{1, 6}, {2, 2}});  // "side¶La"
    editor->insertFromMimeData(mimeData.get());
    const QString pasted = editor->toKml();
    CHECK(pasted == QStringLiteral("<kml><p>Plain <b>bold</b> text.</p>"
                                   "<p align=\"right\"><i>Right</i> <b>bold</b> text.</p>"
                                   "<p align=\"right\"><i>Right</i>st</p></kml>"));
    CHECK(editor->cursorPosition() == CursorPosition{2, 5});
    CHECK_FALSE(editor->hasSelection());

    editor->undo();
    CHECK(editor->toKml() == original);
    CHECK_FALSE(editor->canUndo());

    editor->redo();
    CHECK(editor->toKml() == pasted);
}

TEST_CASE("Stage2 paste: pasted paragraphs keep their alignment, the target keeps its own",
          "[editor][stage2][paste]") {
    const QString target = QStringLiteral("<kml><p align=\"center\">Title</p><p>Body</p></kml>");
    auto editor = editorWith(target);

    SECTION("whole paragraphs before a heading") {
        const auto mimeData = copied(kSource, {{0, 0}, {2, 0}});  // two paragraphs and a break
        editor->setCursorPosition({0, 0});
        editor->insertFromMimeData(mimeData.get());
        CHECK(editor->toKml() == QStringLiteral(
                  "<kml><p>Plain <b>bold</b> text.</p><p align=\"right\"><i>Right</i> side</p>"
                  "<p align=\"center\">Title</p><p>Body</p></kml>"));
    }

    SECTION("paragraphs pasted into the middle of a heading") {
        const auto mimeData = copied(kSource, {{0, 6}, {1, 5}});
        editor->setCursorPosition({0, 2});
        editor->insertFromMimeData(mimeData.get());
        CHECK(editor->toKml() == QStringLiteral(
                  "<kml><p align=\"center\">Ti<b>bold</b> text.</p>"
                  "<p align=\"center\"><i>Right</i>tle</p><p>Body</p></kml>"));
    }

    SECTION("paragraphs pasted at the end of a paragraph") {
        const auto mimeData = copied(kSource, {{0, 6}, {1, 5}});
        editor->setCursorPosition({0, 5});
        editor->insertFromMimeData(mimeData.get());
        CHECK(editor->toKml() == QStringLiteral(
                  "<kml><p align=\"center\">Title<b>bold</b> text.</p>"
                  "<p align=\"right\"><i>Right</i></p><p>Body</p></kml>"));
    }

    SECTION("text inside one paragraph goes inline") {
        const auto mimeData = copied(kSource, {{1, 0}, {1, 5}});
        editor->setCursorPosition({1, 0});
        editor->insertFromMimeData(mimeData.get());
        CHECK(editor->toKml() == QStringLiteral(
                  "<kml><p align=\"center\">Title</p><p><i>Right</i>Body</p></kml>"));
    }
}

TEST_CASE("Stage2 paste: a copied range pastes back as the same content", "[editor][stage2][paste]") {
    const QString formatted = QStringLiteral(
        "<kml><p>A <b>bold</b>, <i>italic</i>, <u>underlined</u> and <s>struck</s> word.</p>"
        "<p align=\"justify\">Some <span color=\"#aa0000\">red</span> and "
        "<b font=\"Georgia\" size=\"14\">big</b> text.</p></kml>");
    const auto mimeData = copied(formatted, {{0, 2}, {1, 16}});

    auto editor = editorWith(QStringLiteral("<kml><p></p></kml>"));
    editor->insertFromMimeData(mimeData.get());
    CHECK(editor->paragraphCount() == 2);

    editor->setSelection({{0, 0}, {1, 16}});
    const auto again = editor->createMimeDataFromSelection();
    REQUIRE(again);
    CHECK(kmlData(*again) == kmlData(*mimeData));
}

TEST_CASE("Stage2 paste: text from other programs takes the format of the insertion point",
          "[editor][stage2][paste]") {
    auto editor = editorWith(QStringLiteral("<kml><p align=\"center\">A <b>bold</b> word</p></kml>"));

    SECTION("inside a bold run, with paragraph breaks") {
        editor->setCursorPosition({0, 4});
        editor->insertFromMimeData(plainText(QStringLiteral("one\ntwo")).get());
        CHECK(editor->toKml() == QStringLiteral("<kml><p align=\"center\">A <b>boone</b></p>"
                                                "<p align=\"center\"><b>twold</b> word</p></kml>"));
        editor->undo();
        CHECK(editor->toKml() ==
              QStringLiteral("<kml><p align=\"center\">A <b>bold</b> word</p></kml>"));
    }

    SECTION("line breaks of every platform, without characters a file cannot hold") {
        editor->setCursorPosition({0, 0});
        const QString text = QStringLiteral("a") + QChar(0x01) + QStringLiteral("b") + QChar(0x0B) +
                             QStringLiteral("c\r\nd\re") + QChar(QChar::LineSeparator) +
                             QStringLiteral("f\tg\n");
        editor->insertFromMimeData(plainText(text).get());
        const QString kml = editor->toKml();
        CHECK(editor->plainText() == QStringLiteral("ab\nc\nd\ne\nf\tg\nA bold word"));

        // The saved chapter loads again
        auto reloaded = editorWith(kml);
        CHECK(reloaded->toKml() == kml);
    }
}

TEST_CASE("Stage2 paste: Enter and Delete over a selection are one undo step",
          "[editor][stage2][paste]") {
    auto editor = editorWith(kSource);
    const QString original = editor->toKml();
    editor->setSelection({{0, 6}, {1, 2}});

    SECTION("Enter") {
        editor->insertNewline();
        CHECK(editor->paragraphCount() == 3);
        CHECK(editor->paragraphPlainText(1) == QStringLiteral("ght side"));
    }

    SECTION("Delete") {
        editor->deleteSelectedText();
        CHECK(editor->paragraphCount() == 2);
    }

    editor->undo();
    CHECK(editor->toKml() == original);
    CHECK_FALSE(editor->canUndo());
}

TEST_CASE("Stage2 paste: the image after paste and undo is the image before",
          "[editor][stage2][paste][render]") {
    QStringList paragraphs;
    for (int i = 0; i < 6; ++i) paragraphs << longParagraph(i);
    auto editor = editorWith(kmlOf(paragraphs));
    editor->setSelection({{3, 10}, {5, 30}});
    const auto mimeData = editor->createMimeDataFromSelection();
    editor->clearSelection();
    editor->setCursorPosition({0, 0});
    editor->scrollTo(0.0);
    const QImage before = editor->grab().toImage();

    editor->setSelection({{1, 5}, {2, 12}});  // on screen
    editor->insertFromMimeData(mimeData.get());
    editor->scrollTo(0.0);
    CHECK(editor->grab().toImage() != before);

    editor->undo();
    editor->setCursorPosition({0, 0});
    editor->scrollTo(0.0);
    CHECK(editor->grab().toImage() == before);
}

TEST_CASE("Stage2 paste: KML becomes plain HTML for other programs", "[editor][stage2][paste]") {
    CHECK(ClipboardHandler::kmlToHtml(QStringLiteral(
              "<kml><p align=\"center\"><b font=\"Georgia\" size=\"14\">Big</b> "
              "<comment id=\"c1\">noted</comment> <span color=\"#aa0000\">red</span></p>"
              "<p></p><p>a &amp; b</p></kml>")) ==
          QStringLiteral("<p style=\"text-align:center\">"
                         "<b style=\"font-family:'Georgia';font-size:14pt\">Big</b> noted "
                         "<span style=\"color:#aa0000\">red</span></p>"
                         "<p>\u00A0</p><p>a &amp; b</p>"));
}

// =============================================================================
// Layout on demand
// =============================================================================

namespace {

/// Paragraphs of different lengths and letter widths: like real text, they make the
/// estimated heights of blocks never laid out miss
QStringList mixedParagraphs(int count) {
    QStringList list;
    for (int i = 0; i < count; ++i) {
        QString text;
        switch (i % 4) {
            case 0: text = QStringLiteral("WWW MMM WWWW MMMM ").repeated(4 + i % 5); break;
            case 1: text = QStringLiteral("iii lll iiii llll ").repeated(6 + i % 5); break;
            case 2:
                text = QStringLiteral("Pneumonoultramicroscopicsilicovolcanoconiosis is long. ")
                           .repeated(2 + i % 3);
                break;
            default: text = (longParagraph(i) + QLatin1Char(' ')).repeated(1 + i % 3); break;
        }
        list << text.trimmed();
    }
    return list;
}

/// The line at the top of the editor's view (document y = scroll offset)
struct TopLine {
    int block = -1;
    int start = 0;   ///< First character of the line, in the block
    int length = 0;
};

TopLine topLine(const BookEditor& editor) {
    const auto* layout = layoutOf(editor);
    const qreal y = editor.scrollOffset();
    TopLine top;
    top.block = layout->blockNumberAtY(y);
    const QTextLayout* lines = editor.textDocument()->findBlockByNumber(top.block).layout();
    const int index = KalahariTextDocumentLayout::lineIndexAt(
        *lines, y - layout->blockY(top.block), layout->typography().lineSpacing);
    top.start = lines->lineAt(index).textStart();
    top.length = lines->lineAt(index).textLength();
    return top;
}

void sendCtrlWheel(BookEditor& editor, int angleDelta) {
    QWheelEvent wheel(QPointF(100, 100), QPointF(100, 100), QPoint(), QPoint(0, angleDelta),
                      Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&editor, &wheel);
}

/// Image of the text area: the editor without its scroll bar, whose thumb follows the
/// document height (it changes as estimated heights are replaced)
QImage textArea(BookEditor& editor) {
    return editorImage(editor).copy(0, 0, editor.width() - editor.verticalScrollBar()->width(),
                                    editor.height());
}

}  // anonymous namespace

TEST_CASE("Stage2 on demand: a new width leaves every block waiting for layout",
          "[editor][stage2][ondemand]") {
    const QStringList paragraphs = mixedParagraphs(40);
    TypesetDocument d;
    d.load(paragraphs);
    d.layout->layoutPendingBlocks();
    REQUIRE(d.layout->pendingBlockCount() == 0);

    LaidOutBlockCounter laidOut(*d.doc);
    d.doc->setTextWidth(2 * kWidth);
    CHECK(laidOut.count() == 0);
    CHECK(d.layout->pendingBlockCount() == 40);
    CHECK_FALSE(d.layout->isLaidOut(0));

    SECTION("ensureLaidOut lays out only the blocks asked for") {
        CHECK(d.layout->ensureLaidOut(10, 12));
        CHECK(laidOut.count() == 3);
        CHECK(d.layout->isLaidOut(10));
        CHECK(d.layout->isLaidOut(12));
        CHECK_FALSE(d.layout->isLaidOut(9));
        CHECK_FALSE(d.layout->isLaidOut(13));
        CHECK_FALSE(d.layout->ensureLaidOut(10, 12));  // nothing waits there any more
    }

    SECTION("laid out, the blocks have the geometry of a layout from scratch") {
        d.layout->layoutPendingBlocks();
        CHECK(laidOut.count() == 40);

        TypesetDocument reference;
        reference.doc->setTextWidth(2 * kWidth);
        reference.load(paragraphs);
        reference.layout->layoutPendingBlocks();
        for (int i = 0; i < 40; ++i) {
            CHECK(d.layout->blockHeight(i) == reference.layout->blockHeight(i));
        }
        CHECK(d.layout->documentSize() == reference.layout->documentSize());
    }

    SECTION("lines are read through blockLayout(), which lays the block out first") {
        const QTextLayout* lines = KalahariTextDocumentLayout::blockLayout(d.block(5));
        CHECK(d.layout->isLaidOut(5));
        CHECK(lines == d.block(5).layout());
        CHECK(laidOut.count() == 1);
    }
}

TEST_CASE("Stage2 on demand: the background pass lays out the waiting blocks in steps",
          "[editor][stage2][ondemand]") {
    TypesetDocument d;
    d.load(mixedParagraphs(3000));
    REQUIRE(d.layout->pendingBlockCount() == 3000);  // a large change waits for layout

    int steps = 0;
    QObject::connect(d.layout, &KalahariTextDocumentLayout::blocksLaidOut, [&steps] { ++steps; });
    REQUIRE(waitUntil([&d] { return d.layout->pendingBlockCount() == 0; }, 20000));
    CHECK(steps > 1);  // a step stops after a few milliseconds, so input is not held up
}

TEST_CASE("Stage2 on demand: small edits are laid out at once, a large paste waits",
          "[editor][stage2][ondemand]") {
    TypesetDocument d;
    d.load(mixedParagraphs(10));
    d.layout->layoutAllBlocks();  // laid out on demand, as after a resize: lines only
    CHECK_FALSE(d.block(3).layout()->cacheEnabled());

    QTextCursor cursor(d.block(3));
    cursor.insertText(QStringLiteral("Typed. "));
    cursor.insertBlock();
    CHECK(d.layout->pendingBlockCount() == 0);
    // The edited blocks keep their glyphs for the cursor working in them
    CHECK(d.block(3).layout()->cacheEnabled());
    CHECK(d.block(4).layout()->cacheEnabled());

    cursor.insertText(mixedParagraphs(100).join(QLatin1Char('\n')));
    CHECK(d.layout->pendingBlockCount() >= 100);

    d.layout->layoutPendingBlocks();
    CHECK_FALSE(d.block(50).layout()->cacheEnabled());
    QStringList text;
    for (QTextBlock b = d.doc->begin(); b.isValid(); b = b.next()) text << b.text();
    TypesetDocument reference;
    reference.load(text);
    reference.layout->layoutPendingBlocks();
    CHECK(d.layout->documentSize() == reference.layout->documentSize());
}

TEST_CASE("Stage2 on demand: the editor lays out what it shows, the rest in the background",
          "[editor][stage2][ondemand]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf(mixedParagraphs(300)));
    auto* layout = layoutOf(editor);
    CHECK(layout->pendingBlockCount() == 300);  // the load lays out nothing

    editor.grab();
    // The view shows the document down to its height less the page's top margin and gap
    // above the text
    const qreal shownBottom = editor.height() - textOrigin(editor).y();
    const int shown = layout->blockNumberAtY(editor.scrollOffset() + shownBottom) + 1;
    for (int i = 0; i < shown; ++i) {
        CHECK(layout->isLaidOut(i));
    }
    // The view finds its blocks by the estimated heights first: a block shown by the
    // estimates can end up below the view once the blocks above it are laid out
    CHECK(layout->pendingBlockCount() <= 300 - shown);
    CHECK(layout->pendingBlockCount() >= 300 - shown - 2);
    // The glyphs of a laid out block are shaped again when drawn, not kept in memory
    CHECK_FALSE(editor.textDocument()->firstBlock().layout()->cacheEnabled());

    REQUIRE(waitUntil([layout] { return layout->pendingBlockCount() == 0; }, 20000));
}

TEST_CASE("Stage2 on demand: blocks laid out above the view leave its text in place",
          "[editor][stage2][ondemand][render]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf(mixedParagraphs(300)));
    auto* layout = layoutOf(editor);

    editor.setScrollOffset(editor.verticalScrollBar()->maximum() * 0.5);
    const QImage shown = textArea(editor);
    const qreal estimatedHeight = layout->documentSize().height();
    const qreal scrollShown = editor.scrollOffset();

    layout->layoutPendingBlocks();  // the background pass, at once
    REQUIRE(layout->documentSize().height() != estimatedHeight);  // the estimates missed
    CHECK(editor.scrollOffset() != scrollShown);  // the scroll position followed the text
    CHECK(editor.verticalScrollBar()->value() == static_cast<int>(editor.scrollOffset()));
    CHECK(textArea(editor) == shown);
}

TEST_CASE("Stage2 on demand: Ctrl+End right after loading shows the end",
          "[editor][stage2][ondemand][render]") {
    // Wide letters at the end: estimated too short, the paragraphs on screen grow when
    // laid out and would push the end out of view
    QStringList paragraphs = mixedParagraphs(300);
    for (int i = 0; i < 15; ++i) {
        paragraphs << QStringLiteral("WWW MMM WWWW MMMM ").repeated(8).trimmed();
    }
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf(paragraphs));
    auto* layout = layoutOf(editor);

    editor.moveCursorToDocEnd();
    const QImage shown = textArea(editor);
    // The view shows the end of the page: the last line, with the page's bottom margin
    // below it
    const QRectF caret = editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    CHECK(editor.verticalScrollBar()->value() == editor.verticalScrollBar()->maximum());
    CHECK(caret.top() >= 0.0);
    CHECK(caret.bottom() <= editor.height());

    layout->layoutPendingBlocks();
    CHECK(textArea(editor) == shown);
}

TEST_CASE("Stage2 on demand: a resize keeps the text in place, laying out nothing",
          "[editor][stage2][ondemand][render]") {
    // Every view wraps the text at the page's width: a resize moves the page in the window
    const double position = GENERATE(0.0, 0.4, 1.0);  // share of the scroll range
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf(mixedParagraphs(300)));
    auto* layout = layoutOf(editor);
    layout->layoutPendingBlocks();
    editor.setScrollOffset(editor.verticalScrollBar()->maximum() * position);
    editor.grab();
    const TopLine before = topLine(editor);

    LaidOutBlockCounter laidOut(editor);
    resizeWidget(editor, QSize(800, 400));
    const QImage resized = textArea(editor);
    CHECK(laidOut.count() == 0);

    // The text stays where it was
    const TopLine after = topLine(editor);
    CHECK(after.block == before.block);
    CHECK(after.start == before.start);

    layout->layoutPendingBlocks();
    CHECK(textArea(editor) == resized);
}

TEST_CASE("Stage2 on demand: a visible editor keeps the page's lines during a resize",
          "[editor][stage2][ondemand]") {
    // Regression (Stage 1): a width change re-laid out the whole document, so a visible
    // editor kept the old width until the window edge stopped for 80 ms, leaving a blank
    // strip or cut lines in the meantime. Every view wraps at the page's width now: a
    // resize moves the page in the window and lays out nothing.
    BookEditor editor;
    editor.setAttribute(Qt::WA_DontShowOnScreen);  // visible to Qt, no window on screen
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf(mixedParagraphs(300)));
    editor.show();
    REQUIRE(editor.isVisible());
    runEventLoop(50);
    layoutOf(editor)->layoutPendingBlocks();  // the background pass, at once

    LaidOutBlockCounter laidOut(editor);
    const qreal width = editor.textDocument()->textWidth();
    for (int windowWidth = 640; windowWidth <= 880; windowWidth += 40) {
        editor.resize(windowWidth, 400);  // a visible widget gets the resize event at once
        CHECK(editor.textDocument()->textWidth() == width);
        editor.repaint();
    }
    CHECK(laidOut.count() == 0);
}

TEST_CASE("Stage2 on demand: Ctrl+wheel zooms at every notch, around the mouse pointer",
          "[editor][stage2][ondemand]") {
    // Regression (Stage 1): font-scaling zoom was applied once the wheel stopped for 80 ms.
    // The painter scales the page now: the lines stay, and the text under the pointer
    // stays under it.
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf(mixedParagraphs(300)));
    layoutOf(editor)->layoutPendingBlocks();
    editor.setScrollOffset(editor.verticalScrollBar()->maximum() * 0.4 + 7);  // within a line
    editor.grab();
    const qreal baseSize = editor.textDocument()->defaultFont().pointSizeF();
    const qreal baseHeight = layoutOf(editor)->documentSize().height();

    // The caret on a line in the view, and its distance from the pointer (sendCtrlWheel's
    // position)
    const TopLine shown = topLine(editor);
    editor.setCursorPosition({shown.block, shown.start});
    const qreal pointerY = 100.0;
    const qreal caretY = editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF().top();
    REQUIRE(caretY > 0.0);
    REQUIRE(caretY < editor.height());

    LaidOutBlockCounter laidOut(editor);
    int notch = 0;
    for (const int delta : {120, 120, 120, -120, -120, -120, -120}) {
        sendCtrlWheel(editor, delta);
        notch += delta > 0 ? 1 : -1;
        const double zoom = std::pow(1.1, notch);
        CHECK(editor.zoomFactor() == Approx(zoom));
        CHECK(editor.textDocument()->defaultFont().pointSizeF() == Approx(baseSize));
        editor.grab();

        // The caret's distance from the pointer follows the zoom
        const QRectF caret = editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
        CHECK(caret.top() - pointerY == Approx((caretY - pointerY) * zoom).margin(1.0));
    }
    CHECK(laidOut.count() == 0);
    CHECK(layoutOf(editor)->documentSize().height() == Approx(baseHeight));
}

TEST_CASE("Stage2 on demand: justified lines reach the right edge without cached glyphs",
          "[editor][stage2][ondemand][render]") {
    BookEditor editor;
    resizeWidget(editor, QSize(900, 400));  // the whole page in the view
    editor.setAppearance(appearanceWith(1.0, 0.0, false, 0.0));
    // No punctuation: every line ends with a letter, whose ink reaches the line's end
    const QString words = QStringLiteral(
        "lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod tempor "
        "incididunt ut labore et dolore magna aliqua ");
    editor.fromKml(QStringLiteral("<kml><p align=\"justify\">") + words.repeated(4).trimmed() +
                   QStringLiteral("</p></kml>"));
    layoutOf(editor)->layoutAllBlocks();  // laid out on demand, as after a resize

    const EditorAppearance& appearance = editor.appearance();
    const QColor background = appearance.colors.background(appearance.colorMode);
    const QPointF origin = textOrigin(editor);
    const int marginLeft = static_cast<int>(std::round(origin.x()));
    const int marginTop = static_cast<int>(std::round(origin.y()));
    const int textRight = marginLeft + static_cast<int>(editor.textDocument()->textWidth());
    const QTextLayout* lines =
        KalahariTextDocumentLayout::blockLayout(editor.textDocument()->firstBlock());
    REQUIRE(lines->lineCount() >= 4);
    REQUIRE_FALSE(lines->cacheEnabled());

    // Rightmost ink of every line but the last (which is not stretched), left of the
    // scroll bar
    const QImage image = editorImage(editor);
    std::vector<int> rightEdges;
    for (int i = 0; i + 1 < lines->lineCount(); ++i) {
        const QTextLine line = lines->lineAt(i);
        const int top = marginTop + static_cast<int>(line.y());
        const int bottom = marginTop + static_cast<int>(line.y() + line.height());
        int rightmost = -1;
        for (int x = std::min(textRight + 4, image.width() - 1); x >= 0 && rightmost < 0; --x) {
            for (int y = top; y < bottom; ++y) {
                if (differs(image, x, y, background)) {
                    rightmost = x;
                    break;
                }
            }
        }
        rightEdges.push_back(rightmost);
    }
    // Qt finds where to stretch a line only when it shapes text with HarfBuzz (qtbase
    // feature "harfbuzz" in vcpkg.json); without it, justified lines stay ragged
    const auto [narrowest, widest] = std::minmax_element(rightEdges.begin(), rightEdges.end());
    CHECK(*narrowest >= textRight - 4);
    CHECK(*widest <= textRight + 1);
}
