// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file text_points.h
/// @brief Fragments of a text for publishers linked to the points of its template
///
/// The writer links a fragment of a synopsis to a point of its template ("Bohater i świat"),
/// so that the list of the points shows which of them the text covers. A link is a property
/// of the characters of the fragment (KmlPropTemplatePoints), like the anchor of an
/// annotation: it moves with the text, undo takes it back, copy and paste keep it. A
/// fragment may serve several points; the fragment of a point continues across paragraph
/// breaks while the characters on both sides of a break are linked to it.
///
/// KML writes a fragment as <points ids="hero event">...</points> (docs/kml_format.md).

#ifndef KALAHARI_EDITOR_TEXT_POINTS_H
#define KALAHARI_EDITOR_TEXT_POINTS_H

#include <QColor>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <array>

class QTextCharFormat;
class QTextDocument;
class QTextFormat;

namespace kalahari::editor {

/// @brief KML tag of a fragment linked to points
inline constexpr const char* TEXT_POINTS_TAG = "points";

/// @brief Attribute of TEXT_POINTS_TAG: the ids of the points, separated by spaces
inline constexpr const char* TEXT_POINTS_IDS = "ids";

/// @brief The colors of the points in their order, repeated after the last: names in the
///        "editor" section of the theme files
inline constexpr std::array<const char*, 8> TEMPLATE_POINT_COLOR_KEYS{
    "templatePoint1", "templatePoint2", "templatePoint3", "templatePoint4",
    "templatePoint5", "templatePoint6", "templatePoint7", "templatePoint8"};

/// @brief A fragment of the text linked to a point
struct PointFragment {
    int start = 0;       ///< Position of its first character
    int end = 0;         ///< Position after its last character
    int characters = 0;  ///< Its characters, without the paragraph breaks

    bool operator==(const PointFragment&) const = default;
};

/// @brief How an editor shows the fragments linked to the points
struct TemplatePointLook {
    QStringList order;     ///< The points of the template, in order; none: no marks
    QList<QColor> colors;  ///< The colors of the points by their places in order, repeated
    bool shown = false;    ///< A fill under every fragment (Show in the Text)
    bool bars = true;      ///< A bar beside the lines of each fragment on the left margin
    QString current;       ///< The point chosen in the list: its fragments filled stronger

    bool operator==(const TemplatePointLook&) const = default;
};

/// @brief The ids of the points a character format links to, in the order of the format
[[nodiscard]] QStringList templatePointsOf(const QTextFormat& format);

/// @brief Link a character format to @p ids; an empty list takes the link away
void setTemplatePoints(QTextFormat& format, const QStringList& ids);

/// @brief The fragments of each point in @p document, in the order of the text
[[nodiscard]] QHash<QString, QList<PointFragment>> templatePointFragments(
    const QTextDocument& document);

/// @brief The points of the characters next to @p position in its paragraph: of the one
///        before it and of the one after it
[[nodiscard]] QStringList templatePointsAt(const QTextDocument& document, int position);

/// @brief The points of any character from @p from to @p to
[[nodiscard]] QStringList templatePointsIn(const QTextDocument& document, int from, int to);

/// @brief Link the characters from @p from to @p to to the point @p id, or take the link
///        away (@p linked false), as one undo step
/// @return Whether a character changed
bool linkTemplatePoint(QTextDocument& document, int from, int to, const QString& id,
                       bool linked);

/// @brief Whether linkTemplatePoint() with these arguments would change a character
[[nodiscard]] bool changesTemplatePointLink(const QTextDocument& document, int from, int to,
                                            const QString& id, bool linked);

/// @brief Take the point @p id away from the whole text, as one undo step
/// @return Whether a character changed
bool unlinkTemplatePoint(QTextDocument& document, const QString& id);

/// @brief Keep in @p format, the format of text typed at @p position, only the points of
///        the characters on both sides of the position in its paragraph: text typed after or
///        before a fragment is not a part of it
void keepTemplatePointsBetween(QTextCharFormat& format, const QTextDocument& document,
                               int position);

} // namespace kalahari::editor

#endif // KALAHARI_EDITOR_TEXT_POINTS_H
