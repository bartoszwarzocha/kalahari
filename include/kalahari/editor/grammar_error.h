/// @file grammar_error.h
/// @brief An issue of grammar, style or punctuation found by LanguageTool

#pragma once

#include <QString>
#include <QStringList>

namespace kalahari::editor {

/// @brief What kind of issue LanguageTool found
enum class GrammarIssueType {
    Grammar,     ///< Grammar
    Style,       ///< Style
    Typography,  ///< Punctuation, typography, capital letters
    Spelling,    ///< Spelling (the dictionary checks it: SpellCheckService)
    Other        ///< Anything else
};

/// @brief One issue LanguageTool found in a text
struct GrammarError {
    int startPos{0};          ///< First character, in the text checked
    int length{0};            ///< Number of characters
    QString text;             ///< The text the issue is about
    QString message;          ///< What is wrong
    QString shortMessage;     ///< What is wrong, in a few words (may be empty)
    QString ruleId;           ///< The rule that found it (e.g. COMMA_BEFORE_AND)
    QString category;         ///< The name of the rule's category (e.g. Punctuation)
    GrammarIssueType type{GrammarIssueType::Grammar};
    QStringList suggestions;  ///< What to put in its place (at most 5)
    bool ignoreForIncompleteSentence{false};  ///< Not an issue while its sentence is unfinished

    GrammarError() = default;
    GrammarError(int start, int len, const QString& txt)
        : startPos(start), length(len), text(txt) {}

    bool operator==(const GrammarError& other) const {
        return startPos == other.startPos && length == other.length && text == other.text &&
               ruleId == other.ruleId;
    }
};

}  // namespace kalahari::editor
