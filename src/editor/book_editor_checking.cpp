/// @file book_editor_checking.cpp
/// @brief BookEditor: spelling and grammar checks, the word read aloud

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/grammar_check_service.h>
#include <kalahari/editor/paragraph_data.h>
#include <kalahari/editor/spell_check_service.h>
#include <QAction>
#include <QElapsedTimer>
#include <QFont>
#include <QFontMetrics>
#include <QMenu>
#include <QShowEvent>
#include <QStringList>
#include <QStringView>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextOption>
#include <QTimer>
#include <algorithm>
#include <utility>
#include <vector>

namespace kalahari::editor {

namespace {

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

/// @brief Keep the grammar issues found in a paragraph, but those that apply only to a
///        finished sentence while their sentence is unfinished
/// @return Whether its waves changed
bool keepGrammarIssues(const QTextBlock& block, const QList<GrammarError>& errors) {
    ParagraphData* paragraphData = ParagraphData::of(block);
    const QString text = block.text();
    std::vector<TextHighlight> issues;
    std::vector<GrammarError> kept;
    for (const GrammarError& error : errors) {
        if (error.ignoreForIncompleteSentence &&
            sentenceUnfinished(text, error.startPos + error.length)) {
            continue;
        }
        issues.push_back({error.startPos, error.length, HighlightKind::Grammar});
        kept.push_back(error);
    }
    const std::vector<TextHighlight>* before = paragraphData->grammar.issuesFor(text);
    const bool changed = before != nullptr ? *before != issues : !issues.empty();
    paragraphData->grammar.issues = std::move(issues);
    paragraphData->grammar.text = paragraphData->grammar.issues.empty() ? QString() : text;
    paragraphData->grammar.current = true;
    paragraphData->grammarErrors = std::move(kept);
    return changed;
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
    scheduleSpellCheck(0);
}

bool BookEditor::isSpellCheckPending() const
{
    return m_spellTimer != nullptr && m_spellTimer->isActive();
}

bool BookEditor::goToNextIssue()
{
    const bool spelling = m_spellCheckService != nullptr && m_spellCheckService->isActive();
    if (!m_textBuffer || (!spelling && m_grammarCheckService == nullptr)) {
        return false;
    }

    // The writer has stopped typing: the word typed last counts too
    if (spelling && m_spellTyping >= 0) {
        endSpellingTyping();
    }

    // With a selection (the issue gone to before), those that start after its start, so an
    // issue in a longer one is not passed over; else from the cursor: the issue the cursor
    // is in, or right after, comes first
    const bool afterSelection = hasSelection();
    const CursorPosition from = afterSelection ? m_selection.normalized().start : m_cursorPosition;
    const auto afterFrom = [afterSelection, &from](const TextHighlight& issue) {
        return afterSelection ? issue.start > from.offset
                              : issue.start + issue.length >= from.offset;
    };

    // Round the text from the paragraph of the cursor back to it, checking the spelling of
    // the paragraphs not checked yet on the way
    bool wavesChanged = false;
    const int count = m_textBuffer->blockCount();
    QTextBlock block = m_textBuffer->findBlockByNumber(from.paragraph);
    for (int visited = 0; visited <= count; ++visited) {
        if (!block.isValid()) {
            block = m_textBuffer->begin();
        }
        const ParagraphData* known = ParagraphData::find(block);
        if (spelling && (known == nullptr || !known->spelling.current)) {
            wavesChanged = checkSpelling(block) || wavesChanged;
        }

        // The first one wanted: the paragraph of the cursor after it first, before it when
        // back at it; the misspelled words before the grammar issues at the same place
        const TextHighlight* first = nullptr;
        const auto consider = [&first, &afterFrom, visited,
                               count](const std::vector<TextHighlight>& issues) {
            for (const TextHighlight& issue : issues) {
                bool wanted = true;
                if (visited == 0) {
                    wanted = afterFrom(issue);
                } else if (visited == count) {
                    wanted = !afterFrom(issue);
                }
                if (wanted && (first == nullptr || issue.start < first->start)) {
                    first = &issue;
                }
            }
        };
        if (const ParagraphData* paragraphData = ParagraphData::find(block)) {
            if (spelling) {
                consider(paragraphData->spelling.issues);
            }
            if (const auto* grammarIssues = paragraphData->grammar.issuesFor(block.text())) {
                consider(*grammarIssues);
            }
        }
        if (first != nullptr) {
            const int paragraph = block.blockNumber();
            const CursorPosition start{paragraph, first->start};
            const CursorPosition end{paragraph, first->start + first->length};
            clearSelection();
            setCursorPosition(end);
            m_selectionAnchor = start;
            setSelection({start, end});
            ensureCursorVisible();
            update();
            return true;
        }
        block = block.next();
    }
    if (wavesChanged) {
        update();
    }
    return false;
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
            hadWaves = hadWaves || !paragraphData->spelling.issues.empty();
            paragraphData->spelling = ParagraphCheck{};
        }
    }
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

bool BookEditor::checkSpelling(const QTextBlock& block)
{
    const QString text = block.text();
    const QList<SpellErrorInfo> errors = m_spellCheckService->checkParagraph(text);

    // The word being typed gets its wave when the cursor leaves it
    const int typed = m_spellTyping >= 0 && block.contains(m_spellTyping)
                          ? m_spellTyping - block.position()
                          : -1;
    std::vector<TextHighlight> issues;
    issues.reserve(static_cast<size_t>(errors.size()));
    for (const SpellErrorInfo& error : errors) {
        if (typed > error.startPos && typed <= error.startPos + error.length) {
            continue;
        }
        issues.push_back({error.startPos, error.length, HighlightKind::Spelling});
    }

    ParagraphCheck& check = ParagraphData::of(block)->spelling;
    const std::vector<TextHighlight>* before = check.issuesFor(text);
    const bool changed = before != nullptr ? *before != issues : !issues.empty();
    check.issues = std::move(issues);
    check.text = check.issues.empty() ? QString() : text;
    check.current = true;
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
        }
        if (block == last) {
            break;
        }
    }
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

std::tuple<QString, int, int> BookEditor::getMisspelledWordAt(int paraIndex, int offset) const
{
    const QTextBlock block = m_textBuffer ? m_textBuffer->findBlockByNumber(paraIndex)
                                          : QTextBlock();
    if (const ParagraphData* paragraphData = ParagraphData::find(block)) {
        const QString text = block.text();
        if (const auto* issues = paragraphData->spelling.issuesFor(text)) {
            for (const TextHighlight& issue : *issues) {
                if (offset >= issue.start && offset <= issue.start + issue.length) {
                    return {text.mid(issue.start, issue.length), issue.start,
                            issue.start + issue.length};
                }
            }
        }
    }
    return {QString(), 0, 0};
}

void BookEditor::addSpellingActions(QMenu& menu, const QString& word, int paraIndex,
                                    int startOffset, int endOffset)
{
    // The words to put in its place, in bold as in other word processors
    const QStringList suggestions = m_spellCheckService != nullptr
                                        ? m_spellCheckService->suggestions(word, MAX_SUGGESTIONS)
                                        : QStringList();
    if (suggestions.isEmpty()) {
        menu.addAction(tr("(No suggestions)"))->setEnabled(false);
    }
    for (const QString& suggestion : suggestions) {
        QAction* action = menu.addAction(suggestion);
        QFont bold = action->font();
        bold.setBold(true);
        action->setFont(bold);
        connect(action, &QAction::triggered, this,
                [this, paraIndex, startOffset, endOffset, suggestion]() {
                    replaceWord(paraIndex, startOffset, endOffset, suggestion);
                });
    }
    menu.addSeparator();

    // The word is right from now on: in every document (Ignore All until the application
    // closes, Add to Dictionary for good)
    QAction* ignore = menu.addAction(tr("Ignore All"));
    connect(ignore, &QAction::triggered, this, [this, word]() {
        if (m_spellCheckService != nullptr) {
            m_spellCheckService->ignoreWord(word);
        }
    });
    QAction* add = menu.addAction(tr("Add to Dictionary"));
    connect(add, &QAction::triggered, this, [this, word]() {
        if (m_spellCheckService != nullptr) {
            m_spellCheckService->addToUserDictionary(word);
        }
    });
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
        }
    }
    scheduleGrammarCheck(0);
}

bool BookEditor::isGrammarCheckPending() const
{
    return (m_grammarTimer != nullptr && m_grammarTimer->isActive()) ||
           !m_grammarRequests.empty();
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

    // Not while the writer is typing
    if (m_grammarEditClock.isValid() && !m_grammarEditClock.hasExpired(GRAMMAR_EDIT_DELAY_MS)) {
        scheduleGrammarCheck(
            static_cast<int>(GRAMMAR_EDIT_DELAY_MS - m_grammarEditClock.elapsed()));
        return;
    }

    // Sends a paragraph that is due; false when no more can be sent now
    const auto send = [this](int number) {
        if (m_grammarRequests.size() >= MAX_GRAMMAR_REQUESTS) {
            return false;
        }
        const QTextBlock block = m_textBuffer->findBlockByNumber(number);
        const ParagraphData* paragraphData = ParagraphData::find(block);
        const bool beingChecked = std::any_of(
            m_grammarRequests.begin(), m_grammarRequests.end(),
            [&block](const auto& request) { return request.second.place.block() == block; });
        if ((paragraphData != nullptr && paragraphData->grammar.current) || beingChecked) {
            return true;
        }
        const QString text = block.text();
        if (text.trimmed().isEmpty()) {
            keepGrammarIssues(block, {});  // nothing to check
            return true;
        }
        const quint64 request = m_grammarCheckService->check(text);
        if (request == 0) {
            return false;
        }
        m_grammarRequests.emplace(request, GrammarRequest{QTextCursor(block), text});
        return true;
    };

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
}

void BookEditor::onGrammarChecked(quint64 request, const QList<GrammarError>& errors)
{
    const auto found = m_grammarRequests.find(request);
    if (found == m_grammarRequests.end()) {
        return;  // another editor's request, or one dropped
    }
    const GrammarRequest sent = std::move(found->second);
    m_grammarRequests.erase(found);

    // The issues apply while the paragraph has the text sent
    const QTextBlock block = sent.place.block();
    if (block.isValid() && block.text() == sent.text && keepGrammarIssues(block, errors)) {
        update();
    }
    scheduleGrammarCheck(0);
}

void BookEditor::onGrammarNotChecked(quint64 request)
{
    m_grammarRequests.erase(request);
}

void BookEditor::onGrammarCheckingChanged()
{
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
        if (paragraphData == nullptr || paragraphData->grammarErrors.empty()) {
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
        update();
    }
}

void BookEditor::onGrammarCheckServiceDestroyed()
{
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
        }
        if (block == last) {
            break;
        }
    }

    // The paragraphs go when the writer stops typing
    m_grammarEditClock.restart();
    scheduleGrammarCheck(GRAMMAR_EDIT_DELAY_MS);
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
            hadWaves = hadWaves || !paragraphData->grammar.issues.empty();
            paragraphData->grammar = ParagraphCheck{};
            paragraphData->grammarErrors.clear();
        }
    }
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

void BookEditor::addGrammarActions(QMenu& menu, const GrammarError& error, int paraIndex)
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
        connect(action, &QAction::triggered, this, [this, paraIndex, error, suggestion]() {
            replaceWord(paraIndex, error.startPos, error.startPos + error.length, suggestion);
        });
    }
    menu.addSeparator();

    // The rule is not reported again, in any document (until the application closes)
    QAction* ignore = menu.addAction(tr("Ignore This Rule"));
    connect(ignore, &QAction::triggered, this, [this, ruleId = error.ruleId]() {
        if (m_grammarCheckService != nullptr) {
            m_grammarCheckService->ignoreRule(ruleId);
        }
    });
    menu.addSeparator();
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

}  // namespace kalahari::editor
