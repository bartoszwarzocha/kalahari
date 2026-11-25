// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_text_points.cpp
/// @brief Fragments of a text for publishers linked to the points of its template: kept in
///        KML, found across paragraph breaks, linked and unlinked as one undo step, kept by
///        typing only inside a fragment, copied with the text

#include "kalahari_tests/editor/editor_test_utils.h"

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/text_points.h>

#include <QImage>
#include <QMimeData>
#include <QTextDocument>

#include <memory>
#include <string>

using kalahari::editor::BookEditor;
using kalahari::editor::TemplatePointLook;
using kalahari::editor::linkTemplatePoint;
using kalahari::editor::PointFragment;
using kalahari::editor::templatePointFragments;
using kalahari::editor::templatePointsAt;
using kalahari::editor::templatePointsIn;
using kalahari::editor::unlinkTemplatePoint;
using kalahari::test::kmlOf;
using kalahari::test::resizeWidget;

namespace {

/// An editor with KML loaded
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    editor->fromKml(kml);
    return editor;
}

/// The fragments of the points of an editor: "id start-end/characters", by id
std::string fragmentsOf(const BookEditor& editor) {
    const auto fragments = templatePointFragments(*editor.textDocument());
    QStringList ids = fragments.keys();
    ids.sort();
    QStringList list;
    for (const QString& id : ids) {
        for (const PointFragment& fragment : fragments.value(id)) {
            list << QStringLiteral("%1 %2-%3/%4")
                        .arg(id)
                        .arg(fragment.start)
                        .arg(fragment.end)
                        .arg(fragment.characters);
        }
    }
    return list.join(QStringLiteral(", ")).toStdString();
}

/// Image of the editor with one pixel per logical pixel
QImage editorImage(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// Pixels of an area that differ between two images of the same size
int differingPixels(const QImage& a, const QImage& b, const QRect& area) {
    int count = 0;
    const QRect clipped = area & a.rect();
    for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
        for (int x = clipped.left(); x <= clipped.right(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                ++count;
            }
        }
    }
    return count;
}

} // anonymous namespace

TEST_CASE("Text points: kept in KML, a fragment continues across a paragraph break",
          "[editor][publishing][points]") {
    // "The hero rides" 0-14, the break 14, "far away" 15-23
    const QString kml =
        kmlOf({QStringLiteral("The <points ids=\"hero\">hero <b>rides</b></points>"),
               QStringLiteral("<points ids=\"hero event\">far</points> away")});
    auto editor = editorWith(kml);
    CHECK(editor->plainText() == QStringLiteral("The hero rides\nfar away"));
    CHECK(fragmentsOf(*editor) == std::string("event 15-18/3, hero 4-18/13"));
    // Written as each piece of one format, as the references are; read back the same
    const QString written = editor->toKml();
    CHECK(written.toStdString() ==
          kmlOf({QStringLiteral("The <points ids=\"hero\">hero </points><points "
                                "ids=\"hero\"><b>rides</b></points>"),
                 QStringLiteral("<points ids=\"hero event\">far</points> away")})
              .toStdString());
    auto reloaded = editorWith(written);
    CHECK(fragmentsOf(*reloaded) == fragmentsOf(*editor));
    CHECK(reloaded->toKml() == written);

    // The text of a fragment is the text of the chapter, as plain text too
    CHECK(kalahari::core::ChapterDocument::kmlToPlainText(kml).contains(
        QStringLiteral("The hero rides")));

    const QTextDocument& document = *editor->textDocument();
    CHECK(templatePointsAt(document, 4) == QStringList{QStringLiteral("hero")});   // its start
    CHECK(templatePointsAt(document, 3).isEmpty());                                // before it
    CHECK(templatePointsAt(document, 15) == (QStringList{QStringLiteral("hero"),
                                                         QStringLiteral("event")}));
    CHECK(templatePointsAt(document, 18) == (QStringList{QStringLiteral("hero"),
                                                         QStringLiteral("event")}));
    CHECK(templatePointsIn(document, 0, 4).isEmpty());
    CHECK(templatePointsIn(document, 0, 16) ==
          (QStringList{QStringLiteral("hero"), QStringLiteral("event")}));
}

TEST_CASE("Text points: linked and unlinked as one undo step", "[editor][publishing][points]") {
    auto editor = editorWith(kmlOf({QStringLiteral("One two three"), QStringLiteral("four")}));
    QTextDocument& document = *editor->textDocument();

    REQUIRE(linkTemplatePoint(document, 4, 16, QStringLiteral("goal"), true));
    CHECK(fragmentsOf(*editor) == std::string("goal 4-16/11"));
    CHECK(editor->toKml().toStdString() ==
          kmlOf({QStringLiteral("One <points ids=\"goal\">two three</points>"),
                 QStringLiteral("<points ids=\"goal\">fo</points>ur")})
              .toStdString());
    CHECK_FALSE(linkTemplatePoint(document, 4, 7, QStringLiteral("goal"), true));  // linked

    // A second point on a part of the fragment
    REQUIRE(linkTemplatePoint(document, 0, 7, QStringLiteral("hero"), true));
    CHECK(fragmentsOf(*editor) == std::string("goal 4-16/11, hero 0-7/7"));

    // Unlinked in the middle: the fragment falls into two
    REQUIRE(linkTemplatePoint(document, 8, 13, QStringLiteral("goal"), false));
    CHECK(fragmentsOf(*editor) == std::string("goal 4-8/4, goal 14-16/2, hero 0-7/7"));

    editor->undo();
    CHECK(fragmentsOf(*editor) == std::string("goal 4-16/11, hero 0-7/7"));
    editor->undo();
    CHECK(fragmentsOf(*editor) == std::string("goal 4-16/11"));
    editor->redo();
    CHECK(fragmentsOf(*editor) == std::string("goal 4-16/11, hero 0-7/7"));

    REQUIRE(unlinkTemplatePoint(document, QStringLiteral("goal")));
    CHECK(fragmentsOf(*editor) == std::string("hero 0-7/7"));
    CHECK_FALSE(unlinkTemplatePoint(document, QStringLiteral("goal")));
    editor->undo();
    CHECK(fragmentsOf(*editor) == std::string("goal 4-16/11, hero 0-7/7"));
}

TEST_CASE("Text points: typed text joins a fragment only inside it",
          "[editor][publishing][points]") {
    // "The quick fox": "quick" 4-9
    auto editor = editorWith(kmlOf({QStringLiteral("The <points ids=\"hero\">quick</points> fox")}));

    SECTION("inside") {
        editor->setCursorPosition({0, 6});
        editor->insertText(QStringLiteral("X"));
        CHECK(fragmentsOf(*editor) == std::string("hero 4-10/6"));
    }

    SECTION("before its first character") {
        editor->setCursorPosition({0, 4});
        editor->insertText(QStringLiteral("X"));
        CHECK(fragmentsOf(*editor) == std::string("hero 5-10/5"));
    }

    SECTION("after its last character") {
        editor->setCursorPosition({0, 9});
        editor->insertText(QStringLiteral("X"));
        CHECK(fragmentsOf(*editor) == std::string("hero 4-9/5"));
    }

    SECTION("deleted, the fragment goes") {
        editor->setSelection({{0, 3}, {0, 10}});
        REQUIRE(editor->deleteSelectedText());
        CHECK(editor->plainText() == QStringLiteral("Thefox"));
        CHECK(fragmentsOf(*editor).empty());
    }
}

TEST_CASE("Text points: copied and pasted with the text", "[editor][publishing][points]") {
    auto editor = editorWith(kmlOf({QStringLiteral("The <points ids=\"hero\">quick</points> fox"),
                                    QStringLiteral("End")}));
    editor->setSelection({{0, 4}, {0, 9}});
    const auto data = editor->createMimeDataFromSelection();
    editor->clearSelection();
    editor->setCursorPosition({1, 3});
    editor->insertFromMimeData(data.get());
    CHECK(editor->plainText() == QStringLiteral("The quick fox\nEndquick"));
    CHECK(fragmentsOf(*editor) == std::string("hero 4-9/5, hero 17-22/5"));
}

TEST_CASE("Text points: a bar beside the fragments on the margin, a fill on demand",
          "[editor][publishing][points]") {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(700, 400));
    // "The quick fox" 0-13: "quick" 4-9 of the hero; "Far away" 14-22: "Far" of the event
    editor->fromKml(kmlOf({QStringLiteral("The <points ids=\"hero\">quick</points> fox"),
                           QStringLiteral("<points ids=\"event hero\">Far</points> away")}));
    const QImage plain = editorImage(*editor);

    // No marks without the points of a template
    CHECK(differingPixels(plain, editorImage(*editor), plain.rect()) == 0);

    TemplatePointLook look;
    look.order = {QStringLiteral("hero"), QStringLiteral("event")};
    look.colors = {QColor(QStringLiteral("#0f8579")), QColor(QStringLiteral("#c2185b"))};
    editor->setTemplatePointLook(look);
    const QImage bars = editorImage(*editor);

    // The bars on the margin, left of the text; the text itself as it was
    const QRectF word = editor->placeRect(4, false);
    const QRectF column = editor->textColumnRect(word.center().y());
    const QRect margin(0, static_cast<int>(word.top()), static_cast<int>(column.left()),
                       static_cast<int>(word.height()));
    CHECK(differingPixels(plain, bars, margin) > 0);
    const QRect text(static_cast<int>(column.left()), 0,
                     static_cast<int>(column.width()), plain.height());
    CHECK(differingPixels(plain, bars, text) == 0);

    // Show in the Text: a fill under the fragments too
    look.shown = true;
    editor->setTemplatePointLook(look);
    const QImage filled = editorImage(*editor);
    const QRectF quick = editor->placeRect(4, false).united(editor->placeRect(9, false));
    CHECK(differingPixels(bars, filled, quick.toAlignedRect()) > 0);
    // The point chosen in the list, alone
    look.shown = false;
    look.current = QStringLiteral("event");
    editor->setTemplatePointLook(look);
    const QImage chosen = editorImage(*editor);
    CHECK(differingPixels(bars, chosen, quick.toAlignedRect()) == 0);

    // Without the bars and the fill: the text as plain
    look.bars = false;
    look.current.clear();
    editor->setTemplatePointLook(look);
    CHECK(differingPixels(plain, editorImage(*editor), plain.rect()) == 0);
}
