// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_tab_guides.cpp
/// @brief The tab guides of a text: dashed lines down its pages that a Tab aligns the text to,
///        kept in its KML, set and moved at the cursor as undo steps, and drawn in the views

#include "kalahari_tests/editor/editor_test_utils.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/tab_guides.h>
#include <kalahari/editor/view_modes.h>

#include <QImage>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

using kalahari::editor::BookEditor;
using kalahari::editor::CursorPosition;
using kalahari::editor::EditorAppearance;
using kalahari::editor::EditorColorMode;
using kalahari::editor::KalahariTextDocumentLayout;
using kalahari::editor::KmlDocumentModel;
using kalahari::editor::KmlSerializer;
using kalahari::editor::MAX_TAB_GUIDES;
using kalahari::editor::normalizedTabGuides;
using kalahari::editor::replacedInKml;
using kalahari::editor::SearchOptions;
using kalahari::editor::TAB_GUIDE_MAX_MM;
using kalahari::editor::TabGuideOutcome;
using kalahari::editor::TabGuideResult;
using kalahari::editor::ViewMode;
using kalahari::test::resizeWidget;
using Catch::Approx;

namespace {

/// The color of the guides in these tests: neither the paper nor the text has it
const QColor GUIDE_COLOR(0, 200, 0);

/// Every line of the editor's text laid out
void layOut(BookEditor& editor) {
    auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    layout->layoutPendingBlocks();
}

/// An editor of the KML @p kml in @p mode, light paper, the cursor not blinking, the guides
/// green, every line laid out
std::unique_ptr<BookEditor> editorOf(const QString& kml, ViewMode mode = ViewMode::Continuous) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(1000, 700));
    EditorAppearance appearance = editor->appearance();
    appearance.colorMode = EditorColorMode::Light;
    appearance.cursor.blinking = false;
    appearance.colors.tabGuides.light = GUIDE_COLOR;
    editor->setAppearance(appearance);
    editor->fromKml(kml);
    editor->setViewMode(mode);
    layOut(*editor);
    return editor;
}

/// Document units in a millimetre, as the editor's layout counts them
double perMillimetre(BookEditor& editor) {
    const auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    REQUIRE(layout->typography().pixelsPerMillimetre > 0.0);
    return layout->typography().pixelsPerMillimetre;
}

/// Where the character at @p offset of paragraph @p paragraph starts, from the left edge of
/// the text, in millimetres
double millimetresAt(BookEditor& editor, int paragraph, int offset) {
    layOut(editor);
    const QTextBlock block = editor.textDocument()->findBlockByNumber(paragraph);
    REQUIRE(block.isValid());
    const QTextLine line = block.layout()->lineForTextPosition(offset);
    REQUIRE(line.isValid());
    return line.cursorToX(offset) / perMillimetre(editor);
}

QImage paint(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    editor.render(&image);
    return image;
}

/// The green pixels of the guides in the columns around @p x of the editor painted
int guideInk(BookEditor& editor, qreal x) {
    const QImage image = paint(editor);
    int ink = 0;
    for (int column = static_cast<int>(x) - 1; column <= static_cast<int>(x) + 1; ++column) {
        for (int y = 0; y < image.height(); ++y) {
            const QColor pixel = image.pixelColor(column, y);
            if (pixel.green() - std::max(pixel.red(), pixel.blue()) > 40) {
                ++ink;
            }
        }
    }
    return ink;
}

/// The left edge of the cursor in the editor, painted at @p position
qreal caretX(BookEditor& editor, const CursorPosition& position) {
    editor.setCursorPosition(position);
    paint(editor);
    return editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF().left();
}

}  // namespace

TEST_CASE("Tab guides: on whole millimetres, in order, apart, inside the text, at most 20",
          "[editor][tabguides]") {
    CHECK(normalizedTabGuides({40.4, 20.0, 21.0, -5.0, 0.0, 600.0, 80.0}) ==
          std::vector<double>{20.0, 40.0, 80.0, TAB_GUIDE_MAX_MM});
    std::vector<double> many;
    for (int i = 1; i <= 30; ++i) {
        many.push_back(i * 10.0);
    }
    const std::vector<double> kept = normalizedTabGuides(many);
    REQUIRE(kept.size() == static_cast<size_t>(MAX_TAB_GUIDES));
    CHECK(kept.front() == 10.0);
    CHECK(kept.back() == 200.0);
}

TEST_CASE("Tab guides: read from the KML of a text and written back before its paragraphs",
          "[editor][tabguides][kml]") {
    const QString kml = QStringLiteral(
        "<kml><tab-guides><guide at=\"80\"/><guide at=\"x\"/><guide at=\"40\"/></tab-guides>"
        "<p>Name\tJohn</p></kml>");
    KmlDocumentModel model;
    REQUIRE(model.loadKml(kml));
    CHECK(model.tabGuides() == std::vector<double>{40.0, 80.0});
    CHECK(model.paragraphCount() == 1);
    CHECK(model.paragraphText(0) == QStringLiteral("Name\tJohn"));

    auto editor = editorOf(kml);
    CHECK(editor->tabGuides() == std::vector<double>{40.0, 80.0});
    CHECK(editor->toKml().startsWith(
        QStringLiteral("<kml><tab-guides><guide at=\"40\"/><guide at=\"80\"/></tab-guides><p")));
    CHECK_FALSE(editor->isModified());

    SECTION("A part of the text has none: the guides are of the whole text") {
        const QString part = KmlSerializer().toKml(editor->textDocument(), 0, 4);
        CHECK_FALSE(part.contains(QStringLiteral("tab-guides")));
    }
    SECTION("A text without them has no section") {
        auto plain = editorOf(QStringLiteral("<kml><p>Name\tJohn</p></kml>"));
        CHECK(plain->tabGuides().empty());
        CHECK_FALSE(plain->toKml().contains(QStringLiteral("tab-guides")));
    }
    SECTION("Replace in a closed text keeps them") {
        int count = 0;
        const QString replaced = replacedInKml(kml, QStringLiteral("John"), QStringLiteral("Jan"),
                                               SearchOptions{}, &count);
        CHECK(count == 1);
        CHECK(replaced.contains(QStringLiteral("<tab-guides><guide at=\"40\"/>")));
        CHECK(replaced.contains(QStringLiteral("Name\tJan")));
    }
    SECTION("A scene moved: they stay at the start of the text") {
        const QString scenes = QStringLiteral(
            "<kml><tab-guides><guide at=\"40\"/></tab-guides><p>One</p><scene-break/>"
            "<p>Two</p></kml>");
        const std::optional<kalahari::core::SceneText> moved =
            kalahari::core::ChapterDocument::withSceneMoved({scenes, {}}, 0, 1);
        REQUIRE(moved.has_value());
        CHECK(moved->kml.startsWith(QStringLiteral("<kml><tab-guides><guide at=\"40\"/>")));
        CHECK(moved->kml.indexOf(QStringLiteral("Two")) <
              moved->kml.indexOf(QStringLiteral("One")));
    }
}

TEST_CASE("Tab guides: a Tab goes to the next guide, counted from the left edge of the text",
          "[editor][tabguides][layout]") {
    // The first lines are indented (the Settings' first-line indent): the guides are not
    auto editor = editorOf(QStringLiteral(
        "<kml><tab-guides><guide at=\"40\"/><guide at=\"120\"/></tab-guides>"
        "<p>Name\tJohn</p>"
        "<p>\tIndented</p>"
        "<p indent-first=\"0\">A name that passes the first guide\tSmith</p></kml>"));
    REQUIRE(millimetresAt(*editor, 0, 0) > 0.5);  // the first line indented
    CHECK(millimetresAt(*editor, 0, 5) == Approx(40.0).margin(0.1));
    CHECK(millimetresAt(*editor, 1, 1) == Approx(40.0).margin(0.1));
    const int smith =
        static_cast<int>(QStringLiteral("A name that passes the first guide\t").size());
    REQUIRE(millimetresAt(*editor, 2, smith - 1) > 40.0);
    REQUIRE(millimetresAt(*editor, 2, smith - 1) < 120.0);
    CHECK(millimetresAt(*editor, 2, smith) == Approx(120.0).margin(0.1));

    SECTION("Without guides a Tab goes to the tab stops of the Settings") {
        REQUIRE(editor->setTabGuides({}));
        CHECK(millimetresAt(*editor, 0, 5) != Approx(40.0).margin(0.1));
    }
}

TEST_CASE("Tab guides: set, taken away and moved at the cursor, each an undo step",
          "[editor][tabguides][commands]") {
    auto editor = editorOf(QStringLiteral("<kml><p indent-first=\"0\">Name\tJohn</p></kml>"));
    editor->setCursorPosition({0, 4});  // after "Name"
    const std::optional<double> at = editor->cursorMillimetres();
    REQUIRE(at.has_value());
    CHECK(*at == Approx(millimetresAt(*editor, 0, 4)).margin(0.1));
    const double guide = std::round(*at);

    // A new guide at the cursor: the text is changed
    TabGuideOutcome outcome = editor->toggleTabGuideAtCursor();
    CHECK(outcome.result == TabGuideResult::Added);
    CHECK(outcome.at == guide);
    CHECK(editor->tabGuides() == std::vector<double>{guide});
    CHECK(editor->isModified());
    CHECK(editor->toKml().contains(QStringLiteral("<guide at=\"%1\"/>").arg(guide)));

    // The guide at the cursor goes
    outcome = editor->toggleTabGuideAtCursor();
    CHECK(outcome.result == TabGuideResult::Removed);
    CHECK(outcome.at == guide);
    CHECK(editor->tabGuides().empty());

    // Each change an undo step, which keeps the cursor where it was
    editor->undo();
    CHECK(editor->tabGuides() == std::vector<double>{guide});
    CHECK(editor->cursorPosition() == CursorPosition{0, 4});
    editor->undo();
    CHECK(editor->tabGuides().empty());
    CHECK_FALSE(editor->isModified());
    editor->redo();
    CHECK(editor->tabGuides() == std::vector<double>{guide});
    CHECK(editor->isModified());

    // Moved a step at a time: the text after the Tab goes with it
    REQUIRE(editor->setTabGuides({40.0}));
    CHECK(millimetresAt(*editor, 0, 5) == Approx(40.0).margin(0.1));
    outcome = editor->moveTabGuide(1);
    CHECK(outcome.result == TabGuideResult::Moved);
    CHECK(outcome.at == 41.0);
    CHECK(millimetresAt(*editor, 0, 5) == Approx(41.0).margin(0.1));
    CHECK(editor->moveTabGuide(-2).at == 39.0);

    // The guide nearest the cursor, not past the one beside it
    REQUIRE(editor->setTabGuides({30.0, 33.0}));
    editor->setCursorPosition({0, 0});
    CHECK(editor->moveTabGuide(1).at == 31.0);
    outcome = editor->moveTabGuide(1);
    CHECK(outcome.result == TabGuideResult::Blocked);
    CHECK(outcome.at == 31.0);
    CHECK(editor->tabGuides() == std::vector<double>{31.0, 33.0});

    // Not at the left edge of the text
    REQUIRE(editor->setTabGuides({}));
    CHECK(editor->toggleTabGuideAtCursor().result == TabGuideResult::AtEdge);
    CHECK(editor->moveTabGuide(1).result == TabGuideResult::NoGuides);

    // All of them at once
    REQUIRE(editor->setTabGuides({20.0, 60.0}));
    CHECK(editor->clearTabGuides().result == TabGuideResult::Cleared);
    CHECK(editor->tabGuides().empty());
    CHECK(editor->clearTabGuides().result == TabGuideResult::NoGuides);

    // A text only read keeps its guides
    REQUIRE(editor->setTabGuides({20.0}));
    editor->setReadOnly(true);
    CHECK(editor->toggleTabGuideAtCursor().result == TabGuideResult::Refused);
    CHECK(editor->clearTabGuides().result == TabGuideResult::Refused);
    CHECK_FALSE(editor->setTabGuides({}));
    CHECK(editor->tabGuides() == std::vector<double>{20.0});
}

TEST_CASE("Tab guides: while the cursor stays, the guide moved goes on moving",
          "[editor][tabguides][commands]") {
    auto editor = editorOf(
        QStringLiteral("<kml><p indent-first=\"0\">A line long enough for the guides</p></kml>"));
    editor->setCursorPosition({0, 20});
    const std::optional<double> at = editor->cursorMillimetres();
    REQUIRE(at.has_value());
    REQUIRE(*at > 20.0);
    const double cursor = std::round(*at);

    // The guide left of the cursor is the nearer one; moved away from it, it is farther than
    // the one on the right, and still the one that moves
    REQUIRE(editor->setTabGuides({cursor - 3.0, cursor + 5.0}));
    for (int step = 0; step < 5; ++step) {
        CHECK(editor->moveTabGuide(-1).result == TabGuideResult::Moved);
    }
    CHECK(editor->tabGuides() == std::vector<double>{cursor - 8.0, cursor + 5.0});

    // With the cursor moved, the guide nearest it
    editor->setCursorPosition({0, 21});
    CHECK(editor->moveTabGuide(1).at == cursor + 6.0);
}

TEST_CASE("Tab guides: dashed lines down the text, in the color of the Settings",
          "[editor][tabguides][render]") {
    for (const ViewMode mode : {ViewMode::Continuous, ViewMode::Page}) {
        INFO(static_cast<int>(mode));
        auto editor = editorOf(QStringLiteral("<kml><tab-guides><guide at=\"40\"/></tab-guides>"
                                              "<p indent-first=\"0\">\tJohn</p></kml>"),
                               mode);
        // The guide is where the text after the Tab starts
        const qreal x = caretX(*editor, {0, 1});
        const int ink = guideInk(*editor, x);
        CHECK(ink > editor->height() / 4);  // down the view, dashed
        CHECK(ink < editor->height() * 3);  // a line, not a fill

        // Not without guides
        REQUIRE(editor->setTabGuides({}));
        CHECK(guideInk(*editor, x) == 0);
    }
}
