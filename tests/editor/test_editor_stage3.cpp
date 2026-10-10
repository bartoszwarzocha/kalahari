/// @file test_editor_stage3.cpp
/// @brief Editor Stage 3 (variant A): drag and drop of text, find and replace, cursor
///
/// The drop itself is checked through BookEditor::dropMimeData(), which dropEvent() calls:
/// QDropEvent::source() is set only during a real drag, which tests cannot run. The mouse
/// and drag events are sent to the editor as the window system delivers them.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/render_context.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/find_replace_bar.h>
#include "editor_test_utils.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QToolButton>
#include <QTextBlock>
#include <QTextLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

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

// =============================================================================
// Find and replace
// =============================================================================

namespace {

const QString kWords = QStringLiteral(
    "<kml><p>One word, then another word.</p><p>No match here.</p><p>The last word.</p></kml>");

FindReplaceBar* findBar(BookEditor& editor) {
    return editor.findChild<FindReplaceBar*>();
}

bool isShown(QWidget* widget) {
    return widget != nullptr && !widget->isHidden();
}

}  // anonymous namespace

TEST_CASE("Stage3 find: Find opens the bar with the selected text and finds it",
          "[editor][stage3][search]") {
    auto editor = editorWith(kWords);
    editor->setSelection({{0, 4}, {0, 8}});  // "word"
    editor->showFind();

    auto* bar = findBar(*editor);
    REQUIRE(isShown(bar));
    CHECK_FALSE(bar->isReplaceMode());
    CHECK(bar->searchText() == QStringLiteral("word"));
    CHECK(editor->searchEngine()->totalMatchCount() == 3);
    CHECK(editor->searchEngine()->currentMatchIndex() == 0);  // the selected one

    SECTION("Find Next goes on from the selected match, then from the top") {
        editor->findNext();
        CHECK(editor->selection().normalized().start == CursorPosition{0, 23});
        editor->findNext();
        CHECK(editor->selection().normalized().start == CursorPosition{2, 9});
        CHECK(editor->selection().normalized().end == CursorPosition{2, 13});
        editor->findNext();
        CHECK(editor->selection().normalized().start == CursorPosition{0, 4});
        editor->findPrevious();
        CHECK(editor->selection().normalized().start == CursorPosition{2, 9});
    }

    SECTION("a selection across paragraphs leaves the search text as it was") {
        editor->setSelection({{0, 23}, {1, 2}});  // "word.", the paragraph end, "No"
        editor->showFind();
        CHECK(bar->searchText() == QStringLiteral("word"));
        editor->showFindReplace();
        CHECK(bar->searchText() == QStringLiteral("word"));
        CHECK(editor->searchEngine()->totalMatchCount() == 3);
    }

    SECTION("the bar opened again searches for its text again") {
        // Closing the bar clears the search; its field keeps the text
        editor->hideFindReplace();
        CHECK_FALSE(editor->searchEngine()->isActive());
        editor->clearSelection();

        editor->showFind();
        CHECK(bar->searchText() == QStringLiteral("word"));
        CHECK(editor->searchEngine()->totalMatchCount() == 3);
        editor->findNext();
        CHECK(editor->selectedText() == QStringLiteral("word"));
    }

    SECTION("Find & Replace shows the replace row, Replace All is one undo step") {
        editor->showFindReplace();
        CHECK(bar->isReplaceMode());
        const QList<QLineEdit*> fields = bar->findChildren<QLineEdit*>();
        REQUIRE(fields.size() == 2);
        fields[1]->setText(QStringLiteral("term"));
        QPushButton* replaceAll = nullptr;
        for (QPushButton* button : bar->findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Replace All")) replaceAll = button;
        }
        REQUIRE(replaceAll != nullptr);

        replaceAll->click();
        CHECK(editor->plainText() ==
              QStringLiteral("One term, then another term.\nNo match here.\nThe last term."));
        editor->undo();
        CHECK(editor->toKml() == kWords);
    }
}

TEST_CASE("Stage3 find: Find Next without a search term opens the bar",
          "[editor][stage3][search]") {
    auto editor = editorWith(kWords);
    editor->findNext();
    CHECK(isShown(findBar(*editor)));
    CHECK_FALSE(editor->hasSelection());
}

TEST_CASE("Stage3 find: Find Next and Find Previous go on from the cursor",
          "[editor][stage3][search]") {
    auto editor = editorWith(kWords);
    editor->showFind();
    auto* bar = findBar(*editor);
    bar->setSearchText(QStringLiteral("word"));

    // A click in the second paragraph: the next match is in the third one, the previous
    // one at the end of the first (regression: always the first or the last match)
    editor->clearSelection();
    editor->setCursorPosition({1, 3});
    editor->findNext();
    CHECK(editor->selection().normalized().start == CursorPosition{2, 9});

    editor->clearSelection();
    editor->setCursorPosition({1, 3});
    editor->findPrevious();
    CHECK(editor->selection().normalized().start == CursorPosition{0, 23});

    SECTION("the bar counts from the selected match") {
        editor->findNext();
        CHECK(editor->searchEngine()->currentMatchIndex() == 2);
        // Moving away from it: no match is the current one any more
        editor->clearSelection();
        CHECK(editor->searchEngine()->currentMatchIndex() == -1);
    }
}

TEST_CASE("Stage3 find: keys typed in the find bar do not reach the text",
          "[editor][stage3][search]") {
    // Regression: the fields leave Enter unused, so it went on to the editor under the bar,
    // which replaced the match just found with a new paragraph
    auto editor = editorWith(kWords);
    editor->showFindReplace();
    auto* bar = findBar(*editor);
    bar->setSearchText(QStringLiteral("word"));
    QLineEdit* find = nullptr;
    QLineEdit* replace = nullptr;
    for (QLineEdit* input : bar->findChildren<QLineEdit*>()) {
        if (input->placeholderText() == QStringLiteral("Find...")) {
            find = input;
        } else {
            replace = input;
        }
    }
    REQUIRE(find != nullptr);
    REQUIRE(replace != nullptr);
    const auto press = [](QWidget* widget, int key, Qt::KeyboardModifiers modifiers = {}) {
        QKeyEvent event(QEvent::KeyPress, key, modifiers);
        QCoreApplication::sendEvent(widget, &event);
    };

    press(find, Qt::Key_Return);
    CHECK(editor->selection().normalized().start == CursorPosition{0, 4});
    press(find, Qt::Key_Enter);  // on the keypad
    CHECK(editor->selection().normalized().start == CursorPosition{0, 23});
    press(find, Qt::Key_Return, Qt::ShiftModifier);  // backwards
    CHECK(editor->selection().normalized().start == CursorPosition{0, 4});
    CHECK(editor->toKml() == kWords);

    // Enter in the replace field replaces the selected match and selects the next one
    replace->setText(QStringLiteral("term"));
    press(replace, Qt::Key_Return);
    CHECK(editor->plainText() ==
          QStringLiteral("One term, then another word.\nNo match here.\nThe last word."));
    CHECK(editor->selection().normalized().start == CursorPosition{0, 23});

    // A key the field leaves unused does not move the editor's cursor
    press(find, Qt::Key_Down);
    CHECK(editor->selection().normalized().start == CursorPosition{0, 23});
}

TEST_CASE("Stage3 find: after an edit, Find Next goes on from the cursor",
          "[editor][stage3][search]") {
    auto editor = editorWith(kWords);
    editor->showFind();
    findBar(*editor)->setSearchText(QStringLiteral("word"));
    editor->findNext();
    editor->findNext();
    REQUIRE(editor->selection().normalized().start == CursorPosition{0, 23});

    // The found word corrected (regression: an edit sent Find Next back to the first match)
    editor->insertText(QStringLiteral("text"));
    CHECK(editor->plainText().startsWith(QStringLiteral("One word, then another text.")));
    CHECK(editor->searchEngine()->totalMatchCount() == 2);
    editor->findNext();
    CHECK(editor->selection().normalized().start == CursorPosition{2, 9});
}

TEST_CASE("Stage3 find: the matches follow typing in a long chapter",
          "[editor][stage3][search]") {
    QStringList paragraphs;
    for (int i = 0; i < 300; ++i) {
        paragraphs << QStringLiteral("Paragraph %1: a word, another word and one more word.").arg(i);
    }
    auto editor = editorWith(kmlOf(paragraphs));
    editor->showFind();
    findBar(*editor)->setSearchText(QStringLiteral("word"));
    REQUIRE(editor->searchEngine()->totalMatchCount() == 900);

    // The matches are updated as the text changes, from the edited paragraph only, instead
    // of searching the whole chapter again on the next paint
    int updates = 0;
    QObject::connect(editor->searchEngine(), &SearchEngine::matchesChanged,
                     [&updates]() { ++updates; });
    editor->clearSelection();
    editor->setCursorPosition({150, 0});
    editor->insertText(QStringLiteral("word "));
    CHECK(updates > 0);
    editor->insertNewline();
    editor->setCursorPosition({0, 0});
    editor->deleteForward();

    // The same matches as a search from scratch
    const std::vector<SearchMatch> followed = editor->searchEngine()->matches();
    SearchEngine fresh;
    fresh.setDocument(editor->searchEngine()->document());
    fresh.setSearchText(QStringLiteral("word"));
    const std::vector<SearchMatch>& expected = fresh.matches();
    REQUIRE(followed.size() == 901);
    REQUIRE(followed.size() == expected.size());
    for (size_t i = 0; i < followed.size(); ++i) {
        INFO("match " << i);
        CHECK(followed[i].start == expected[i].start);
        CHECK(followed[i].paragraph == expected[i].paragraph);
        CHECK(followed[i].paragraphOffset == expected[i].paragraphOffset);
    }
}

TEST_CASE("Stage3 find: matches are painted under the text", "[editor][stage3][search]") {
    auto editor = editorWith(kWords);
    editor->setCursorPosition({0, 8});
    const QRectF end = editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    editor->setCursorPosition({0, 4});
    const QRectF begin = editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    const QImage plain = editorImage(*editor);

    // Searching without the bar, which would cover the first line
    editor->showFind();
    editor->hideFindReplace();
    editor->searchEngine()->setSearchText(QStringLiteral("word"));
    const QImage found = editorImage(*editor);

    // The first match, without the caret
    const QRect match(QPoint(static_cast<int>(begin.right()) + 1, static_cast<int>(begin.top()) + 1),
                      QPoint(static_cast<int>(end.left()) - 2, static_cast<int>(begin.bottom()) - 2));
    REQUIRE(match.width() > 10);
    const auto mostCommon = [&match](const QImage& image) {
        std::map<QRgb, int> counts;
        for (int y = match.top(); y <= match.bottom(); ++y) {
            for (int x = match.left(); x <= match.right(); ++x) {
                ++counts[image.pixel(x, y)];
            }
        }
        return std::max_element(counts.begin(), counts.end(),
                                [](const auto& a, const auto& b) { return a.second < b.second; })
            ->first;
    };
    const QRgb background = mostCommon(plain);
    const QRgb highlight = mostCommon(found);
    CHECK(highlight != background);

    // The background around the letters is tinted, and the letters stand out from the tint
    // in light and dark color modes alike
    int tinted = 0;
    int letters = 0;
    for (int y = match.top(); y <= match.bottom(); ++y) {
        for (int x = match.left(); x <= match.right(); ++x) {
            const QRgb before = plain.pixel(x, y);
            const QRgb after = found.pixel(x, y);
            if (before == background && after != background) {
                ++tinted;
            }
            if (std::abs(qGray(before) - qGray(background)) > 100 &&
                std::abs(qGray(after) - qGray(highlight)) > 60) {
                ++letters;
            }
        }
    }
    CHECK(tinted > 20);
    CHECK(letters > 5);
}

TEST_CASE("Stage3 find: a match far down a long chapter is shown when found",
          "[editor][stage3][search]") {
    QStringList paragraphs;
    for (int i = 0; i < 400; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 with enough words to wrap onto a second line "
                                     "in a window of this width, so the heights matter.").arg(i);
    }
    paragraphs << QStringLiteral("The needle is here.");
    auto editor = editorWith(kmlOf(paragraphs));

    editor->showFind();
    findBar(*editor)->setSearchText(QStringLiteral("needle"));
    editor->findNext();

    CHECK(editor->selection().normalized().start == CursorPosition{400, 4});
    const QRectF caret = editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    CHECK(caret.top() >= 0.0);
    CHECK(caret.bottom() <= editor->height());
}

TEST_CASE("Stage3 find: a match found above the view is not under the bar",
          "[editor][stage3][search]") {
    // Regression: the line of the match came into view at the top edge, under the bar
    QStringList paragraphs;
    for (int i = 0; i < 200; ++i) {
        paragraphs << (i == 50 ? QStringLiteral("The needle is here.")
                               : QStringLiteral("Paragraph %1 with enough words to wrap onto a "
                                                "second line in a window of this width.").arg(i));
    }
    auto editor = editorWith(kmlOf(paragraphs));
    editor->setCursorPosition({199, 0});
    editor->showFindReplace();
    auto* bar = findBar(*editor);
    bar->setSearchText(QStringLiteral("needle"));

    editor->findPrevious();

    CHECK(editor->selection().normalized().start == CursorPosition{50, 4});
    const QRectF caret = editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    CHECK(caret.top() >= bar->geometry().bottom());
    CHECK(caret.bottom() <= editor->height());
}

TEST_CASE("Stage3 find: in a narrow view the buttons of the bar go on in more rows",
          "[editor][stage3][search]") {
    // Regression: on a small screen with the panels shown the view of the text is narrow,
    // and the bar cut off its buttons at the right edge
    auto editor = editorWith(kWords);
    editor->show();  // the rows of the bar are laid out in a shown bar
    editor->showFindReplace();
    QApplication::processEvents();
    auto* bar = findBar(*editor);
    REQUIRE(isShown(bar));
    const int rowsHeight = bar->height();  // the find row and the replace row
    const auto onOneRow = [bar](const QWidget* left, const QWidget* right) {
        return left->mapTo(bar, left->rect().center()).y() ==
               right->mapTo(bar, right->rect().center()).y();
    };
    const QList<QLineEdit*> fields = bar->findChildren<QLineEdit*>();
    REQUIRE(fields.size() == 2);
    QToolButton* next = nullptr;
    for (QToolButton* button : bar->findChildren<QToolButton*>()) {
        if (button->toolTip().startsWith(QStringLiteral("Next Match"))) {
            next = button;
        }
    }
    REQUIRE(next != nullptr);
    CHECK(onOneRow(fields[0], next));

    resizeWidget(*editor, QSize(300, 400));
    QApplication::processEvents();

    CHECK(bar->width() < 300);
    CHECK(bar->minimumSizeHint().width() <= bar->width());
    CHECK(bar->height() > rowsHeight);
    CHECK(bar->height() == bar->heightForWidth(bar->width()));
    CHECK_FALSE(onOneRow(fields[0], next));
    for (const QAbstractButton* button : bar->findChildren<QAbstractButton*>()) {
        if (button->isVisibleTo(bar)) {
            CAPTURE(button->toolTip().toStdString());
            CHECK(bar->rect().contains(QRect(button->mapTo(bar, QPoint(0, 0)), button->size())));
        }
    }

    SECTION("a wide view has them in one row again") {
        resizeWidget(*editor, QSize(600, 400));
        QApplication::processEvents();
        CHECK(bar->height() == rowsHeight);
        CHECK(onOneRow(fields[0], next));
    }

    SECTION("Find alone takes only the rows of the find row") {
        const int narrowHeight = bar->height();
        editor->showFind();
        QApplication::processEvents();
        CHECK_FALSE(bar->isReplaceMode());
        CHECK(bar->height() < narrowHeight);
        CHECK(bar->height() == bar->heightForWidth(bar->width()));
    }
}

TEST_CASE("Stage3 find: the text starts below the bar, and goes back up when it closes",
          "[editor][stage3][search]") {
    // Regression: in a narrow view (a small screen with the panels shown) the bar takes
    // more rows, and at a small zoom the first lines of the text were under it, showing
    // through its gaps
    auto editor = editorWith(kWords);
    resizeWidget(*editor, QSize(300, 400));
    editor->setZoomFactor(0.5);
    editor->show();
    QApplication::processEvents();
    const auto caretTop = [&editor] {
        return editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF().top();
    };
    const qreal top = caretTop();  // the cursor at the start of the text

    editor->showFindReplace();
    QApplication::processEvents();
    auto* bar = findBar(*editor);
    REQUIRE(isShown(bar));
    REQUIRE(bar->height() > top);  // higher than the room above the text
    CHECK(bar->autoFillBackground());  // the lines scrolled under it do not show through
    CHECK(caretTop() >= bar->geometry().bottom());
    CHECK(caretTop() == Approx(top + bar->height()).margin(0.5));

    SECTION("closed, the bar gives the room back") {
        editor->hideFindReplace();
        CHECK(caretTop() == Approx(top).margin(0.5));
    }
}

TEST_CASE("Stage3 find: with the bar open the whole text comes into view",
          "[editor][stage3][search]") {
    // A narrow view at the smallest zoom: the bar is higher than the room below the text
    QStringList paragraphs;
    for (int i = 0; i < 300; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 with enough words to wrap onto a second "
                                     "line in a window of this width.").arg(i);
    }
    auto editor = editorWith(kmlOf(paragraphs));
    resizeWidget(*editor, QSize(300, 400));
    editor->setZoomFactor(MIN_ZOOM_FACTOR);
    editor->show();
    editor->showFindReplace();
    QApplication::processEvents();
    auto* bar = findBar(*editor);
    REQUIRE(isShown(bar));
    const auto caret = [&editor] {
        return editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    };
    const auto press = [&editor](Qt::Key key) {
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
        QCoreApplication::sendEvent(editor.get(), &event);
    };

    SECTION("the last line, above the bottom edge") {
        editor->setCursorPosition({299, static_cast<int>(paragraphs[299].size())});
        CHECK(caret().top() >= bar->geometry().bottom());
        CHECK(caret().bottom() <= editor->height());
    }

    SECTION("a click beside the scroll bar's handle scrolls by the view below the bar") {
        // No line is skipped: the bar's rows are not a part of the view
        const double height = editor->height();
        const int belowBar = editor->verticalScrollBar()->pageStep();
        editor->hideFindReplace();
        REQUIRE(editor->verticalScrollBar()->pageStep() ==
                static_cast<int>(height / editor->zoomFactor()));
        CHECK(belowBar == static_cast<int>((height - bar->height()) / editor->zoomFactor()));
    }

    SECTION("Page Down and Page Up keep the cursor's line below the bar") {
        for (const Qt::Key key : {Qt::Key_PageDown, Qt::Key_PageDown, Qt::Key_PageDown,
                                  Qt::Key_PageUp, Qt::Key_PageUp, Qt::Key_PageUp}) {
            press(key);
            CAPTURE(editor->cursorPosition().paragraph);
            CHECK(caret().top() >= bar->geometry().bottom());
            CHECK(caret().bottom() <= editor->height());
        }
        CHECK(editor->cursorPosition() == CursorPosition{0, 0});
    }
}

// =============================================================================
// Paragraph alignment
// =============================================================================

TEST_CASE("Stage3 alignment: aligning selected paragraphs is one undo step",
          "[editor][stage3][format]") {
    // Regression: each paragraph was a separate undo step
    const QString kml = kmlOf({QStringLiteral("One"), QStringLiteral("Two"), QStringLiteral("Three")});
    auto editor = editorWith(kml);
    editor->setSelection({{0, 1}, {2, 2}});
    editor->setAlignCenter();
    for (QTextBlock block = editor->textDocument()->begin(); block.isValid(); block = block.next()) {
        CHECK(block.blockFormat().alignment() == Qt::AlignHCenter);
    }
    editor->undo();
    CHECK(editor->toKml() == kml);
    editor->redo();
    CHECK(editor->textDocument()->lastBlock().blockFormat().alignment() == Qt::AlignHCenter);
    CHECK(editor->textDocument()->firstBlock().blockFormat().alignment() == Qt::AlignHCenter);
}

TEST_CASE("Stage3 alignment: undo and redo keep the cursor on the aligned paragraphs",
          "[editor][stage3][format]") {
    // Regression: QTextDocument put the cursor after the last aligned paragraph, so the
    // toolbar showed the alignment of the next one
    auto editor = editorWith(QStringLiteral(
        "<kml><p>One</p><p>Two</p><p align=\"right\">Three</p><p align=\"center\">Four</p></kml>"));

    SECTION("the cursor in one paragraph") {
        editor->setCursorPosition({1, 2});
        editor->setAlignCenter();
        editor->undo();
        CHECK(editor->cursorPosition() == CursorPosition{1, 2});
        CHECK(editor->currentAlignment() == Qt::AlignJustify);
        editor->redo();
        CHECK(editor->cursorPosition() == CursorPosition{1, 2});
        CHECK(editor->currentAlignment() == Qt::AlignHCenter);
    }

    SECTION("a selection over several paragraphs stays selected") {
        const SelectionRange selected{{0, 1}, {2, 3}};
        editor->setCursorPosition({2, 3});
        editor->setSelection(selected);
        editor->setAlignLeft();
        editor->undo();
        CHECK(editor->selection().start == selected.start);
        CHECK(editor->selection().end == selected.end);
        CHECK(editor->cursorPosition() == CursorPosition{2, 3});
        CHECK(editor->currentAlignment() == Qt::AlignRight);
        editor->redo();
        CHECK(editor->selection().start == selected.start);
        CHECK(editor->selection().end == selected.end);
        CHECK(editor->currentAlignment() == Qt::AlignLeft);
    }

    SECTION("undoing a later edit puts the cursor at that edit") {
        editor->setCursorPosition({1, 2});
        editor->setAlignCenter();
        editor->undo();
        editor->setCursorPosition({3, 4});
        editor->insertText(QStringLiteral("!"));
        editor->undo();
        CHECK(editor->toKml().contains(QStringLiteral("<p>Two</p>")));
        CHECK(editor->cursorPosition() == CursorPosition{3, 4});
        editor->redo();
        CHECK(editor->cursorPosition() == CursorPosition{3, 5});
    }
}

namespace {

/// Right edge of the text on line @p lineIndex of @p block, trailing spaces left out
qreal textRightEdge(const QTextBlock& block, int lineIndex) {
    const QTextLine line = KalahariTextDocumentLayout::blockLayout(block)->lineAt(lineIndex);
    int end = line.textStart() + line.textLength();
    while (end > line.textStart() && block.text().at(end - 1).isSpace()) {
        --end;
    }
    return line.cursorToX(end);
}

}  // anonymous namespace

TEST_CASE("Stage3 alignment: a paragraph without its own alignment is justified",
          "[editor][stage3][format]") {
    const QString words =
        QStringLiteral("The quick brown fox jumps over the lazy dog. ").repeated(8).trimmed();
    auto editor = editorWith(QStringLiteral("<kml><p>%1</p><p align=\"left\">%1</p></kml>").arg(words));
    const QTextBlock plain = editor->textDocument()->firstBlock();
    const QTextBlock left = plain.next();
    const QTextLayout* plainLayout = KalahariTextDocumentLayout::blockLayout(plain);
    const QTextLayout* leftLayout = KalahariTextDocumentLayout::blockLayout(left);
    REQUIRE(plainLayout->lineCount() > 2);
    REQUIRE(leftLayout->lineCount() == plainLayout->lineCount());

    SECTION("its lines reach both edges, the last one stays at the leading edge") {
        CHECK(plainLayout->textOption().alignment() == Qt::AlignJustify);
        for (int i = 0; i + 1 < plainLayout->lineCount(); ++i) {
            const QTextLine line = plainLayout->lineAt(i);
            CHECK(std::abs(textRightEdge(plain, i) - (line.x() + line.width())) < 1.0);
        }
        const int lastIndex = plainLayout->lineCount() - 1;
        const QTextLine last = plainLayout->lineAt(lastIndex);
        CHECK(std::abs(textRightEdge(plain, lastIndex) - (last.x() + last.naturalTextWidth())) < 1.0);
    }

    SECTION("left alignment set on purpose stays") {
        CHECK(leftLayout->textOption().alignment() == Qt::AlignLeft);
        const QTextLine first = leftLayout->lineAt(0);
        CHECK(std::abs(textRightEdge(left, 0) - (first.x() + first.naturalTextWidth())) < 1.0);
    }

    SECTION("Justify is the current alignment, the file keeps no attribute") {
        editor->setCursorPosition({0, 0});
        CHECK(editor->currentAlignment() == Qt::AlignJustify);
        editor->setCursorPosition({1, 0});
        CHECK(editor->currentAlignment() == Qt::AlignLeft);
        CHECK(editor->toKml().startsWith(QStringLiteral("<kml><p>The quick")));
    }

    SECTION("Align Left there is saved, and undone in one step") {
        const QString before = editor->toKml();
        editor->setCursorPosition({0, 3});
        editor->setAlignLeft();
        CHECK(editor->currentAlignment() == Qt::AlignLeft);
        CHECK(editor->toKml().startsWith(QStringLiteral("<kml><p align=\"left\">The quick")));
        editor->undo();
        CHECK(editor->currentAlignment() == Qt::AlignJustify);
        CHECK(editor->toKml() == before);
    }
}

// =============================================================================
// Cursor
// =============================================================================

namespace {

/// Show @p editor and give it focus (offscreen platform only, see test_editor_stage1.cpp)
bool focusEditor(BookEditor& editor) {
    editor.show();
    editor.activateWindow();
    editor.setFocus();
    return waitUntil([&editor] { return editor.hasFocus(); });
}

/// The editor with focus, and the area its cursor is painted in: what differs without focus
struct CursorShot {
    QImage image;
    QRect box;
};

CursorShot cursorShot(BookEditor& editor) {
    editor.setFocus();
    waitUntil([&editor] { return editor.hasFocus(); });
    CursorShot shot;
    shot.image = editorImage(editor);
    editor.clearFocus();
    shot.box = differenceBox(shot.image, editorImage(editor));
    return shot;
}

}  // anonymous namespace

TEST_CASE("Stage3 cursor: the cursor shows at once when the view scrolls",
          "[editor][stage3][cursor]") {
    // Regression: when the view scrolled during the hidden phase of a blink, the cursor
    // stayed hidden for up to one blink interval where the view stopped
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs window focus without a window on screen: run with QT_QPA_PLATFORM=offscreen");
    }
    QStringList paragraphs;
    for (int i = 0; i < 60; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 of a chapter long enough to scroll.").arg(i);
    }
    auto editor = editorWith(kmlOf(paragraphs));
    editor->setCursorBlinkInterval(100);
    if (!focusEditor(*editor)) {
        SKIP("the platform did not give the editor focus");
    }
    REQUIRE(waitUntil([&editor] { return !editor->isCursorVisible(); }));

    SECTION("with the mouse wheel") {
        const QPointF pos(100, 100);
        QWheelEvent wheel(pos, editor->mapToGlobal(pos), QPoint(), QPoint(0, -120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(editor.get(), &wheel);
    }
    SECTION("with the scroll bar") {
        auto* scrollBar = editor->findChild<QScrollBar*>();
        REQUIRE(scrollBar != nullptr);
        scrollBar->setValue(scrollBar->value() + 60);
    }
    CHECK(editor->scrollOffset() > 0.0);
    CHECK(editor->isCursorVisible());
}

TEST_CASE("Stage3 cursor: the cursor shape and width follow the settings and the text",
          "[editor][stage3][cursor]") {
    // Regression: the cursor shape and width chosen in the settings were ignored, and the
    // block and underline sizes came from the unzoomed font, not the character's format
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs window focus without a window on screen: run with QT_QPA_PLATFORM=offscreen");
    }
    auto editor = editorWith(QStringLiteral("<kml><p>Wide iii <b>WWW</b></p></kml>"));
    editor->setCursorBlinkingEnabled(false);
    if (!focusEditor(*editor)) {
        SKIP("the platform did not give the editor focus");
    }
    EditorAppearance appearance = editor->appearance();
    const auto useCursor = [&](CursorStyle style, int lineWidth) {
        appearance.cursor.style = style;
        appearance.cursor.lineWidth = lineWidth;
        editor->setAppearance(appearance);
    };
    const auto boxAt = [&editor](const CursorPosition& position) {
        editor->setCursorPosition(position);
        return cursorShot(*editor).box;
    };

    useCursor(CursorStyle::Line, 2);
    const QRect line = boxAt({0, 0});
    REQUIRE_FALSE(line.isEmpty());

    SECTION("a block covers the character, which stays readable") {
        useCursor(CursorStyle::Block, 2);
        const int wide = boxAt({0, 0}).width();   // W
        const int thin = boxAt({0, 5}).width();   // i
        const int bold = boxAt({0, 9}).width();   // bold W
        CHECK(wide > 2 * thin);
        CHECK(bold >= wide);

        editor->setCursorPosition({0, 0});
        const CursorShot shot = cursorShot(*editor);
        CHECK(shot.box.height() == line.height());
        const QRgb block = shot.image.pixel(shot.box.left(), shot.box.top());
        int letter = 0;
        for (int y = shot.box.top(); y <= shot.box.bottom(); ++y) {
            for (int x = shot.box.left(); x <= shot.box.right(); ++x) {
                if (std::abs(qGray(shot.image.pixel(x, y)) - qGray(block)) > 100) {
                    ++letter;
                }
            }
        }
        CHECK(letter > 10);

        // The zoom keeps the middle of the view; moving the cursor brings it into view
        editor->setZoomFactor(2.0);
        editor->setCursorPosition({0, 1});
        CHECK(std::abs(boxAt({0, 0}).width() - 2 * wide) <= 3);
    }

    SECTION("an underline is as wide as the character") {
        useCursor(CursorStyle::Block, 2);
        const int wide = boxAt({0, 0}).width();
        useCursor(CursorStyle::Underline, 2);
        const QRect underline = boxAt({0, 0});
        CHECK(underline.height() <= 3);
        CHECK(underline.bottom() == line.bottom());
        CHECK(std::abs(underline.width() - wide) <= 1);
    }

    SECTION("a line has the width from the settings, at every zoom") {
        useCursor(CursorStyle::Line, 4);
        const int width = boxAt({0, 0}).width();
        CHECK(width >= 4);
        CHECK(width <= 5);
        editor->setZoomFactor(2.0);
        editor->setCursorPosition({0, 1});
        CHECK(boxAt({0, 0}).width() == width);
    }
}
