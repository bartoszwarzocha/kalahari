/// @file test_editor_stage0_layout.cpp
/// @brief Stage 0 safety net: paragraph geometry of the edit-mode layout stack
///
/// Covers KalahariTextDocumentLayout (block heights / positions), QTextDocumentSource
/// (what the render pipeline reads) and ViewportManager (scrolling geometry), after:
/// load, a multi-paragraph paste, undo of a large deletion, and a width change.
///
/// The oracle is always a *reference document*: the same text laid out from scratch
/// with layoutAllBlocks(). Whatever an incremental update produces must match it.
///
/// Tests that expose an existing defect are tagged [known-bug] and [!mayfail]: they are
/// documented, not fixed, in Stage 0.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_source_adapter.h>
#include <kalahari/editor/viewport_manager.h>
#include <QCoreApplication>
#include <QResizeEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <cmath>
#include <memory>
#include <vector>

using namespace kalahari::editor;
using Catch::Approx;

namespace {

constexpr qreal kNarrowWidth = 240.0;
constexpr qreal kWideWidth = 520.0;

/// A paragraph long enough to wrap onto several lines at kNarrowWidth.
QString longParagraph(int index) {
    return QStringLiteral("Paragraph %1 has enough words to wrap onto several lines "
                          "when the layout is narrow, which makes its height differ "
                          "clearly from a single unlaid line of text.").arg(index);
}

QFont testFont() {
    QFont font(QStringLiteral("Arial"));
    font.setPointSizeF(12.0);
    return font;
}

/// A QTextDocument wired exactly like BookEditor::ensureEditMode() wires it.
struct LaidOutDocument {
    std::unique_ptr<QTextDocument> doc = std::make_unique<QTextDocument>();
    KalahariTextDocumentLayout* layout = nullptr;

    explicit LaidOutDocument(qreal width) {
        layout = new KalahariTextDocumentLayout(doc.get());
        layout->setFont(testFont());
        layout->setTextWidth(width);
        doc->setDocumentLayout(layout);
        doc->setDefaultFont(testFont());
        doc->setDocumentMargin(0);
    }

    /// Replace the content with @p paragraphs and lay everything out (initial load).
    void load(const QStringList& paragraphs) {
        QTextCursor cursor(doc.get());
        cursor.beginEditBlock();
        cursor.select(QTextCursor::Document);
        cursor.removeSelectedText();
        for (int i = 0; i < paragraphs.size(); ++i) {
            if (i > 0) cursor.insertBlock();
            cursor.insertText(paragraphs[i]);
        }
        cursor.endEditBlock();
        layout->layoutAllBlocks();
        doc->clearUndoRedoStacks();
    }

    QStringList paragraphs() const {
        QStringList out;
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) out << b.text();
        return out;
    }
};

/// Paragraph heights/positions as reported by the layout under test.
struct Geometry {
    std::vector<qreal> y;
    std::vector<qreal> height;
    std::vector<int> lineCount;
};

Geometry geometryOf(const QTextDocument* doc) {
    Geometry g;
    auto* layout = doc->documentLayout();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        const QRectF r = layout->blockBoundingRect(b);
        g.y.push_back(r.y());
        g.height.push_back(r.height());
        g.lineCount.push_back(b.layout() ? b.layout()->lineCount() : 0);
    }
    return g;
}

/// Geometry of the same text laid out from scratch at @p width.
Geometry referenceGeometry(const QStringList& paragraphs, qreal width) {
    LaidOutDocument ref(width);
    ref.load(paragraphs);
    return geometryOf(ref.doc.get());
}

/// Number of blocks whose height/position differs from the reference.
int mismatchCount(const Geometry& actual, const Geometry& expected) {
    if (actual.height.size() != expected.height.size()) return -1;
    int mismatches = 0;
    for (size_t i = 0; i < actual.height.size(); ++i) {
        if (std::abs(actual.height[i] - expected.height[i]) > 0.5 ||
            std::abs(actual.y[i] - expected.y[i]) > 0.5) {
            ++mismatches;
        }
    }
    return mismatches;
}

int unlaidBlockCount(const Geometry& g) {
    int n = 0;
    for (int lines : g.lineCount) {
        if (lines == 0) ++n;
    }
    return n;
}

QStringList longParagraphs(int count, int firstIndex = 0) {
    QStringList list;
    for (int i = 0; i < count; ++i) list << longParagraph(firstIndex + i);
    return list;
}

/// Deliver a resize to a hidden widget (QWidget::resize() alone only queues it).
void resizeWidget(QWidget& widget, const QSize& newSize) {
    const QSize oldSize = widget.size();
    widget.resize(newSize);
    QResizeEvent event(newSize, oldSize);
    QCoreApplication::sendEvent(&widget, &event);
}

}  // anonymous namespace

// =============================================================================
// KalahariTextDocumentLayout - incremental updates vs. a from-scratch layout
// =============================================================================

TEST_CASE("Stage0 layout: initial load lays out every block", "[editor][stage0][layout]") {
    LaidOutDocument d(kNarrowWidth);
    d.load(longParagraphs(12));

    const Geometry g = geometryOf(d.doc.get());
    CHECK(unlaidBlockCount(g) == 0);
    // Long paragraphs at a narrow width must wrap
    for (int lines : g.lineCount) CHECK(lines > 1);
    // Positions are cumulative heights with no gaps
    for (size_t i = 1; i < g.y.size(); ++i) {
        CHECK(g.y[i] == Approx(g.y[i - 1] + g.height[i - 1]));
    }
}

TEST_CASE("Stage0 layout: pasting more than 3 paragraphs at once",
          "[editor][stage0][layout][known-bug][!mayfail]") {
    // KNOWN BUG (suspected): KalahariTextDocumentLayout::documentChanged() lays out only
    // the changed block and the next 2 ("blocksToLayout = 3") for any edit that is not a
    // whole-document change. A paste that creates N > 3 paragraphs leaves N-3 blocks with
    // no lines, so they report a one-line fallback height until something re-lays them out.
    LaidOutDocument d(kNarrowWidth);
    d.load({QStringLiteral("Intro paragraph.")});

    const QStringList pasted = longParagraphs(10, 100);
    QTextCursor cursor(d.doc.get());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("\n") + pasted.join(QLatin1Char('\n')));

    const Geometry actual = geometryOf(d.doc.get());
    const Geometry expected = referenceGeometry(d.paragraphs(), kNarrowWidth);

    REQUIRE(d.doc->blockCount() == 11);
    CHECK(unlaidBlockCount(actual) == 0);
    CHECK(mismatchCount(actual, expected) == 0);
    CHECK(d.layout->documentSize().height() ==
          Approx(expected.y.back() + expected.height.back()));
}

TEST_CASE("Stage0 layout: pasting up to 3 paragraphs stays consistent", "[editor][stage0][layout]") {
    // Control case for the test above: within the 3-block window the update is correct.
    LaidOutDocument d(kNarrowWidth);
    d.load({QStringLiteral("Intro paragraph.")});

    QTextCursor cursor(d.doc.get());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("\n") + longParagraphs(2, 100).join(QLatin1Char('\n')));

    const Geometry actual = geometryOf(d.doc.get());
    CHECK(mismatchCount(actual, referenceGeometry(d.paragraphs(), kNarrowWidth)) == 0);
}

TEST_CASE("Stage0 layout: undo of a large deletion",
          "[editor][stage0][layout][known-bug][!mayfail]") {
    // KNOWN BUG (suspected): same 3-block window as the paste test - undo re-inserts many
    // paragraphs in one change notification.
    LaidOutDocument d(kNarrowWidth);
    const QStringList original = longParagraphs(20);
    d.load(original);

    // Delete paragraphs 2..15 in one operation
    QTextCursor cursor(d.doc.get());
    cursor.setPosition(d.doc->findBlockByNumber(2).position());
    cursor.setPosition(d.doc->findBlockByNumber(16).position(), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    REQUIRE(d.doc->blockCount() == 6);
    CHECK(mismatchCount(geometryOf(d.doc.get()), referenceGeometry(d.paragraphs(), kNarrowWidth)) == 0);

    d.doc->undo();
    REQUIRE(d.paragraphs() == original);

    const Geometry actual = geometryOf(d.doc.get());
    CHECK(unlaidBlockCount(actual) == 0);
    CHECK(mismatchCount(actual, referenceGeometry(original, kNarrowWidth)) == 0);
}

TEST_CASE("Stage0 layout: width change re-lays out every block", "[editor][stage0][layout]") {
    LaidOutDocument d(kNarrowWidth);
    const QStringList paras = longParagraphs(15);
    d.load(paras);
    const Geometry narrow = geometryOf(d.doc.get());

    d.layout->setTextWidth(kWideWidth);
    const Geometry wide = geometryOf(d.doc.get());
    CHECK(mismatchCount(wide, referenceGeometry(paras, kWideWidth)) == 0);
    CHECK(wide.y.back() < narrow.y.back());  // fewer lines -> shorter document

    d.layout->setTextWidth(kNarrowWidth);
    CHECK(mismatchCount(geometryOf(d.doc.get()), narrow) == 0);
}

TEST_CASE("Stage0 layout: QTextDocument::setTextWidth alone does not change the wrap width",
          "[editor][stage0][layout][known-bug][!mayfail]") {
    // KNOWN BUG (suspected): QTextDocumentSource::setTextWidth() - the path the render
    // pipeline uses (applyWidthToSource) - calls only QTextDocument::setTextWidth(). That
    // re-lays out every block, but KalahariTextDocumentLayout wraps at its own m_textWidth,
    // which only KalahariTextDocumentLayout::setTextWidth() changes. So the pipeline's width
    // is ignored for wrapping (it is a full but useless relayout).
    LaidOutDocument d(kNarrowWidth);
    const QStringList paras = longParagraphs(8);
    d.load(paras);

    QTextDocumentSource source(d.doc.get());
    source.setTextWidth(kWideWidth);

    CHECK(mismatchCount(geometryOf(d.doc.get()), referenceGeometry(paras, kWideWidth)) == 0);
}

TEST_CASE("Stage0 layout: QTextDocumentSource geometry matches the layout", "[editor][stage0][layout]") {
    LaidOutDocument d(kNarrowWidth);
    d.load(longParagraphs(10));
    QTextDocumentSource source(d.doc.get());

    const Geometry g = geometryOf(d.doc.get());
    for (size_t i = 0; i < g.y.size(); ++i) {
        CHECK(source.paragraphY(i) == Approx(g.y[i]));
        CHECK(source.paragraphHeight(i) == Approx(g.height[i]));
    }
    CHECK(source.totalHeight() == Approx(g.y.back() + g.height.back()));
}

// =============================================================================
// ViewportManager vs. KalahariTextDocumentLayout
// =============================================================================

TEST_CASE("Stage0 layout: ViewportManager::paragraphY matches blockBoundingRect",
          "[editor][stage0][layout][viewport]") {
    LaidOutDocument d(kNarrowWidth);
    d.load(longParagraphs(25));

    ViewportManager vm;
    vm.setViewportSize(QSize(static_cast<int>(kNarrowWidth), 300));
    vm.setDocument(d.doc.get());

    const Geometry g = geometryOf(d.doc.get());
    for (size_t i = 0; i < g.y.size(); ++i) {
        CHECK(vm.paragraphY(i) == Approx(g.y[i]));
        CHECK(vm.paragraphHeight(i) == Approx(g.height[i]));
    }
    CHECK(vm.totalDocumentHeight() == Approx(d.layout->documentSize().height()));

    SECTION("paragraphAtY agrees with the layout") {
        for (size_t i = 0; i < g.y.size(); ++i) {
            CHECK(vm.paragraphAtY(g.y[i] + g.height[i] / 2.0) == i);
        }
    }

    SECTION("still consistent after a width change") {
        d.layout->setTextWidth(kWideWidth);
        const Geometry wide = geometryOf(d.doc.get());
        for (size_t i = 0; i < wide.y.size(); ++i) {
            CHECK(vm.paragraphY(i) == Approx(wide.y[i]));
        }
    }
}

TEST_CASE("Stage0 layout: ViewportManager matches layout after a multi-paragraph paste",
          "[editor][stage0][layout][viewport]") {
    // Both sides read the same (possibly unlaid) QTextLayouts, so they should agree with
    // each other even where they disagree with the reference - this pins the invariant
    // "one geometry source", independent of the 3-block bug above.
    LaidOutDocument d(kNarrowWidth);
    d.load({QStringLiteral("Intro.")});

    ViewportManager vm;
    vm.setViewportSize(QSize(static_cast<int>(kNarrowWidth), 300));
    vm.setDocument(d.doc.get());

    QTextCursor cursor(d.doc.get());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("\n") + longParagraphs(8).join(QLatin1Char('\n')));

    const Geometry g = geometryOf(d.doc.get());
    for (size_t i = 0; i < g.y.size(); ++i) {
        CHECK(vm.paragraphY(i) == Approx(g.y[i]));
    }
    CHECK(vm.totalDocumentHeight() == Approx(d.layout->documentSize().height()));
}

// =============================================================================
// Through BookEditor (the real wiring)
// =============================================================================

namespace {

QString kmlOf(const QStringList& paragraphs) {
    QString kml = QStringLiteral("<kml>");
    for (const auto& p : paragraphs) kml += QStringLiteral("<p>") + p + QStringLiteral("</p>");
    return kml + QStringLiteral("</kml>");
}

/// Reference geometry for the editor's document: same text, same font, same wrap width.
Geometry editorReference(BookEditor& editor) {
    auto* layout = qobject_cast<KalahariTextDocumentLayout*>(
        editor.textDocument()->documentLayout());
    REQUIRE(layout != nullptr);

    LaidOutDocument ref(layout->textWidth());
    ref.layout->setFont(layout->font());
    ref.doc->setDefaultFont(editor.textDocument()->defaultFont());
    QStringList paras;
    for (QTextBlock b = editor.textDocument()->begin(); b.isValid(); b = b.next()) paras << b.text();
    ref.load(paras);
    return geometryOf(ref.doc.get());
}

}  // anonymous namespace

TEST_CASE("Stage0 layout: BookEditor load produces a complete layout", "[editor][stage0][layout]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf(longParagraphs(30)));

    REQUIRE(editor.textDocument() != nullptr);
    const Geometry g = geometryOf(editor.textDocument());
    CHECK(unlaidBlockCount(g) == 0);
    CHECK(mismatchCount(g, editorReference(editor)) == 0);
}

TEST_CASE("Stage0 layout: BookEditor paste of many paragraphs",
          "[editor][stage0][layout][known-bug][!mayfail]") {
    // KNOWN BUG (suspected): see "pasting more than 3 paragraphs at once".
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf({QStringLiteral("Intro.")}));
    editor.setCursorPosition({0, 6});
    editor.insertText(QStringLiteral("\n") + longParagraphs(10).join(QLatin1Char('\n')));

    REQUIRE(editor.paragraphCount() == 11);
    const Geometry g = geometryOf(editor.textDocument());
    CHECK(unlaidBlockCount(g) == 0);
    CHECK(mismatchCount(g, editorReference(editor)) == 0);
}

TEST_CASE("Stage0 layout: BookEditor undo of a large deletion",
          "[editor][stage0][layout][known-bug][!mayfail]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    editor.fromKml(kmlOf(longParagraphs(20)));
    const QString before = editor.plainText();

    editor.setSelection({{2, 0}, {16, 0}});
    REQUIRE(editor.deleteSelectedText());
    editor.undo();
    REQUIRE(editor.plainText() == before);

    const Geometry g = geometryOf(editor.textDocument());
    CHECK(unlaidBlockCount(g) == 0);
    CHECK(mismatchCount(g, editorReference(editor)) == 0);
}

TEST_CASE("Stage0 layout: BookEditor resize re-wraps to the new width", "[editor][stage0][layout]") {
    BookEditor editor;
    resizeWidget(editor, QSize(500, 400));
    editor.fromKml(kmlOf(longParagraphs(15)));
    const qreal heightBefore = editor.textDocument()->documentLayout()->documentSize().height();

    resizeWidget(editor, QSize(1000, 400));
    const Geometry g = geometryOf(editor.textDocument());
    CHECK(mismatchCount(g, editorReference(editor)) == 0);
    CHECK(editor.textDocument()->documentLayout()->documentSize().height() < heightBefore);
}

TEST_CASE("Stage0 layout: BookEditor resize performs a single full relayout",
          "[editor][stage0][layout][known-bug][!mayfail]") {
    // KNOWN BUG (suspected): BookEditor::resizeEvent() -> updateLayoutWidth() calls
    // QTextDocument::setTextWidth() (full relayout via setPageSize -> documentChanged) and
    // then KalahariTextDocumentLayout::setTextWidth() (second full relayout); the render
    // pipeline's setConfigViewportSize() -> QTextDocumentSource::setTextWidth() can add a
    // third. Each full relayout emits documentSizeChanged exactly once, so the emission
    // count during one resize equals the number of full relayouts.
    BookEditor editor;
    resizeWidget(editor, QSize(500, 400));
    editor.fromKml(kmlOf(longParagraphs(15)));

    int fullRelayouts = 0;
    QObject::connect(editor.textDocument()->documentLayout(),
                     &QAbstractTextDocumentLayout::documentSizeChanged,
                     [&fullRelayouts](const QSizeF&) { ++fullRelayouts; });

    resizeWidget(editor, QSize(900, 400));
    INFO("full relayouts during one resize: " << fullRelayouts);
    CHECK(fullRelayouts == 1);
}
