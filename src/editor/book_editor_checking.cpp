/// @file book_editor_checking.cpp
/// @brief BookEditor: spelling and grammar checks, the word read aloud

#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>
#include <kalahari/editor/paragraph_data.h>
#include <kalahari/editor/spell_check_service.h>
#include <QAction>
#include <QElapsedTimer>
#include <QFont>
#include <QMenu>
#include <QMessageBox>
#include <QShowEvent>
#include <QStringView>
#include <QTextBlock>
#include <QTimer>
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

/// @brief Keep the results of a check with the paragraph they were made for
///
/// Results can arrive after the paragraph was edited: an issue is kept only while the
/// text it is about is still at its place (an empty text skips that test), and the results
/// apply while the paragraph keeps its current text (ParagraphCheck).
/// @param found Issues with the text each is about
void storeCheckResults(const QTextDocument* doc, int paragraph,
                       ParagraphCheck ParagraphData::*check,
                       const std::vector<std::pair<TextHighlight, QString>>& found) {
    const QTextBlock block = doc ? doc->findBlockByNumber(paragraph) : QTextBlock();
    if (!block.isValid()) {
        return;
    }
    ParagraphCheck results;
    results.text = block.text();
    for (const auto& [issue, issueText] : found) {
        const bool inText = issue.start >= 0 && issue.length > 0 &&
                            issue.start + issue.length <= results.text.length();
        if (inText && (issueText.isEmpty() ||
                       QStringView(results.text).mid(issue.start, issue.length) == issueText)) {
            results.issues.push_back(issue);
        }
    }
    if (results.issues.empty()) {
        if (ParagraphData* data = ParagraphData::find(block)) {
            data->*check = ParagraphCheck{};
        }
    } else if (ParagraphData* data = ParagraphData::of(block)) {
        data->*check = std::move(results);
    }
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
        if (ParagraphData* data = ParagraphData::find(block)) {
            data->spelling.current = false;
        }
    }
    scheduleSpellCheck(0);
}

bool BookEditor::isSpellCheckPending() const
{
    return m_spellTimer != nullptr && m_spellTimer->isActive();
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
        if (ParagraphData* data = ParagraphData::find(block)) {
            hadWaves = hadWaves || !data->spelling.issues.empty();
            data->spelling = ParagraphCheck{};
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
        const ParagraphData* data = ParagraphData::find(block);
        return data == nullptr || !data->spelling.current;
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
        ParagraphData* data = ParagraphData::find(block);  // none: a new paragraph, due
        if (data != nullptr) {
            ParagraphCheck& check = data->spelling;
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
    if (ParagraphData* data = ParagraphData::find(typed)) {
        data->spelling.current = false;
    }
    if (m_spellCheckService != nullptr && m_spellCheckService->isActive()) {
        scheduleSpellCheck(0);
    }
}

void BookEditor::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_spellCheckService != nullptr && m_spellCheckService->isActive()) {
        scheduleSpellCheck(0);
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
// Grammar Check Integration (Phase 6.17)
// =============================================================================

void BookEditor::setGrammarCheckService(GrammarCheckService* service)
{
    // Disconnect from previous service
    if (m_grammarCheckService) {
        disconnect(m_grammarCheckService, nullptr, this, nullptr);
        m_grammarCheckService->setBookEditor(nullptr);
    }

    m_grammarCheckService = service;

    // Connect to new service
    if (m_grammarCheckService) {
        connect(m_grammarCheckService, &GrammarCheckService::paragraphChecked,
                this, &BookEditor::onGrammarCheckParagraph);

        // Connect service to this BookEditor for paragraph signals
        m_grammarCheckService->setBookEditor(this);

        core::Logger::getInstance().debug("BookEditor: Grammar check service connected");
    }
}

GrammarCheckService* BookEditor::grammarCheckService() const
{
    return m_grammarCheckService;
}

void BookEditor::requestGrammarCheck()
{
    if (m_grammarCheckService && m_textBuffer) {
        m_grammarCheckService->checkDocumentAsync();
    }
}

void BookEditor::onGrammarCheckParagraph(int paragraphIndex, const QList<GrammarError>& errors)
{
    // Kept with the paragraph; the render pipeline draws them as waves
    std::vector<std::pair<TextHighlight, QString>> found;
    found.reserve(static_cast<size_t>(errors.size()));
    for (const GrammarError& error : errors) {
        found.emplace_back(TextHighlight{error.startPos, error.length, HighlightKind::Grammar},
                           error.text);
    }
    storeCheckResults(m_textBuffer.get(), paragraphIndex, &ParagraphData::grammar, found);
    update();
}

void BookEditor::setSpokenWord(int paragraph, int offset, int length)
{
    if (m_renderPipeline) {
        m_renderPipeline->setSpokenWord(paragraph, offset, length);
    }
}

std::optional<GrammarError> BookEditor::getGrammarErrorAt(int paraIndex, int offset) const
{
    if (!m_grammarCheckService) {
        return std::nullopt;
    }

    // Get cached errors for the paragraph
    QList<GrammarError> errors = m_grammarCheckService->errorsForParagraph(paraIndex);

    for (const GrammarError& error : errors) {
        if (offset >= error.startPos && offset < error.startPos + error.length) {
            return error;
        }
    }

    return std::nullopt;
}

QMenu* BookEditor::createGrammarContextMenu(const GrammarError& error, int paraIndex)
{
    QMenu* menu = new QMenu(this);

    // Show the error message as a disabled item (header)
    QAction* headerAction = menu->addAction(error.shortMessage.isEmpty() ? error.message : error.shortMessage);
    headerAction->setEnabled(false);

    // Show the problematic text
    if (!error.text.isEmpty()) {
        QAction* textAction = menu->addAction(tr("Error: \"%1\"").arg(error.text));
        textAction->setEnabled(false);
    }

    menu->addSeparator();

    // Add suggestions
    if (!error.suggestions.isEmpty()) {
        for (const QString& suggestion : error.suggestions) {
            QAction* action = menu->addAction(suggestion);
            connect(action, &QAction::triggered, this, [this, paraIndex, error, suggestion]() {
                replaceWord(paraIndex, error.startPos, error.startPos + error.length, suggestion);
            });
        }
        menu->addSeparator();
    }

    // Show full explanation if different from short message
    if (!error.message.isEmpty() && error.message != error.shortMessage) {
        QAction* explainAction = menu->addAction(tr("Explanation..."));
        connect(explainAction, &QAction::triggered, this, [error]() {
            QMessageBox::information(nullptr, QObject::tr("Grammar Issue"),
                QObject::tr("<b>%1</b><br><br>%2<br><br><i>Rule: %3 (%4)</i>")
                    .arg(error.shortMessage.isEmpty() ? error.text : error.shortMessage)
                    .arg(error.message)
                    .arg(error.ruleId)
                    .arg(error.category));
        });
    }

    // Ignore rule option
    QAction* ignoreAction = menu->addAction(tr("Ignore this rule"));
    connect(ignoreAction, &QAction::triggered, this, [this, error]() {
        if (m_grammarCheckService) {
            m_grammarCheckService->ignoreRule(error.ruleId);
            requestGrammarCheck();
        }
    });

    return menu;
}

}  // namespace kalahari::editor
