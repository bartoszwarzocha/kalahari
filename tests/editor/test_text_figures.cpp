// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_text_figures.cpp
/// @brief The images and the tables of a text in text order, each with its caption: in the
///        editor's document and in a text's KML, and Edit > Go To by their places

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_figures.h>
#include <kalahari/editor/text_places.h>
#include "kalahari_tests/editor/editor_test_utils.h"

#include <QStringList>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>

#include <memory>
#include <string>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// An image with a numbered caption, a table with one above it and an image in a cell, an image
/// in a line of text, a table with a caption without a number after it
const QString FIGURES = QStringLiteral(
    "<kml><p>Start</p>"
    "<p align=\"center\"><img src=\"images/a.png\" alt=\"A bridge\"/></p>"
    "<p style=\"caption\">Figure <caption-number label=\"Figure\" id=\"f1\"/>. The bridge</p>"
    "<p style=\"caption\" caption=\"above\">Table <caption-number label=\"Table\" id=\"t1\"/>: "
    "The wells</p>"
    "<table><tr><td><p>A</p></td><td><p>B</p></td><td><p>C</p></td></tr>"
    "<tr><td><p><img src=\"images/cell.png\"/></p></td><td><p>E</p></td><td><p>F</p></td></tr>"
    "</table>"
    "<p>Between <img src=\"images/camel.png\" name=\"Camel\"/> words</p>"
    "<table><tr><td><p>x</p></td></tr></table>"
    "<p style=\"caption\">A plain caption</p>"
    "<p>See <cross-reference to=\"f1\" show=\"label\"/>.</p></kml>");

std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(700, 500));
    EditorAppearance appearance;
    appearance.cursor.blinking = false;
    editor->setAppearance(appearance);
    editor->fromKml(kml);
    return editor;
}

/// The figures as "i1 Figure 1|The bridge" ("i": an image, "t": a table, its ordinal, its title
/// and its caption), parted by " / "
std::string described(const std::vector<FigureInText>& figures) {
    QStringList parts;
    for (const FigureInText& figure : figures) {
        parts << (figure.kind == FigureKind::Table ? QStringLiteral("t") : QStringLiteral("i")) +
                     QString::number(figure.ordinal) + QLatin1Char(' ') + figure.title +
                     QLatin1Char('|') + figure.caption;
    }
    return parts.join(QStringLiteral(" / ")).toStdString();
}

}  // namespace

TEST_CASE("Figures: the images and tables of a text in text order, with their captions",
          "[editor][figures]") {
    auto editor = editorWith(FIGURES);
    const std::vector<FigureInText> figures =
        figuresIn(*editor->textDocument(), editor->captionNumbers());
    // A table before the image in its cell; numbered captions with their numbers
    CHECK(described(figures) ==
          "i1 Figure 1|The bridge / t1 Table 1|The wells / i2 | / i3 | / t2 |A plain caption");
    REQUIRE(figures.size() == 5);

    const FigureInText& bridge = figures.at(0);
    CHECK(bridge.captioned);
    CHECK(bridge.whole == QStringLiteral("Figure 1. The bridge"));
    CHECK(bridge.captionId == QStringLiteral("f1"));
    CHECK(bridge.image.source == QStringLiteral("images/a.png"));
    CHECK(imageNameOf(bridge.image) == QStringLiteral("A bridge"));

    const FigureInText& wells = figures.at(1);
    CHECK(wells.kind == FigureKind::Table);
    CHECK(wells.rows == 2);
    CHECK(wells.columns == 3);
    CHECK(wells.whole == QStringLiteral("Table 1: The wells"));

    // Without a caption: the image's name, else the name of its file
    CHECK_FALSE(figures.at(2).captioned);
    CHECK(imageNameOf(figures.at(2).image) == QStringLiteral("cell.png"));
    CHECK_FALSE(figures.at(3).captioned);
    CHECK(imageNameOf(figures.at(3).image) == QStringLiteral("Camel"));

    // A caption without a number: its text, no title
    const FigureInText& plain = figures.at(4);
    CHECK(plain.captioned);
    CHECK(plain.title.isEmpty());
    CHECK(plain.captionId.isEmpty());
    CHECK(plain.whole == QStringLiteral("A plain caption"));

    // The places are those of Edit > Go To: the same ordinals reach the same figures
    CHECK(editor->placePositions(PlaceKind::Image) ==
          std::vector<int>{figures.at(0).position, figures.at(2).position,
                           figures.at(3).position});
    CHECK(editor->placePositions(PlaceKind::Table) ==
          std::vector<int>{figures.at(1).position, figures.at(4).position});
    CHECK(editor->goToPlace(PlaceKind::Image, QStringLiteral("3")).result == GoToResult::Done);
    REQUIRE(editor->selectedImage().has_value());
    CHECK(*editor->selectedImage() == figures.at(3).position);
}

TEST_CASE("Figures: a text's KML lists them as its editor does, numbered in the book around it",
          "[editor][figures][kml]") {
    // The figures of the texts before it come first in the numbers
    CaptionContext context;
    context.counted.insert(QStringLiteral("Figure"), 4);
    context.counted.insert(QStringLiteral("Table"), 2);
    const std::vector<FigureInText> read = figuresInKml(FIGURES, context);
    CHECK(described(read) ==
          "i1 Figure 5|The bridge / t1 Table 3|The wells / i2 | / i3 | / t2 |A plain caption");

    // As the editor lists them in the same book
    auto editor = editorWith(FIGURES);
    editor->setCaptionContext(context);
    CHECK(read == figuresIn(*editor->textDocument(), editor->captionNumbers()));

    // A text without images and tables: none, and the text is not read
    CHECK(figuresInKml(QStringLiteral("<kml><p>Only words</p></kml>"), context).empty());
}

TEST_CASE("Figures: a caption changed in the editor is listed as it is now", "[editor][figures]") {
    auto editor = editorWith(FIGURES);
    // A word added to the caption of the image
    QTextCursor cursor(editor->textDocument()->findBlockByNumber(2));
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertText(QStringLiteral(" in 1931"));
    const std::vector<FigureInText> figures =
        figuresIn(*editor->textDocument(), editor->captionNumbers());
    REQUIRE_FALSE(figures.empty());
    CHECK(figures.front().caption == QStringLiteral("The bridge in 1931"));
}
