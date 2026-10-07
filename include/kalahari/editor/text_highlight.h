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
    SpokenWord           ///< Word being read aloud
};

/// @brief Highlighted range of one paragraph (offsets in the paragraph's text)
struct TextHighlight {
    int start = 0;                                    ///< First character
    int length = 0;                                   ///< Number of characters
    HighlightKind kind = HighlightKind::SearchMatch;  ///< What the range is

    bool operator==(const TextHighlight& other) const = default;
};

}  // namespace kalahari::editor
