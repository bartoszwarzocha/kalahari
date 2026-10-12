/// @file test_chapter_cursor.cpp
/// @brief A chapter opens where its cursor was when it was closed last

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/project_database.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_types.h"
#include "kalahari/gui/utils/chapter_cursor.h"

#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariant>

#include <memory>

using Catch::Approx;
using kalahari::core::ProjectDatabase;
using kalahari::editor::BookEditor;
using kalahari::editor::CursorPosition;
using kalahari::gui::utils::rememberChapterCursor;
using kalahari::gui::utils::restoreChapterCursor;
namespace test = kalahari::test;

namespace {

/// The text of a chapter: three paragraphs
QString chapterText() {
    return test::kmlOf({QStringLiteral("The first paragraph."),
                        QStringLiteral("The second paragraph, a longer one."),
                        QStringLiteral("The third paragraph of the chapter.")});
}

/// The editor of a chapter long enough to scroll in a small window
std::unique_ptr<BookEditor> longChapterEditor() {
    QStringList paragraphs;
    for (int i = 0; i < 200; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 of a chapter long enough to scroll.").arg(i);
    }
    auto editor = std::make_unique<BookEditor>();
    test::resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(test::kmlOf(paragraphs));
    return editor;
}

/// Whether the cursor of @p editor is fully in its view
bool cursorInView(BookEditor& editor) {
    const QRectF caret = editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    return caret.top() >= 0.0 && caret.bottom() <= editor.height();
}

}  // anonymous namespace

TEST_CASE("A chapter opens where its cursor was when it was closed last", "[gui][cursor]") {
    QTemporaryDir book;
    REQUIRE(book.isValid());
    ProjectDatabase database;
    REQUIRE(database.open(book.path()));

    // The chapter closed with the cursor in its third paragraph
    {
        BookEditor closed;
        closed.fromKml(chapterText());
        closed.setCursorPosition(CursorPosition{2, 9});
        rememberChapterCursor(database, QStringLiteral("chapter-1"), closed);
    }

    BookEditor opened;
    opened.fromKml(chapterText());

    SECTION("there, when it is opened again") {
        CHECK(restoreChapterCursor(database, QStringLiteral("chapter-1"), opened));
        CHECK(opened.cursorPosition() == CursorPosition{2, 9});
    }

    SECTION("also when the book is opened again, after the program was closed") {
        database.close();
        ProjectDatabase reopened;
        REQUIRE(reopened.open(book.path()));
        CHECK(restoreChapterCursor(reopened, QStringLiteral("chapter-1"), opened));
        CHECK(opened.cursorPosition() == CursorPosition{2, 9});
    }

    SECTION("a chapter never closed before opens at its start") {
        CHECK_FALSE(restoreChapterCursor(database, QStringLiteral("chapter-2"), opened));
        CHECK(opened.cursorPosition() == CursorPosition{0, 0});
    }

    SECTION("each chapter keeps its own place, the last one") {
        BookEditor other;
        other.fromKml(chapterText());
        other.setCursorPosition(CursorPosition{1, 4});
        rememberChapterCursor(database, QStringLiteral("chapter-2"), other);
        other.setCursorPosition(CursorPosition{1, 12});
        rememberChapterCursor(database, QStringLiteral("chapter-2"), other);

        CHECK(restoreChapterCursor(database, QStringLiteral("chapter-2"), opened));
        CHECK(opened.cursorPosition() == CursorPosition{1, 12});
        CHECK(restoreChapterCursor(database, QStringLiteral("chapter-1"), opened));
        CHECK(opened.cursorPosition() == CursorPosition{2, 9});
    }

    SECTION("a place past the end of a shorter text is its end") {
        BookEditor shorter;
        shorter.fromKml(test::kmlOf({QStringLiteral("One paragraph.")}));
        CHECK(restoreChapterCursor(database, QStringLiteral("chapter-1"), shorter));
        CHECK(shorter.cursorPosition() == CursorPosition{0, 14});
    }

    SECTION("a place past the end of a shorter paragraph is the end of the paragraph") {
        BookEditor shorter;
        shorter.fromKml(test::kmlOf({QStringLiteral("The first paragraph."),
                                     QStringLiteral("The second one."), QStringLiteral("End.")}));
        CHECK(restoreChapterCursor(database, QStringLiteral("chapter-1"), shorter));
        CHECK(shorter.cursorPosition() == CursorPosition{2, 4});
    }

    SECTION("a damaged place is not used") {
        for (const char* damaged : {"two,1,0.5", "-1,3,0.5", "2,9", "2,9,high", "2,9,inf"}) {
            database.setSetting(QStringLiteral("cursor.chapter-1"), QString::fromLatin1(damaged));
            CHECK_FALSE(restoreChapterCursor(database, QStringLiteral("chapter-1"), opened));
        }
        CHECK(opened.cursorPosition() == CursorPosition{0, 0});
    }
}

TEST_CASE("A chapter opens with the line of its cursor where it was in the view",
          "[gui][cursor]") {
    QTemporaryDir book;
    REQUIRE(book.isValid());
    ProjectDatabase database;
    REQUIRE(database.open(book.path()));
    const QString chapter = QStringLiteral("chapter-1");
    auto closed = longChapterEditor();
    auto opened = longChapterEditor();
    closed->setCursorPosition(CursorPosition{120, 5});

    SECTION("at the same height") {
        closed->scrollCursorLineTo(0.4);
        const qreal height = closed->cursorLineViewHeight();
        REQUIRE(height == Approx(0.4).margin(0.01));
        rememberChapterCursor(database, chapter, *closed);

        CHECK(restoreChapterCursor(database, chapter, *opened));
        CHECK(opened->cursorPosition() == CursorPosition{120, 5});
        CHECK(opened->cursorLineViewHeight() == Approx(height).margin(0.005));
        CHECK(cursorInView(*opened));
    }

    SECTION("at the same height, also when the view gets a new size before it is painted") {
        // The panels around a new tab settle: the bar of a file outside the book hides
        closed->scrollCursorLineTo(0.4);
        const qreal height = closed->cursorLineViewHeight();
        rememberChapterCursor(database, chapter, *closed);
        test::resizeWidget(*opened, QSize(600, 360));

        REQUIRE(restoreChapterCursor(database, chapter, *opened));
        test::resizeWidget(*opened, QSize(600, 400));
        CHECK(opened->cursorPosition() == CursorPosition{120, 5});
        CHECK(opened->cursorLineViewHeight() == Approx(height).margin(0.005));
        CHECK(cursorInView(*opened));
    }

    SECTION("the view keeps its scroll on a new size once the cursor moved") {
        closed->scrollCursorLineTo(0.4);
        rememberChapterCursor(database, chapter, *closed);
        REQUIRE(restoreChapterCursor(database, chapter, *opened));
        opened->setCursorPosition(CursorPosition{121, 0});
        const qreal scroll = opened->scrollOffset();

        test::resizeWidget(*opened, QSize(600, 440));
        CHECK(opened->scrollOffset() == Approx(scroll));
    }

    SECTION("the view keeps its scroll on a new size once it was painted") {
        closed->scrollCursorLineTo(0.4);
        rememberChapterCursor(database, chapter, *closed);
        REQUIRE(restoreChapterCursor(database, chapter, *opened));
        opened->grab();
        const qreal scroll = opened->scrollOffset();

        test::resizeWidget(*opened, QSize(600, 440));
        CHECK(opened->scrollOffset() == Approx(scroll));
    }

    SECTION("in the view, when it was scrolled off the view") {
        closed->setScrollOffset(0.0);
        REQUIRE(closed->cursorLineViewHeight() > 1.0);
        rememberChapterCursor(database, chapter, *closed);

        CHECK(restoreChapterCursor(database, chapter, *opened));
        CHECK(cursorInView(*opened));
    }

    SECTION("in the view, when the view is smaller now") {
        closed->scrollCursorLineTo(0.9);
        rememberChapterCursor(database, chapter, *closed);
        test::resizeWidget(*opened, QSize(600, 150));

        CHECK(restoreChapterCursor(database, chapter, *opened));
        CHECK(cursorInView(*opened));
    }

    SECTION("with the top of the text, when it is in the first lines") {
        database.setSetting(QStringLiteral("cursor.chapter-1"), QStringLiteral("0,3,0.9"));

        CHECK(restoreChapterCursor(database, chapter, *opened));
        CHECK(opened->cursorPosition() == CursorPosition{0, 3});
        CHECK(opened->scrollOffset() == 0.0);
    }
}
