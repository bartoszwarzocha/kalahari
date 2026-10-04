/// @file test_editor_stage3.cpp
/// @brief Editor Stage 3 (variant A): drag and drop of text
///
/// The drop itself is checked through BookEditor::dropMimeData(), which dropEvent() calls:
/// QDropEvent::source() is set only during a real drag, which tests cannot run. The mouse
/// and drag events are sent to the editor as the window system delivers them.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include "editor_test_utils.h"

#include <QAbstractTextDocumentLayout>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QImage>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>
#include <cmath>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

const QString kSource = QStringLiteral(
    "<kml><p>Plain <b>bold</b> text.</p><p align=\"right\"><i>Right</i> side</p>"
    "<p>Last</p></kml>");

/// Editor holding @p kml, sized like a small window
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(kml);
    return editor;
}

/// Plain text as another program drags it
std::unique_ptr<QMimeData> plainText(const QString& text) {
    auto mimeData = std::make_unique<QMimeData>();
    mimeData->setText(text);
    return mimeData;
}

/// Point in the middle of the line at @p position, @p dx pixels right of the caret there
QPointF pointAt(BookEditor& editor, const CursorPosition& position, qreal dx = 0.0) {
    editor.setCursorPosition(position);
    const QRectF caret = editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    return {caret.left() + dx, caret.center().y()};
}

void sendMouse(BookEditor& editor, QEvent::Type type, const QPointF& pos, Qt::MouseButton button,
               Qt::MouseButtons buttons) {
    QMouseEvent event(type, pos, editor.mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &event);
}

/// Image of the editor with one pixel per logical pixel (see test_editor_stage2.cpp)
QImage editorImage(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// Bounding box of the pixels that differ between two images of the same size
QRect differenceBox(const QImage& a, const QImage& b) {
    QRect box;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                box |= QRect(x, y, 1, 1);
            }
        }
    }
    return box;
}

}  // anonymous namespace

// =============================================================================
// Dropping text
// =============================================================================

TEST_CASE("Stage3 drag: moving the selection is one undo step", "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);
    const QString original = editor->toKml();

    editor->setSelection({{0, 6}, {0, 11}});  // "bold "
    const auto dragged = editor->createMimeDataFromSelection();
    REQUIRE(editor->dropMimeData(dragged.get(), {2, 4}, true));

    const QString moved = editor->toKml();
    CHECK(moved == QStringLiteral("<kml><p>Plain text.</p><p align=\"right\"><i>Right</i> side</p>"
                                  "<p>Last<b>bold</b> </p></kml>"));
    CHECK(editor->selection().normalized().start == CursorPosition{2, 4});
    CHECK(editor->selection().normalized().end == CursorPosition{2, 9});
    CHECK(editor->cursorPosition() == CursorPosition{2, 9});

    editor->undo();
    CHECK(editor->toKml() == original);
    CHECK_FALSE(editor->canUndo());

    editor->redo();
    CHECK(editor->toKml() == moved);
}

TEST_CASE("Stage3 drag: text moves backward and forward across paragraphs",
          "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);

    SECTION("a word to the start of the document") {
        editor->setSelection({{1, 0}, {1, 6}});  // "Right "
        const auto dragged = editor->createMimeDataFromSelection();
        REQUIRE(editor->dropMimeData(dragged.get(), {0, 0}, true));
        CHECK(editor->toKml() ==
              QStringLiteral("<kml><p><i>Right</i> Plain <b>bold</b> text.</p>"
                             "<p align=\"right\">side</p><p>Last</p></kml>"));
        CHECK(editor->selection().normalized().start == CursorPosition{0, 0});
        CHECK(editor->selection().normalized().end == CursorPosition{0, 6});
    }

    SECTION("a range across a paragraph break into the last paragraph") {
        editor->setSelection({{0, 11}, {1, 5}});  // "text.¶Right"
        const auto dragged = editor->createMimeDataFromSelection();
        REQUIRE(editor->dropMimeData(dragged.get(), {2, 4}, true));
        CHECK(editor->toKml() ==
              QStringLiteral("<kml><p>Plain <b>bold</b>  side</p>"
                             "<p>Lasttext.</p><p align=\"right\"><i>Right</i></p></kml>"));
        CHECK(editor->selection().normalized().start == CursorPosition{1, 4});
        CHECK(editor->selection().normalized().end == CursorPosition{2, 5});

        editor->undo();
        CHECK(editor->toKml() == kSource);
    }
}

TEST_CASE("Stage3 drag: copying leaves the selected text in place", "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);
    editor->setSelection({{0, 6}, {0, 10}});  // "bold"
    const auto dragged = editor->createMimeDataFromSelection();

    REQUIRE(editor->dropMimeData(dragged.get(), {2, 0}, false));
    CHECK(editor->toKml() == QStringLiteral("<kml><p>Plain <b>bold</b> text.</p>"
                                            "<p align=\"right\"><i>Right</i> side</p>"
                                            "<p><b>bold</b>Last</p></kml>"));
    CHECK(editor->selection().normalized().start == CursorPosition{2, 0});
    CHECK(editor->selection().normalized().end == CursorPosition{2, 4});

    editor->undo();
    CHECK(editor->toKml() == kSource);
}

TEST_CASE("Stage3 drag: the selection is not moved onto itself", "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);
    const SelectionRange selection{{0, 6}, {0, 10}};
    editor->setSelection(selection);
    const auto dragged = editor->createMimeDataFromSelection();

    const CursorPosition position = GENERATE(CursorPosition{0, 6}, CursorPosition{0, 8},
                                             CursorPosition{0, 10});
    CHECK_FALSE(editor->dropMimeData(dragged.get(), position, true));
    CHECK(editor->toKml() == kSource);
    CHECK_FALSE(editor->canUndo());
    CHECK(editor->selection().normalized().start == selection.start);
    CHECK(editor->selection().normalized().end == selection.end);
}

TEST_CASE("Stage3 drag: text from another program takes the format of the drop point",
          "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);
    const auto text = plainText(QStringLiteral("new\nwords"));

    REQUIRE(editor->dropMimeData(text.get(), {2, 4}, false));
    CHECK(editor->toKml() == QStringLiteral("<kml><p>Plain <b>bold</b> text.</p>"
                                            "<p align=\"right\"><i>Right</i> side</p>"
                                            "<p>Lastnew</p><p>words</p></kml>"));
    CHECK(editor->selection().normalized().start == CursorPosition{2, 4});
    CHECK(editor->selection().normalized().end == CursorPosition{3, 5});

    SECTION("nothing to insert, nothing dropped") {
        auto empty = std::make_unique<QMimeData>();
        empty->setHtml(QStringLiteral("<b>html only</b>"));
        CHECK_FALSE(editor->dropMimeData(empty.get(), {0, 0}, false));
        CHECK_FALSE(editor->dropMimeData(nullptr, {0, 0}, false));
    }
}

// =============================================================================
// Drag events
// =============================================================================

TEST_CASE("Stage3 drag: dragging over the editor shows where the text lands",
          "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);
    const QPointF target = pointAt(*editor, {2, 2}, 1.0);  // between "La" and "st"
    editor->setCursorPosition({0, 0});
    const QImage before = editorImage(*editor);
    const auto text = plainText(QStringLiteral("XY"));

    QDragEnterEvent enter(target.toPoint(), Qt::CopyAction | Qt::MoveAction, text.get(),
                          Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(editor.get(), &enter);
    CHECK(enter.isAccepted());

    QDragMoveEvent move(target.toPoint(), Qt::CopyAction | Qt::MoveAction, text.get(),
                        Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(editor.get(), &move);
    CHECK(move.isAccepted());

    // A thin caret line at the drop point
    const QRect caret = differenceBox(before, editorImage(*editor));
    REQUIRE_FALSE(caret.isEmpty());
    CHECK(caret.width() <= 3);
    CHECK(std::abs(caret.center().x() - target.x()) <= 6);
    CHECK(caret.contains(QPoint(caret.center().x(), static_cast<int>(target.y()))));

    SECTION("the caret goes when the drag leaves") {
        QDragLeaveEvent leave;
        QCoreApplication::sendEvent(editor.get(), &leave);
        CHECK(differenceBox(before, editorImage(*editor)).isEmpty());
    }

    SECTION("dropping inserts the text there and selects it") {
        QDropEvent drop(target, Qt::CopyAction | Qt::MoveAction, text.get(), Qt::LeftButton,
                        Qt::NoModifier);
        QCoreApplication::sendEvent(editor.get(), &drop);
        CHECK(drop.isAccepted());
        CHECK(editor->toKml().contains(QStringLiteral("<p>LaXYst</p>")));
        CHECK(editor->selection().normalized().start == CursorPosition{2, 2});
        CHECK(editor->selection().normalized().end == CursorPosition{2, 4});
    }
}

TEST_CASE("Stage3 drag: content the editor cannot insert is refused", "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);
    QMimeData image;
    image.setImageData(QImage(4, 4, QImage::Format_RGB32));

    QDragEnterEvent enter(QPoint(20, 10), Qt::CopyAction, &image, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(editor.get(), &enter);
    CHECK_FALSE(enter.isAccepted());
}

// =============================================================================
// Mouse
// =============================================================================

TEST_CASE("Stage3 drag: a click on the selected text places the cursor there",
          "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);
    const QPointF onBold = pointAt(*editor, {0, 8}, 1.0);  // on the second "l" of "bold"
    editor->setSelection({{0, 6}, {0, 10}});

    sendMouse(*editor, QEvent::MouseButtonPress, onBold, Qt::LeftButton, Qt::LeftButton);
    CHECK(editor->hasSelection());  // a press may start dragging the selection
    sendMouse(*editor, QEvent::MouseButtonRelease, onBold, Qt::LeftButton, Qt::NoButton);

    CHECK_FALSE(editor->hasSelection());
    CHECK(editor->cursorPosition() == CursorPosition{0, 8});
}

TEST_CASE("Stage3 drag: a press next to the selected text starts a new selection",
          "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);
    const QPointF onText = pointAt(*editor, {0, 13}, 1.0);  // on "text."
    editor->setSelection({{0, 6}, {0, 10}});

    sendMouse(*editor, QEvent::MouseButtonPress, onText, Qt::LeftButton, Qt::LeftButton);
    CHECK_FALSE(editor->hasSelection());
    CHECK(editor->cursorPosition() == CursorPosition{0, 13});
    sendMouse(*editor, QEvent::MouseButtonRelease, onText, Qt::LeftButton, Qt::NoButton);
}

TEST_CASE("Stage3 drag: the pointer is an arrow over the selected text, an I-beam elsewhere",
          "[editor][stage3][drag]") {
    auto editor = editorWith(kSource);
    const QPointF onBold = pointAt(*editor, {0, 8}, 1.0);
    const QPointF onText = pointAt(*editor, {0, 13}, 1.0);
    const QPointF afterLine = pointAt(*editor, {0, 16}, 40.0);  // right of "text."

    CHECK(editor->cursor().shape() == Qt::IBeamCursor);
    editor->setSelection({{0, 6}, {0, 16}});

    sendMouse(*editor, QEvent::MouseMove, onBold, Qt::NoButton, Qt::NoButton);
    CHECK(editor->cursor().shape() == Qt::ArrowCursor);
    sendMouse(*editor, QEvent::MouseMove, afterLine, Qt::NoButton, Qt::NoButton);
    CHECK(editor->cursor().shape() == Qt::IBeamCursor);
    sendMouse(*editor, QEvent::MouseMove, onText, Qt::NoButton, Qt::NoButton);
    CHECK(editor->cursor().shape() == Qt::ArrowCursor);
    editor->clearSelection();
    sendMouse(*editor, QEvent::MouseMove, onText, Qt::NoButton, Qt::NoButton);
    CHECK(editor->cursor().shape() == Qt::IBeamCursor);
}

TEST_CASE("Stage3 drag: selecting past the bottom edge scrolls the view",
          "[editor][stage3][drag]") {
    QStringList paragraphs;
    for (int i = 0; i < 80; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 of a document longer than the view.").arg(i);
    }
    auto editor = editorWith(kmlOf(paragraphs));
    const QPointF start = pointAt(*editor, {0, 2}, 1.0);
    editor->setCursorPosition({0, 0});
    REQUIRE(editor->scrollOffset() == 0.0);

    sendMouse(*editor, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    sendMouse(*editor, QEvent::MouseMove, QPointF(start.x(), editor->height() + 40.0),
              Qt::NoButton, Qt::LeftButton);
    const int endBefore = editor->selection().normalized().end.paragraph;

    runEventLoop(250);
    const qreal scrolled = editor->scrollOffset();
    CHECK(scrolled > 0.0);
    CHECK(editor->selection().normalized().start == CursorPosition{0, 2});
    CHECK(editor->selection().normalized().end.paragraph > endBefore);

    SECTION("releasing the button stops scrolling") {
        sendMouse(*editor, QEvent::MouseButtonRelease, QPointF(start.x(), editor->height() + 40.0),
                  Qt::LeftButton, Qt::NoButton);
        const qreal released = editor->scrollOffset();
        runEventLoop(100);
        CHECK(editor->scrollOffset() == released);
    }

    SECTION("moving back into the view stops scrolling") {
        sendMouse(*editor, QEvent::MouseMove, QPointF(start.x(), editor->height() / 2.0),
                  Qt::NoButton, Qt::LeftButton);
        const qreal inside = editor->scrollOffset();
        runEventLoop(100);
        CHECK(editor->scrollOffset() == inside);
        sendMouse(*editor, QEvent::MouseButtonRelease, QPointF(start.x(), editor->height() / 2.0),
                  Qt::LeftButton, Qt::NoButton);
    }
}

// =============================================================================
// Hit test
// =============================================================================

TEST_CASE("Stage3 hit test: an exact hit is on the text of a line", "[editor][stage3][drag]") {
    auto editor = editorWith(QStringLiteral("<kml><p>Short</p><p>Second line</p></kml>"));
    QAbstractTextDocumentLayout* layout = editor->textDocument()->documentLayout();
    const QTextBlock first = editor->textDocument()->firstBlock();
    const QTextLine line = first.layout()->lineAt(0);
    const qreal y = first.layout()->position().y() + line.y() + line.height() / 2.0;
    const qreal endX = line.cursorToX(5);

    CHECK(layout->hitTest(QPointF(line.cursorToX(1) + 1.0, y), Qt::ExactHit) == 1);
    CHECK(layout->hitTest(QPointF(endX + 20.0, y), Qt::ExactHit) == -1);
    CHECK(layout->hitTest(QPointF(endX + 20.0, y), Qt::FuzzyHit) == 5);
    CHECK(layout->hitTest(QPointF(10.0, -5.0), Qt::ExactHit) == -1);
}
