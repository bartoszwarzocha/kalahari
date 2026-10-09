/// @file book_editor_checking.cpp
/// @brief BookEditor: spelling and grammar checks, the word read aloud

#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>
#include <kalahari/editor/paragraph_data.h>
#include <QMenu>
#include <QMessageBox>
#include <utility>

namespace kalahari::editor {

namespace {

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
    ParagraphCheck results{block.text(), {}};
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
// Spell Check Integration (Phase 6.9)
// =============================================================================

void BookEditor::setSpellCheckService(SpellCheckService* service)
{
    // Disconnect from previous service
    if (m_spellCheckService) {
        disconnect(m_spellCheckService, nullptr, this, nullptr);
        m_spellCheckService->setBookEditor(nullptr);
    }

    m_spellCheckService = service;

    // Connect to new service
    if (m_spellCheckService) {
        connect(m_spellCheckService, &SpellCheckService::paragraphChecked,
                this, &BookEditor::onSpellCheckParagraph);

        // Connect service to this BookEditor for paragraph signals
        m_spellCheckService->setBookEditor(this);

        core::Logger::getInstance().debug("BookEditor: Spell check service connected");
    }
}

SpellCheckService* BookEditor::spellCheckService() const
{
    return m_spellCheckService;
}

void BookEditor::requestSpellCheck()
{
    if (m_spellCheckService && m_textBuffer) {
        m_spellCheckService->checkDocumentAsync();
    }
}

void BookEditor::onSpellCheckParagraph(int paragraphIndex, const QList<SpellErrorInfo>& errors)
{
    // Kept with the paragraph; the render pipeline draws them as waves
    std::vector<std::pair<TextHighlight, QString>> found;
    found.reserve(static_cast<size_t>(errors.size()));
    for (const SpellErrorInfo& error : errors) {
        found.emplace_back(TextHighlight{error.startPos, error.length, HighlightKind::Spelling},
                           error.word);
    }
    storeCheckResults(m_textBuffer.get(), paragraphIndex, &ParagraphData::spelling, found);
    update();
}

std::tuple<QString, int, int> BookEditor::getMisspelledWordAt(int paraIndex, int offset) const
{
    const QTextBlock block = m_textBuffer ? m_textBuffer->findBlockByNumber(paraIndex)
                                          : QTextBlock();
    if (const ParagraphData* paragraphData = ParagraphData::find(block)) {
        const QString text = block.text();
        if (const auto* issues = paragraphData->spelling.issuesFor(text)) {
            for (const TextHighlight& issue : *issues) {
                if (offset >= issue.start && offset < issue.start + issue.length) {
                    return {text.mid(issue.start, issue.length), issue.start,
                            issue.start + issue.length};
                }
            }
        }
    }
    return {QString(), 0, 0};
}

QMenu* BookEditor::createSpellCheckContextMenu(const QString& word, int paraIndex,
                                                int startOffset, int endOffset)
{
    QMenu* menu = new QMenu(this);

    // Get suggestions from spell check service
    QStringList suggestions;
    if (m_spellCheckService) {
        suggestions = m_spellCheckService->suggestions(word, 5);
    }

    // Add suggestion actions
    if (suggestions.isEmpty()) {
        QAction* noSuggestionsAction = menu->addAction(tr("(No suggestions)"));
        noSuggestionsAction->setEnabled(false);
    } else {
        for (const QString& suggestion : suggestions) {
            QAction* action = menu->addAction(suggestion);
            connect(action, &QAction::triggered, this, [this, paraIndex, startOffset, endOffset, suggestion]() {
                replaceWord(paraIndex, startOffset, endOffset, suggestion);
            });
        }
    }

    menu->addSeparator();

    // Add to dictionary option
    QAction* addToDictAction = menu->addAction(tr("Add to Dictionary"));
    connect(addToDictAction, &QAction::triggered, this, [this, word]() {
        if (m_spellCheckService) {
            m_spellCheckService->addToUserDictionary(word);
            // Re-check affected paragraph
            requestSpellCheck();
        }
    });

    // Ignore option
    QAction* ignoreAction = menu->addAction(tr("Ignore"));
    connect(ignoreAction, &QAction::triggered, this, [this, word]() {
        if (m_spellCheckService) {
            m_spellCheckService->ignoreWord(word);
            // Re-check affected paragraph
            requestSpellCheck();
        }
    });

    return menu;
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

    // Get text for debug log using QTextDocument
    QString replacedText;
    QTextBlock block = m_textBuffer->findBlockByNumber(paraIndex);
    if (block.isValid()) {
        replacedText = block.text().mid(startOffset, endOffset - startOffset);
    }
    core::Logger::getInstance().debug("BookEditor: Replaced '{}' at ({}, {}-{}) with '{}'",
        replacedText.toStdString(),
        paraIndex, startOffset, endOffset, replacement.toStdString());
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
