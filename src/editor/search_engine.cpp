/// @file search_engine.cpp
/// @brief Search engine implementation for Find/Replace operations (OpenSpec #00044 Task 9.4)

#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/annotation.h>
#include <QTextBlock>
#include <QTextCursor>
#include <QRegularExpression>
#include <algorithm>
#include <iterator>

namespace kalahari::editor {

namespace {

/// Whether text[start, start + length) is a whole word: no letter or digit just before or
/// after it
bool isWholeWord(const QString& text, qsizetype start, qsizetype length) {
    const qsizetype end = start + length;
    return (start == 0 || !text.at(start - 1).isLetterOrNumber()) &&
           (end == text.size() || !text.at(end).isLetterOrNumber());
}

/// Matches ordered by where they start (std::lower_bound() by position)
bool startsBefore(const SearchMatch& match, size_t position) {
    return match.start < position;
}

}  // anonymous namespace

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
        disconnect(m_documentEdits);
        disconnect(m_documentChange);
        m_document = document;
        m_origin = QTextCursor();
        m_matchesDirty = true;
        m_currentMatchIndex = -1;
        m_matches.clear();

        if (m_document) {
            // The matches follow the edits: only the edited paragraphs are searched again
            // (searching the whole chapter on every keystroke made typing slow while the
            // find bar was open)
            m_documentChange = connect(m_document, &QTextDocument::contentsChange, this,
                                       &SearchEngine::onContentsChange);

            // QTextDocument reports the edited range only while it has a layout: without
            // one, the matches are searched again when needed (cached positions would be
            // stale, and replacing at one would change other text)
            m_documentEdits = connect(m_document, &QTextDocument::contentsChanged, this, [this]() {
                if (!m_changeReported || m_document->characterCount() != m_characterCount) {
                    m_matchesDirty = true;
                    m_currentMatchIndex = -1;
                }
                m_changeReported = false;
            });
        }
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

    // After the current match, or else after the cursor (setOrigin())
    const int count = static_cast<int>(m_matches.size());
    int index = m_currentMatchIndex >= 0
                    ? m_currentMatchIndex + 1
                    : firstMatchFrom(m_origin.isNull() ? 0 : static_cast<size_t>(m_origin.selectionEnd()));
    if (index >= count) {
        if (!m_options.wrapAround) {
            return SearchMatch{};  // No more matches
        }
        index = 0;
    }
    return selectMatch(index);
}

SearchMatch SearchEngine::previousMatch() {
    if (m_matchesDirty) {
        rebuildMatches();
    }

    if (m_matches.empty()) {
        return SearchMatch{};
    }

    // Before the current match, or else before the cursor (setOrigin())
    int index = m_currentMatchIndex >= 0
                    ? m_currentMatchIndex - 1
                    : firstMatchFrom(m_origin.isNull() ? 0 : static_cast<size_t>(m_origin.selectionStart())) - 1;
    if (index < 0) {
        if (!m_options.wrapAround) {
            return SearchMatch{};  // No more matches
        }
        index = static_cast<int>(m_matches.size()) - 1;
    }
    return selectMatch(index);
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

    selectMatch(index);
    return true;
}

void SearchEngine::setOrigin(size_t start, size_t end) {
    if (!m_document) {
        return;
    }

    // A cursor in the document, so the range moves with later edits
    const auto last = static_cast<size_t>(std::max(0, m_document->characterCount() - 1));
    if (m_origin.isNull()) {
        m_origin = QTextCursor(m_document);
    }
    m_origin.setPosition(static_cast<int>(std::min(start, last)));
    m_origin.setPosition(static_cast<int>(std::min(end, last)), QTextCursor::KeepAnchor);

    if (m_matchesDirty) {
        return;  // rebuildMatches() picks the current match
    }
    const int index = originMatchIndex();
    if (index != m_currentMatchIndex) {
        m_currentMatchIndex = index;
        emit matchesChanged();
    }
}

// =============================================================================
// Replace Operations (Task 9.5)
// =============================================================================

bool SearchEngine::replaceCurrent() {
    if (!m_document || m_currentMatchIndex < 0 ||
        m_currentMatchIndex >= static_cast<int>(m_matches.size())) {
        return false;
    }

    // A copy: the edit below updates m_matches
    const SearchMatch match = m_matches[static_cast<size_t>(m_currentMatchIndex)];

    // A direct edit, recorded by the document's native undo; the annotations of the
    // replaced text stay
    QTextCursor cursor(m_document);
    cursor.setPosition(static_cast<int>(match.start));
    cursor.setPosition(static_cast<int>(match.end()), QTextCursor::KeepAnchor);
    insertKeepingAnnotations(cursor, m_replaceText);

    // The matches have followed the edit (searched again here if the document reports
    // no edited ranges); go on with the match after the replaced text
    if (m_matchesDirty) {
        rebuildMatches();
    }
    int index = firstMatchFrom(match.start + static_cast<size_t>(m_replaceText.length()));
    if (index >= static_cast<int>(m_matches.size())) {
        if (!m_options.wrapAround || m_matches.empty()) {
            m_currentMatchIndex = -1;
            return true;
        }
        index = 0;
    }
    selectMatch(index);
    return true;
}

int SearchEngine::replaceAll() {
    // Ensure matches are up to date
    if (m_matchesDirty) {
        rebuildMatches();
    }

    if (!m_document || m_matches.empty()) {
        return 0;
    }

    const int count = static_cast<int>(m_matches.size());

    // Direct replacement — recorded by QTextDocument's native undo as ONE step
    // (beginEditBlock/endEditBlock). Process in reverse order to keep positions valid.
    QTextCursor cursor(m_document);
    cursor.beginEditBlock();
    for (auto it = m_matches.rbegin(); it != m_matches.rend(); ++it) {
        const SearchMatch& match = *it;
        cursor.setPosition(static_cast<int>(match.start));
        cursor.setPosition(static_cast<int>(match.end()), QTextCursor::KeepAnchor);
        insertKeepingAnnotations(cursor, m_replaceText);
    }
    cursor.endEditBlock();

    // The matches have followed the edit (searched again here if the document reports
    // no edited ranges)
    if (m_matchesDirty) {
        rebuildMatches();
    }
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
    m_characterCount = m_document ? m_document->characterCount() : 0;
    m_blockCount = m_document ? m_document->blockCount() : 0;

    if (m_document && prepareSearch()) {
        searchBlocks(m_document->begin(), m_document->lastBlock(), m_matches);
    }

    // A selected match is the current one (the text selected when the find bar opened)
    m_currentMatchIndex = originMatchIndex();
    emit matchesChanged();
}

bool SearchEngine::prepareSearch() {
    m_searchReady = false;
    if (m_searchText.isEmpty()) {
        return false;
    }

    if (m_options.useRegex) {
        // Unicode properties: \w and \b know Polish and other non-ASCII letters
        QRegularExpression::PatternOptions regexOptions =
            QRegularExpression::UseUnicodePropertiesOption;
        if (!m_options.caseSensitive) {
            regexOptions |= QRegularExpression::CaseInsensitiveOption;
        }
        m_regex = QRegularExpression(m_searchText, regexOptions);
        m_searchReady = m_regex.isValid();
    } else {
        // A space matches a non-breaking space, and the other way round
        m_needle = m_searchText;
        m_needle.replace(QChar::Nbsp, u' ');
        m_searchReady = true;
    }
    return m_searchReady;
}

void SearchEngine::searchBlocks(QTextBlock block, const QTextBlock& last,
                                std::vector<SearchMatch>& found) const {
    const Qt::CaseSensitivity sensitivity =
        m_options.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

    // Paragraph by paragraph, as QTextDocument::find() searches: a match never spans two
    for (int number = block.blockNumber(); block.isValid(); block = block.next(), ++number) {
        const int position = block.position();
        const QString original = block.text();
        QString text = original;
        text.replace(QChar::Nbsp, u' ');  // copies the text only if it has one

        const auto add = [&](qsizetype offset, qsizetype length) {
            SearchMatch match;
            match.start = static_cast<size_t>(position + offset);
            match.length = static_cast<size_t>(length);
            match.paragraph = number;
            match.paragraphOffset = static_cast<int>(offset);
            match.matchedText = original.mid(offset, length);
            found.push_back(std::move(match));
        };

        if (m_options.useRegex) {
            // An expression that can match nothing (x*, ^) finds only the text it does match
            QRegularExpressionMatchIterator it = m_regex.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                const qsizetype length = match.capturedLength();
                if (length > 0 &&
                    (!m_options.wholeWord || isWholeWord(text, match.capturedStart(), length))) {
                    add(match.capturedStart(), length);
                }
            }
        } else {
            qsizetype from = 0;
            qsizetype offset = 0;
            while ((offset = text.indexOf(m_needle, from, sensitivity)) >= 0) {
                if (m_options.wholeWord && !isWholeWord(text, offset, m_needle.size())) {
                    from = offset + 1;  // a whole word can still start within this one
                    continue;
                }
                add(offset, m_needle.size());
                from = offset + m_needle.size();
            }
        }

        if (block == last) {
            break;
        }
    }
}

void SearchEngine::onContentsChange(int position, int charsRemoved, int charsAdded) {
    m_changeReported = true;
    if (m_matchesDirty || !m_document) {
        m_currentMatchIndex = -1;
        return;  // searched again when needed
    }

    // Lengths that do not add up: search everything again
    const int characterCount = m_document->characterCount();
    if (position < 0 || charsRemoved < 0 || charsAdded < 0 ||
        position + charsRemoved > m_characterCount ||
        characterCount != m_characterCount - charsRemoved + charsAdded) {
        m_matchesDirty = true;
        m_currentMatchIndex = -1;
        return;
    }

    // The edited paragraphs, whole: a match never spans two, so the matches before them stay
    // and the ones after them only move. Their end is a paragraph end in the document before
    // the edit, too (the text after the edited range is unchanged).
    const int lastPosition = characterCount - 1;
    const QTextBlock first = m_document->findBlock(std::min(position, lastPosition));
    const QTextBlock last = m_document->findBlock(std::min(position + charsAdded, lastPosition));
    if (!first.isValid() || !last.isValid()) {
        m_matchesDirty = true;
        m_currentMatchIndex = -1;
        return;
    }
    const qsizetype shift = charsAdded - charsRemoved;
    const int blockShift = m_document->blockCount() - m_blockCount;
    const auto regionStart = static_cast<size_t>(first.position());
    const auto oldRegionEnd = static_cast<size_t>(last.position() + last.length() - shift);

    std::vector<SearchMatch> found;
    if (m_searchReady) {
        searchBlocks(first, last, found);
    }

    auto begin = std::lower_bound(m_matches.begin(), m_matches.end(), regionStart, startsBefore);
    auto end = std::lower_bound(begin, m_matches.end(), oldRegionEnd, startsBefore);
    if (shift != 0 || blockShift != 0) {
        for (auto it = end; it != m_matches.end(); ++it) {
            it->start = static_cast<size_t>(static_cast<qsizetype>(it->start) + shift);
            it->paragraph += blockShift;
        }
    }
    if (end - begin == static_cast<std::ptrdiff_t>(found.size())) {
        // As many as before (a format change, typing outside the matches): in place
        std::move(found.begin(), found.end(), begin);
    } else {
        m_matches.insert(m_matches.erase(begin, end), std::make_move_iterator(found.begin()),
                         std::make_move_iterator(found.end()));
    }
    m_characterCount = characterCount;
    m_blockCount = m_document->blockCount();

    // The selection the search goes on from has moved with the edit as well
    m_currentMatchIndex = originMatchIndex();
    emit matchesChanged();
}

int SearchEngine::firstMatchFrom(size_t position) const {
    return static_cast<int>(
        std::lower_bound(m_matches.begin(), m_matches.end(), position, startsBefore) -
        m_matches.begin());
}

int SearchEngine::originMatchIndex() const {
    if (m_origin.isNull() || !m_origin.hasSelection()) {
        return -1;
    }
    const auto start = static_cast<size_t>(m_origin.selectionStart());
    const auto end = static_cast<size_t>(m_origin.selectionEnd());
    const int index = firstMatchFrom(start);
    if (index < static_cast<int>(m_matches.size())) {
        const SearchMatch& match = m_matches[static_cast<size_t>(index)];
        if (match.start == start && match.end() == end) {
            return index;
        }
    }
    return -1;
}

SearchMatch SearchEngine::selectMatch(int index) {
    m_currentMatchIndex = index;
    // A copy: whoever moves to the match may change the matches meanwhile
    const SearchMatch match = m_matches[static_cast<size_t>(index)];
    emit currentMatchChanged(match);
    return match;
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

    // Forward: the first match at or after fromPosition; backward: the last one before it
    const int count = static_cast<int>(m_matches.size());
    int index = firstMatchFrom(fromPosition) - (forward ? 0 : 1);
    if (index < 0 || index >= count) {
        if (!m_options.wrapAround) {
            return SearchMatch{};
        }
        index = forward ? 0 : count - 1;
    }
    return selectMatch(index);
}

}  // namespace kalahari::editor
