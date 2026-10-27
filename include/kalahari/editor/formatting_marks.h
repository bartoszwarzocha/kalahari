/// @file formatting_marks.h
/// @brief The formatting marks of a paragraph (View > Show Formatting Marks): ¶ ¤ · ° → ↵ ¬
///
/// The view draws the marks over the text, in a color of their own; they take no room, so
/// the lines break as without them, as on paper. Print and export never show them.

#pragma once

#include <QChar>
#include <QFont>
#include <QPointF>

#include <vector>

class QTextBlock;

namespace kalahari::editor {

/// @brief The end of a paragraph (Enter): ¶
inline constexpr char16_t PARAGRAPH_MARK = u'¶';

/// @brief The end of a cell of a table, after its last paragraph: ¤
inline constexpr char16_t CELL_MARK = u'¤';

/// @brief A space: ·
inline constexpr char16_t SPACE_MARK = u'·';

/// @brief A no-break space: °
inline constexpr char16_t NO_BREAK_SPACE_MARK = u'°';

/// @brief A tab: →
inline constexpr char16_t TAB_MARK = u'→';

/// @brief A line break inside a paragraph (Shift+Enter): ↵
inline constexpr char16_t LINE_BREAK_MARK = u'↵';

/// @brief An optional hyphen: ¬
inline constexpr char16_t OPTIONAL_HYPHEN_MARK = u'¬';

/// @brief Size of the ¬ of an optional hyphen inside a line, as a share of the text's font
///        (it has no room there: it is drawn small, above the letters' middle)
inline constexpr qreal INLINE_HYPHEN_MARK_SCALE = 0.6;

/// @brief A formatting mark of a paragraph, where the view draws it
struct FormattingMark {
    QChar glyph;       ///< One of the marks above
    QPointF position;  ///< Left end of its baseline, in the coordinates of the paragraph's lines
    QFont font;        ///< The font of the text there (smaller for a ¬ inside a line)
};

/// @brief The formatting marks of a paragraph, at its laid out lines
///
/// ¶ after the end of a paragraph, also of a heading, an empty paragraph and a scene break
/// (a horizontal line and a page break are as wide as the text: none), ¤ instead after the
/// last paragraph of a cell of a table, · in the middle of a
/// space, ° of a no-break space, → of a tab, ↵ after a line break, ¬ after the hyphen of an
/// optional hyphen at the end of a line and small above the text where it is inside a line.
/// @return Nothing for a block without lines
std::vector<FormattingMark> formattingMarksOf(const QTextBlock& block);

}  // namespace kalahari::editor
