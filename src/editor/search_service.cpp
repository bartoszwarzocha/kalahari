/// @file search_service.cpp
/// @brief QTextDocument-based search service implementation (OpenSpec #00043 Phase 11.7)

#include <kalahari/editor/search_service.h>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QRegularExpression>

namespace kalahari::editor {

// =============================================================================
// SearchService Implementation
// =============================================================================

QTextDocument::FindFlags SearchService::buildFindFlags(const DocSearchOptions& options, bool backward) const {
    QTextDocument::FindFlags flags;
    if (options.caseSensitive) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    if (options.wholeWord) {
        flags |= QTextDocument::FindWholeWords;
    }
    if (backward) {
        flags |= QTextDocument::FindBackward;
    }
    return flags;
}

DocSearchMatch SearchService::buildMatch(const QTextCursor& cursor) const {
    if (!cursor.hasSelection()) {
        return {};
    }

    DocSearchMatch match;
    match.position = cursor.selectionStart();
    match.length = cursor.selectionEnd() - cursor.selectionStart();
    match.matchedText = cursor.selectedText();

    QTextBlock block = cursor.document()->findBlock(match.position);
    match.blockNumber = block.blockNumber();
    match.positionInBlock = match.position - block.position();

    return match;
}

QVector<DocSearchMatch> SearchService::findAll(
    QTextDocument* document,
    const QString& query,
    const DocSearchOptions& options)
{
    QVector<DocSearchMatch> results;
    if (!document || query.isEmpty()) {
        return results;
    }

    QTextDocument::FindFlags flags = buildFindFlags(options);
    QTextCursor cursor(document);

    while (true) {
        QTextCursor result;
        if (options.useRegex) {
            QRegularExpression::PatternOptions regexOpts;
            if (!options.caseSensitive) {
                regexOpts |= QRegularExpression::CaseInsensitiveOption;
            }
            QRegularExpression regex(query, regexOpts);
            if (!regex.isValid()) {
                // Invalid regex pattern - return empty results
                return results;
            }
            result = document->find(regex, cursor, flags);
        } else {
            result = document->find(query, cursor, flags);
        }

        if (result.isNull() || !result.hasSelection()) {
            break;
        }

        results.append(buildMatch(result));
        cursor = result;
    }

    return results;
}

DocSearchMatch SearchService::findNext(
    QTextDocument* document,
    const QString& query,
    int fromPosition,
    const DocSearchOptions& options)
{
    if (!document || query.isEmpty()) {
        return {};
    }

    QTextDocument::FindFlags flags = buildFindFlags(options, false);
    QTextCursor cursor(document);
    cursor.setPosition(fromPosition);

    QTextCursor result;
    if (options.useRegex) {
        QRegularExpression::PatternOptions regexOpts;
        if (!options.caseSensitive) {
            regexOpts |= QRegularExpression::CaseInsensitiveOption;
        }
        QRegularExpression regex(query, regexOpts);
        if (!regex.isValid()) {
            return {};
        }
        result = document->find(regex, cursor, flags);
    } else {
        result = document->find(query, cursor, flags);
    }

    // Wrap around if enabled and not found
    if (result.isNull() && options.wrapAround && fromPosition > 0) {
        cursor.setPosition(0);
        if (options.useRegex) {
            QRegularExpression::PatternOptions regexOpts;
            if (!options.caseSensitive) {
                regexOpts |= QRegularExpression::CaseInsensitiveOption;
            }
            QRegularExpression regex(query, regexOpts);
            result = document->find(regex, cursor, flags);
        } else {
            result = document->find(query, cursor, flags);
        }
    }

    return buildMatch(result);
}

DocSearchMatch SearchService::findPrevious(
    QTextDocument* document,
    const QString& query,
    int fromPosition,
    const DocSearchOptions& options)
{
    if (!document || query.isEmpty()) {
        return {};
    }

    QTextDocument::FindFlags flags = buildFindFlags(options, true);
    QTextCursor cursor(document);
    cursor.setPosition(fromPosition);

    QTextCursor result;
    if (options.useRegex) {
        QRegularExpression::PatternOptions regexOpts;
        if (!options.caseSensitive) {
            regexOpts |= QRegularExpression::CaseInsensitiveOption;
        }
        QRegularExpression regex(query, regexOpts);
        if (!regex.isValid()) {
            return {};
        }
        result = document->find(regex, cursor, flags);
    } else {
        result = document->find(query, cursor, flags);
    }

    // Wrap around if enabled and not found
    if (result.isNull() && options.wrapAround) {
        cursor.movePosition(QTextCursor::End);
        if (options.useRegex) {
            QRegularExpression::PatternOptions regexOpts;
            if (!options.caseSensitive) {
                regexOpts |= QRegularExpression::CaseInsensitiveOption;
            }
            QRegularExpression regex(query, regexOpts);
            result = document->find(regex, cursor, flags);
        } else {
            result = document->find(query, cursor, flags);
        }
    }

    return buildMatch(result);
}

bool SearchService::replace(
    QTextDocument* document,
    const DocSearchMatch& match,
    const QString& replacement)
{
    if (!document || !match.isValid()) {
        return false;
    }

    QTextCursor cursor(document);
    cursor.setPosition(match.position);
    cursor.setPosition(match.position + match.length, QTextCursor::KeepAnchor);

    // QTextDocument's built-in undo handles this
    cursor.insertText(replacement);

    return true;
}

int SearchService::replaceAll(
    QTextDocument* document,
    const QString& query,
    const QString& replacement,
    const DocSearchOptions& options)
{
    if (!document || query.isEmpty()) {
        return 0;
    }

    QVector<DocSearchMatch> matches = findAll(document, query, options);
    if (matches.isEmpty()) {
        return 0;
    }

    // Replace in reverse order to maintain position validity
    QTextCursor cursor(document);
    cursor.beginEditBlock();  // Group for undo

    for (int i = matches.size() - 1; i >= 0; --i) {
        const DocSearchMatch& match = matches[i];
        cursor.setPosition(match.position);
        cursor.setPosition(match.position + match.length, QTextCursor::KeepAnchor);
        cursor.insertText(replacement);
    }

    cursor.endEditBlock();

    return matches.size();
}

// =============================================================================
// SearchSession Implementation
// =============================================================================

SearchSession::SearchSession(QObject* parent)
    : QObject(parent)
    , m_defaultService(std::make_unique<SearchService>())
{
}

SearchSession::~SearchSession() = default;

void SearchSession::setDocument(QTextDocument* document) {
    m_document = document;
    performSearch();
}

void SearchSession::setSearchService(ISearchService* service) {
    m_externalService = service;
}

ISearchService* SearchSession::searchService() {
    return m_externalService ? m_externalService : m_defaultService.get();
}

void SearchSession::setSearchText(const QString& text) {
    if (m_searchText == text) {
        return;
    }
    m_searchText = text;
    emit searchTextChanged(text);
    performSearch();
}

void SearchSession::setReplaceText(const QString& text) {
    m_replaceText = text;
}

void SearchSession::setOptions(const DocSearchOptions& options) {
    m_options = options;
    performSearch();
}

void SearchSession::performSearch() {
    m_matches.clear();
    m_currentMatchIndex = -1;

    if (m_document && !m_searchText.isEmpty()) {
        m_matches = searchService()->findAll(m_document, m_searchText, m_options);
        if (!m_matches.isEmpty()) {
            m_currentMatchIndex = 0;
        }
    }

    emit matchesChanged();
    if (m_currentMatchIndex >= 0) {
        emit currentMatchChanged(m_matches[m_currentMatchIndex]);
    }
}

DocSearchMatch SearchSession::nextMatch() {
    if (m_matches.isEmpty()) {
        return {};
    }

    m_currentMatchIndex = (m_currentMatchIndex + 1) % m_matches.size();
    emit currentMatchChanged(m_matches[m_currentMatchIndex]);
    return m_matches[m_currentMatchIndex];
}

DocSearchMatch SearchSession::previousMatch() {
    if (m_matches.isEmpty()) {
        return {};
    }

    m_currentMatchIndex = (m_currentMatchIndex - 1 + m_matches.size()) % m_matches.size();
    emit currentMatchChanged(m_matches[m_currentMatchIndex]);
    return m_matches[m_currentMatchIndex];
}

DocSearchMatch SearchSession::currentMatch() const {
    if (m_currentMatchIndex < 0 || m_currentMatchIndex >= m_matches.size()) {
        return {};
    }
    return m_matches[m_currentMatchIndex];
}

bool SearchSession::setCurrentMatchIndex(int index) {
    if (index < 0 || index >= m_matches.size()) {
        return false;
    }
    m_currentMatchIndex = index;
    emit currentMatchChanged(m_matches[m_currentMatchIndex]);
    return true;
}

bool SearchSession::replaceCurrent() {
    if (!m_document || m_currentMatchIndex < 0) {
        return false;
    }

    bool result = searchService()->replace(m_document, currentMatch(), m_replaceText);
    if (result) {
        performSearch();  // Rebuild matches
    }
    return result;
}

int SearchSession::replaceAll() {
    if (!m_document || m_searchText.isEmpty()) {
        return 0;
    }

    int count = searchService()->replaceAll(m_document, m_searchText, m_replaceText, m_options);
    if (count > 0) {
        performSearch();  // Rebuild matches (will be empty)
    }
    return count;
}

void SearchSession::clear() {
    m_searchText.clear();
    m_replaceText.clear();
    m_matches.clear();
    m_currentMatchIndex = -1;
    emit searchTextChanged(QString());
    emit matchesChanged();
}

}  // namespace kalahari::editor
