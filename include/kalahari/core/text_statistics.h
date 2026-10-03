/// @file text_statistics.h
/// @brief Word and character counting shared by the whole application

#pragma once

#include <QStringView>

namespace kalahari::core {

/// @brief Word and character counts of a piece of text
struct TextCounts {
    int words = 0;               ///< Words, as defined by countText()
    int nonSpaceCharacters = 0;  ///< Characters other than whitespace
};

/// @brief Count the words and non-space characters of @p text in one pass
///
/// The single definition of a word used everywhere (editor, chapter files, snapshots,
/// panels): a run of non-whitespace characters containing at least one letter or digit.
/// So "don't", "e-mail" and "2026" are one word each, while a dialogue dash ("–") or a
/// scene break ("***") is not a word. Any Unicode whitespace separates words, including
/// the no-break space and paragraph separators. Characters are counted as code points.
TextCounts countText(QStringView text);

}  // namespace kalahari::core
