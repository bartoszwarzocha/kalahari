// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_skeleton.h
/// @brief The skeleton of a text for publishers: the headings of the points of its template,
///        each with the hint the empty paragraph under it shows
///
/// A heading of a point is a paragraph in the style Template Point (TEMPLATE_POINT_STYLE_ID)
/// whose block format keeps the id of the point (TEMPLATE_POINT_PROPERTY) and its hint
/// (TEMPLATE_HINT_PROPERTY). The hint is no text: the view draws it, gray, in the empty
/// paragraph after the heading until the writer types there; print, export, the counts and
/// search never see it. The length of the text, the set for a publisher and its copies leave
/// the headings out (textWithoutSkeleton(), core::ChapterDocument::kmlToPlainText()).
///
/// KML writes a heading as <p style="template_point" point="p1" hint="...">Beginning</p>
/// (docs/kml_format.md).

#ifndef KALAHARI_EDITOR_TEXT_SKELETON_H
#define KALAHARI_EDITOR_TEXT_SKELETON_H

#include <QList>
#include <QString>
#include <QTextFormat>

#include <optional>

class QTextBlock;
class QTextBlockFormat;
class QTextCursor;
class QTextDocument;

namespace kalahari::editor {

/// @brief Id of the style of the headings of the points
inline constexpr const char* TEMPLATE_POINT_STYLE_ID = "template_point";

/// @brief Block format property of a heading of a point: the id of the point (QString)
inline constexpr int TEMPLATE_POINT_PROPERTY = QTextFormat::UserProperty + 240;

/// @brief Block format property of a heading of a point: its hint (QString)
inline constexpr int TEMPLATE_HINT_PROPERTY = QTextFormat::UserProperty + 241;

/// @brief A point of the skeleton: the heading and the hint under it
struct SkeletonPoint {
    QString id;    ///< The id of the point in its template ("p1")
    QString name;  ///< The text of its heading
    QString hint;  ///< What to write under it; empty: none

    bool operator==(const SkeletonPoint&) const = default;
};

/// @brief The point of a heading's block format (its id and hint, without its name); none
///        without an id
[[nodiscard]] std::optional<SkeletonPoint> templatePointOf(const QTextBlockFormat& format);

/// @brief Give a block format the id and the hint of @p point; none takes them away
void setTemplatePoint(QTextBlockFormat& format, const std::optional<SkeletonPoint>& point);

/// @brief Whether @p block is a heading of a point: a paragraph in the style Template Point
[[nodiscard]] bool isTemplatePointBlock(const QTextBlock& block);

/// @brief The hint the empty paragraph @p block shows: that of the heading just before it;
///        empty for a paragraph with text and after any other paragraph
[[nodiscard]] QString skeletonHintOf(const QTextBlock& block);

/// @brief The KML of the skeleton of @p points: each heading with an empty paragraph after it
[[nodiscard]] QString skeletonKml(const QList<SkeletonPoint>& points);

/// @brief Insert the skeleton of @p points at @p cursor, in paragraphs of their own: after
///        the paragraph of the cursor when it has text, in its place when it is empty
/// @return The position of the empty paragraph under the first heading; -1 when nothing was
///         inserted (no points, the cursor in a table)
int insertSkeleton(QTextCursor& cursor, const QList<SkeletonPoint>& points);

/// @brief Remove the headings of the points of @p cursor's document, each with the empty
///        paragraph after it, as edits of @p cursor (in its edit block, when it has one)
/// @return The headings removed
int removeSkeleton(QTextCursor& cursor);

/// @brief Whether @p document has a heading of a point
[[nodiscard]] bool hasSkeleton(const QTextDocument& document);

/// @brief The text of @p document without the headings of the points: its paragraphs joined
///        by '\n', without the characters of the objects, as BookEditor::plainText() gives it
[[nodiscard]] QString textWithoutSkeleton(const QTextDocument& document);

}  // namespace kalahari::editor

#endif  // KALAHARI_EDITOR_TEXT_SKELETON_H
