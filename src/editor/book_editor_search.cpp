/// @file book_editor_search.cpp
/// @brief BookEditor: find and replace

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/find_replace_bar.h>
#include <kalahari/editor/search_engine.h>
#include "book_editor_internal.h"
#include <QScrollBar>
#include <QTextBlock>

namespace kalahari::editor {

// =============================================================================
// Find/Replace (Phase 9.4-9.6)
// =============================================================================

void BookEditor::setupFindReplace()
{
    m_searchEngine = std::make_unique<SearchEngine>(this);
    m_searchEngine->setDocument(m_textBuffer.get());

    // Phase 12.3: Connect search engine to pipeline
    if (m_renderPipeline) {
        m_renderPipeline->setSearchEngine(m_searchEngine.get());
    }

    // Create FindReplaceBar (will be shown when needed), with an arrow pointer over its
    // buttons instead of the editor's I-beam
    m_findReplaceBar = new FindReplaceBar(this);
    m_findReplaceBar->setCursor(Qt::ArrowCursor);
    m_findReplaceBar->setSearchEngine(m_searchEngine.get());
    // Find/Replace performs its edits directly on the document, which QTextDocument's
    // native undo records
    m_findReplaceBar->hide();

    connect(m_findReplaceBar, &FindReplaceBar::navigateToMatch,
            this, &BookEditor::onNavigateToMatch);
    connect(m_findReplaceBar, &FindReplaceBar::textReplaced,
            this, &BookEditor::onTextReplaced);
    connect(m_findReplaceBar, &FindReplaceBar::closed,
            this, &BookEditor::hideFindReplace);
    connect(m_findReplaceBar, &FindReplaceBar::textFocusRequested,
            this, [this]() { setFocus(); });
    connect(m_searchEngine.get(), &SearchEngine::matchesChanged,
            this, [this]() { update(); });  // Repaint on match change

    // Undoing a replacement brings back the cursor and selection it was made with, redoing
    // it the cursor at the end of the replaced text: after undoing Replace All QTextDocument
    // would put the cursor at the start of the text. Of the step's undo items, the first is
    // undone last and the last redone last.
    connect(m_searchEngine.get(), &SearchEngine::replacementStepBegun, this, [this]() {
        m_searchEngine->document()->appendUndoItem(new CallbackUndoItem(
            [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    });
    connect(m_searchEngine.get(), &SearchEngine::replacementStepEnding, this, [this]() {
        m_searchEngine->document()->appendUndoItem(new CallbackUndoItem(
            [this, state = StepCursor{replacedTextEnd(), {}}] { m_stepCursor = state; }));
    });
    syncSearchOrigin();
}

SearchEngine* BookEditor::searchEngine() const
{
    return m_searchEngine.get();
}

void BookEditor::syncSearchOrigin()
{
    if (!m_searchEngine || !m_textBuffer) {
        return;
    }

    // Find Next goes on from the selection, or from the cursor without one; a selected
    // match is the current one
    const SelectionRange range = hasSelection() ? m_selection.normalized()
                                                : SelectionRange{m_cursorPosition, m_cursorPosition};
    m_searchEngine->setOrigin(
        static_cast<size_t>(editor::calculateAbsolutePosition(m_textBuffer.get(), range.start)),
        static_cast<size_t>(editor::calculateAbsolutePosition(m_textBuffer.get(), range.end)));
}

void BookEditor::takeSearchTextFromSelection()
{
    // The search goes paragraph by paragraph: text across paragraphs would never be found
    if (hasSelection() && m_selection.start.paragraph == m_selection.end.paragraph) {
        m_findReplaceBar->setSearchText(selectedText());
    }
}

void BookEditor::showFind()
{
    if (!m_findReplaceBar) {
        setupFindReplace();
    }

    takeSearchTextFromSelection();
    openFindBar(false);
    m_findReplaceBar->focusSearchInput();
}

void BookEditor::showFindReplace()
{
    if (!m_findReplaceBar) {
        setupFindReplace();
    }

    takeSearchTextFromSelection();
    openFindBar(true);
    m_findReplaceBar->focusSearchInput();
}

void BookEditor::openFindBar(bool replace)
{
    if (replace) {
        m_findReplaceBar->showFindReplace();
    } else {
        m_findReplaceBar->showFind();
    }

    // Position at top of editor
    const int scrollBarWidth =
        m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
    m_findReplaceBar->setGeometry(0, 0, width() - scrollBarWidth,
                                  m_findReplaceBar->sizeHint().height());

    m_findReplaceBar->show();
}

bool BookEditor::resumeSearch()
{
    if (m_searchEngine && m_searchEngine->isActive()) {
        return true;
    }

    // Without a search term, open the bar to type one
    if (!m_findReplaceBar || m_findReplaceBar->searchText().isEmpty()) {
        showFind();
        return false;
    }

    // The bar was closed (Esc): it shows again, in its mode and with its text, and the keys
    // stay in the text, as F3 goes on in Word, the browsers and the code editors
    openFindBar(m_findReplaceBar->isReplaceMode());
    return m_searchEngine->isActive();
}

void BookEditor::findNext()
{
    if (!resumeSearch()) {
        return;
    }
    auto match = m_searchEngine->nextMatch();
    if (match.isValid()) {
        onNavigateToMatch(match);
    }
}

void BookEditor::findPrevious()
{
    if (!resumeSearch()) {
        return;
    }
    auto match = m_searchEngine->previousMatch();
    if (match.isValid()) {
        onNavigateToMatch(match);
    }
}

void BookEditor::hideFindReplace()
{
    if (m_findReplaceBar) {
        m_findReplaceBar->hide();
    }
    if (m_searchEngine) {
        m_searchEngine->clear();
    }
    update();  // Clear highlights
    setFocus();
}

void BookEditor::onNavigateToMatch(const SearchMatch& match)
{
    // Move cursor to match position
    CursorPosition newPos{match.paragraph, match.paragraphOffset};
    setCursorPosition(newPos);

    // Select the match
    CursorPosition endPos{match.paragraph, match.paragraphOffset + static_cast<int>(match.length)};
    SelectionRange newSelection{newPos, endPos};
    setSelection(newSelection);

    ensureCursorVisible();
    update();
}

CursorPosition BookEditor::replacedTextEnd() const
{
    const int end = m_searchEngine ? m_searchEngine->replacementEnd() : -1;
    const QTextBlock block =
        end >= 0 && m_textBuffer ? m_textBuffer->findBlock(end) : QTextBlock();
    return block.isValid() ? CursorPosition{block.blockNumber(), end - block.position()}
                           : m_cursorPosition;
}

void BookEditor::onTextReplaced()
{
    // The cursor goes to the end of the replaced text, after Replace All to the end of the
    // last replacement: where it stood, at the end of the match, the replacement may be
    // longer or shorter, and the cursor would land inside a word
    clearSelection();
    m_cursorPosition = validateCursorPosition(replacedTextEnd());

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit cursorPositionChanged(m_cursorPosition);
}

}  // namespace kalahari::editor
