/// @file editor_types.h
/// @brief Common types for Kalahari text editor module (OpenSpec #00042)
///
/// This header provides forward declarations and basic types used throughout
/// the custom text editor implementation.

#pragma once

#include <QString>
#include <QTextFormat>
#include <QVector>

namespace kalahari::editor {

/// @brief Cursor position in document (paragraph + character offset)
struct CursorPosition {
    int paragraph = 0;   ///< Paragraph index (0-based)
    int offset = 0;      ///< Character offset within paragraph (0-based)

    bool operator==(const CursorPosition& other) const {
        return paragraph == other.paragraph && offset == other.offset;
    }

    bool operator!=(const CursorPosition& other) const {
        return !(*this == other);
    }

    bool operator<(const CursorPosition& other) const {
        if (paragraph != other.paragraph) {
            return paragraph < other.paragraph;
        }
        return offset < other.offset;
    }

    bool operator<=(const CursorPosition& other) const {
        return *this < other || *this == other;
    }

    bool operator>(const CursorPosition& other) const {
        return !(*this <= other);
    }

    bool operator>=(const CursorPosition& other) const {
        return !(*this < other);
    }
};

/// @brief Selection range in document (from start to end cursor)
struct SelectionRange {
    CursorPosition start;
    CursorPosition end;

    /// @brief Check if selection is empty (start == end)
    bool isEmpty() const {
        return start == end;
    }

    /// @brief Check if selection spans multiple paragraphs
    bool isMultiParagraph() const {
        return start.paragraph != end.paragraph;
    }

    /// @brief Normalize range so start <= end
    SelectionRange normalized() const {
        if (start <= end) {
            return *this;
        }
        return {end, start};
    }
};

/// @brief View typography applied when blocks are broken into lines
///
/// A view setting, not a block format: changing it re-lays out the text but does not
/// touch the document, its formats or the undo history, so it is never saved with a
/// chapter.
///
/// Lengths are pixels for a document font of @c referencePointSize. They scale with the
/// document's default font, so a larger font in the settings spaces the text out with it
/// (in the one relayout the font needs). A reference size of 0 uses the lengths as they
/// are.
struct LayoutTypography {
    qreal lineSpacing = 1.0;         ///< Multiplier of each line's natural height
    qreal paragraphSpacing = 0.0;    ///< Space below every paragraph
    qreal firstLineIndent = 0.0;     ///< Indent of the first line (left-aligned and justified paragraphs)
    qreal referencePointSize = 0.0;  ///< Font size the lengths are given for

    bool operator==(const LayoutTypography&) const = default;
};

/// @brief Page flow of a document layout: the text as a stack of page text areas
///
/// Text area i spans [i * pitch, i * pitch + textHeight) in document coordinates. The
/// space between two areas holds the bottom margin of one page, the gap and the top
/// margin of the next one, so no line is placed there. Without the flow (the continuous
/// views) the text is one endless text area, at least textHeight high, as the first page
/// is. Lengths in document units (the layout rounds them to whole pixels).
struct PageFlow {
    bool enabled = false;
    qreal pitch = 0.0;       ///< Distance between the tops of two consecutive text areas
    qreal textHeight = 0.0;  ///< Height of one text area

    bool operator==(const PageFlow&) const = default;
};

/// @brief Alignment of a paragraph without one of its own: justified (Qt leaves the last
/// line of a justified paragraph at its leading edge)
inline constexpr Qt::Alignment DEFAULT_PARAGRAPH_ALIGNMENT = Qt::AlignJustify;

/// @brief The horizontal alignment a block format sets itself, none without one
///
/// QTextBlockFormat::alignment() reports Qt::AlignLeft for a block without an alignment,
/// so left alignment set on purpose could not be told from none.
inline Qt::Alignment ownAlignment(const QTextBlockFormat& format) {
    return Qt::Alignment(format.intProperty(QTextFormat::BlockAlignment)) &
           Qt::AlignHorizontal_Mask;
}

/// @brief The alignment a paragraph is shown with: its own, or the default without one
inline Qt::Alignment effectiveAlignment(Qt::Alignment own) {
    own &= Qt::AlignHorizontal_Mask;
    if (!own) {
        return DEFAULT_PARAGRAPH_ALIGNMENT;
    }
    return own;
}

}  // namespace kalahari::editor
