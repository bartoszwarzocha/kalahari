/// @file test_margin_options.cpp
/// @brief Editor options of the margins: mirror margins, the numbers of the lines and the
///        paragraphs on the margin

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/view_modes.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QImage>
#include <QMouseEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <functional>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

namespace {

/// The color of the numbers in these tests: none of the paper or the text has it
const QColor NUMBERS_COLOR(255, 0, 0);

/// Paragraphs long enough to wrap over a few lines, so that pages break inside them too
QString paragraphs(int count) {
    QString kml;
    for (int i = 0; i < count; ++i) {
        kml += QStringLiteral("<p>Paragraph %1 tells a story long enough to wrap over a few "
                              "lines of the page, so that pages break inside paragraphs too.</p>")
                   .arg(i);
    }
    return kml;
}

/// An editor in @p mode with the KML @p content, light paper, the cursor not blinking, the
/// numbers red; A4 pages fit its width at 100%
std::unique_ptr<BookEditor> editorWith(ViewMode mode, const QString& content) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(1000, 700));
    EditorAppearance appearance = editor->appearance();
    appearance.colorMode = EditorColorMode::Light;
    appearance.cursor.blinking = false;
    appearance.marginNumbers.color = NUMBERS_COLOR;
    editor->setAppearance(appearance);
    editor->fromKml(QStringLiteral("<kml>") + content + QStringLiteral("</kml>"));
    editor->setViewMode(mode);
    return editor;
}

/// Change the editor's appearance with @p change
void change(BookEditor& editor, const std::function<void(EditorAppearance&)>& change) {
    EditorAppearance appearance = editor.appearance();
    change(appearance);
    editor.setAppearance(appearance);
}

QImage paint(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    editor.render(&image);
    return image;
}

QRectF caret(BookEditor& editor) {
    return editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
}

void click(BookEditor& editor, const QPointF& pos) {
    QMouseEvent press(QEvent::MouseButtonPress, pos, editor.mapToGlobal(pos), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, editor.mapToGlobal(pos), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &release);
}

/// The x of the cursor at @p position, the editor painted
qreal caretX(BookEditor& editor, const CursorPosition& position) {
    editor.setCursorPosition(position);
    paint(editor);
    return caret(editor).left();
}

/// The red pixels of the numbers left of the cursor, on its line (also on the margin of a
/// centred heading)
int numberInk(BookEditor& editor) {
    const QImage image = paint(editor);
    const QRectF at = caret(editor);
    const int right = static_cast<int>(at.left()) - 3;
    const int left = 0;
    int ink = 0;
    for (int y = std::max(0, static_cast<int>(at.top())); y < std::min(image.height(), static_cast<int>(at.bottom())); ++y) {
        for (int x = left; x < right; ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.red() - std::max(pixel.green(), pixel.blue()) > 60) {
                ++ink;
            }
        }
    }
    return ink;
}

/// A place a character into line @p number of the chapter, counted from 1
CursorPosition atLine(BookEditor& editor, int number) {
    auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    layout->layoutPendingBlocks();
    int lines = 0;
    CursorPosition found{-1, 0};
    for (QTextBlock block = editor.textDocument()->begin(); block.isValid() && lines < number;
         block = block.next()) {
        for (int line = 0; line < block.layout()->lineCount() && lines < number; ++line) {
            if (++lines == number) {
                found = {block.blockNumber(), block.layout()->lineAt(line).textStart() + 1};
            }
        }
    }
    REQUIRE(found.paragraph >= 0);
    return found;
}

/// The numbers on the margin, as the Settings choose them
void number(BookEditor& editor, MarginNumbering numbering, int interval = 1, bool restart = false) {
    change(editor, [numbering, interval, restart](EditorAppearance& appearance) {
        appearance.marginNumbers.numbering = numbering;
        appearance.marginNumbers.interval = interval;
        appearance.marginNumbers.restartEachPage = restart;
    });
}

}  // namespace

TEST_CASE("Mirror margins: pages 2, 4... have the margins of page 1 the other way round, the "
          "cursor and the clicks with them",
          "[editor][margins][pagemode]") {
    auto editor = editorWith(ViewMode::Page, paragraphs(60));
    paint(*editor);
    REQUIRE(editor->totalPages() >= 3);

    // The margins of the inner and the outer one, both ways: the same text width, so the same
    // lines and pages
    const auto margins = [&editor](double left, double right, bool mirrored, bool firstLeft) {
        change(*editor, [=](EditorAppearance& appearance) {
            appearance.pageMargins.left = left;
            appearance.pageMargins.right = right;
            appearance.pageMargins.inner = 40.0;
            appearance.pageMargins.outer = 10.0;
            appearance.pageMargins.mirrored = mirrored;
            appearance.pageMargins.firstPageLeft = firstLeft;
        });
    };
    margins(40.0, 10.0, false, false);
    std::vector<CursorPosition> starts;  // a few characters into the first line of each page
    for (int page = 1; page <= 3; ++page) {
        editor->goToPage(page);
        paint(*editor);
        REQUIRE(editor->currentPage() == page);
        starts.push_back({editor->cursorPosition().paragraph, editor->cursorPosition().offset + 3});
    }
    const auto xs = [&editor, &starts]() {
        std::vector<qreal> result;
        result.reserve(starts.size());
        for (const CursorPosition& start : starts) {
            result.push_back(caretX(*editor, start));
        }
        return result;
    };
    const std::vector<qreal> innerLeft = xs();  // every page as a right page
    margins(10.0, 40.0, false, false);
    const std::vector<qreal> innerRight = xs();  // every page as a left page
    REQUIRE(innerRight.at(0) < innerLeft.at(0) - 50.0);

    // A right page first: page 2 is a left one, page 3 a right one
    margins(25.4, 25.4, true, false);
    const std::vector<qreal> mirrored = xs();
    CHECK(mirrored.at(0) == Approx(innerLeft.at(0)).margin(0.5));
    CHECK(mirrored.at(1) == Approx(innerRight.at(1)).margin(0.5));
    CHECK(mirrored.at(2) == Approx(innerLeft.at(2)).margin(0.5));

    // A chapter that starts on a left page
    margins(25.4, 25.4, true, true);
    const std::vector<qreal> leftFirst = xs();
    CHECK(leftFirst.at(0) == Approx(innerRight.at(0)).margin(0.5));
    CHECK(leftFirst.at(1) == Approx(innerLeft.at(1)).margin(0.5));

    // A click lands on the character under it on a shifted page too, at every zoom
    margins(25.4, 25.4, true, false);
    for (const double zoom : {0.75, 1.0, 1.5}) {
        editor->setZoomFactor(zoom);
        editor->goToPage(2);
        paint(*editor);
        const CursorPosition target = starts.at(1);
        editor->setCursorPosition(target);
        paint(*editor);
        const QRectF at = caret(*editor);
        editor->setCursorPosition({target.paragraph, target.offset + 5});  // on the same line
        CAPTURE(zoom);
        click(*editor, QPointF(at.left() + 1.0, at.center().y()));
        CHECK(editor->cursorPosition() == target);
    }
}

TEST_CASE("Mirror margins: the continuous view has the margins of page 1",
          "[editor][margins]") {
    auto editor = editorWith(ViewMode::Continuous, paragraphs(3));
    change(*editor, [](EditorAppearance& appearance) {
        appearance.pageMargins.left = 40.0;
        appearance.pageMargins.right = 10.0;
    });
    const qreal plain = caretX(*editor, {2, 3});
    change(*editor, [](EditorAppearance& appearance) {
        appearance.pageMargins.left = 25.4;
        appearance.pageMargins.right = 25.4;
        appearance.pageMargins.inner = 40.0;
        appearance.pageMargins.outer = 10.0;
        appearance.pageMargins.mirrored = true;
    });
    CHECK(caretX(*editor, {2, 3}) == Approx(plain).margin(0.5));
}

TEST_CASE("Margin numbers: the lines counted with every line, the paragraphs only with their text",
          "[editor][marginNumbers]") {
    auto editor = editorWith(ViewMode::Continuous,
                             QStringLiteral("<h1>Title</h1><p>One</p><p></p>") + paragraphs(1) +
                                 QStringLiteral("<scene-break/><p>Last</p>"));
    auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    layout->layoutPendingBlocks();
    const QTextDocument* document = editor->textDocument();
    const int long_ = document->findBlockByNumber(3).layout()->lineCount();
    REQUIRE(long_ > 1);

    // The heading, the empty paragraph and the scene break are lines, not paragraphs
    CHECK(layout->marginCountsBefore(0) == MarginCounts{0, 0});
    CHECK(layout->marginCountsBefore(1) == MarginCounts{1, 0});
    CHECK(layout->marginCountsBefore(2) == MarginCounts{2, 1});
    CHECK(layout->marginCountsBefore(3) == MarginCounts{3, 1});
    CHECK(layout->marginCountsBefore(4) == MarginCounts{3 + long_, 2});
    CHECK(layout->marginCountsBefore(5) == MarginCounts{4 + long_, 2});
    CHECK(layout->marginCountsBefore(6) == MarginCounts{5 + long_, 3});
    for (const auto& [block, numbered] : {std::pair{0, false}, std::pair{1, true},
                                          std::pair{2, false}, std::pair{3, true},
                                          std::pair{4, false}, std::pair{5, true}}) {
        CAPTURE(block);
        CHECK(KalahariTextDocumentLayout::isNumberedParagraph(document->findBlockByNumber(block)) ==
              numbered);
    }

    // An edit counts again: a paragraph more at the start
    QTextCursor cursor(editor->textDocument()->findBlockByNumber(1));
    cursor.insertText(QStringLiteral("New"));
    cursor.insertBlock();
    layout->layoutPendingBlocks();
    CHECK(layout->marginCountsBefore(3) == MarginCounts{3, 2});
    CHECK(layout->marginCountsBefore(7) == MarginCounts{6 + long_, 4});
}

TEST_CASE("Margin numbers: after every edit the counts are those of the whole text counted anew",
          "[editor][marginNumbers]") {
    auto editor = editorWith(ViewMode::Continuous, paragraphs(12));
    auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    QTextDocument* document = editor->textDocument();

    // The counts walked over every block, laid out
    const auto expectSame = [layout, document](const char* step) {
        CAPTURE(step);
        layout->layoutPendingBlocks();
        MarginCounts counts;
        int number = 0;
        for (QTextBlock block = document->begin(); block.isValid(); block = block.next(), ++number) {
            CAPTURE(number);
            CHECK(layout->marginCountsBefore(number) == counts);
            counts.lines += block.layout()->lineCount();
            counts.paragraphs += KalahariTextDocumentLayout::isNumberedParagraph(block) ? 1 : 0;
        }
        CHECK(layout->marginCountsBefore(number) == counts);
    };
    expectSame("loaded");

    // Counted before the layout too, then edited further on and further back
    QTextCursor cursor(document->findBlockByNumber(6));
    cursor.insertText(QStringLiteral("Longer, so that this paragraph gets another line or two of "
                                     "text on the page, as the writer goes on and on with it. "));
    (void)layout->marginCountsBefore(12);
    expectSame("typed in the middle");
    cursor.insertBlock();
    expectSame("split");
    cursor = QTextCursor(document->findBlockByNumber(2));
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    expectSame("emptied");
    cursor = QTextCursor(document->lastBlock());
    cursor.movePosition(QTextCursor::PreviousBlock);
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.deleteChar();
    expectSame("joined at the end");
    document->undo();
    document->undo();
    expectSame("undone");
}

TEST_CASE("Margin numbers: on the margin of the lines, every few, from 1 on every page",
          "[editor][marginNumbers][pagemode]") {
    auto editor = editorWith(ViewMode::Continuous,
                             QStringLiteral("<h1>Title</h1>") + paragraphs(60));

    // None by default
    editor->setCursorPosition({0, 0});
    CHECK(numberInk(*editor) == 0);
    editor->setCursorPosition({1, 0});
    CHECK(numberInk(*editor) == 0);

    // Lines: also the heading's
    number(*editor, MarginNumbering::Lines);
    editor->setCursorPosition({0, 0});
    CHECK(numberInk(*editor) > 0);
    editor->setCursorPosition({1, 0});
    CHECK(numberInk(*editor) > 0);

    // Paragraphs: those with text only
    number(*editor, MarginNumbering::Paragraphs);
    editor->setCursorPosition({0, 0});
    CHECK(numberInk(*editor) == 0);
    editor->setCursorPosition({1, 0});
    CHECK(numberInk(*editor) > 0);

    // Every fifth line: line 5 has its number, line 1 has none
    number(*editor, MarginNumbering::Lines, 5);
    editor->setCursorPosition({0, 0});
    CHECK(numberInk(*editor) == 0);
    // Line 5 of the chapter (the heading is its line 1) has its number, line 4 none
    editor->setCursorPosition(atLine(*editor, 5));
    CHECK(numberInk(*editor) > 0);
    editor->setCursorPosition(atLine(*editor, 4));
    CHECK(numberInk(*editor) == 0);

    // On every page from 1 in the page view: the first line of page 2 has as narrow a number
    // as that of page 1; counted on, two digits
    editor->setViewMode(ViewMode::Page);
    paint(*editor);
    REQUIRE(editor->totalPages() >= 2);
    const auto firstLineInk = [&editor](int page) {
        editor->goToPage(page);
        paint(*editor);
        REQUIRE(editor->currentPage() == page);
        return numberInk(*editor);
    };
    number(*editor, MarginNumbering::Lines, 1, false);
    const int pageOne = firstLineInk(1);
    const int counted = firstLineInk(2);
    const CursorPosition pageTwo = editor->cursorPosition();
    number(*editor, MarginNumbering::Lines, 1, true);
    const int restarted = firstLineInk(2);
    REQUIRE(pageOne > 0);
    CHECK(counted > pageOne * 3 / 2);
    CHECK(restarted == Approx(pageOne).epsilon(0.25));
    CHECK(firstLineInk(1) == pageOne);

    // The continuous view counts on through the chapter
    editor->setViewMode(ViewMode::Continuous);
    editor->setCursorPosition(pageTwo);
    CHECK(numberInk(*editor) > pageOne * 3 / 2);
}
