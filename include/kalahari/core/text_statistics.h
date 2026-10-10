/// @file text_statistics.h
/// @brief Word and character counting shared by the whole application

#pragma once

#include <QStringView>

namespace kalahari::core {

/// @brief Word and character counts of a piece of text
struct TextCounts {
    int words = 0;               ///< Words, as defined by countText()
    int characters = 0;          ///< Characters with spaces, without line and paragraph breaks
    int nonSpaceCharacters = 0;  ///< Characters other than whitespace
};

/// @brief The choices of the user that change what countText() counts as a word
struct WordCountRules {
    /// A dash standing alone between spaces, as at the start of a line of dialogue, is a
    /// word (Microsoft Word counts it, LibreOffice does not)
    bool dashesAreWords = false;

    bool operator==(const WordCountRules&) const = default;
};

/// @brief Count the words and characters of @p text in one pass
///
/// The single definition of a word used everywhere (editor, chapter files, snapshots,
/// panels): a run of non-whitespace characters containing at least one letter or digit.
/// So "don't", "e-mail" and "2026" are one word each, while a scene break ("***") is not a
/// word, nor is a dialogue dash, unless @p rules make a run of dashes alone a word. Any
/// Unicode whitespace separates words, including the no-break space and paragraph
/// separators. Characters are counted as code points; the characters with spaces leave out
/// the line and paragraph breaks.
/// @param text The text to count
/// @param rules What counts as a word: the user's choice is wordCountRules()
TextCounts countText(QStringView text, const WordCountRules& rules);

/// @brief The rules of the settings (Settings > Editor > General > Word Count)
WordCountRules wordCountRules();

}  // namespace kalahari::core
