/// @file paragraph_format.h
/// @brief A paragraph's own formatting (Format > Paragraph): its indents, spacing, line
///        spacing and the breaks of lines and pages, over what its style gives it
///
/// Each part is a property of the block format; a paragraph without one has what its style
/// gives it. Lengths are in points, as in KML: the layout shows them in the proportions of
/// the style's lengths (KalahariTextDocumentLayout).

#pragma once

#include <QString>
#include <QTextFormat>
#include <QXmlStreamAttributes>

#include <optional>
#include <utility>
#include <vector>

class QTextBlockFormat;

namespace kalahari::editor {

// =============================================================================
// Properties of the block format
// =============================================================================

/// @brief Left and right indent of every line, in points (double)
inline constexpr int PARAGRAPH_LEFT_INDENT_PROPERTY = QTextFormat::UserProperty + 120;
inline constexpr int PARAGRAPH_RIGHT_INDENT_PROPERTY = QTextFormat::UserProperty + 121;

/// @brief Indent of the first line from the left indent, in points (double); a negative one
///        is a hanging indent: the first line starts left of the others
inline constexpr int PARAGRAPH_FIRST_LINE_INDENT_PROPERTY = QTextFormat::UserProperty + 122;

/// @brief Space above and below the paragraph, in points (double)
inline constexpr int PARAGRAPH_SPACE_BEFORE_PROPERTY = QTextFormat::UserProperty + 123;
inline constexpr int PARAGRAPH_SPACE_AFTER_PROPERTY = QTextFormat::UserProperty + 124;

/// @brief Line spacing: its rule (int, LineSpacingRule) and value (double: a multiple of a
///        single line, or points)
inline constexpr int PARAGRAPH_LINE_SPACING_RULE_PROPERTY = QTextFormat::UserProperty + 125;
inline constexpr int PARAGRAPH_LINE_SPACING_PROPERTY = QTextFormat::UserProperty + 126;

/// @brief The breaks of lines and pages (bool)
inline constexpr int PARAGRAPH_KEEP_WITH_NEXT_PROPERTY = QTextFormat::UserProperty + 127;
inline constexpr int PARAGRAPH_KEEP_TOGETHER_PROPERTY = QTextFormat::UserProperty + 128;
inline constexpr int PARAGRAPH_PAGE_BREAK_BEFORE_PROPERTY = QTextFormat::UserProperty + 129;
inline constexpr int PARAGRAPH_WIDOW_CONTROL_PROPERTY = QTextFormat::UserProperty + 130;

/// @brief How the value of the line spacing is read
enum class LineSpacingRule {
    Multiple,  ///< A multiple of a single line: 1 single, 1.5, 2 double...
    AtLeast,   ///< At least so many points: a larger letter makes its line higher
    Exactly    ///< Every line exactly so many points high
};

/// @brief Line spacing of a paragraph
struct LineSpacing {
    LineSpacingRule rule = LineSpacingRule::Multiple;
    qreal value = 1.0;  ///< A multiple (Multiple) or points (AtLeast, Exactly)

    bool operator==(const LineSpacing&) const = default;
};

/// @brief Accepted ranges of the values
inline constexpr qreal MAX_PARAGRAPH_INDENT = 1000.0;  ///< Points, both ways
inline constexpr qreal MAX_PARAGRAPH_SPACING = 1000.0;  ///< Points
inline constexpr qreal MIN_LINE_SPACING_MULTIPLE = 0.5;
inline constexpr qreal MAX_LINE_SPACING_MULTIPLE = 4.0;
inline constexpr qreal MIN_LINE_SPACING_POINTS = 1.0;
inline constexpr qreal MAX_LINE_SPACING_POINTS = 1000.0;

/// @brief Step of Format > Increase Indent and Decrease Indent outside a list: 1.25 cm
inline constexpr qreal INDENT_STEP_POINTS = 1.25 / 2.54 * 72.0;

/// @brief A paragraph's own formatting; each part without a value comes from its style
struct ParagraphFormat {
    std::optional<qreal> leftIndent;       ///< Points
    std::optional<qreal> rightIndent;      ///< Points
    std::optional<qreal> firstLineIndent;  ///< Points; negative: a hanging indent
    std::optional<qreal> spaceBefore;      ///< Points
    std::optional<qreal> spaceAfter;       ///< Points
    std::optional<LineSpacing> lineSpacing;
    std::optional<bool> keepWithNext;      ///< On the page of the paragraph after it
    std::optional<bool> keepTogether;      ///< All its lines on one page
    std::optional<bool> pageBreakBefore;   ///< Starts on a new page
    std::optional<bool> widowControl;      ///< No single line of it alone on a page (on
                                           ///< without a value)

    /// @brief Whether no part has a value
    bool isEmpty() const;

    bool operator==(const ParagraphFormat&) const = default;
};

/// @brief The paragraph's own formatting from its block format
ParagraphFormat paragraphFormatOf(const QTextBlockFormat& format);

/// @brief Give a block format the parts of @p paragraph that have a value; the others are
///        left as they are
void mergeParagraphFormat(QTextBlockFormat& format, const ParagraphFormat& paragraph);

/// @brief Give a block format exactly @p paragraph: the parts without a value are removed
void setParagraphFormat(QTextBlockFormat& format, const ParagraphFormat& paragraph);

/// @brief Remove every part of the paragraph's own formatting (Format > Clear Formatting)
void clearParagraphFormat(QTextBlockFormat& format);

/// @brief The properties of the paragraph's own formatting
const std::vector<int>& paragraphFormatProperties();

/// @brief A text that tells the paragraph's own formatting apart from any other: empty
///        without any (for the layout's caches)
QString paragraphFormatKey(const QTextBlockFormat& format);

// =============================================================================
// KML
// =============================================================================

/// @brief The paragraph's own formatting from the attributes of its KML element (<p>, a
///        heading, <li>): indent-left, indent-right, indent-first, space-before,
///        space-after, line-height, line-height-min, line-height-exact, keep-with-next,
///        keep-together, page-break-before, widow-control
///
/// A value outside its range is brought into it; one that is not a number is ignored.
ParagraphFormat paragraphFormatFromKml(const QXmlStreamAttributes& attributes);

/// @brief The KML attributes of a paragraph's own formatting, as name and value, in the
///        order the editor writes them
std::vector<std::pair<QString, QString>> paragraphFormatToKml(const ParagraphFormat& paragraph);

/// @brief A number of points as KML writes it: at most two decimals, without trailing zeros
QString kmlNumber(qreal value);

}  // namespace kalahari::editor
