// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_statistics.h
/// @brief Word and character counting shared by the whole application

#pragma once

#include <QStringView>

#include <string>

namespace kalahari::core {

/// @brief Word, character and paragraph counts of a piece of text
struct TextCounts {
    int words = 0;               ///< Words, as defined by countText()
    int characters = 0;          ///< Characters with spaces, without line and paragraph breaks
    int nonSpaceCharacters = 0;  ///< Characters other than whitespace
    int paragraphs = 0;          ///< Paragraphs with text, as defined by countText()
    int dashes = 0;              ///< Dashes standing alone (WordCountRules::dashesAreWords),
                                 ///< whatever the rules; in words too when the rules make
                                 ///< them words
    /// The words of the texts of the footnotes and endnotes (as the rules count dashes),
    /// whatever WordCountRules::notesAreWords; in words, and their characters in the
    /// characters, when the rules count them
    int noteWords = 0;
    int noteDashes = 0;  ///< Of those, the dashes standing alone, whatever the rules
};

/// @brief The reading speed of the reading times, in words a minute
inline constexpr int READING_WORDS_PER_MINUTE = 200;

/// @brief The choices of the user that change what countText() counts as a word
struct WordCountRules {
    /// A dash standing alone between spaces, as at the start of a line of dialogue, is a
    /// word (Microsoft Word counts it, LibreOffice does not)
    bool dashesAreWords = false;

    /// The words of the footnotes and endnotes count, as in Microsoft Word (on by default)
    bool notesAreWords = true;

    bool operator==(const WordCountRules&) const = default;
};

/// @brief The words of a text under any rules of counting, so that new rules (a choice in the
///        settings) need no counting again: the counts of a whole book follow them at once
struct WordTally {
    int words = 0;   ///< Words under any rules: runs with a letter or a digit
    int dashes = 0;  ///< Dashes standing alone, words when the rules make them words
    int noteWords = 0;   ///< The same of the texts of the footnotes and endnotes, words when
    int noteDashes = 0;  ///< the rules count them

    /// @brief The words under @p rules, as countText() counts them (with the notes' words
    ///        when the rules count them)
    int wordsUnder(const WordCountRules& rules) const {
        const int notes = noteWords + (rules.dashesAreWords ? noteDashes : 0);
        return words + (rules.dashesAreWords ? dashes : 0) + (rules.notesAreWords ? notes : 0);
    }

    /// @brief The tally of the text whose counts under @p rules are @p counts
    static WordTally of(const TextCounts& counts, const WordCountRules& rules) {
        const int textWords = counts.words - (rules.notesAreWords ? counts.noteWords : 0);
        return {textWords - (rules.dashesAreWords ? counts.dashes : 0), counts.dashes,
                counts.noteWords - (rules.dashesAreWords ? counts.noteDashes : 0),
                counts.noteDashes};
    }

    bool operator==(const WordTally&) const = default;
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

/// @brief The words of @p text under any rules, as countText() counts them
WordTally tallyWords(QStringView text);

/// @brief Add the counts of the text of a note (a footnote or an endnote) to @p counts: in
///        noteWords and noteDashes, and in the words and characters when @p rules count the
///        notes' words; its paragraphs are not paragraphs of the text
/// @param noteText The note's text: its paragraphs one per line
void addNoteCounts(TextCounts& counts, QStringView noteText, const WordCountRules& rules);

/// @brief The reading time of @p words words in minutes, at READING_WORDS_PER_MINUTE, a
///        minute begun being a minute (no words: no time)
int readingMinutes(int words);

/// @brief The rules of the settings (Settings > Editor > General > Word Count)
WordCountRules wordCountRules();

/// @brief The keys of the settings of wordCountRules(): a change of one counts again
bool isWordCountRulesKey(const std::string& key);

}  // namespace kalahari::core
