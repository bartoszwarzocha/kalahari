/// @file paragraph_data.h
/// @brief Data the editor keeps with each paragraph of its document

#pragma once

#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/grammar_error.h>
#include <kalahari/editor/text_highlight.h>
#include <QString>
#include <QTextBlock>
#include <QTextBlockUserData>
#include <vector>

namespace kalahari::editor {

/// @brief Results of a check (spelling, grammar) of one paragraph
///
/// They are kept with the text they were made for and apply only while the paragraph
/// still has that text: an edit makes them stale until the paragraph is checked again.
/// The spelling check moves them with the edits instead (BookEditor), so the waves of the
/// words an edit leaves as they were stay.
struct ParagraphCheck {
    QString text;                       ///< Paragraph text the results are for (with issues)
    std::vector<TextHighlight> issues;  ///< Ranges found, offsets in that text
    bool current = false;               ///< Checked since the text or the dictionary changed

    /// @brief Issues that apply to a paragraph with @p currentText (none when it changed)
    const std::vector<TextHighlight>* issuesFor(const QString& currentText) const {
        return !issues.empty() && text == currentText ? &issues : nullptr;
    }
};

/// @brief Results of a check of a paragraph of a note's text, kept with the paragraph that
///        holds the note's number
///
/// As a ParagraphCheck, they apply while the note's paragraph has the text they were made
/// for. A result is kept also without issues: the paragraph is known to be right.
struct NoteCheck {
    QString id;                         ///< The note's
    int line = 0;                       ///< Its paragraph: a line of noteText()
    QString text;                       ///< That paragraph's text the results are for
    std::vector<TextHighlight> issues;  ///< Ranges found, offsets in that text
    std::vector<GrammarError> errors;   ///< What each grammar issue is, in the order of issues
    bool current = true;                ///< False: to be checked again (the checking changed)

    bool operator==(const NoteCheck&) const = default;

    /// @brief The result for paragraph @p line of a note with @p text (nullptr: none)
    static const NoteCheck* find(const std::vector<NoteCheck>& checks, const QString& id,
                                 int line, const QString& text) {
        for (const NoteCheck& check : checks) {
            if (check.line == line && check.id == id && check.text == text) {
                return &check;
            }
        }
        return nullptr;
    }
};

/// @brief Data kept with a paragraph, as the user data of its block
///
/// The document owns it. A block made by splitting a paragraph starts without data.
class ParagraphData : public QTextBlockUserData {
public:
    core::TextCounts counts;   ///< Word and character counts of the paragraph
    bool countsValid = false;  ///< False until counted, and after the paragraph changes

    ParagraphCheck spelling;  ///< Misspelled words
    ParagraphCheck grammar;   ///< Grammar and style issues

    /// @brief What each grammar issue is, in the order of grammar.issues
    std::vector<GrammarError> grammarErrors;

    /// @brief The misspelled words of the texts of its notes: each of their paragraphs, as
    ///        checked with the paragraph (spelling.current covers them)
    std::vector<NoteCheck> noteSpelling;

    /// @brief The grammar issues of the texts of its notes: each of their paragraphs is sent
    ///        on its own, as the paragraphs of the text are
    std::vector<NoteCheck> noteGrammar;

    /// @brief Data of a block, or nullptr when it has none
    static ParagraphData* find(const QTextBlock& block) {
        // The editor's blocks hold no other user data
        return static_cast<ParagraphData*>(block.userData());
    }

    /// @brief Data of a block, created when it has none
    static ParagraphData* of(QTextBlock block) {
        ParagraphData* data = find(block);
        if (!data && block.isValid()) {
            data = new ParagraphData;
            block.setUserData(data);  // the document takes ownership
        }
        return data;
    }
};

}  // namespace kalahari::editor
