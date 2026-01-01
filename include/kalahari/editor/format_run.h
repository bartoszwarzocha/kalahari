/// @file format_run.h
/// @brief FormatRun struct for paragraph formatting ranges (OpenSpec #00043)

#pragma once

#include <QTextCharFormat>
#include <kalahari/editor/kml_format_registry.h>

namespace kalahari::editor {

/// @brief Represents a formatting run within a paragraph
///
/// A FormatRun describes a contiguous range of characters within a paragraph
/// that share the same formatting. Used by LazyKmlDocument for efficient
/// storage of formatting information.
struct FormatRun {
    size_t start = 0;           ///< Start offset within paragraph (inclusive)
    size_t end = 0;             ///< End offset within paragraph (exclusive)
    QTextCharFormat format;     ///< Format to apply

    /// @brief Check if this run has any non-default formatting
    bool hasFormatting() const {
        return format.fontWeight() != QFont::Normal ||
               format.fontItalic() ||
               format.fontUnderline() ||
               format.fontStrikeOut() ||
               format.verticalAlignment() != QTextCharFormat::AlignNormal;
    }

    /// @brief Check if this run has comment metadata
    bool hasComment() const {
        return format.hasProperty(KmlPropComment);
    }

    /// @brief Check if this run has TODO metadata
    bool hasTodo() const {
        return format.hasProperty(KmlPropTodo);
    }

    /// @brief Check if this run has footnote metadata
    bool hasFootnote() const {
        return format.hasProperty(KmlPropFootnote);
    }

    /// @brief Check if this run has any metadata
    bool hasMetadata() const {
        return hasComment() || hasTodo() || hasFootnote();
    }

    /// @brief Get length of this run
    size_t length() const {
        return end > start ? end - start : 0;
    }

    /// @brief Check if run is empty
    bool isEmpty() const {
        return start >= end;
    }

    /// @brief Check if position is within this run
    bool contains(size_t pos) const {
        return pos >= start && pos < end;
    }

    /// @brief Check if this run overlaps with another range
    bool overlaps(size_t rangeStart, size_t rangeEnd) const {
        return start < rangeEnd && end > rangeStart;
    }
};

} // namespace kalahari::editor
