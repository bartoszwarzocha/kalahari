/// @file text_statistics.h
/// @brief Word and character counting shared by the whole application

#pragma once

#include <QStringView>

namespace kalahari::core {

/// @brief Word, character and paragraph counts of a piece of text
struct TextCounts {
    int words = 0;               ///< Words, as defined by countText()
    int characters = 0;          ///< Characters with spaces, without line and paragraph breaks
    int nonSpaceCharacters = 0;  ///< Characters other than whitespace
    int paragraphs = 0;          ///< Paragraphs with text, as defined by countText()
};

/// @brief The reading speed of the reading times, in words a minute
inline constexpr int READING_WORDS_PER_MINUTE = 200;

/// @brief The choices of the user that change what countText() counts as a word
struct WordCountRules {
    /// A dash standing alone between spaces, as at the start of a line of dialogue, is a
    /// word (Microsoft Word counts it, LibreOffice does not)
    bool dashesAreWords = false;

    bool operator==(const WordCountRules&) const = default;
};

/// @brief Count the words, characters and paragraphs of @p text in one pass
///
/// The single definition of a word used everywhere (editor, chapter files, snapshots,
/// panels): a run of non-whitespace characters containing at least one letter or digit.
/// So "don't", "e-mail" and "2026" are one word each, while a scene break ("***") is not a
/// word, nor is a dialogue dash, unless @p rules make a run of dashes alone a word. Any
/// Unicode whitespace separates words, including the no-break space and paragraph
/// separators. Characters are counted as code points; the characters with spaces leave out
/// the line and paragraph breaks. Paragraphs end at a new line (LF, CR, CR LF, NEL), a page
/// break or a paragraph separator, not at a line separator; only the paragraphs with a
/// character other than whitespace are counted, as in Word and LibreOffice.
/// @param text The text to count
/// @param rules What counts as a word: the user's choice is wordCountRules()
TextCounts countText(QStringView text, const WordCountRules& rules);

/// @brief The reading time of @p words words in minutes, at READING_WORDS_PER_MINUTE, a
///        minute begun being a minute (no words: no time)
int readingMinutes(int words);

/// @brief The rules of the settings (Settings > Editor > General > Word Count)
WordCountRules wordCountRules();

}  // namespace kalahari::core
