// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_text_skeleton.cpp
/// @brief The skeleton of a text for publishers: the headings of the points of its template in
///        the style Template Point, kept in KML with their ids and hints; the hint drawn in
///        the empty paragraph under a heading until the writer types there; Insert Skeleton
///        and Remove Headings and Hints as one undo step each; the text without the headings

#include "kalahari_tests/editor/editor_test_utils.h"

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_package.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_skeleton.h>
#include <kalahari/editor/text_source_adapter.h>

#include <QImage>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextDocument>

#include <memory>
#include <optional>

using kalahari::editor::BookEditor;
using kalahari::editor::CursorPosition;
using kalahari::editor::EditorAppearance;
using kalahari::editor::EditorColorMode;
using kalahari::editor::hasSkeleton;
using kalahari::editor::isTemplatePointBlock;
using kalahari::editor::ParagraphStyleSet;
using kalahari::editor::paragraphStyleOf;
using kalahari::editor::QTextDocumentSource;
using kalahari::editor::setTemplatePoint;
using kalahari::editor::skeletonHintOf;
using kalahari::editor::skeletonKml;
using kalahari::editor::SkeletonPoint;
using kalahari::editor::TEMPLATE_HINT_PROPERTY;
using kalahari::editor::TEMPLATE_POINT_STYLE_ID;
using kalahari::editor::templatePointOf;
using kalahari::editor::textWithoutSkeleton;
using kalahari::test::kmlOf;
using kalahari::test::resizeWidget;

namespace {

/// The points of a short skeleton
QList<SkeletonPoint> twoPoints() {
    return {{QStringLiteral("hero"), QStringLiteral("The hero"),
             QStringLiteral("Who is the hero, and where?")},
            {QStringLiteral("end"), QStringLiteral("The end"), QStringLiteral("How it ends.")}};
}

/// A paragraph style of a package with @p properties
kalahari::core::PackageParagraphStyle packageStyle(const QString& id, const QString& next = {},
                                                   const QVariantMap& properties = {}) {
    kalahari::core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), id);
    style.nextStyle = next;
    style.properties = properties;
    return style;
}

/// Light paper, black text, a cursor that does not blink
EditorAppearance lightAppearance() {
    EditorAppearance appearance;
    appearance.colorMode = EditorColorMode::Light;
    appearance.colors.continuous.backgroundLight = QColor(255, 255, 255);
    appearance.colors.continuous.textLight = QColor(0, 0, 0);
    appearance.cursor.blinking = false;
    return appearance;
}

/// An editor with Normal, Quote and Template Point (followed by Normal) holding @p kml
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->setAppearance(lightAppearance());
    editor->setParagraphStyles(ParagraphStyleSet::fromPackage(
        {packageStyle(QStringLiteral("normal")), packageStyle(QStringLiteral("quote")),
         packageStyle(QString::fromLatin1(TEMPLATE_POINT_STYLE_ID), QStringLiteral("normal"),
                      {{QStringLiteral("bold"), true}})},
        QStringLiteral("en")));
    editor->fromKml(kml);
    return editor;
}

/// The KML of a whole text of @p fragment
QString wholeKml(const QString& fragment) {
    return QStringLiteral("<kml>") + fragment + QStringLiteral("</kml>");
}

/// The block of paragraph @p index of @p editor
QTextBlock blockOf(const BookEditor& editor, int index) {
    return editor.textDocument()->findBlockByNumber(index);
}

/// The image of @p editor, one pixel per logical pixel
QImage imageOf(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

}  // namespace

TEST_CASE("Text skeleton: the headings of the points in KML, with their ids and hints",
          "[editor][skeleton]") {
    const QList<SkeletonPoint> points{
        {QStringLiteral("p1"), QStringLiteral("Hero & world"),
         QStringLiteral("Say \"who\" <and> where.")},
        {QStringLiteral("p2"), QStringLiteral("Ending"), QString()}};
    const QString kml = skeletonKml(points);
    CHECK(kml == QStringLiteral("<p style=\"template_point\" point=\"p1\" "
                                "hint=\"Say &quot;who&quot; &lt;and&gt; where.\">"
                                "Hero &amp; world</p><p></p>"
                                "<p style=\"template_point\" point=\"p2\">Ending</p><p></p>"));
    CHECK(skeletonKml({}).isEmpty());

    const auto editor = editorWith(wholeKml(kml));
    const QTextDocument& document = *editor->textDocument();
    REQUIRE(document.blockCount() == 4);
    CHECK(hasSkeleton(document));
    CHECK(isTemplatePointBlock(blockOf(*editor, 0)));
    CHECK_FALSE(isTemplatePointBlock(blockOf(*editor, 1)));
    CHECK(blockOf(*editor, 0).text() == QStringLiteral("Hero & world"));
    const std::optional<SkeletonPoint> first = templatePointOf(blockOf(*editor, 0).blockFormat());
    REQUIRE(first.has_value());
    CHECK(first->id == QStringLiteral("p1"));
    CHECK(first->hint == QStringLiteral("Say \"who\" <and> where."));
    CHECK(templatePointOf(blockOf(*editor, 2).blockFormat())->hint.isEmpty());
    CHECK_FALSE(templatePointOf(blockOf(*editor, 1).blockFormat()).has_value());

    // The hints: in the empty paragraph under a heading only
    CHECK(skeletonHintOf(blockOf(*editor, 1)) == first->hint);
    CHECK(skeletonHintOf(blockOf(*editor, 0)).isEmpty());
    CHECK(skeletonHintOf(blockOf(*editor, 3)).isEmpty());  // a heading without a hint
    const QTextDocumentSource source(editor->textDocument());
    CHECK(source.paragraphHint(1).text == first->hint);
    CHECK(source.paragraphHint(0).text.isEmpty());

    // Saved as it was read; the text without the headings
    const QString saved = editor->toKml();
    CHECK(saved.contains(QStringLiteral("point=\"p1\"")));
    CHECK(saved.contains(QStringLiteral("hint=\"Say &quot;who&quot; &lt;and&gt; where.\"")));
    CHECK(saved.contains(QStringLiteral("point=\"p2\"")));
    const auto again = editorWith(saved);
    CHECK(again->toKml() == saved);
    CHECK(textWithoutSkeleton(document) == QStringLiteral("\n"));

    // The point of a block format, given and taken away
    QTextBlockFormat format;
    setTemplatePoint(format, SkeletonPoint{QStringLiteral("p9"), QString(), QStringLiteral("x")});
    CHECK(templatePointOf(format)->id == QStringLiteral("p9"));
    setTemplatePoint(format, std::nullopt);
    CHECK_FALSE(templatePointOf(format).has_value());
    CHECK_FALSE(format.hasProperty(TEMPLATE_HINT_PROPERTY));
}

TEST_CASE("Text skeleton: the hint is drawn until the writer types under the heading",
          "[editor][skeleton]") {
    const auto withHint = editorWith(wholeKml(skeletonKml(twoPoints())));
    QList<SkeletonPoint> silent = twoPoints();
    for (SkeletonPoint& point : silent) {
        point.hint.clear();
    }
    const auto withoutHint = editorWith(wholeKml(skeletonKml(silent)));
    CHECK(imageOf(*withHint) != imageOf(*withoutHint));

    // Typed in the paragraph of the hint: the hint goes, the heading keeps it
    withHint->setCursorPosition({1, 0});
    withHint->insertText(QStringLiteral("Leila"));
    CHECK(skeletonHintOf(blockOf(*withHint, 1)).isEmpty());
    CHECK(skeletonHintOf(blockOf(*withHint, 3)) == QStringLiteral("How it ends."));

    // Enter at the end of a heading: the new paragraph is no heading and shows the hint
    withHint->setCursorPosition({2, 7});
    withHint->insertNewline();
    const QTextBlock added = blockOf(*withHint, 3);
    CHECK_FALSE(isTemplatePointBlock(added));
    CHECK_FALSE(templatePointOf(added.blockFormat()).has_value());
    CHECK(skeletonHintOf(added) == QStringLiteral("How it ends."));
    CHECK(textWithoutSkeleton(*withHint->textDocument()) == QStringLiteral("Leila\n\n"));
}

TEST_CASE("Text skeleton: Insert Skeleton and Remove Headings and Hints, one undo step each",
          "[editor][skeleton]") {
    const auto editor = editorWith(kmlOf({QStringLiteral("My synopsis.")}));
    const QString before = editor->toKml();
    editor->setCursorPosition({0, 3});

    // After the paragraph with text; the cursor under the first heading
    REQUIRE(editor->insertTemplateSkeleton(twoPoints()));
    const QTextDocument& document = *editor->textDocument();
    REQUIRE(document.blockCount() == 5);
    CHECK(blockOf(*editor, 0).text() == QStringLiteral("My synopsis."));
    CHECK(isTemplatePointBlock(blockOf(*editor, 1)));
    CHECK(blockOf(*editor, 1).text() == QStringLiteral("The hero"));
    CHECK(skeletonHintOf(blockOf(*editor, 2)) == QStringLiteral("Who is the hero, and where?"));
    CHECK(isTemplatePointBlock(blockOf(*editor, 3)));
    CHECK(editor->cursorPosition() == CursorPosition{2, 0});
    CHECK(textWithoutSkeleton(document) == QStringLiteral("My synopsis.\n\n"));

    editor->undo();
    CHECK(editor->toKml() == before);
    CHECK(editor->cursorPosition() == CursorPosition{0, 3});
    editor->redo();
    CHECK(hasSkeleton(document));

    // Typed under the first heading: that paragraph stays, the empty one goes
    editor->insertText(QStringLiteral("Leila."));
    const QString written = editor->toKml();
    editor->setCursorPosition({2, 2});
    CHECK(editor->removeTemplateSkeleton() == 2);
    CHECK_FALSE(hasSkeleton(document));
    REQUIRE(document.blockCount() == 2);
    CHECK(blockOf(*editor, 0).text() == QStringLiteral("My synopsis."));
    CHECK(blockOf(*editor, 1).text() == QStringLiteral("Leila."));
    CHECK(editor->cursorPosition() == CursorPosition{1, 2});  // at its character
    CHECK(editor->removeTemplateSkeleton() == 0);
    editor->undo();
    CHECK(editor->toKml() == written);

    // Nothing without points or in a read-only text
    CHECK_FALSE(editor->insertTemplateSkeleton({}));
    editor->setReadOnly(true);
    CHECK_FALSE(editor->insertTemplateSkeleton(twoPoints()));
    CHECK(editor->removeTemplateSkeleton() == 0);
    CHECK(editor->toKml() == written);
}

TEST_CASE("Text skeleton: in an empty text and at the beginning of a text",
          "[editor][skeleton]") {
    // In the place of an empty paragraph
    const auto empty = editorWith(kmlOf({QString()}));
    REQUIRE(empty->insertTemplateSkeleton(twoPoints()));
    CHECK(empty->textDocument()->blockCount() == 4);
    CHECK(isTemplatePointBlock(blockOf(*empty, 0)));
    CHECK(empty->cursorPosition() == CursorPosition{1, 0});
    // Alone in the text: an empty Normal paragraph stays
    CHECK(empty->removeTemplateSkeleton() == 2);
    REQUIRE(empty->textDocument()->blockCount() == 1);
    CHECK(blockOf(*empty, 0).text().isEmpty());
    CHECK_FALSE(isTemplatePointBlock(blockOf(*empty, 0)));

    // At the beginning of the text: the paragraph after it keeps its formats
    const auto first = editorWith(wholeKml(skeletonKml(twoPoints()) +
                                           QStringLiteral("<p style=\"quote\">Quoted.</p>")));
    CHECK(first->removeTemplateSkeleton() == 2);
    REQUIRE(first->textDocument()->blockCount() == 1);
    CHECK(blockOf(*first, 0).text() == QStringLiteral("Quoted."));
    CHECK(paragraphStyleOf(blockOf(*first, 0).blockFormat()) == QStringLiteral("quote"));
}
