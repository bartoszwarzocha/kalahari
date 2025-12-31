/// @file search_service.h
/// @brief QTextDocument-based search service API (OpenSpec #00043 Phase 11.7)
///
/// Provides a clean interface for search operations on QTextDocument:
/// - ISearchService: Abstract interface for dependency injection
/// - SearchService: Concrete implementation
/// - SearchSession: UI navigation state manager
///
/// Phase 11.7: Unified with SearchEngine - both use QTextDocument directly.

#pragma once

#include <QString>
#include <QVector>
#include <QObject>
#include <QTextDocument>
#include <memory>

class QTextCursor;
class QUndoStack;

namespace kalahari::editor {

// =============================================================================
// Search Options (QTextDocument-based API)
// =============================================================================

/// @brief Configuration options for search operations on QTextDocument
/// @note Named DocSearchOptions to avoid conflict with SearchEngine's SearchOptions
struct DocSearchOptions {
    bool caseSensitive = false;  ///< Match case exactly
    bool wholeWord = false;      ///< Match whole words only
    bool useRegex = false;       ///< Interpret search text as regex
    bool wrapAround = true;      ///< Wrap to start/end when reaching boundary
};

// =============================================================================
// Search Match (QTextDocument-based API)
// =============================================================================

/// @brief Represents a single search match in a QTextDocument
/// @note Named DocSearchMatch to avoid conflict with SearchEngine's SearchMatch
struct DocSearchMatch {
    int position = 0;           ///< Absolute character position in document
    int length = 0;             ///< Match length in characters
    int blockNumber = 0;        ///< Block number (paragraph index)
    int positionInBlock = 0;    ///< Position within block
    QString matchedText;        ///< The matched text

    /// @brief Check if match is valid
    /// @return true if match has non-zero length
    bool isValid() const { return length > 0; }

    /// @brief Get end position (exclusive)
    /// @return Absolute character position after the match
    int end() const { return position + length; }

    /// @brief Equality comparison
    bool operator==(const DocSearchMatch& other) const {
        return position == other.position && length == other.length;
    }

    /// @brief Compare matches by position
    bool operator<(const DocSearchMatch& other) const { return position < other.position; }
};

// =============================================================================
// ISearchService Interface
// =============================================================================

/// @brief Abstract interface for search operations on QTextDocument
///
/// Use this interface for dependency injection and testing:
/// @code
/// class MyEditor {
///     ISearchService* m_search;
/// public:
///     void setSearchService(ISearchService* service) { m_search = service; }
/// };
/// @endcode
class ISearchService {
public:
    virtual ~ISearchService() = default;

    /// @brief Find all matches in document
    /// @param document QTextDocument to search in
    /// @param query Search text
    /// @param options Search configuration
    /// @return Vector of all matches
    virtual QVector<DocSearchMatch> findAll(
        QTextDocument* document,
        const QString& query,
        const DocSearchOptions& options = {}) = 0;

    /// @brief Find next match from position
    /// @param document QTextDocument to search in
    /// @param query Search text
    /// @param fromPosition Starting position
    /// @param options Search configuration
    /// @return Found match, or invalid match if not found
    virtual DocSearchMatch findNext(
        QTextDocument* document,
        const QString& query,
        int fromPosition,
        const DocSearchOptions& options = {}) = 0;

    /// @brief Find previous match from position
    /// @param document QTextDocument to search in
    /// @param query Search text
    /// @param fromPosition Starting position
    /// @param options Search configuration
    /// @return Found match, or invalid match if not found
    virtual DocSearchMatch findPrevious(
        QTextDocument* document,
        const QString& query,
        int fromPosition,
        const DocSearchOptions& options = {}) = 0;

    /// @brief Replace text at match position
    /// @param document QTextDocument to modify
    /// @param match Match to replace
    /// @param replacement Replacement text
    /// @return true if replacement was made
    virtual bool replace(
        QTextDocument* document,
        const DocSearchMatch& match,
        const QString& replacement) = 0;

    /// @brief Replace all matches, returns count
    /// @param document QTextDocument to modify
    /// @param query Search text
    /// @param replacement Replacement text
    /// @param options Search configuration
    /// @return Number of replacements made
    virtual int replaceAll(
        QTextDocument* document,
        const QString& query,
        const QString& replacement,
        const DocSearchOptions& options = {}) = 0;
};

// =============================================================================
// SearchService Implementation
// =============================================================================

/// @brief Concrete implementation of ISearchService
///
/// Usage:
/// @code
/// SearchService service;
/// QTextDocument doc;
/// doc.setPlainText("Hello World Hello");
///
/// auto matches = service.findAll(&doc, "Hello");
/// // matches.size() == 2
///
/// int count = service.replaceAll(&doc, "Hello", "Hi");
/// // count == 2, doc now contains "Hi World Hi"
/// @endcode
class SearchService : public ISearchService {
public:
    SearchService() = default;

    QVector<DocSearchMatch> findAll(
        QTextDocument* document,
        const QString& query,
        const DocSearchOptions& options = {}) override;

    DocSearchMatch findNext(
        QTextDocument* document,
        const QString& query,
        int fromPosition,
        const DocSearchOptions& options = {}) override;

    DocSearchMatch findPrevious(
        QTextDocument* document,
        const QString& query,
        int fromPosition,
        const DocSearchOptions& options = {}) override;

    bool replace(
        QTextDocument* document,
        const DocSearchMatch& match,
        const QString& replacement) override;

    int replaceAll(
        QTextDocument* document,
        const QString& query,
        const QString& replacement,
        const DocSearchOptions& options = {}) override;

private:
    /// @brief Build Qt find flags from options
    QTextDocument::FindFlags buildFindFlags(const DocSearchOptions& options, bool backward = false) const;

    /// @brief Build DocSearchMatch from cursor
    DocSearchMatch buildMatch(const QTextCursor& cursor) const;
};

// =============================================================================
// SearchSession - Navigation state manager for UI
// =============================================================================

/// @brief Navigation state manager for search UI
///
/// Maintains search state and provides navigation through matches:
/// @code
/// SearchSession session;
/// session.setDocument(&doc);
/// session.setSearchText("Hello");
///
/// // Navigate through matches
/// DocSearchMatch m1 = session.nextMatch();
/// DocSearchMatch m2 = session.nextMatch();
///
/// // Replace current
/// session.setReplaceText("Hi");
/// session.replaceCurrent();
/// @endcode
class SearchSession : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent QObject
    explicit SearchSession(QObject* parent = nullptr);

    /// @brief Destructor
    ~SearchSession() override;

    /// @brief Set the document to search in
    /// @param document QTextDocument (not owned)
    void setDocument(QTextDocument* document);

    /// @brief Get current document
    /// @return Pointer to document
    QTextDocument* document() const { return m_document; }

    /// @brief Set external search service (for testing)
    /// @param service ISearchService (not owned)
    void setSearchService(ISearchService* service);

    // =========================================================================
    // Search text
    // =========================================================================

    /// @brief Set search text
    /// @param text Text to search for
    void setSearchText(const QString& text);

    /// @brief Get current search text
    /// @return Search text
    QString searchText() const { return m_searchText; }

    // =========================================================================
    // Replace text
    // =========================================================================

    /// @brief Set replacement text
    /// @param text Replacement text
    void setReplaceText(const QString& text);

    /// @brief Get current replacement text
    /// @return Replacement text
    QString replaceText() const { return m_replaceText; }

    // =========================================================================
    // Options
    // =========================================================================

    /// @brief Set search options
    /// @param options Search configuration
    void setOptions(const DocSearchOptions& options);

    /// @brief Get current search options
    /// @return Search configuration
    DocSearchOptions options() const { return m_options; }

    // =========================================================================
    // Navigation
    // =========================================================================

    /// @brief Navigate to next match
    /// @return Next match, or invalid match if none
    DocSearchMatch nextMatch();

    /// @brief Navigate to previous match
    /// @return Previous match, or invalid match if none
    DocSearchMatch previousMatch();

    /// @brief Get current match without navigation
    /// @return Current match, or invalid match if none
    DocSearchMatch currentMatch() const;

    /// @brief Set current match by index
    /// @param index Match index (0-based)
    /// @return true if index is valid
    bool setCurrentMatchIndex(int index);

    /// @brief Get current match index
    /// @return Current match index, or -1 if none
    int currentMatchIndex() const { return m_currentMatchIndex; }

    /// @brief Get total number of matches
    /// @return Total match count
    int totalMatchCount() const { return m_matches.size(); }

    /// @brief Get all cached matches
    /// @return Reference to matches vector
    const QVector<DocSearchMatch>& matches() const { return m_matches; }

    // =========================================================================
    // Replace
    // =========================================================================

    /// @brief Replace current match with replacement text
    /// @return true if replacement was made
    bool replaceCurrent();

    /// @brief Replace all matches with replacement text
    /// @return Number of replacements made
    int replaceAll();

    // =========================================================================
    // State
    // =========================================================================

    /// @brief Clear search state
    void clear();

    /// @brief Check if search is active
    /// @return true if search text is non-empty and document is set
    bool isActive() const { return !m_searchText.isEmpty() && m_document; }

signals:
    /// @brief Emitted when matches list changes
    void matchesChanged();

    /// @brief Emitted when current match changes
    /// @param match The new current match
    void currentMatchChanged(const DocSearchMatch& match);

    /// @brief Emitted when search text changes
    /// @param text The new search text
    void searchTextChanged(const QString& text);

private:
    /// @brief Rebuild match cache from document
    void performSearch();

    /// @brief Get search service (external or default)
    ISearchService* searchService();

    QTextDocument* m_document = nullptr;              ///< Document (not owned)
    ISearchService* m_externalService = nullptr;      ///< External service (not owned)
    std::unique_ptr<SearchService> m_defaultService;  ///< Default service (owned)

    QString m_searchText;                             ///< Current search text
    QString m_replaceText;                            ///< Current replacement text
    DocSearchOptions m_options;                       ///< Current search options
    QVector<DocSearchMatch> m_matches;                ///< Cached matches
    int m_currentMatchIndex = -1;                     ///< Current match index
};

}  // namespace kalahari::editor

// Register DocSearchMatch with Qt meta-type system for use in signals/slots
Q_DECLARE_METATYPE(kalahari::editor::DocSearchMatch)
