/// @file test_paragraph_layout.cpp
/// @brief How the editor lays out a paragraph's own formatting and the items of a list
///
/// The indents, spacing and line spacing of a paragraph go over its style; in the page view a
/// paragraph may start a new page, keep its lines together, and never leaves its first or
/// last line alone on a page unless it turns widow control off. A list item stands at the
/// indent of its level with its number in front of its first line; after an edit the numbers
/// of the items below follow.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_package.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_format.h>
#include <kalahari/editor/text_lists.h>
#include <kalahari/editor/view_modes.h>
#include "editor_test_utils.h"

#include <QImage>
#include <QScrollBar>
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

/// A style of a package
kalahari::core::PackageParagraphStyle packageStyle(const QString& id,
                                                   const QVariantMap& properties = {}) {
    kalahari::core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), id);
    style.properties = properties;
    return style;
}

/// Light paper, black text
EditorAppearance lightAppearance() {
    EditorAppearance appearance;
    appearance.colorMode = EditorColorMode::Light;
    appearance.colors.continuous.backgroundLight = QColor(255, 255, 255);
    appearance.colors.continuous.textLight = QColor(0, 0, 0);
    appearance.cursor.blinking = false;
    return appearance;
}

/// An editor with Normal of 12 pt and the KML content
std::unique_ptr<BookEditor> editorWith(const QString& content) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->setAppearance(lightAppearance());
    editor->setParagraphStyles(ParagraphStyleSet::fromPackage(
        {packageStyle(QStringLiteral("normal"), {{QStringLiteral("fontSize"), 12}})},
        QStringLiteral("en")));
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

/// Pixels of a point of the package in the view: Normal's 12 pt shown in the text's font
qreal pixelsOfPoint(const BookEditor& editor) {
    const QFont font = editor.textDocument()->defaultFont();
    return font.pointSizeF() / 12.0 * QFontMetricsF(font).fontDpi() / 72.0;
}

/// A paragraph long enough for several lines
QString longText(int words = 60) {
    return QStringLiteral("word ").repeated(words).trimmed();
}

/// What the editor shows
QImage editorImage(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// Dark pixels of a rectangle of an image
int inkIn(const QImage& image, const QRect& rect) {
    int count = 0;
    for (int y = std::max(0, rect.top()); y <= std::min(image.height() - 1, rect.bottom()); ++y) {
        for (int x = std::max(0, rect.left()); x <= std::min(image.width() - 1, rect.right()); ++x) {
            if (qGray(image.pixel(x, y)) < 128) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace

// =============================================================================
// A paragraph's own formatting
// =============================================================================

TEST_CASE("Paragraph layout: indents and spacing of a paragraph over its style",
          "[editor][paragraph][layout]") {
    auto editor = editorWith(QStringLiteral(
        "<p>Before</p><p indent-left=\"36\" indent-right=\"24\" indent-first=\"-18\" "
        "space-before=\"12\" space-after=\"30\">") + longText() + QStringLiteral("</p><p>After</p>"));
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    const qreal pixels = pixelsOfPoint(*editor);
    const QTextBlock block = blockOf(*editor, 1);
    const KalahariTextDocumentLayout::BlockMetrics& metrics = layout->blockMetrics(block);
    CHECK(metrics.leftMargin == std::round(36 * pixels));
    CHECK(metrics.rightMargin == std::round(24 * pixels));
    CHECK(metrics.firstLineIndent == Approx(-18 * pixels));
    CHECK(metrics.spaceBefore == std::round(12 * pixels));
    CHECK(metrics.spaceAfter == std::round(30 * pixels));

    // The first line hangs out of the others, all of them end at the right indent
    const QTextLayout* lines = block.layout();
    REQUIRE(lines->lineCount() >= 3);
    CHECK(lines->lineAt(0).x() == Approx(metrics.leftMargin + metrics.firstLineIndent));
    CHECK(lines->lineAt(1).x() == Approx(metrics.leftMargin));
    const qreal textWidth = editor->textDocument()->textWidth();
    CHECK(lines->lineAt(1).x() + lines->lineAt(1).width() ==
          Approx(textWidth - metrics.rightMargin).margin(0.05));

    // The other paragraphs have their style's
    CHECK(layout->blockMetrics(blockOf(*editor, 0)).leftMargin == 0.0);
    CHECK(lineTop(*editor, 1, 0) - layout->blockY(1) >= metrics.spaceBefore);
}

TEST_CASE("Paragraph layout: line spacing exactly, at least and a multiple",
          "[editor][paragraph][layout]") {
    auto editor = editorWith(QStringLiteral("<p line-height-exact=\"30\">") + longText() +
                             QStringLiteral("</p><p line-height-min=\"2\">") + longText() +
                             QStringLiteral("</p><p>") + longText() +
                             QStringLiteral("</p><p line-height=\"2\">") + longText() +
                             QStringLiteral("</p>"));
    const qreal pixels = pixelsOfPoint(*editor);
    const auto pitchOf = [&editor](int paragraph) {
        const QTextLayout* lines = blockOf(*editor, paragraph).layout();
        REQUIRE(lines->lineCount() >= 2);
        return lines->lineAt(1).y() - lines->lineAt(0).y();
    };
    CHECK(pitchOf(0) == Approx(std::round(30 * pixels)));
    CHECK(pitchOf(1) == Approx(pitchOf(2)));  // at least 2 pt: the lines as the style has them
    CHECK(pitchOf(3) > pitchOf(2) * 1.5);
}

TEST_CASE("Paragraph layout: the page view keeps the breaks of a paragraph",
          "[editor][paragraph][layout][pageflow]") {
    SECTION("A paragraph that starts a new page") {
        auto editor = editorWith(
            QStringLiteral("<p>Before</p><p page-break-before=\"true\">After</p>"));
        CHECK(lineTop(*editor, 1, 0) < 200.0);  // the endless page: no new page
        editor->setViewMode(ViewMode::Page);
        layoutOf(*editor)->layoutPendingBlocks();
        CHECK(pageOf(*editor, lineTop(*editor, 0, 0)) == 0);
        CHECK(pageOf(*editor, lineTop(*editor, 1, 0)) == 1);
        CHECK(lineTop(*editor, 1, 0) ==
              Approx(layoutOf(*editor)->pageFlow().pitch + lineTop(*editor, 0, 0)).margin(0.5));
    }

    // Wherever a long paragraph starts on a page, it never leaves a single line alone on a
    // page, nor is it split with its lines kept together; without widow control some place
    // leaves one alone. The space after the paragraph before it moves its start down the
    // whole text area in steps shorter than a line, whatever the metrics of the font
    const auto lonelyLines = [](const QString& attributes) {
        int lonely = 0;
        int split = 0;
        const auto probe = editorWith(QStringLiteral("<p>Before</p>"));
        probe->setViewMode(ViewMode::Page);
        const qreal areaPoints = layoutOf(*probe)->pageFlow().textHeight / pixelsOfPoint(*probe);
        REQUIRE(areaPoints > 100.0);
        for (int space = 0; space < areaPoints; space += 4) {
            auto editor = editorWith(QStringLiteral("<p space-after=\"%1\">Before</p><p ").arg(space) +
                                     attributes + QLatin1Char('>') + longText(70) +
                                     QStringLiteral("</p><p>After</p>"));
            editor->setViewMode(ViewMode::Page);
            layoutOf(*editor)->layoutPendingBlocks();
            const int lines = blockOf(*editor, 1).layout()->lineCount();
            REQUIRE(lines >= 4);
            const int firstPage = pageOf(*editor, lineTop(*editor, 1, 0));
            const int lastPage = pageOf(*editor, lineTop(*editor, 1, lines - 1));
            split += firstPage != lastPage ? 1 : 0;
            if (pageOf(*editor, lineTop(*editor, 1, 1)) != firstPage ||
                pageOf(*editor, lineTop(*editor, 1, lines - 2)) != lastPage) {
                ++lonely;
            }
        }
        return std::make_pair(lonely, split);
    };
    SECTION("Widow control, on without the attribute") {
        const auto [lonely, split] = lonelyLines(QString());
        CHECK(lonely == 0);
        CHECK(split > 0);
        CHECK(lonelyLines(QStringLiteral("widow-control=\"false\"")).first > 0);
    }
    SECTION("All the lines of a paragraph on one page") {
        CHECK(lonelyLines(QStringLiteral("keep-together=\"true\"")).second == 0);
    }
}

// =============================================================================
// Lists
// =============================================================================

TEST_CASE("Paragraph layout: a list item at the indent of its level, its number in front",
          "[editor][lists][layout]") {
    auto editor = editorWith(QStringLiteral("<ol><li>") + longText() +
                             QStringLiteral("<ol><li>Inner</li></ol></li></ol><p>After</p>"));
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    const qreal pixels = pixelsOfPoint(*editor);

    const QTextBlock item = blockOf(*editor, 0);
    const QTextLayout* lines = item.layout();
    REQUIRE(lines->lineCount() >= 2);
    CHECK(lines->lineAt(0).x() == Approx(std::round(36 * pixels)));
    CHECK(lines->lineAt(1).x() == Approx(std::round(36 * pixels)));
    const std::optional<KalahariTextDocumentLayout::ListMarker> marker = layout->listMarker(item);
    REQUIRE(marker.has_value());
    CHECK(marker->text == QStringLiteral("1."));
    CHECK(marker->x == Approx(std::round(18 * pixels)));
    CHECK(marker->width > 0.0);

    const QTextBlock inner = blockOf(*editor, 1);
    CHECK(inner.layout()->lineAt(0).x() == Approx(std::round(54 * pixels)));
    CHECK(layout->listMarker(inner)->x == Approx(std::round(36 * pixels)));
    CHECK_FALSE(layout->listMarker(blockOf(*editor, 2)).has_value());

    SECTION("A number wider than its room pushes the text") {
        auto wide = editorWith(QStringLiteral("<ol type=\"I\" start=\"3888\"><li>Text</li></ol>"));
        const QTextBlock block = blockOf(*wide, 0);
        const auto wideMarker = layoutOf(*wide)->listMarker(block);
        REQUIRE(wideMarker.has_value());
        CHECK(wideMarker->text == QStringLiteral("MMMDCCCLXXXVIII."));
        CHECK(block.layout()->lineAt(0).x() >= wideMarker->x + wideMarker->width);
    }

    SECTION("An item without a number: its text at the indent, no number") {
        auto unnumbered = editorWith(QStringLiteral("<ol><li numbered=\"false\">Text</li></ol>"));
        const QTextBlock block = blockOf(*unnumbered, 0);
        CHECK_FALSE(layoutOf(*unnumbered)->listMarker(block).has_value());
        CHECK(block.layout()->lineAt(0).x() == Approx(std::round(36 * pixels)));
    }
}

TEST_CASE("Paragraph layout: the numbers below an edit follow it", "[editor][lists][layout]") {
    auto editor = editorWith(
        QStringLiteral("<ol><li>One</li><li>Two</li><li>Three</li></ol><p>After</p>"));
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    layout->layoutPendingBlocks();
    CHECK(layout->listMarker(blockOf(*editor, 2))->text == QStringLiteral("3."));

    // A new item at the start of the list (Enter at the start of its first item)
    QTextCursor cursor(editor->textDocument());
    cursor.insertBlock(cursor.blockFormat());
    CHECK(layout->listMarker(blockOf(*editor, 3))->text == QStringLiteral("4."));

    // The item taken out of the list
    QTextBlockFormat format = editor->textDocument()->firstBlock().blockFormat();
    clearListItem(format);
    cursor.setPosition(0);
    cursor.setBlockFormat(format);
    CHECK_FALSE(layout->listMarker(blockOf(*editor, 0)).has_value());
    CHECK(layout->listMarker(blockOf(*editor, 3))->text == QStringLiteral("3."));
}

TEST_CASE("Paragraph layout: the number of an item is drawn", "[editor][lists][layout][view]") {
    // The same paragraph with a number and without one: the number is ink in front of it
    const auto inkOfNumber = [](const QString& attributes) {
        auto editor = editorWith(QStringLiteral("<ol><li ") + attributes +
                                 QStringLiteral(">Text</li></ol>"));
        editor->setCursorPosition({0, 0});
        const QRect caret = editor->inputMethodQuery(Qt::ImCursorRectangle).toRect();
        const QImage image = editorImage(*editor);
        return inkIn(image, QRect(0, caret.top(), caret.left() - 1, caret.height()));
    };
    CHECK(inkOfNumber(QString()) > 0);
    CHECK(inkOfNumber(QStringLiteral("numbered=\"false\"")) == 0);
}

TEST_CASE("Paragraph layout: the letter spacing of text, in points", "[editor][format][layout]") {
    auto editor = editorWith(QStringLiteral(
        "<p>abcdef</p><p><span spacing=\"3\">abcdef</span></p><p><span spacing=\"-1\">abcdef</span></p>"));
    const qreal normal = blockOf(*editor, 0).layout()->lineAt(0).naturalTextWidth();
    const qreal expanded = blockOf(*editor, 1).layout()->lineAt(0).naturalTextWidth();
    const qreal condensed = blockOf(*editor, 2).layout()->lineAt(0).naturalTextWidth();

    // Each of the six letters takes 3 points more, or 1 less, in the pixels of the view; the
    // text keeps its size (only the letter spacing is given)
    const qreal pixelsPerPoint = QFontMetricsF(editor->textDocument()->defaultFont()).fontDpi() / 72.0;
    CHECK(expanded - normal == Approx(6 * 3 * pixelsPerPoint).margin(1.0));
    CHECK(normal - condensed == Approx(6 * 1 * pixelsPerPoint).margin(1.0));
}
