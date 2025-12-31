/// @file search_engine.cpp
/// @brief Search engine implementation for Find/Replace operations (OpenSpec #00044 Task 9.4)

#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/buffer_commands.h>
#include <QTextBlock>
#include <QTextCursor>
#include <QRegularExpression>
#include <QUndoStack>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructor
// =============================================================================

SearchEngine::SearchEngine(QObject* parent)
    : QObject(parent)
{
}

// =============================================================================
// Configuration
// =============================================================================

void SearchEngine::setDocument(QTextDocument* document) {
    if (m_document != document) {
        m_document = document;
        m_matchesDirty = true;
        m_currentMatchIndex = -1;
        m_matches.clear();
    }
}

QTextDocument* SearchEngine::document() const {
    return m_document;
}

void SearchEngine::setSearchText(const QString& text) {
    if (m_searchText != text) {
        m_searchText = text;
        m_matchesDirty = true;
        m_currentMatchIndex = -1;
        emit searchTextChanged(text);
    }
}

QString SearchEngine::searchText() const {
    return m_searchText;
}

void SearchEngine::setReplaceText(const QString& text) {
    m_replaceText = text;
}

QString SearchEngine::replaceText() const {
    return m_replaceText;
}

void SearchEngine::setOptions(const SearchOptions& options) {
    // Check if options that affect matching have changed
    if (m_options.caseSensitive != options.caseSensitive ||
        m_options.wholeWord != options.wholeWord ||
        m_options.useRegex != options.useRegex) {
        m_matchesDirty = true;
        m_currentMatchIndex = -1;
    }
    m_options = options;
}

SearchOptions SearchEngine::options() const {
    return m_options;
}

// =============================================================================
// Search Operations
// =============================================================================

SearchMatch SearchEngine::findNext(size_t fromPosition) {
    return findMatch(fromPosition, true);
}

SearchMatch SearchEngine::findPrevious(size_t fromPosition) {
    return findMatch(fromPosition, false);
}

std::vector<SearchMatch> SearchEngine::findAll() {
    if (m_matchesDirty) {
        rebuildMatches();
    }
    return m_matches;
}

// =============================================================================
// Navigation
// =============================================================================

int SearchEngine::currentMatchIndex() const {
    return m_currentMatchIndex;
}

int SearchEngine::totalMatchCount() const {
    if (m_matchesDirty) {
        const_cast<SearchEngine*>(this)->rebuildMatches();
    }
    return static_cast<int>(m_matches.size());
}

SearchMatch SearchEngine::nextMatch() {
    if (m_matchesDirty) {
        rebuildMatches();
    }

    if (m_matches.empty()) {
        return SearchMatch{};
    }

    if (m_currentMatchIndex < 0) {
        // No current match, start from first
        m_currentMatchIndex = 0;
    } else {
        // Move to next
        m_currentMatchIndex++;
        if (m_currentMatchIndex >= static_cast<int>(m_matches.size())) {
            if (m_options.wrapAround) {
                m_currentMatchIndex = 0;
            } else {
                m_currentMatchIndex = static_cast<int>(m_matches.size()) - 1;
                return SearchMatch{};  // No more matches
            }
        }
    }

    SearchMatch match = m_matches[static_cast<size_t>(m_currentMatchIndex)];
    emit currentMatchChanged(match);
    return match;
}

SearchMatch SearchEngine::previousMatch() {
    if (m_matchesDirty) {
        rebuildMatches();
    }

    if (m_matches.empty()) {
        return SearchMatch{};
    }

    if (m_currentMatchIndex < 0) {
        // No current match, start from last
        m_currentMatchIndex = static_cast<int>(m_matches.size()) - 1;
    } else {
        // Move to previous
        m_currentMatchIndex--;
        if (m_currentMatchIndex < 0) {
            if (m_options.wrapAround) {
                m_currentMatchIndex = static_cast<int>(m_matches.size()) - 1;
            } else {
                m_currentMatchIndex = 0;
                return SearchMatch{};  // No more matches
            }
        }
    }

    SearchMatch match = m_matches[static_cast<size_t>(m_currentMatchIndex)];
    emit currentMatchChanged(match);
    return match;
}

SearchMatch SearchEngine::currentMatch() const {
    if (m_currentMatchIndex < 0 || m_currentMatchIndex >= static_cast<int>(m_matches.size())) {
        return SearchMatch{};
    }
    return m_matches[static_cast<size_t>(m_currentMatchIndex)];
}

bool SearchEngine::setCurrentMatchIndex(int index) {
    if (m_matchesDirty) {
        rebuildMatches();
    }

    if (index < 0 || index >= static_cast<int>(m_matches.size())) {
        return false;
    }

    m_currentMatchIndex = index;
    emit currentMatchChanged(m_matches[static_cast<size_t>(index)]);
    return true;
}

// =============================================================================
// Replace Operations (Task 9.5)
// =============================================================================

bool SearchEngine::replaceCurrent(QUndoStack* undoStack) {
    if (!m_document || m_currentMatchIndex < 0 ||
        m_currentMatchIndex >= static_cast<int>(m_matches.size())) {
        return false;
    }

    const SearchMatch& match = m_matches[static_cast<size_t>(m_currentMatchIndex)];

    if (undoStack) {
        // Create cursor positions from match
        CursorPosition startPos = absoluteToCursorPosition(
            m_document, static_cast<int>(match.start));
        CursorPosition endPos = absoluteToCursorPosition(
            m_document, static_cast<int>(match.end()));

        // Push replacement command to undo stack
        undoStack->push(new TextReplaceCommand(
            m_document, startPos, endPos, match.matchedText, m_replaceText));
    } else {
        // Direct replacement without undo (fallback)
        // Phase 11.6: Use QTextCursor for direct document modification
        QTextCursor cursor(m_document);
        cursor.setPosition(static_cast<int>(match.start));
        cursor.setPosition(static_cast<int>(match.end()), QTextCursor::KeepAnchor);
        cursor.insertText(m_replaceText);
    }

    // Rebuild matches after replacement
    m_matchesDirty = true;
    rebuildMatches();

    // Navigate to next match if available
    if (!m_matches.empty()) {
        // Find match at or after the replacement position
        // Account for text length change
        const size_t searchFrom = match.start + static_cast<size_t>(m_replaceText.length());

        for (size_t i = 0; i < m_matches.size(); ++i) {
            if (m_matches[i].start >= searchFrom) {
                m_currentMatchIndex = static_cast<int>(i);
                emit currentMatchChanged(m_matches[i]);
                return true;
            }
        }

        // If no match found after, wrap to first match if wrap around is enabled
        if (m_options.wrapAround && !m_matches.empty()) {
            m_currentMatchIndex = 0;
            emit currentMatchChanged(m_matches[0]);
        } else {
            m_currentMatchIndex = -1;
        }
    } else {
        m_currentMatchIndex = -1;
    }

    return true;
}

int SearchEngine::replaceAll(QUndoStack* undoStack) {
    if (!m_document || m_matches.empty()) {
        return 0;
    }

    // Ensure matches are up to date
    if (m_matchesDirty) {
        rebuildMatches();
    }

    const int count = static_cast<int>(m_matches.size());

    if (undoStack) {
        // Build replacements list for ReplaceAllCommand
        std::vector<ReplaceAllCommand::Replacement> replacements;
        replacements.reserve(m_matches.size());

        for (const auto& match : m_matches) {
            ReplaceAllCommand::Replacement repl;
            repl.startPos = static_cast<int>(match.start);
            repl.endPos = static_cast<int>(match.end());
            repl.oldText = match.matchedText;
            repl.newText = m_replaceText;
            replacements.push_back(repl);
        }

        // Create cursor position from first match
        CursorPosition cursor = absoluteToCursorPosition(
            m_document, static_cast<int>(m_matches[0].start));

        // Push replace all command to undo stack
        undoStack->push(new ReplaceAllCommand(
            m_document, cursor, replacements));
    } else {
        // Direct replacement without undo (fallback)
        // Phase 11.6: Use QTextCursor for direct document modification
        // Process in reverse order to maintain position validity
        for (auto it = m_matches.rbegin(); it != m_matches.rend(); ++it) {
            const SearchMatch& match = *it;
            QTextCursor cursor(m_document);
            cursor.setPosition(static_cast<int>(match.start));
            cursor.setPosition(static_cast<int>(match.end()), QTextCursor::KeepAnchor);
            cursor.insertText(m_replaceText);
        }
    }

    // Clear matches after replace all
    m_matchesDirty = true;
    rebuildMatches();
    m_currentMatchIndex = -1;

    return count;
}

// =============================================================================
// Highlight Access
// =============================================================================

const std::vector<SearchMatch>& SearchEngine::matches() const {
    if (m_matchesDirty) {
        const_cast<SearchEngine*>(this)->rebuildMatches();
    }
    return m_matches;
}

void SearchEngine::clear() {
    m_searchText.clear();
    m_replaceText.clear();
    m_matches.clear();
    m_currentMatchIndex = -1;
    m_matchesDirty = true;
    emit searchTextChanged(QString());
    emit matchesChanged();
}

bool SearchEngine::isActive() const {
    return !m_searchText.isEmpty();
}

// =============================================================================
// Private Methods
// =============================================================================

void SearchEngine::rebuildMatches() {
    m_matches.clear();
    m_matchesDirty = false;

    if (!m_document || m_searchText.isEmpty()) {
        emit matchesChanged();
        return;
    }

    QTextDocument* doc = m_document;
    if (!doc) {
        emit matchesChanged();
        return;
    }

    QTextDocument::FindFlags flags = buildFindFlags();

    if (m_options.useRegex) {
        // Regex search
        QRegularExpression::PatternOptions regexOptions = QRegularExpression::NoPatternOption;
        if (!m_options.caseSensitive) {
            regexOptions |= QRegularExpression::CaseInsensitiveOption;
        }

        QRegularExpression regex(m_searchText, regexOptions);
        if (!regex.isValid()) {
            emit matchesChanged();
            return;
        }

        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start);

        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(regex, cursor, flags);
            if (!cursor.isNull() && cursor.hasSelection()) {
                int start = cursor.selectionStart();
                int length = cursor.selectionEnd() - start;
                m_matches.push_back(buildMatch(static_cast<size_t>(start),
                                               static_cast<size_t>(length)));
            }
        }
    } else {
        // Plain text search
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start);

        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(m_searchText, cursor, flags);
            if (!cursor.isNull() && cursor.hasSelection()) {
                int start = cursor.selectionStart();
                int length = cursor.selectionEnd() - start;
                m_matches.push_back(buildMatch(static_cast<size_t>(start),
                                               static_cast<size_t>(length)));
            }
        }
    }

    // Sort matches by position (should already be sorted, but ensure)
    std::sort(m_matches.begin(), m_matches.end());

    // Emit signal
    emit matchesChanged();
}

SearchMatch SearchEngine::buildMatch(size_t start, size_t length) const {
    SearchMatch match;
    match.start = start;
    match.length = length;

    if (m_document) {
        QTextDocument* doc = m_document;
        if (doc) {
            QTextCursor cursor(doc);
            cursor.setPosition(static_cast<int>(start));

            // Get paragraph info directly from QTextDocument block
            QTextBlock block = cursor.block();
            match.paragraph = block.blockNumber();
            match.paragraphOffset = static_cast<int>(start) - block.position();

            // Get matched text
            cursor.setPosition(static_cast<int>(start + length), QTextCursor::KeepAnchor);
            match.matchedText = cursor.selectedText();
        }
    }

    return match;
}

QTextDocument::FindFlags SearchEngine::buildFindFlags() const {
    QTextDocument::FindFlags flags;

    if (m_options.caseSensitive) {
        flags |= QTextDocument::FindCaseSensitively;
    }

    if (m_options.wholeWord) {
        flags |= QTextDocument::FindWholeWords;
    }

    // Note: FindBackward is not used here because we iterate forward
    // and handle direction in navigation methods

    return flags;
}

SearchMatch SearchEngine::findMatch(size_t fromPosition, bool forward) {
    if (!m_document || m_searchText.isEmpty()) {
        return SearchMatch{};
    }

    // Ensure matches are up to date
    if (m_matchesDirty) {
        rebuildMatches();
    }

    if (m_matches.empty()) {
        return SearchMatch{};
    }

    if (forward) {
        // Find first match at or after fromPosition
        for (size_t i = 0; i < m_matches.size(); ++i) {
            if (m_matches[i].start >= fromPosition) {
                m_currentMatchIndex = static_cast<int>(i);
                emit currentMatchChanged(m_matches[i]);
                return m_matches[i];
            }
        }

        // No match found after position
        if (m_options.wrapAround && !m_matches.empty()) {
            m_currentMatchIndex = 0;
            emit currentMatchChanged(m_matches[0]);
            return m_matches[0];
        }
    } else {
        // Find last match before fromPosition
        for (int i = static_cast<int>(m_matches.size()) - 1; i >= 0; --i) {
            if (m_matches[static_cast<size_t>(i)].start < fromPosition) {
                m_currentMatchIndex = i;
                emit currentMatchChanged(m_matches[static_cast<size_t>(i)]);
                return m_matches[static_cast<size_t>(i)];
            }
        }

        // No match found before position
        if (m_options.wrapAround && !m_matches.empty()) {
            m_currentMatchIndex = static_cast<int>(m_matches.size()) - 1;
            emit currentMatchChanged(m_matches.back());
            return m_matches.back();
        }
    }

    return SearchMatch{};
}

void SearchEngine::updateCurrentMatchForPosition(size_t position) {
    if (m_matchesDirty) {
        rebuildMatches();
    }

    for (size_t i = 0; i < m_matches.size(); ++i) {
        const auto& match = m_matches[i];
        if (position >= match.start && position < match.end()) {
            m_currentMatchIndex = static_cast<int>(i);
            return;
        }
    }

    // Position not in any match
    m_currentMatchIndex = -1;
}

}  // namespace kalahari::editor
