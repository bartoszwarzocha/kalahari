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

}  // namespace kalahari::editor
