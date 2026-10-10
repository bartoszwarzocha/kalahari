/// @file book_editor_search.cpp
/// @brief BookEditor: find and replace

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/find_replace_bar.h>
#include <QScrollBar>

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
    connect(m_searchEngine.get(), &SearchEngine::matchesChanged,
            this, [this]() { update(); });  // Repaint on match change
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

    m_findReplaceBar->showFind();
    placeFindReplaceBar();
    m_findReplaceBar->show();
    m_findReplaceBar->focusSearchInput();
}

void BookEditor::showFindReplace()
{
    if (!m_findReplaceBar) {
        setupFindReplace();
    }

    takeSearchTextFromSelection();

    m_findReplaceBar->showFindReplace();
    placeFindReplaceBar();
    m_findReplaceBar->show();
    m_findReplaceBar->focusSearchInput();
}

void BookEditor::placeFindReplaceBar()
{
    const int scrollBarWidth = m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
    const int barWidth = qMax(0, width() - scrollBarWidth);
    const int barHeight = m_findReplaceBar->hasHeightForWidth()
                              ? m_findReplaceBar->heightForWidth(barWidth)
                              : m_findReplaceBar->sizeHint().height();
    m_findReplaceBar->setGeometry(0, 0, barWidth, barHeight);
    setTopBarHeight(barHeight);
}

int BookEditor::findReplaceBarHeight() const
{
    return m_findReplaceBar && !m_findReplaceBar->isHidden() ? m_findReplaceBar->height() : 0;
}

void BookEditor::setTopBarHeight(int height)
{
    if (!m_renderPipeline || !m_viewportManager) {
        return;
    }
    m_renderPipeline->setConfigTopBarHeight(height);
    updateScrollBarRange();
    setScrollOffset(scrollOffset());  // within the new scroll range
}

void BookEditor::findNext()
{
    // Without a search term, open the bar to type one
    if (!m_searchEngine || !m_searchEngine->isActive()) {
        showFind();
        return;
    }
    auto match = m_searchEngine->nextMatch();
    if (match.isValid()) {
        onNavigateToMatch(match);
    }
}

void BookEditor::findPrevious()
{
    if (!m_searchEngine || !m_searchEngine->isActive()) {
        showFind();
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
    setTopBarHeight(0);
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

void BookEditor::onTextReplaced()
{
    // The replaced text may be shorter than the selected match the cursor stood on
    clearSelection();
    m_cursorPosition = validateCursorPosition(m_cursorPosition);

    syncPipelineCursor();
    update();
    emit contentChanged();
    emit cursorPositionChanged(m_cursorPosition);
}

}  // namespace kalahari::editor
