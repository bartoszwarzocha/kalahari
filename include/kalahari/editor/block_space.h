// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file block_space.h
/// @brief The room a paragraph keeps for an image or a table next to it
///
/// An image alone in its paragraph and a table keep some room from the text above and below
/// them, so the writer needs no empty paragraph there. The room is a part of the paragraphs:
/// of the image's own (imageBlockSpace()), and of the paragraphs before and after a table
/// (tableBlockSpace()); a caption stays close to its image or table, the room on its far side.
/// The layout, the print, DOCX and EPUB add it to the paragraph's own space above and below.

#ifndef KALAHARI_EDITOR_BLOCK_SPACE_H
#define KALAHARI_EDITOR_BLOCK_SPACE_H

class QTextBlock;

namespace kalahari::editor {

/// @brief The room a block keeps above it and below it for an image or a table, in points,
///        besides the room of its paragraph
struct BlockSpace {
    double before = 0.0;
    double after = 0.0;

    [[nodiscard]] bool isEmpty() const { return before <= 0.0 && after <= 0.0; }
    bool operator==(const BlockSpace&) const = default;
};

/// @brief The room of an image around a block: of the block holding an image alone
///        (holdsImageAlone()) its room above and below, of its caption the room on the
///        caption's side (above a caption above the image, below one below it): the image
///        and its caption stay together, the room around both (text_images.cpp)
BlockSpace imageBlockSpace(const QTextBlock& block);

/// @brief The room of a table around a block outside it: of the paragraph before a table
///        the table's room above it, of the paragraph after it its room below it; of a
///        caption of the table the room on the caption's far side (text_tables.cpp)
BlockSpace tableBlockSpace(const QTextBlock& block);

/// @brief The room of a block for the images and the tables next to it, both together
BlockSpace blockSpace(const QTextBlock& block);

}  // namespace kalahari::editor

#endif  // KALAHARI_EDITOR_BLOCK_SPACE_H
