// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file layout_flow_internal.h
/// @brief KalahariTextDocumentLayout: what its sources share of the page flow (included only
///        by the layout's sources)

#pragma once

#include <QtGlobal>

#include <cstdint>

namespace kalahari::editor {

/// Page flow offset of a block whose lines follow its own top (not placed on a page)
inline constexpr qreal FLOW_UNPLACED = -1.0;

/// Page flow: the lines of a block (or a row of a table) were placed on a page with notes,
/// which may take another part of it the next time: they are placed again
inline constexpr qreal FLOW_PLACE_AGAIN = -2.0;

/// The lines of a block were laid out beside an image the text flows around, or in the box
/// of such an image: they are laid out again whenever the blocks are placed (across the whole
/// width once no image is beside them)
inline constexpr qreal FLOW_WRAPPED = -3.0;

/// Tolerance of page flow comparisons (its lengths are whole pixels)
inline constexpr qreal FLOW_EPSILON = 0.01;

/// Page flow flags of a block: kept on the page of the next block; a page break; starts on a
/// new page
inline constexpr std::uint16_t FLOW_KEEP_WITH_NEXT = 1;
inline constexpr std::uint16_t FLOW_PAGE_BREAK = 2;
inline constexpr std::uint16_t FLOW_PAGE_BREAK_BEFORE = 4;

/// Page flow flags of a block of a table, and of the first block of a row of a table (the
/// row is placed as a whole)
inline constexpr std::uint16_t FLOW_TABLE = 8;
inline constexpr std::uint16_t FLOW_ROW_START = 16;

/// Page flow flag of the first block of a row of a table that may be split between pages
inline constexpr std::uint16_t FLOW_ROW_SPLIT = 32;

/// Page flow flag of a block with notes: placed again whenever the blocks are (the notes
/// take the bottom of the pages)
inline constexpr std::uint16_t FLOW_NOTES = 64;

/// Flag of a block holding an image alone that the text flows around (FlowEntry::floatWrap)
inline constexpr std::uint16_t FLOW_FLOAT = 128;

/// Rounds of moving lines of a paragraph to the next page to keep its first or last lines
/// together (a three-line paragraph takes two: its last two lines, then all three; a long
/// one at most three: its last two lines, all of it, its last two lines again)
inline constexpr int KEEP_LINES_ROUNDS = 4;

/// Line width used when the document has no text width (no wrapping)
inline constexpr qreal UNWRAPPED_LINE_WIDTH = 10000.0;

}  // namespace kalahari::editor
