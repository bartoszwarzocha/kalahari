/// @file text_highlight.h
/// @brief Highlighted ranges of a paragraph's text (search matches, checks, the word read aloud)

#pragma once

namespace kalahari::editor {

/// @brief What a highlighted range of text is
///
/// The render pipeline draws every kind in one layer: the backgrounds under the text, the
/// waves over it.
enum class HighlightKind {
    SearchMatch,         ///< Match of the search
    CurrentSearchMatch,  ///< Current match of the search
    Spelling,            ///< Misspelled word
    Grammar,             ///< Grammar or style issue
    SpokenWord,          ///< Word being read aloud
    Repetition,          ///< Word repeated too close to another (the Repetitions panel)
    CurrentRepetition    ///< Repeated word chosen in the Repetitions panel: framed as well
};

/// @brief Highlighted range of one paragraph (offsets in the paragraph's text)
struct TextHighlight {
    int start = 0;                                    ///< First character
    int length = 0;                                   ///< Number of characters
    HighlightKind kind = HighlightKind::SearchMatch;  ///< What the range is

    bool operator==(const TextHighlight& other) const = default;
};

/// @brief How the repeated words are marked in the text (Settings > Editor > Repetitions)
enum class RepetitionMarkStyle {
    Background,  ///< A background under the word
    Wave         ///< A wave under the word
};

/// @brief A repeated word marked in the text (the Repetitions panel)
struct RepetitionMark {
    int paragraph = 0;     ///< Paragraph of the word
    int start = 0;         ///< First character of the word in the paragraph
    int length = 0;        ///< Number of characters
    bool current = false;  ///< Of the repetition chosen in the panel: framed

    bool operator==(const RepetitionMark& other) const = default;
};

}  // namespace kalahari::editor
