/// @file test_editor_stage2.cpp
/// @brief Editor Stage 2 (variant A): typography, paste and undo, on-demand layout
///
/// Geometry is checked on the layout itself; what the user sees is checked on images of
/// the editor (QWidget::grab()), compared pixel by pixel with the background.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include "editor_test_utils.h"

#include <QImage>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
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

/// A document wired like BookEditor::ensureEditMode() wires it
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

/// True when the pixel differs visibly from @p background
bool differs(const QImage& image, int x, int y, QColor background) {
    const QColor c = image.pixelColor(x, y);
    return std::abs(c.red() - background.red()) + std::abs(c.green() - background.green()) +
               std::abs(c.blue() - background.blue()) > 24;
}

/// Leftmost column in rows [top, bottom) with a pixel differing from the background
int leftmostInk(const QImage& image, int top, int bottom, QColor background) {
    for (int x = 0; x < image.width(); ++x) {
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

    // A font-scaling zoom doubles the font: spacing and indent double in the same relayout
    QFont zoomed = testFont();
    zoomed.setPointSizeF(24.0);
    d.doc->setDefaultFont(zoomed);
    CHECK(d.layout->paragraphSpacing() == Approx(20.0));
    CHECK(d.layout->firstLineIndent() == Approx(40.0));
    CHECK(d.block(0).layout()->lineAt(0).x() == Approx(40.0));
}

TEST_CASE("Stage2 typography: hit test between lines and in the paragraph spacing",
          "[editor][stage2][typography]") {
    TypesetDocument d;
    d.load({longParagraph(0), longParagraph(1)});
    d.layout->setTypography({2.0, 16.0, 0.0, 0.0});

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
    CHECK(layout->typography().lineSpacing == Approx(1.0));
    CHECK(layout->paragraphSpacing() == Approx(0.0));
    CHECK(layout->firstLineIndent() == Approx(0.0));
    CHECK(layout->documentSize().height() < spacedHeight);

    SECTION("zoom scales the spacing with the font, in one relayout") {
        editor.setAppearance(appearanceWith(1.5, 10.0, true, 20.0));
        FullRelayoutCounter relayouts(editor);
        editor.setZoomFactor(2.0);
        CHECK(relayouts.count() == 1);
        CHECK(layout->paragraphSpacing() == Approx(20.0));
        CHECK(layout->firstLineIndent() == Approx(40.0));
    }
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
    const int marginLeft = static_cast<int>(appearance.viewMargins.horizontal);
    const int marginTop = static_cast<int>(appearance.viewMargins.vertical);

    const QTextLayout* layout = editor.textDocument()->firstBlock().layout();
    REQUIRE(layout->lineCount() >= 3);
    const QTextLine line0 = layout->lineAt(0);
    const QTextLine line1 = layout->lineAt(1);

    SECTION("the first line starts at the indent, the next ones at the margin") {
        const QImage image = editor.grab().toImage();
        const int ink0 = leftmostInk(image, marginTop + static_cast<int>(line0.y()),
                                     marginTop + static_cast<int>(line0.y() + line0.height()),
                                     background);
        const int ink1 = leftmostInk(image, marginTop + static_cast<int>(line1.y()),
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

        const QImage unselected = editor.grab().toImage();
        CHECK_FALSE(differs(unselected, column, gapRow, background));

        editor.selectAll();
        const QImage selected = editor.grab().toImage();
        CHECK(differs(selected, column, gapRow, background));
    }
}
