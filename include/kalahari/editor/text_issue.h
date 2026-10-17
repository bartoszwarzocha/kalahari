/// @file text_issue.h
/// @brief A misspelled word or a grammar issue of a text, as its wave marks it

#pragma once

#include <QString>
#include <QStringList>
#include <string>

namespace kalahari::editor {

/// @brief What found an issue of a text
enum class TextIssueKind {
    Spelling,  ///< The dictionary does not know the word (SpellCheckService)
    Grammar    ///< Grammar, style or punctuation (GrammarCheckService)
};

/// @brief A misspelled word or a grammar issue of a text (BookEditor::textIssues())
struct TextIssue {
    TextIssueKind kind{TextIssueKind::Spelling};
    int paragraph{0};  ///< The paragraph (block number)
    int start{0};      ///< First character, in the paragraph
    int length{0};     ///< Number of characters
    QString text;      ///< The text marked: the word, or the words of the issue

    // A grammar issue's
    QString message;          ///< What is wrong
    QString shortMessage;     ///< What is wrong, in a few words (may be empty)
    QString ruleId;           ///< The rule that found it
    QString ruleDescription;  ///< What the rule looks for (may be empty)
    QStringList suggestions;  ///< What to put in its place; a misspelled word has none here:
                              ///< SpellCheckService::suggestions() gives them, for one word
                              ///< at a time, as they take the dictionary a moment

    bool operator==(const TextIssue& other) const = default;
};

/// @brief The theme's editor color of a kind (the "editor" section of a theme file)
///
/// As for the annotations (annotationColorKey()), each kind has a color for the light and
/// one for the dark paper: "spellingLightPaper", "grammarDarkPaper"...
/// @param kind The kind
/// @param darkPaper true: the color on dark paper (and on a dark panel)
inline std::string issueColorKey(TextIssueKind kind, bool darkPaper) {
    return std::string(kind == TextIssueKind::Spelling ? "spelling" : "grammar") +
           (darkPaper ? "DarkPaper" : "LightPaper");
}

}  // namespace kalahari::editor
