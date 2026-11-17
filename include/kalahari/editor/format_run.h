// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file format_run.h
/// @brief FormatRun struct for paragraph formatting ranges (OpenSpec #00043)

#ifndef KALAHARI_EDITOR_FORMAT_RUN_H
#define KALAHARI_EDITOR_FORMAT_RUN_H

#include <QTextCharFormat>
#include <kalahari/editor/kml_format_registry.h>

namespace kalahari::editor {

/// @brief Represents a formatting run within a paragraph
///
/// A FormatRun describes a contiguous range of characters within a paragraph
/// that share the same formatting. The KML reader (KmlDocumentModel) stores the
/// formatting of a paragraph as runs.
struct FormatRun {
    size_t start = 0;           ///< Start offset within paragraph (inclusive)
    size_t end = 0;             ///< End offset within paragraph (exclusive)
    QTextCharFormat format;     ///< Format to apply

    /// @brief Check if this run has any non-default formatting
    [[nodiscard]] bool hasFormatting() const {
        return format.fontWeight() != QFont::Normal ||
               format.fontItalic() ||
               format.fontUnderline() ||
               format.fontStrikeOut() ||
               format.verticalAlignment() != QTextCharFormat::AlignNormal;
    }

    /// @brief Get length of this run
    [[nodiscard]] size_t length() const {
        return end > start ? end - start : 0;
    }

    /// @brief Check if run is empty
    [[nodiscard]] bool isEmpty() const {
        return start >= end;
    }

    /// @brief Check if position is within this run
    [[nodiscard]] bool contains(size_t pos) const {
        return pos >= start && pos < end;
    }

    /// @brief Check if this run overlaps with another range
    [[nodiscard]] bool overlaps(size_t rangeStart, size_t rangeEnd) const {
        return start < rangeEnd && end > rangeStart;
    }
};

} // namespace kalahari::editor

#endif  // KALAHARI_EDITOR_FORMAT_RUN_H
