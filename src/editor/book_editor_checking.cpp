// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_editor_checking.cpp
/// @brief BookEditor: spelling and grammar checks, the word read aloud

#include <kalahari/editor/book_editor.h>
#include <kalahari/core/book_dictionary.h>
#include <kalahari/editor/grammar_check_service.h>
#include <kalahari/editor/paragraph_data.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/text_highlight.h>
#include <kalahari/editor/text_issue.h>
#include <QAction>
#include <QElapsedTimer>
#include <QFont>
#include <QFontMetrics>
#include <QLocale>
#include <QMenu>
#include <QShowEvent>
#include <QStringList>
#include <QStringView>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextOption>
#include <QTimer>
#include <algorithm>
#include <optional>
#include <cstddef>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

namespace kalahari::editor {

namespace {

/// @brief The issue of @p issues at @p offset (its end included), or nullptr
const TextHighlight* issueAt(const std::vector<TextHighlight>& issues, int offset)
{
    const auto found = std::ranges::find_if(issues, [offset](const TextHighlight& issue) {
        return offset >= issue.start && offset <= issue.start + issue.length;
    });
    return found != issues.end() ? &*found : nullptr;
}

/// @brief How long a turn of the spelling check runs at most (ns): typing stays smooth
constexpr qint64 SPELL_TURN_NS = 8'000'000;

/// @brief How long after an edit the paragraphs it changed are checked (ms)
constexpr int SPELL_EDIT_DELAY_MS = 150;

/// @brief How many words the context menu offers in place of a misspelled one
constexpr int MAX_SUGGESTIONS = 5;

/// @brief How long after the last edit the paragraphs are sent to the grammar check (ms):
///        not while the writer is typing
constexpr int GRAMMAR_EDIT_DELAY_MS = 1000;

/// @brief How long after the view moved the paragraphs in it are sent (ms)
constexpr int GRAMMAR_VIEW_DELAY_MS = 250;

/// @brief How many paragraphs of an editor wait for their grammar at most
constexpr std::size_t MAX_GRAMMAR_REQUESTS = 4;

/// @brief How many paragraphs before and after the view are checked with it
constexpr int GRAMMAR_PARAGRAPHS_BEFORE_VIEW = 5;
constexpr int GRAMMAR_PARAGRAPHS_AFTER_VIEW = 20;

/// @brief How many paragraphs a search of the next issue sends at a time (the one it waits
///        for and those after it)
constexpr int ISSUE_SEARCH_AHEAD = static_cast<int>(MAX_GRAMMAR_REQUESTS);

/// @brief How wide the message of a grammar issue is on the context menu (characters)
constexpr int GRAMMAR_MESSAGE_CHARS = 60;

/// @brief A text in lines no wider than @p width in a font, broken between words where it can
QStringList textLines(const QString& text, const QFont& font, int width) {
    QStringList lines;
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(width);
        lines.append(text.mid(line.textStart(), line.textLength()).trimmed());
    }
    layout.endLayout();
    return lines;
}

/// @brief A text shown as it is on a menu: an ampersand marks no key
QString menuText(QString text) {
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

/// @brief Whether the character at @p i of a text goes on a word from the side of @p step:
///        a letter, a digit or an accent, or an apostrophe or a hyphen with a letter after it
bool continuesWord(const QString& text, qsizetype i, int step) {
    if (i < 0 || i >= text.size()) {
        return false;
    }
    const QChar c = text[i];
    if (c.isLetterOrNumber() || c.isMark() || c == QLatin1Char('_')) {
        return true;
    }
    const bool joiner = c == QLatin1Char('\'') || c == QChar(0x2019) || c == QLatin1Char('-') ||
                        c == QChar(0x2010) || c == QChar(0x2011);
    const qsizetype next = i + step;
    return joiner && next >= 0 && next < text.size() && text[next].isLetter();
}

/// @brief Whether a wave found in @p oldText marks the same word at @p start of @p newText,
///        not joined to other letters
bool sameWordAt(const QString& oldText, const TextHighlight& issue, const QString& newText,
                int start) {
    return start >= 0 && start + issue.length <= newText.size() &&
           QStringView(newText).mid(start, issue.length) ==
               QStringView(oldText).mid(issue.start, issue.length) &&
           !continuesWord(newText, start - 1, -1) &&
           !continuesWord(newText, start + issue.length, 1);
}

/// @brief Move the spelling waves of a paragraph with an edit made in it
///
/// The waves before the edit stay where they are. Those after it go with the text after
/// it, which ends the last paragraph of the edit (the same one, when the edit put in no new
/// paragraph). A wave goes only with its word as it was.
/// @param first The paragraph (with waves for its text before the edit)
/// @param offset Where the edit starts in it
/// @param charsRemoved How much text the edit removed
/// @param last The paragraph holding the end of the edit
void moveSpellingWaves(const QTextBlock& first, int offset, int charsRemoved,
                       const QTextBlock& last) {
    ParagraphCheck& check = ParagraphData::find(first)->spelling;
    const QString old = std::exchange(check.text, QString());
    const std::vector<TextHighlight> issues = std::exchange(check.issues, {});
    const QString text = first.text();
    const QString lastText = last == first ? text : last.text();

    // The text after the edit is the paragraph's old end, unless the edit removed it
    const int removedEnd = offset + charsRemoved;
    const bool endKept = removedEnd <= old.size();
    const int shift = static_cast<int>(lastText.size() - old.size());

    std::vector<TextHighlight> lastWaves;
    for (const TextHighlight& issue : issues) {
        if (issue.start + issue.length <= offset) {
            if (sameWordAt(old, issue, text, issue.start)) {
                check.issues.push_back(issue);
            }
        } else if (issue.start >= removedEnd && endKept) {
            const TextHighlight moved{issue.start + shift, issue.length, issue.kind};
            if (sameWordAt(old, issue, lastText, moved.start)) {
                (last == first ? check.issues : lastWaves).push_back(moved);
            }
        }
    }
    if (!check.issues.empty()) {
        check.text = text;
    }
    if (!lastWaves.empty()) {
        ParagraphCheck& lastCheck = ParagraphData::of(last)->spelling;
        if (lastCheck.issuesFor(lastText) == nullptr) {
            lastCheck.issues = std::move(lastWaves);
            lastCheck.text = lastText;
            lastCheck.current = false;
        }
    }
}

/// @brief Move the grammar waves of a paragraph with an edit made in it
///
/// The waves before the edit stay where they are, those after it go with the text after
/// it (see moveSpellingWaves()). A wave the edit touches goes: its issue may be gone.
/// @param first The paragraph (with waves for its text before the edit)
/// @param offset Where the edit starts in it
/// @param charsRemoved How much text the edit removed
/// @param last The paragraph holding the end of the edit
void moveGrammarWaves(const QTextBlock& first, int offset, int charsRemoved,
                      const QTextBlock& last) {
    ParagraphData* paragraphData = ParagraphData::find(first);
    ParagraphCheck& check = paragraphData->grammar;
    const QString old = std::exchange(check.text, QString());
    const std::vector<TextHighlight> issues = std::exchange(check.issues, {});
    const std::vector<GrammarError> errors = std::exchange(paragraphData->grammarErrors, {});
    const QString text = first.text();
    const QString lastText = last == first ? text : last.text();

    const int removedEnd = offset + charsRemoved;
    const bool endKept = removedEnd <= old.size();
    const int shift = static_cast<int>(lastText.size() - old.size());

    std::vector<TextHighlight> lastWaves;
    std::vector<GrammarError> lastErrors;
    for (std::size_t i = 0; i < issues.size() && i < errors.size(); ++i) {
        const TextHighlight& issue = issues[i];
        if (issue.start + issue.length < offset) {
            check.issues.push_back(issue);
            paragraphData->grammarErrors.push_back(errors[i]);
        } else if (issue.start > removedEnd && endKept) {
            const TextHighlight moved{issue.start + shift, issue.length, issue.kind};
            GrammarError error = errors[i];
            error.startPos = moved.start;
            if (last == first) {
                check.issues.push_back(moved);
                paragraphData->grammarErrors.push_back(std::move(error));
            } else {
                lastWaves.push_back(moved);
                lastErrors.push_back(std::move(error));
            }
        }
    }
    if (!check.issues.empty()) {
        check.text = text;
    }
    if (!lastWaves.empty()) {
        ParagraphData* lastData = ParagraphData::of(last);
        if (lastData->grammar.issuesFor(lastText) == nullptr) {
            lastData->grammar.issues = std::move(lastWaves);
            lastData->grammar.text = lastText;
            lastData->grammar.current = false;
            lastData->grammarErrors = std::move(lastErrors);
        }
    }
}

/// @brief How the book's dictionary checks a text
struct BookChecking {
    const core::BookDictionary* dictionary = nullptr;  ///< Its words are right; nullptr: none
    bool rules = true;  ///< The places breaking its rules are marked
};

/// @brief The spelling issues of a text (one paragraph): its misspelled words, but those the
///        book's dictionary takes for right, and the places breaking a spelling rule of the
///        book, but those ignored (Ignore All); a place breaking a rule is marked in place of
///        the misspellings it covers. In the order of the text.
/// @param book The book's dictionary, and whether the places breaking its rules are marked
std::vector<TextHighlight> spellingIssuesOf(const SpellCheckService& service,
                                            const BookChecking& book, const QString& text) {
    std::vector<TextHighlight> issues;
    QList<core::RuleDeviation> deviations;
    SpellCheckService::WordFilter alsoRight;
    if (const core::BookDictionary* dictionary = book.dictionary; dictionary != nullptr) {
        alsoRight = [dictionary](const QString& word) { return dictionary->isRight(word); };
        if (book.rules) {
            deviations = dictionary->deviations(text);
        }
        deviations.removeIf([&service](const core::RuleDeviation& deviation) {
            return service.isIgnored(deviation.text);
        });
    }
    for (const SpellErrorInfo& error : service.checkParagraph(text, alsoRight)) {
        const bool covered =
            std::ranges::any_of(deviations, [&error](const core::RuleDeviation& deviation) {
                return error.startPos < deviation.start + deviation.length &&
                       deviation.start < error.startPos + error.length;
            });
        if (!covered) {
            issues.push_back({.start = error.startPos,
                              .length = error.length,
                              .kind = HighlightKind::Spelling});
        }
    }
    for (const core::RuleDeviation& deviation : std::as_const(deviations)) {
        issues.push_back({.start = static_cast<int>(deviation.start),
                          .length = static_cast<int>(deviation.length),
                          .kind = HighlightKind::BookRule});
    }
    std::ranges::stable_sort(issues, std::ranges::less{}, &TextHighlight::start);
    return issues;
}

/// @brief The kind of issue a spelling wave marks
TextIssueKind spellingKindOf(const TextHighlight& wave) {
    return wave.kind == HighlightKind::BookRule ? TextIssueKind::BookRule : TextIssueKind::Spelling;
}

/// @brief A paragraph of a note's text, with the paragraph holding the note's number
struct NoteLine {
    QString id;      ///< The note's
    int number = 0;  ///< Where the note's number is in the paragraph
    int line = 0;    ///< The paragraph, in the note's text
    int start = 0;   ///< Where it starts in the note's text (noteText())
    QString text;    ///< Its text
};

/// @brief Add the paragraphs of a note's text
/// @param number Where the note's number is in its paragraph
void addNoteLines(const NotePlace& place, int number, std::vector<NoteLine>& lines) {
    int start = 0;
    int line = 0;
    for (QString& text : noteText(place.note.content).split(QLatin1Char('\n'))) {
        const int length = static_cast<int>(text.size());
        lines.push_back({place.note.id, number, line++, start, std::move(text)});
        start += length + 1;
    }
}

/// @brief The paragraphs of the texts of the notes of a paragraph, in text order
std::vector<NoteLine> noteLinesOf(const QTextBlock& block) {
    std::vector<NoteLine> lines;
    for (const NotePlace& place : notesIn(block)) {
        addNoteLines(place, place.position - block.position(), lines);
    }
    return lines;
}

/// @brief Whether a paragraph's data holds a result of a check for every paragraph of the
///        texts of its notes
bool notesChecked(const std::vector<NoteCheck>& checks, const std::vector<NoteLine>& lines) {
    return std::all_of(lines.begin(), lines.end(), [&checks](const NoteLine& line) {
        return NoteCheck::find(checks, line.id, line.line, line.text) != nullptr;
    });
}

/// @brief Whether the grammar of a paragraph of a note's text is known: checked, or nothing
///        to check
bool isNoteGrammarKnown(const ParagraphData* paragraphData, const NoteLine& line) {
    if (line.text.trimmed().isEmpty()) {
        return true;
    }
    const NoteCheck* check =
        paragraphData != nullptr
            ? NoteCheck::find(paragraphData->noteGrammar, line.id, line.line, line.text)
            : nullptr;
    return check != nullptr && check->current;
}

/// @brief Whether new results of the notes' texts change their waves
bool noteWavesChanged(const std::vector<NoteCheck>& before, const std::vector<NoteCheck>& after) {
    return std::any_of(after.begin(), after.end(), [&before](const NoteCheck& check) {
        const NoteCheck* old = NoteCheck::find(before, check.id, check.line, check.text);
        return old != nullptr ? old->issues != check.issues : !check.issues.empty();
    });
}

/// @brief Check the spelling of the paragraphs of the texts of a paragraph's notes
/// @param book The book's dictionary (spellingIssuesOf())
/// @param checks The paragraph's results (ParagraphData::noteSpelling): replaced by these
/// @param keep Results still for a paragraph's text are kept, not made again
/// @return Whether their waves changed
bool checkNoteSpelling(const SpellCheckService& service, const BookChecking& book,
                       const QTextBlock& block, std::vector<NoteCheck>& checks, bool keep) {
    std::vector<NoteCheck> notes;
    for (NoteLine& line : noteLinesOf(block)) {
        if (const NoteCheck* known = NoteCheck::find(checks, line.id, line.line, line.text);
            keep && known != nullptr) {
            notes.push_back(*known);
            continue;
        }
        NoteCheck& note = notes.emplace_back();
        note.issues = spellingIssuesOf(service, book, line.text);
        note.id = std::move(line.id);
        note.line = line.line;
        note.text = std::move(line.text);
    }
    const bool changed = noteWavesChanged(checks, notes);
    checks = std::move(notes);
    return changed;
}

/// @brief Whether the sentence holding a place of a text is unfinished: no end of a
///        sentence follows it
bool sentenceUnfinished(const QString& text, int from) {
    for (qsizetype i = from; i < text.size(); ++i) {
        const QChar c = text[i];
        if (c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?') ||
            c == QChar(0x2026)) {
            return false;
        }
    }
    return true;
}

/// @brief The grammar issues found in a text, but those that apply only to a finished
///        sentence while their sentence is unfinished: their waves, and what each is
std::pair<std::vector<TextHighlight>, std::vector<GrammarError>> grammarIssuesOf(
    const QString& text, const QList<GrammarError>& errors) {
    std::vector<TextHighlight> issues;
    std::vector<GrammarError> kept;
    for (const GrammarError& error : errors) {
        if (error.ignoreForIncompleteSentence &&
            sentenceUnfinished(text, error.startPos + error.length)) {
            continue;
        }
        issues.push_back({.start = error.startPos,
                          .length = error.length,
                          .kind = HighlightKind::Grammar});
        kept.push_back(error);
    }
    return {std::move(issues), std::move(kept)};
}

/// @brief Keep the grammar issues found in a paragraph (grammarIssuesOf())
/// @return Whether its waves changed
bool keepGrammarIssues(const QTextBlock& block, const QList<GrammarError>& errors) {
    ParagraphData* paragraphData = ParagraphData::of(block);
    const QString text = block.text();
    auto [issues, kept] = grammarIssuesOf(text, errors);
    const std::vector<TextHighlight>* before = paragraphData->grammar.issuesFor(text);
    const bool changed = before != nullptr ? *before != issues : !issues.empty();
    paragraphData->grammar.issues = std::move(issues);
    paragraphData->grammar.text = paragraphData->grammar.issues.empty() ? QString() : text;
    paragraphData->grammar.current = true;
    paragraphData->grammarErrors = std::move(kept);
    return changed;
}

/// @brief Keep the grammar issues found in a paragraph of a note's text (grammarIssuesOf()),
///        in place of the paragraph's results before
/// @return Whether its waves changed
bool keepNoteGrammarIssues(const QTextBlock& block, const NoteLine& line,
                           const QList<GrammarError>& errors) {
    std::vector<NoteCheck>& checks = ParagraphData::of(block)->noteGrammar;
    NoteCheck check;
    check.id = line.id;
    check.line = line.line;
    check.text = line.text;
    std::tie(check.issues, check.errors) = grammarIssuesOf(line.text, errors);
    const NoteCheck* before = NoteCheck::find(checks, line.id, line.line, line.text);
    const bool changed = before != nullptr ? before->issues != check.issues
                                           : !check.issues.empty();
    std::erase_if(checks, [&line](const NoteCheck& old) {
        return old.line == line.line && old.id == line.id;
    });
    checks.push_back(std::move(check));
    return changed;
}

/// @brief The paragraph of a note's text a place of the text is in (nullptr: none)
const NoteLine* noteLineAt(const std::vector<NoteLine>& lines, int position) {
    for (const NoteLine& line : lines) {
        if (position >= line.start && position <= line.start + line.text.size()) {
            return &line;
        }
    }
    return nullptr;
}

}  // anonymous namespace

// =============================================================================
// Spelling
// =============================================================================

void BookEditor::setSpellCheckService(SpellCheckService* service)
{
    if (m_spellCheckService == service) {
        return;
    }
    if (m_spellCheckService != nullptr) {
        disconnect(m_spellCheckService, nullptr, this, nullptr);
    }
    m_spellCheckService = service;
    if (m_spellCheckService != nullptr) {
        connect(m_spellCheckService, &SpellCheckService::wordsChanged, this,
                &BookEditor::onSpellingWordsChanged);
        connect(m_spellCheckService, &QObject::destroyed, this,
                &BookEditor::onSpellCheckServiceDestroyed);
        connect(this, &BookEditor::cursorPositionChanged, this,
                &BookEditor::onSpellingCursorMoved, Qt::UniqueConnection);
    }
    onSpellingWordsChanged();
}

SpellCheckService* BookEditor::spellCheckService() const
{
    return m_spellCheckService;
}

void BookEditor::setBookDictionary(const core::BookDictionary* dictionary, bool editable)
{
    m_bookDictionaryEditable = editable;
    if (m_bookDictionary == dictionary) {
        return;
    }
    m_bookDictionary = dictionary;
    requestSpellCheck();
}

void BookEditor::setBookRulesMarked(bool marked)
{
    if (m_bookRulesMarked == marked) {
        return;
    }
    m_bookRulesMarked = marked;
    requestSpellCheck();
}

void BookEditor::requestSpellCheck()
{
    if (!m_textBuffer || m_spellCheckService == nullptr || !m_spellCheckService->isActive()) {
        return;
    }
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        if (ParagraphData* paragraphData = ParagraphData::find(block)) {
            paragraphData->spelling.current = false;
        }
    }
    noteIssuesChanged();
    scheduleSpellCheck(0);
}

bool BookEditor::isSpellCheckPending() const
{
    return m_spellTimer != nullptr && m_spellTimer->isActive();
}

BookEditor::IssueSearch BookEditor::goToNextIssue()
{
    if (m_issueSearch.active) {
        endIssueSearch(IssueSearch::Cancelled);  // this one instead
    }
    const bool spelling = m_spellCheckService != nullptr && m_spellCheckService->isActive();
    if (!m_textBuffer || (!spelling && m_grammarCheckService == nullptr)) {
        return IssueSearch::NotFound;
    }

    // The writer has stopped typing: the word typed last counts too
    if (spelling && m_spellTyping >= 0) {
        endSpellingTyping();
    }

    // With a selection (the issue gone to before), those that start after its start, so an
    // issue in a longer one is not passed over; else from the cursor: the issue the cursor
    // is in, or right after, comes first
    m_issueSearch = IssueSearchState{};
    m_issueSearch.afterSelection = hasSelection();
    m_issueSearch.from =
        m_issueSearch.afterSelection ? m_selection.normalized().start : m_cursorPosition;
    m_issueSearch.paragraph = m_issueSearch.from.paragraph;
    m_issueSearch.cursor = m_cursorPosition;
    m_issueSearch.selection = m_selection;
    if (m_issueSearch.afterSelection) {
        m_issueSearch.fromNote = m_noteSelection;
    }
    const IssueSearch result = continueIssueSearch();
    m_issueSearch.active = result == IssueSearch::Searching;
    return result;
}

BookEditor::IssueSearch BookEditor::continueIssueSearch()
{
    const bool spelling = m_spellCheckService != nullptr && m_spellCheckService->isActive();
    const bool afterSelection = m_issueSearch.afterSelection;
    const CursorPosition from = m_issueSearch.from;

    // An issue's place in the order of its paragraph: its start; one of a note's text right
    // after the note's number, in the order of the note's text
    using Place = std::pair<int, int>;
    const auto ownPlace = [](int start) { return Place{2 * start, 0}; };
    const auto notePlace = [](int number, int start) { return Place{(2 * number) + 1, start}; };
    const Place fromPlace = m_issueSearch.fromNote
                                ? notePlace(from.offset, m_issueSearch.fromNote->start)
                                : ownPlace(from.offset);
    struct Candidate {
        Place place;
        NoteRange range;  ///< Its id empty: in the paragraph's own text
    };

    // Round the text from the paragraph of the cursor back to it, checking the spelling of
    // the paragraphs not checked yet on the way, and waiting for their grammar
    bool wavesChanged = false;
    const int count = m_textBuffer->blockCount();
    QTextBlock block = m_textBuffer->findBlockByNumber(m_issueSearch.paragraph);
    for (int& visited = m_issueSearch.visited; visited <= count; ++visited) {
        if (!block.isValid()) {
            block = m_textBuffer->begin();
        }
        const ParagraphData* known = ParagraphData::find(block);
        if (spelling && (known == nullptr || !known->spelling.current)) {
            wavesChanged = checkSpelling(block) || wavesChanged;
        }

        // A paragraph whose grammar is not known is sent with a few after it; the search
        // waits for its answer while the server is asked
        if (m_grammarCheckService != nullptr && m_grammarCheckService->isActive() &&
            !isGrammarKnown(block)) {
            const int number = block.blockNumber();
            for (int ahead = number; ahead < std::min(count, number + ISSUE_SEARCH_AHEAD);
                 ++ahead) {
                if (!sendGrammarCheck(ahead)) {
                    break;
                }
            }
            if (!isGrammarKnown(block) && !m_grammarRequests.empty()) {
                m_issueSearch.paragraph = number;
                if (wavesChanged) {
                    update();
                }
                return IssueSearch::Searching;
            }
        }

        // The first one wanted: the paragraph of the cursor after it first, before it when
        // back at it; the misspelled words before the grammar issues at the same place. After
        // the cursor: the issues it is in or right after, those of the notes whose numbers
        // follow it
        std::optional<Candidate> first;
        const auto consider = [&first, visited, count](Candidate candidate, bool after) {
            bool wanted = true;
            if (visited == 0) {
                wanted = after;
            } else if (visited == count) {
                wanted = !after;
            }
            if (wanted && (!first || candidate.place < first->place)) {
                first = std::move(candidate);
            }
        };
        const auto considerOwn = [&](const std::vector<TextHighlight>& issues) {
            for (const TextHighlight& issue : issues) {
                const Place place = ownPlace(issue.start);
                consider({place, {QString(), issue.start, issue.length}},
                         afterSelection ? place > fromPlace
                                        : issue.start + issue.length >= from.offset);
            }
        };
        const auto considerNote = [&](const NoteLine& line, const NoteCheck* check) {
            if (check == nullptr) {
                return;
            }
            for (const TextHighlight& issue : check->issues) {
                const int start = line.start + issue.start;
                const Place place = notePlace(line.number, start);
                consider({place, {line.id, start, issue.length}},
                         afterSelection ? place > fromPlace : line.number >= from.offset);
            }
        };
        if (const ParagraphData* paragraphData = ParagraphData::find(block)) {
            if (spelling) {
                considerOwn(paragraphData->spelling.issues);
            }
            if (const auto* grammarIssues = paragraphData->grammar.issuesFor(block.text())) {
                considerOwn(*grammarIssues);
            }
            if (!paragraphData->noteSpelling.empty() || !paragraphData->noteGrammar.empty()) {
                for (const NoteLine& line : noteLinesOf(block)) {
                    if (spelling) {
                        considerNote(line, NoteCheck::find(paragraphData->noteSpelling, line.id,
                                                           line.line, line.text));
                    }
                    considerNote(line, NoteCheck::find(paragraphData->noteGrammar, line.id,
                                                       line.line, line.text));
                }
            }
        }
        if (first) {
            if (first->range.id.isEmpty()) {
                const int paragraph = block.blockNumber();
                const NoteRange& range = first->range;
                selectRange({paragraph, range.start}, {paragraph, range.start + range.length});
            } else {
                selectNoteRange(first->range);
            }
            return IssueSearch::Found;
        }
        block = block.next();
    }
    if (wavesChanged) {
        update();
    }
    return IssueSearch::NotFound;
}

void BookEditor::resumeIssueSearch()
{
    if (!m_issueSearch.active || !m_textBuffer) {
        return;
    }
    // The writer went elsewhere meanwhile
    const SelectionRange& before = m_issueSearch.selection;
    if (m_cursorPosition != m_issueSearch.cursor || m_selection.start != before.start ||
        m_selection.end != before.end) {
        endIssueSearch(IssueSearch::Cancelled);
        return;
    }
    const IssueSearch result = continueIssueSearch();
    if (result != IssueSearch::Searching) {
        endIssueSearch(result);
    }
}

void BookEditor::endIssueSearch(IssueSearch result)
{
    if (m_issueSearch.active) {
        m_issueSearch.active = false;
        emit issueSearchFinished(result);
    }
}

void BookEditor::onSpellingWordsChanged()
{
    if (m_spellCheckService == nullptr || !m_spellCheckService->isActive()) {
        clearSpelling();
        return;
    }
    requestSpellCheck();
}

void BookEditor::onSpellCheckServiceDestroyed()
{
    m_spellCheckService = nullptr;
    clearSpelling();
}

void BookEditor::clearSpelling()
{
    if (m_spellTimer != nullptr) {
        m_spellTimer->stop();
    }
    m_spellTyping = -1;
    if (!m_textBuffer) {
        return;
    }
    bool hadWaves = false;
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        if (ParagraphData* paragraphData = ParagraphData::find(block)) {
            hadWaves = hadWaves || !paragraphData->spelling.issues.empty() ||
                       !paragraphData->noteSpelling.empty();
            paragraphData->spelling = ParagraphCheck{};
            paragraphData->noteSpelling.clear();
        }
    }
    noteIssuesChanged();
    if (hadWaves) {
        update();
    }
}

void BookEditor::scheduleSpellCheck(int delayMs)
{
    if (m_spellTimer == nullptr) {
        m_spellTimer = new QTimer(this);
        m_spellTimer->setSingleShot(true);
        connect(m_spellTimer, &QTimer::timeout, this, &BookEditor::runSpellCheck);
    }
    // A turn due sooner stays
    if (!m_spellTimer->isActive() || m_spellTimer->remainingTime() > delayMs) {
        m_spellTimer->start(delayMs);
    }
}

void BookEditor::runSpellCheck()
{
    if (!m_textBuffer || m_spellCheckService == nullptr || !m_spellCheckService->isActive() ||
        !isVisible()) {
        return;  // a hidden editor goes on when it is shown (showEvent())
    }

    QElapsedTimer clock;
    clock.start();
    const auto turnIsOver = [&clock]() { return clock.nsecsElapsed() >= SPELL_TURN_NS; };
    const auto due = [](const QTextBlock& block) {
        const ParagraphData* paragraphData = ParagraphData::find(block);
        return paragraphData == nullptr || !paragraphData->spelling.current;
    };

    // The paragraphs in view first; the view is painted again when their waves change
    const auto [firstInView, lastInView] =
        m_viewportManager ? m_viewportManager->visibleRange() : std::pair<size_t, size_t>{0, 0};
    const auto inView = [first = firstInView, last = lastInView](int number) {
        return static_cast<size_t>(number) >= first && static_cast<size_t>(number) <= last;
    };
    bool viewChanged = false;
    const auto endTurn = [this, &viewChanged](bool more) {
        if (viewChanged) {
            update();
        }
        if (more) {
            scheduleSpellCheck(0);
        }
    };
    for (QTextBlock block = m_textBuffer->findBlockByNumber(static_cast<int>(firstInView));
         block.isValid() && inView(block.blockNumber()); block = block.next()) {
        if (due(block)) {
            viewChanged = checkSpelling(block) || viewChanged;
            if (turnIsOver()) {
                endTurn(true);
                return;
            }
        }
    }

    // Then the others, round the text from where the last turn stopped
    const int count = m_textBuffer->blockCount();
    QTextBlock block = m_textBuffer->findBlockByNumber(m_spellNext < count ? m_spellNext : 0);
    for (int visited = 0; visited < count; ++visited) {
        if (!block.isValid()) {
            block = m_textBuffer->begin();
        }
        if (due(block)) {
            const int number = block.blockNumber();
            if (checkSpelling(block) && inView(number)) {
                viewChanged = true;
            }
            if (turnIsOver()) {
                m_spellNext = number + 1;
                endTurn(true);
                return;
            }
        }
        block = block.next();
    }
    m_spellNext = 0;
    endTurn(false);
}

bool BookEditor::isCheckedParagraph(const QTextBlock& block) const
{
    return blockObject(block) == TextObject::None &&
           m_paragraphStyles.isChecked(paragraphStyleOf(block.blockFormat()));
}

void BookEditor::checkParagraphsAgain(const std::vector<QTextBlock>& blocks)
{
    bool hadWaves = false;
    for (const QTextBlock& block : blocks) {
        if (ParagraphData* paragraphData = ParagraphData::find(block)) {
            hadWaves = hadWaves || !paragraphData->spelling.issues.empty() ||
                       !paragraphData->grammar.issues.empty() ||
                       !paragraphData->noteSpelling.empty() ||
                       !paragraphData->noteGrammar.empty();
            paragraphData->spelling = ParagraphCheck{};
            paragraphData->grammar = ParagraphCheck{};
            paragraphData->grammarErrors.clear();
            paragraphData->noteSpelling.clear();
            paragraphData->noteGrammar.clear();
        }
    }
    noteIssuesChanged();
    if (m_spellCheckService != nullptr) {
        scheduleSpellCheck(0);
    }
    if (m_grammarCheckService != nullptr) {
        scheduleGrammarCheck(0);
    }
    if (hadWaves) {
        update();
    }
}

bool BookEditor::checkSpelling(const QTextBlock& block)
{
    // A paragraph in a style without checking (Code) has no misspellings
    const QString text = isCheckedParagraph(block) ? block.text() : QString();

    // The word being typed gets its wave when the cursor leaves it
    const int typed = m_spellTyping >= 0 && block.contains(m_spellTyping)
                          ? m_spellTyping - block.position()
                          : -1;
    const BookChecking book{.dictionary = m_bookDictionary, .rules = m_bookRulesMarked};
    std::vector<TextHighlight> issues = spellingIssuesOf(*m_spellCheckService, book, text);
    std::erase_if(issues, [typed](const TextHighlight& issue) {
        return typed > issue.start && typed <= issue.start + issue.length;
    });

    ParagraphData* paragraphData = ParagraphData::of(block);
    ParagraphCheck& check = paragraphData->spelling;
    const std::vector<TextHighlight>* before = check.issuesFor(text);
    bool changed = before != nullptr ? *before != issues : !issues.empty();
    check.issues = std::move(issues);
    check.text = check.issues.empty() ? QString() : text;
    check.current = true;

    // The texts of its notes, a paragraph at a time, also in a paragraph not checked: they
    // are written in their frame, so no word of them is being typed
    changed = checkNoteSpelling(*m_spellCheckService, book, block, paragraphData->noteSpelling,
                                false) ||
              changed;
    noteIssuesChanged();  // one more paragraph checked, at least
    return changed;
}

void BookEditor::adjustSpellingToEdit(int from, int charsRemoved, int charsAdded)
{
    if (!m_textBuffer || m_spellCheckService == nullptr) {
        return;
    }
    const QTextBlock first = m_textBuffer->findBlock(from);
    const QTextBlock last = m_textBuffer->findBlock(from + charsAdded);
    if (!first.isValid()) {
        return;
    }

    // An edit not made by typing (a paste, undo, a replacement) ends the typing
    if (!m_spellTypingEdit && m_spellTyping >= 0) {
        endSpellingTyping();
    }

    for (QTextBlock block = first; block.isValid(); block = block.next()) {
        ParagraphData* paragraphData = ParagraphData::find(block);  // none: a new paragraph, due
        if (paragraphData != nullptr) {
            ParagraphCheck& check = paragraphData->spelling;
            const QString text = block.text();
            // A paragraph with the text its waves were found in keeps them (a new format)
            if (check.issues.empty() || check.text != text) {
                check.current = false;
                if (block == first && !check.issues.empty()) {
                    moveSpellingWaves(block, from - block.position(), charsRemoved, last);
                } else {
                    check.issues.clear();
                    check.text.clear();
                }
            }
            // A note given a new text (the format of its number changed): its paragraphs that
            // changed are checked at once, so the words of the others keep their waves and
            // the list of the issues goes on from them (a fix chosen)
            if (!notesChecked(paragraphData->noteSpelling, noteLinesOf(block))) {
                if (m_spellCheckService->isActive()) {
                    const BookChecking book{.dictionary = m_bookDictionary,
                                            .rules = m_bookRulesMarked};
                    checkNoteSpelling(*m_spellCheckService, book, block,
                                      paragraphData->noteSpelling, true);
                } else {
                    check.current = false;
                }
            }
        }
        if (block == last) {
            break;
        }
    }
    noteIssuesChanged();  // the waves moved with the text, or went
    scheduleSpellCheck(SPELL_EDIT_DELAY_MS);
}

void BookEditor::onSpellingCursorMoved()
{
    if (m_spellTypingEdit || m_spellTyping < 0 || !m_textBuffer) {
        return;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid() || block.position() + m_cursorPosition.offset != m_spellTyping) {
        endSpellingTyping();
    }
}

void BookEditor::noteSpellingTyping()
{
    if (!m_textBuffer || m_spellCheckService == nullptr) {
        return;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    m_spellTyping = block.isValid() ? block.position() + m_cursorPosition.offset : -1;
}

void BookEditor::endSpellingTyping()
{
    const QTextBlock typed = m_textBuffer ? m_textBuffer->findBlock(m_spellTyping) : QTextBlock();
    m_spellTyping = -1;
    if (ParagraphData* paragraphData = ParagraphData::find(typed)) {
        paragraphData->spelling.current = false;
        noteIssuesChanged();
    }
    if (m_spellCheckService != nullptr && m_spellCheckService->isActive()) {
        scheduleSpellCheck(0);
    }
}

void BookEditor::checkSpellingAtCursor()
{
    if (!m_textBuffer || m_spellCheckService == nullptr || !m_spellCheckService->isActive()) {
        return;
    }
    if (m_spellTyping >= 0) {
        endSpellingTyping();
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    const ParagraphData* known = ParagraphData::find(block);
    if (block.isValid() && (known == nullptr || !known->spelling.current) &&
        checkSpelling(block)) {
        update();
    }
}

void BookEditor::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_spellCheckService != nullptr && m_spellCheckService->isActive()) {
        scheduleSpellCheck(0);
    }
    if (m_grammarCheckService != nullptr && m_grammarCheckService->isActive()) {
        scheduleGrammarCheck(0);
    }
}

std::tuple<QString, int, int> BookEditor::getMisspelledWordAt(int paraIndex, int offset,
                                                              HighlightKind* kind) const
{
    const QTextBlock block = m_textBuffer ? m_textBuffer->findBlockByNumber(paraIndex)
                                          : QTextBlock();
    const ParagraphData* paragraphData = ParagraphData::find(block);
    const QString text = block.text();
    const auto* issues = paragraphData != nullptr ? paragraphData->spelling.issuesFor(text)
                                                  : nullptr;
    const TextHighlight* issue = issues != nullptr ? issueAt(*issues, offset) : nullptr;
    if (issue == nullptr) {
        return {QString(), 0, 0};
    }
    if (kind != nullptr) {
        *kind = issue->kind;
    }
    return {text.mid(issue->start, issue->length), issue->start, issue->start + issue->length};
}

std::tuple<QString, int, int> BookEditor::getMisspelledNoteWordAt(const NoteRange& place,
                                                                  HighlightKind* kind) const
{
    const std::optional<NotePlace> note = noteOf(place.id);
    if (!note) {
        return {};
    }
    const ParagraphData* paragraphData =
        ParagraphData::find(m_textBuffer->findBlock(note->position));
    if (paragraphData == nullptr || paragraphData->noteSpelling.empty()) {
        return {QString(), 0, 0};
    }
    std::vector<NoteLine> lines;
    addNoteLines(*note, 0, lines);
    const NoteLine* line = noteLineAt(lines, place.start);
    const NoteCheck* check =
        line != nullptr
            ? NoteCheck::find(paragraphData->noteSpelling, line->id, line->line, line->text)
            : nullptr;
    if (check == nullptr) {
        return {QString(), 0, 0};
    }
    const TextHighlight* issue = issueAt(check->issues, place.start - line->start);
    if (issue == nullptr) {
        return {QString(), 0, 0};
    }
    if (kind != nullptr) {
        *kind = issue->kind;
    }
    const int start = line->start + issue->start;
    return {line->text.mid(issue->start, issue->length), start, start + issue->length};
}

void BookEditor::addSpellingActions(QMenu& menu, const QString& word,
                                    const std::function<void(const QString&)>& replace)
{
    // The words to put in its place, in bold as in other word processors. They get free
    // access keys as the menu shows (gui::giveFreeAccessKeys()); the other items have their own.
    // A name of the book written small: the name first
    QStringList suggestions = m_spellCheckService != nullptr
                                  ? m_spellCheckService->suggestions(word, MAX_SUGGESTIONS)
                                  : QStringList();
    if (m_bookDictionary != nullptr) {
        if (const QString name = m_bookDictionary->nameSpelling(word); !name.isEmpty()) {
            suggestions.removeAll(name);
            suggestions.prepend(name);
            while (suggestions.size() > MAX_SUGGESTIONS) {
                suggestions.removeLast();
            }
        }
    }
    if (suggestions.isEmpty()) {
        menu.addAction(tr("(No suggestions)"))->setEnabled(false);
    }
    for (const QString& suggestion : suggestions) {
        QAction* action = menu.addAction(menuText(suggestion));
        QFont bold = action->font();
        bold.setBold(true);
        action->setFont(bold);
        disableWhileReadOnly(action);  // a read-only text keeps its words
        connect(action, &QAction::triggered, this,
                [replace, suggestion]() { replace(suggestion); });
    }
    menu.addSeparator();

    // The word is right from now on: in every document (Ignore All until the application
    // closes, Add to Your Words for good), or in the book (Add to Book Dictionary, in its
    // chapters). Greyed out in a read-only text as well
    QAction* ignore = menu.addAction(tr("&Ignore All"));
    disableWhileReadOnly(ignore);
    connect(ignore, &QAction::triggered, this, [this, word]() {
        if (m_spellCheckService != nullptr) {
            m_spellCheckService->ignoreWord(word);
        }
    });
    if (m_bookDictionary != nullptr && m_bookDictionaryEditable) {
        //: In Polish: Dodaj do &słownika książki
        QAction* book = menu.addAction(tr("Add to &Book Dictionary"));
        book->setToolTip(tr("The word is right in this book only"));
        disableWhileReadOnly(book);
        connect(book, &QAction::triggered, this,
                [this, word]() { emit addToBookDictionaryAsked(word); });
    }
    //: The user's own dictionary, which every book has. In Polish: &Dodaj do Twoich słów
    //: (wszystkie książki)
    QAction* add = menu.addAction(tr("Add to Your &Words (All Books)"));
    disableWhileReadOnly(add);
    connect(add, &QAction::triggered, this, [this, word]() {
        if (m_spellCheckService != nullptr) {
            m_spellCheckService->addToUserDictionary(word);
        }
    });
    // The word as a name of the book, with its forms: the window of the name opens with it
    if (m_bookDictionary != nullptr && m_bookDictionaryEditable) {
        //: Opens the window of a new name of the book's dictionary, with the word in it. In
        //: Polish: Dodaj &imię lub nazwę…
        QAction* name = menu.addAction(tr("Add Nam&e..."));
        name->setToolTip(tr("The word is a name of the book: right in all its forms"));
        disableWhileReadOnly(name);
        connect(name, &QAction::triggered, this, [this, word]() { emit addNameAsked(word); });
    }
    // The word as the name of a new card, which takes its forms to the book's dictionary; the
    // commands of the cards follow it (addCardMenus())
    menu.addSeparator();
    addCreateCardMenu(menu, word);
}

std::optional<core::RuleDeviation> BookEditor::bookRuleDeviation(const QString& text) const
{
    if (m_bookDictionary == nullptr) {
        return std::nullopt;
    }
    for (const core::RuleDeviation& deviation : m_bookDictionary->deviations(text)) {
        if (deviation.start == 0 && deviation.length == text.size()) {
            return deviation;
        }
    }
    return std::nullopt;
}

void BookEditor::addBookRuleActions(QMenu& menu, const core::RuleDeviation& deviation,
                                    const std::function<void(const QString&)>& replace)
{
    // The rule's form, in bold as the words for a misspelled one; then the rule and the
    // author's note on it, lines that only tell
    QAction* fix = menu.addAction(menuText(deviation.right));
    QFont bold = fix->font();
    bold.setBold(true);
    fix->setFont(bold);
    disableWhileReadOnly(fix);
    connect(fix, &QAction::triggered, this,
            [replace, right = deviation.right]() { replace(right); });
    const core::SpellingRule* rule = m_bookDictionary != nullptr
                                         ? m_bookDictionary->findRule(deviation.ruleId)
                                         : nullptr;
    if (rule != nullptr) {
        //: %1 is the right spelling in quotes, as the rule has it: Book rule: write "Smok"
        menu.addAction(menuText(tr("Book rule: write %1").arg(QLocale().quoteString(rule->right))))
            ->setEnabled(false);
        if (!rule->note.trimmed().isEmpty()) {
            const QFontMetrics metrics(menu.font());
            for (const QString& line :
                 textLines(rule->note.simplified(), menu.font(),
                           metrics.averageCharWidth() * GRAMMAR_MESSAGE_CHARS)) {
                menu.addAction(menuText(line))->setEnabled(false);
            }
        }
    }
    menu.addSeparator();

    // Not marked until the application closes, or the rule is gone
    QAction* ignore = menu.addAction(tr("&Ignore All"));
    disableWhileReadOnly(ignore);
    connect(ignore, &QAction::triggered, this, [this, text = deviation.text]() {
        if (m_spellCheckService != nullptr) {
            m_spellCheckService->ignoreWord(text);
        }
    });
    if (m_bookDictionaryEditable && rule != nullptr) {
        //: Opens the window of the spelling rule of the book. In Polish: Zmień z&asadę…
        QAction* edit = menu.addAction(tr("&Edit Rule..."));
        disableWhileReadOnly(edit);
        connect(edit, &QAction::triggered, this,
                [this, ruleId = rule->id]() { emit editBookRuleAsked(ruleId); });
        QAction* remove = menu.addAction(tr("&Remove Rule"));
        remove->setToolTip(tr("The book's dictionary no longer has this rule"));
        disableWhileReadOnly(remove);
        connect(remove, &QAction::triggered, this,
                [this, ruleId = rule->id]() { emit removeBookRuleAsked(ruleId); });
    }
    menu.addSeparator();
}

void BookEditor::replaceWord(int paraIndex, int startOffset, int endOffset, const QString& replacement)
{
    if (!m_textBuffer || paraIndex >= m_textBuffer->blockCount()) {
        return;
    }

    // Select the word to replace
    SelectionRange range;
    range.start = CursorPosition{paraIndex, startOffset};
    range.end = CursorPosition{paraIndex, endOffset};
    m_selection = range;

    // Replace the word, as one undo step; its annotations stay
    if (replacement.isEmpty()) {
        deleteSelectedText();
    } else {
        insertText(replacement);
    }
}

// =============================================================================
// Grammar
// =============================================================================

void BookEditor::setGrammarCheckService(GrammarCheckService* service)
{
    if (m_grammarCheckService == service) {
        return;
    }
    if (m_grammarCheckService != nullptr) {
        dropGrammarRequests();
        disconnect(m_grammarCheckService, nullptr, this, nullptr);
    }
    m_grammarCheckService = service;
    if (m_grammarCheckService != nullptr) {
        connect(m_grammarCheckService, &GrammarCheckService::textChecked, this,
                &BookEditor::onGrammarChecked);
        connect(m_grammarCheckService, &GrammarCheckService::textNotChecked, this,
                &BookEditor::onGrammarNotChecked);
        connect(m_grammarCheckService, &GrammarCheckService::checkingChanged, this,
                &BookEditor::onGrammarCheckingChanged);
        connect(m_grammarCheckService, &GrammarCheckService::available, this,
                [this]() { scheduleGrammarCheck(0); });
        connect(m_grammarCheckService, &GrammarCheckService::ruleIgnored, this,
                &BookEditor::onGrammarRuleIgnored);
        connect(m_grammarCheckService, &QObject::destroyed, this,
                &BookEditor::onGrammarCheckServiceDestroyed);
        if (m_viewportManager) {
            connect(m_viewportManager.get(), &ViewportManager::viewportChanged, this,
                    &BookEditor::onGrammarViewChanged, Qt::UniqueConnection);
        }
    }
    onGrammarCheckingChanged();
}

GrammarCheckService* BookEditor::grammarCheckService() const
{
    return m_grammarCheckService;
}

void BookEditor::requestGrammarCheck()
{
    if (!m_textBuffer || m_grammarCheckService == nullptr ||
        !m_grammarCheckService->isActive()) {
        return;
    }
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        if (ParagraphData* paragraphData = ParagraphData::find(block)) {
            paragraphData->grammar.current = false;
            for (NoteCheck& note : paragraphData->noteGrammar) {
                note.current = false;
            }
        }
    }
    noteIssuesChanged();
    scheduleGrammarCheck(0);
}

bool BookEditor::isGrammarCheckPending() const
{
    return (m_grammarTimer != nullptr && m_grammarTimer->isActive()) ||
           !m_grammarRequests.empty();
}

void BookEditor::setWholeTextGrammar(bool whole)
{
    if (m_wholeTextGrammar == whole) {
        return;
    }
    m_wholeTextGrammar = whole;
    if (whole && m_grammarCheckService != nullptr && m_grammarCheckService->isActive()) {
        scheduleGrammarCheck(0);
    }
}

void BookEditor::scheduleGrammarCheck(int delayMs)
{
    if (m_grammarTimer == nullptr) {
        m_grammarTimer = new QTimer(this);
        m_grammarTimer->setSingleShot(true);
        connect(m_grammarTimer, &QTimer::timeout, this, &BookEditor::runGrammarCheck);
    }
    // A turn due sooner stays
    if (!m_grammarTimer->isActive() || m_grammarTimer->remainingTime() > delayMs) {
        m_grammarTimer->start(delayMs);
    }
}

void BookEditor::runGrammarCheck()
{
    if (!m_textBuffer || m_grammarCheckService == nullptr ||
        !m_grammarCheckService->isActive() || !isVisible()) {
        return;  // shown: showEvent(); the server back: GrammarCheckService::available()
    }

    // Not while the writer is typing; the clock is read once, so the rest is never below zero
    if (m_grammarEditClock.isValid()) {
        const qint64 rest = GRAMMAR_EDIT_DELAY_MS - m_grammarEditClock.elapsed();
        if (rest > 0) {
            scheduleGrammarCheck(static_cast<int>(rest));
            return;
        }
    }

    const auto send = [this](int number) { return sendGrammarCheck(number); };

    // The paragraphs in view, then those after them and before them
    const auto [firstInView, lastInView] =
        m_viewportManager ? m_viewportManager->visibleRange() : std::pair<size_t, size_t>{0, 0};
    const int last = m_textBuffer->blockCount() - 1;
    const int viewFirst = std::min(static_cast<int>(firstInView), last);
    const int viewLast = std::min(static_cast<int>(lastInView), last);
    for (int number = viewFirst; number <= viewLast; ++number) {
        if (!send(number)) {
            return;
        }
    }
    for (int number = viewLast + 1;
         number <= std::min(last, viewLast + GRAMMAR_PARAGRAPHS_AFTER_VIEW); ++number) {
        if (!send(number)) {
            return;
        }
    }
    for (int number = viewFirst - 1;
         number >= std::max(0, viewFirst - GRAMMAR_PARAGRAPHS_BEFORE_VIEW); --number) {
        if (!send(number)) {
            return;
        }
    }

    // The whole text, for a list of all its issues: on to its end, then from its start
    if (m_wholeTextGrammar) {
        for (int number = viewLast + GRAMMAR_PARAGRAPHS_AFTER_VIEW + 1; number <= last;
             ++number) {
            if (!send(number)) {
                return;
            }
        }
        for (int number = 0; number < viewFirst - GRAMMAR_PARAGRAPHS_BEFORE_VIEW; ++number) {
            if (!send(number)) {
                return;
            }
        }
    }
}

bool BookEditor::sendGrammarCheck(int paragraph)
{
    if (m_grammarRequests.size() >= MAX_GRAMMAR_REQUESTS) {
        return false;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(paragraph);
    if (!block.isValid()) {
        return true;
    }
    const auto beingChecked = [this, &block](const QString& noteId, int line) {
        return std::any_of(m_grammarRequests.begin(), m_grammarRequests.end(),
                           [&block, &noteId, line](const auto& request) {
                               const GrammarRequest& sent = request.second;
                               return sent.place.block() == block && sent.noteLine == line &&
                                      sent.noteId == noteId;
                           });
    };
    const ParagraphData* paragraphData = ParagraphData::find(block);
    if ((paragraphData == nullptr || !paragraphData->grammar.current) &&
        !beingChecked(QString(), 0)) {
        const QString text = block.text();
        if (text.trimmed().isEmpty() || !isCheckedParagraph(block)) {
            keepGrammarIssues(block, {});  // nothing to check
            noteIssuesChanged();
        } else {
            const quint64 request = m_grammarCheckService->check(text);
            if (request == 0) {
                return false;
            }
            m_grammarRequests.emplace(request,
                                      GrammarRequest{QTextCursor(block), text, QString(), 0});
        }
    }

    // The texts of its notes, a paragraph at a time as those of the text; also in a paragraph
    // not checked (Code), as their spelling
    paragraphData = ParagraphData::find(block);
    for (NoteLine& line : noteLinesOf(block)) {
        if (isNoteGrammarKnown(paragraphData, line) || beingChecked(line.id, line.line)) {
            continue;
        }
        if (m_grammarRequests.size() >= MAX_GRAMMAR_REQUESTS) {
            return false;
        }
        const quint64 request = m_grammarCheckService->check(line.text);
        if (request == 0) {
            return false;
        }
        m_grammarRequests.emplace(request, GrammarRequest{QTextCursor(block), std::move(line.text),
                                                          std::move(line.id), line.line});
    }
    return true;
}

bool BookEditor::isGrammarKnown(const QTextBlock& block) const
{
    const ParagraphData* paragraphData = ParagraphData::find(block);
    if ((paragraphData == nullptr || !paragraphData->grammar.current) &&
        !block.text().trimmed().isEmpty() && isCheckedParagraph(block)) {
        return false;
    }
    const std::vector<NoteLine> lines = noteLinesOf(block);
    return std::all_of(lines.begin(), lines.end(), [paragraphData](const NoteLine& line) {
        return isNoteGrammarKnown(paragraphData, line);
    });
}

void BookEditor::onGrammarChecked(quint64 request, const QList<GrammarError>& errors)
{
    const auto found = m_grammarRequests.find(request);
    if (found == m_grammarRequests.end()) {
        return;  // another editor's request, or one dropped
    }
    const GrammarRequest sent = std::move(found->second);
    m_grammarRequests.erase(found);

    // The issues apply while the paragraph has the text sent; a paragraph of a note's text,
    // while it has
    const QTextBlock block = sent.place.block();
    if (!block.isValid()) {
        // The paragraph is gone
    } else if (sent.noteId.isEmpty()) {
        if (block.text() == sent.text) {
            if (keepGrammarIssues(block, errors)) {
                update();
            }
            noteIssuesChanged();  // its grammar is known now
        }
    } else {
        const std::vector<NoteLine> lines = noteLinesOf(block);
        const auto line = std::find_if(lines.begin(), lines.end(), [&sent](const NoteLine& at) {
            return at.line == sent.noteLine && at.id == sent.noteId && at.text == sent.text;
        });
        if (line != lines.end()) {
            if (keepNoteGrammarIssues(block, *line, errors)) {
                update();
            }
            noteIssuesChanged();
        }
    }
    resumeIssueSearch();
    scheduleGrammarCheck(0);
}

void BookEditor::onGrammarNotChecked(quint64 request)
{
    if (m_grammarRequests.erase(request) > 0) {
        resumeIssueSearch();  // on with the issues found so far
    }
}

void BookEditor::onGrammarCheckingChanged()
{
    endIssueSearch(IssueSearch::Cancelled);
    m_grammarRequests.clear();  // the checking dropped them
    clearGrammar();
    if (m_grammarCheckService != nullptr && m_grammarCheckService->isActive()) {
        scheduleGrammarCheck(0);
    }
}

void BookEditor::onGrammarRuleIgnored(const QString& ruleId)
{
    if (!m_textBuffer) {
        return;
    }
    bool changed = false;
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        ParagraphData* paragraphData = ParagraphData::find(block);
        if (paragraphData == nullptr) {
            continue;
        }
        for (NoteCheck& note : paragraphData->noteGrammar) {
            std::vector<TextHighlight> issues;
            std::vector<GrammarError> kept;
            for (std::size_t i = 0; i < note.errors.size() && i < note.issues.size(); ++i) {
                if (note.errors[i].ruleId != ruleId) {
                    issues.push_back(note.issues[i]);
                    kept.push_back(note.errors[i]);
                }
            }
            if (kept.size() != note.errors.size()) {
                changed = true;
                note.issues = std::move(issues);
                note.errors = std::move(kept);
            }
        }
        if (paragraphData->grammarErrors.empty()) {
            continue;
        }
        std::vector<TextHighlight> issues;
        std::vector<GrammarError> kept;
        const std::size_t count =
            std::min(paragraphData->grammarErrors.size(), paragraphData->grammar.issues.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (paragraphData->grammarErrors[i].ruleId != ruleId) {
                issues.push_back(paragraphData->grammar.issues[i]);
                kept.push_back(paragraphData->grammarErrors[i]);
            }
        }
        if (kept.size() != paragraphData->grammarErrors.size()) {
            changed = true;
            paragraphData->grammar.issues = std::move(issues);
            if (paragraphData->grammar.issues.empty()) {
                paragraphData->grammar.text.clear();
            }
            paragraphData->grammarErrors = std::move(kept);
        }
    }
    if (changed) {
        noteIssuesChanged();
        update();
    }
}

void BookEditor::onGrammarCheckServiceDestroyed()
{
    endIssueSearch(IssueSearch::Cancelled);
    m_grammarCheckService = nullptr;
    m_grammarRequests.clear();
    clearGrammar();
}

void BookEditor::onGrammarViewChanged()
{
    if (m_grammarCheckService != nullptr && m_grammarCheckService->isActive()) {
        scheduleGrammarCheck(GRAMMAR_VIEW_DELAY_MS);
    }
}

void BookEditor::adjustGrammarToEdit(int from, int charsRemoved, int charsAdded)
{
    endIssueSearch(IssueSearch::Cancelled);  // the places it went through changed
    if (!m_textBuffer || m_grammarCheckService == nullptr) {
        return;
    }
    const QTextBlock first = m_textBuffer->findBlock(from);
    const QTextBlock last = m_textBuffer->findBlock(from + charsAdded);
    if (!first.isValid()) {
        return;
    }

    // The answers for the paragraphs edited would not apply
    const int firstNumber = first.blockNumber();
    const int lastNumber = last.isValid() ? last.blockNumber() : firstNumber;
    for (auto request = m_grammarRequests.begin(); request != m_grammarRequests.end();) {
        const int number = request->second.place.block().blockNumber();
        if (number >= firstNumber && number <= lastNumber) {
            m_grammarCheckService->cancel(request->first);
            request = m_grammarRequests.erase(request);
        } else {
            ++request;
        }
    }

    for (QTextBlock block = first; block.isValid(); block = block.next()) {
        if (ParagraphData* paragraphData = ParagraphData::find(block)) {
            ParagraphCheck& check = paragraphData->grammar;
            const QString text = block.text();
            // A paragraph with the text its waves were found in keeps them (a new format)
            if (check.issues.empty() || check.text != text) {
                check.current = false;
                if (block == first && !check.issues.empty()) {
                    moveGrammarWaves(block, from - block.position(), charsRemoved, last);
                } else {
                    check.issues.clear();
                    check.text.clear();
                    paragraphData->grammarErrors.clear();
                }
            }
            // The results of the notes' paragraphs whose text changed, or gone, go: the
            // paragraphs are sent again (sendGrammarCheck())
            if (!paragraphData->noteGrammar.empty()) {
                const std::vector<NoteLine> lines = noteLinesOf(block);
                std::erase_if(paragraphData->noteGrammar, [&lines](const NoteCheck& note) {
                    return std::none_of(lines.begin(), lines.end(), [&note](const NoteLine& line) {
                        return line.line == note.line && line.id == note.id &&
                               line.text == note.text;
                    });
                });
            }
        }
        if (block == last) {
            break;
        }
    }

    noteIssuesChanged();  // the waves moved with the text, or went

    // The paragraphs go when the writer stops typing; at once after an edit not made by typing
    // (a fix chosen, undo, a paste): its issues are known again without a wait
    if (m_spellTypingEdit) {
        m_grammarEditClock.restart();
        scheduleGrammarCheck(GRAMMAR_EDIT_DELAY_MS);
    } else {
        m_grammarEditClock.invalidate();
        scheduleGrammarCheck(0);
    }
}

void BookEditor::dropGrammarRequests()
{
    const auto requests = std::exchange(m_grammarRequests, {});
    if (m_grammarCheckService != nullptr) {
        for (const auto& [request, sent] : requests) {
            m_grammarCheckService->cancel(request);
        }
    }
}

void BookEditor::clearGrammar()
{
    if (m_grammarTimer != nullptr) {
        m_grammarTimer->stop();
    }
    dropGrammarRequests();
    if (!m_textBuffer) {
        return;
    }
    bool hadWaves = false;
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        if (ParagraphData* paragraphData = ParagraphData::find(block)) {
            hadWaves = hadWaves || !paragraphData->grammar.issues.empty() ||
                       !paragraphData->noteGrammar.empty();
            paragraphData->grammar = ParagraphCheck{};
            paragraphData->grammarErrors.clear();
            paragraphData->noteGrammar.clear();
        }
    }
    noteIssuesChanged();
    if (hadWaves) {
        update();
    }
}

std::optional<GrammarError> BookEditor::getGrammarErrorAt(int paraIndex, int offset) const
{
    const QTextBlock block = m_textBuffer ? m_textBuffer->findBlockByNumber(paraIndex)
                                          : QTextBlock();
    const ParagraphData* paragraphData = ParagraphData::find(block);
    if (paragraphData == nullptr || paragraphData->grammar.issuesFor(block.text()) == nullptr) {
        return std::nullopt;
    }
    // Issues can lie in one another: the one selected (Next Spelling or Grammar Issue
    // selects one) before the others there
    const SelectionRange selected = m_selection.normalized();
    std::optional<GrammarError> found;
    for (const GrammarError& error : paragraphData->grammarErrors) {
        if (offset < error.startPos || offset > error.startPos + error.length) {
            continue;
        }
        if (hasSelection() && selected.start == CursorPosition{paraIndex, error.startPos} &&
            selected.end == CursorPosition{paraIndex, error.startPos + error.length}) {
            return error;
        }
        if (!found.has_value()) {
            found = error;
        }
    }
    return found;
}

std::optional<GrammarError> BookEditor::getNoteGrammarErrorAt(const NoteRange& place) const
{
    const std::optional<NotePlace> note = noteOf(place.id);
    if (!note) {
        return {};
    }
    const ParagraphData* paragraphData =
        ParagraphData::find(m_textBuffer->findBlock(note->position));
    if (paragraphData == nullptr || paragraphData->noteGrammar.empty()) {
        return std::nullopt;
    }
    std::vector<NoteLine> lines;
    addNoteLines(*note, 0, lines);
    const NoteLine* line = noteLineAt(lines, place.start);
    const NoteCheck* check =
        line != nullptr
            ? NoteCheck::find(paragraphData->noteGrammar, line->id, line->line, line->text)
            : nullptr;
    if (check == nullptr) {
        return std::nullopt;
    }
    // As in the text: the issue selected first, where they lie in one another
    const int offset = place.start - line->start;
    std::optional<GrammarError> found;
    for (const GrammarError& error : check->errors) {
        if (offset < error.startPos || offset > error.startPos + error.length) {
            continue;
        }
        GrammarError inNote = error;
        inNote.startPos += line->start;
        if (m_noteSelection == NoteRange{place.id, inNote.startPos, inNote.length}) {
            return inNote;
        }
        if (!found.has_value()) {
            found = std::move(inNote);
        }
    }
    return found;
}

void BookEditor::addGrammarActions(QMenu& menu, const GrammarError& error,
                                   const std::function<void(const QString&)>& replace)
{
    // What is wrong, whole, in lines as wide as the menu allows: read also from the
    // keyboard, which has no tool tip
    const QString message =
        (error.message.isEmpty() ? error.shortMessage : error.message).simplified();
    const QFontMetrics metrics(menu.font());
    for (const QString& line :
         textLines(message, menu.font(), metrics.averageCharWidth() * GRAMMAR_MESSAGE_CHARS)) {
        QAction* what = menu.addAction(menuText(line));
        what->setEnabled(false);
    }

    // What to put in its place, in bold as the words for a misspelled one
    for (const QString& suggestion : error.suggestions) {
        QAction* action = menu.addAction(menuText(suggestion));
        QFont bold = action->font();
        bold.setBold(true);
        action->setFont(bold);
        disableWhileReadOnly(action);  // a read-only text keeps its words
        connect(action, &QAction::triggered, this,
                [replace, suggestion]() { replace(suggestion); });
    }
    menu.addSeparator();

    // The rule is not reported again, in any book (the Settings dialog can bring it back).
    // Greyed out in a read-only text as well
    QAction* ignore = menu.addAction(tr("&Ignore This Rule"));
    disableWhileReadOnly(ignore);
    connect(ignore, &QAction::triggered, this,
            [this, ruleId = error.ruleId, description = error.ruleDescription]() {
                if (m_grammarCheckService != nullptr) {
                    m_grammarCheckService->ignoreRule(ruleId, description);
                }
            });
    menu.addSeparator();
}

// =============================================================================
// Issues
// =============================================================================

void BookEditor::describeSpellingIssue(TextIssue& issue) const
{
    if (issue.kind != TextIssueKind::BookRule) {
        return;
    }
    const std::optional<core::RuleDeviation> deviation = bookRuleDeviation(issue.text);
    if (!deviation) {
        return;
    }
    const core::SpellingRule* rule = m_bookDictionary->findRule(deviation->ruleId);
    if (rule == nullptr) {
        return;
    }
    //: %1 is the right spelling in quotes, as the rule has it: Write "Smok"
    issue.message = tr("Write %1").arg(QLocale().quoteString(rule->right));
    issue.shortMessage = tr("Book rule");
    issue.ruleId = rule->id;
    issue.ruleDescription = rule->note.simplified();
    issue.suggestions = {deviation->right};
}

std::vector<TextIssue> BookEditor::textIssues() const
{
    std::vector<TextIssue> issues;
    if (!m_textBuffer) {
        return issues;
    }
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        const ParagraphData* paragraphData = ParagraphData::find(block);
        if (paragraphData == nullptr ||
            (paragraphData->spelling.issues.empty() && paragraphData->grammar.issues.empty() &&
             paragraphData->noteSpelling.empty() && paragraphData->noteGrammar.empty())) {
            continue;
        }

        // Those the waves show: made for the paragraph's text as it is
        const QString text = block.text();
        const int paragraph = block.blockNumber();
        const auto first = static_cast<std::ptrdiff_t>(issues.size());
        const auto add = [&issues, &text, paragraph](TextIssueKind kind,
                                                     const TextHighlight& range) -> TextIssue& {
            TextIssue& issue = issues.emplace_back();
            issue.kind = kind;
            issue.paragraph = paragraph;
            issue.start = range.start;
            issue.length = range.length;
            issue.text = text.mid(range.start, range.length);
            return issue;
        };
        if (const auto* words = paragraphData->spelling.issuesFor(text)) {
            for (const TextHighlight& word : *words) {
                describeSpellingIssue(add(spellingKindOf(word), word));
            }
        }
        if (const auto* grammar = paragraphData->grammar.issuesFor(text)) {
            const std::size_t count = std::min(grammar->size(), paragraphData->grammarErrors.size());
            for (std::size_t i = 0; i < count; ++i) {
                const GrammarError& error = paragraphData->grammarErrors[i];
                TextIssue& issue = add(TextIssueKind::Grammar, (*grammar)[i]);
                issue.message = error.message;
                issue.shortMessage = error.shortMessage;
                issue.ruleId = error.ruleId;
                issue.ruleDescription = error.ruleDescription;
                issue.suggestions = error.suggestions;
            }
        }

        // Those of the texts of its notes, their paragraphs one after another
        if (!paragraphData->noteSpelling.empty() || !paragraphData->noteGrammar.empty()) {
            for (const NoteLine& line : noteLinesOf(block)) {
                const auto addInNote = [&](TextIssueKind kind,
                                           const TextHighlight& range) -> TextIssue& {
                    TextIssue& issue = issues.emplace_back();
                    issue.kind = kind;
                    issue.paragraph = paragraph;
                    issue.start = line.start + range.start;
                    issue.length = range.length;
                    issue.text = line.text.mid(range.start, range.length);
                    issue.noteId = line.id;
                    issue.numberOffset = line.number;
                    return issue;
                };
                if (const NoteCheck* words = NoteCheck::find(paragraphData->noteSpelling,
                                                             line.id, line.line, line.text)) {
                    for (const TextHighlight& word : words->issues) {
                        describeSpellingIssue(addInNote(spellingKindOf(word), word));
                    }
                }
                if (const NoteCheck* grammar = NoteCheck::find(paragraphData->noteGrammar,
                                                               line.id, line.line, line.text)) {
                    const std::size_t count =
                        std::min(grammar->issues.size(), grammar->errors.size());
                    for (std::size_t i = 0; i < count; ++i) {
                        const GrammarError& error = grammar->errors[i];
                        TextIssue& issue = addInNote(TextIssueKind::Grammar, grammar->issues[i]);
                        issue.message = error.message;
                        issue.shortMessage = error.shortMessage;
                        issue.ruleId = error.ruleId;
                        issue.ruleDescription = error.ruleDescription;
                        issue.suggestions = error.suggestions;
                    }
                }
            }
        }

        // In the order of the paragraph; at one place the word (put in first) before the
        // grammar issue; those of a note's text right after its number, in the note's order
        const auto place = [](const TextIssue& issue) {
            return issue.inNote() ? std::pair{(2 * issue.numberOffset) + 1, issue.start}
                                  : std::pair{2 * issue.start, 0};
        };
        std::stable_sort(issues.begin() + first, issues.end(),
                         [&place](const TextIssue& a, const TextIssue& b) {
                             return place(a) < place(b);
                         });
    }
    return issues;
}

BookEditor::CheckProgress BookEditor::checkProgress() const
{
    CheckProgress progress;
    if (!m_textBuffer) {
        return progress;
    }
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        if (block.length() <= 1 || block.text().trimmed().isEmpty() ||
            !isCheckedParagraph(block)) {
            continue;  // nothing to check
        }
        ++progress.paragraphs;
        if (const ParagraphData* paragraphData = ParagraphData::find(block)) {
            progress.spellingChecked += paragraphData->spelling.current ? 1 : 0;
            progress.grammarChecked += isGrammarKnown(block) ? 1 : 0;  // its notes' too
        }
    }
    return progress;
}

bool BookEditor::selectIssue(const TextIssue& issue)
{
    if (!isIssueAt(issue)) {
        return false;
    }
    if (issue.inNote()) {
        return selectNoteRange({issue.noteId, issue.start, issue.length});
    }
    selectRange({issue.paragraph, issue.start}, {issue.paragraph, issue.start + issue.length});
    return true;
}

bool BookEditor::replaceIssue(const TextIssue& issue, const QString& replacement)
{
    if (refusesChange() || !isIssueAt(issue)) {
        return false;
    }
    if (issue.inNote()) {
        return replaceInNote({issue.noteId, issue.start, issue.length}, replacement);
    }
    replaceWord(issue.paragraph, issue.start, issue.start + issue.length, replacement);
    return true;
}

bool BookEditor::isIssueAt(const TextIssue& issue) const
{
    if (!m_textBuffer || issue.length <= 0 || issue.start < 0) {
        return false;
    }
    if (issue.inNote()) {
        const std::optional<NotePlace> note = noteOf(issue.noteId);
        const QString text = note ? noteText(note->note.content) : QString();
        return issue.start + issue.length <= text.size() &&
               QStringView(text).mid(issue.start, issue.length) == issue.text;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(issue.paragraph);
    return block.isValid() && issue.start + issue.length <= block.length() - 1 &&
           QStringView(block.text()).mid(issue.start, issue.length) == issue.text;
}

void BookEditor::selectRange(const CursorPosition& start, const CursorPosition& end)
{
    clearSelection();
    setCursorPosition(end);
    m_selectionAnchor = start;
    setSelection({start, end});
    ensureCursorVisible();
    update();
}

void BookEditor::noteIssuesChanged()
{
    m_noteIssueMarksStale = true;
    if (m_issuesTimer == nullptr) {
        m_issuesTimer = new QTimer(this);
        m_issuesTimer->setSingleShot(true);
        m_issuesTimer->setInterval(0);
        connect(m_issuesTimer, &QTimer::timeout, this, &BookEditor::issuesChanged);
    }
    if (!m_issuesTimer->isActive()) {
        m_issuesTimer->start();
    }
}

void BookEditor::updateNoteIssueMarks()
{
    if (!m_noteIssueMarksStale) {
        return;
    }
    m_noteIssueMarksStale = false;
    m_noteIssueMarks.clear();
    if (!m_textBuffer) {
        return;
    }
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        const ParagraphData* paragraphData = ParagraphData::find(block);
        if (paragraphData == nullptr ||
            (paragraphData->noteSpelling.empty() && paragraphData->noteGrammar.empty())) {
            continue;
        }
        for (const NoteLine& line : noteLinesOf(block)) {
            for (const std::vector<NoteCheck>* checks :
                 {&paragraphData->noteSpelling, &paragraphData->noteGrammar}) {
                if (const NoteCheck* check =
                        NoteCheck::find(*checks, line.id, line.line, line.text)) {
                    std::vector<TextHighlight>& marks = m_noteIssueMarks[line.id];
                    for (const TextHighlight& issue : check->issues) {
                        marks.push_back({line.start + issue.start, issue.length, issue.kind});
                    }
                }
            }
        }
    }
}

// =============================================================================
// Reading aloud
// =============================================================================

void BookEditor::setSpokenWord(int paragraph, int offset, int length)
{
    if (m_renderPipeline) {
        m_renderPipeline->setSpokenWord(paragraph, offset, length);
    }
}

void BookEditor::setRepetitionMarks(const std::vector<RepetitionMark>& marks,
                                    RepetitionMarkStyle style)
{
    m_repetitionMarks = marks;
    if (m_renderPipeline) {
        m_renderPipeline->setRepetitionMarks(marks, style);
    }
    update();
}

}  // namespace kalahari::editor
