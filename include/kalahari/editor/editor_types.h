/// @file editor_types.h
/// @brief Common types for Kalahari text editor module (OpenSpec #00042)
///
/// This header provides forward declarations and basic types used throughout
/// the custom text editor implementation.

#pragma once

#include <QString>
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
/// document's default font, so a zoom that scales the font scales the spacing with it
/// (and re-lays out the text once, for the font). A reference size of 0 uses the
/// lengths as they are.
struct LayoutTypography {
    qreal lineSpacing = 1.0;         ///< Multiplier of each line's natural height
    qreal paragraphSpacing = 0.0;    ///< Space below every paragraph
    qreal firstLineIndent = 0.0;     ///< Indent of the first line (left-aligned and justified paragraphs)
    qreal referencePointSize = 0.0;  ///< Font size the lengths are given for

    bool operator==(const LayoutTypography&) const = default;
};

}  // namespace kalahari::editor
