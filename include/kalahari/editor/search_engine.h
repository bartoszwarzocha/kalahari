/// @file search_engine.h
/// @brief Search engine for Find/Replace operations (OpenSpec #00044 Task 9.4)
///
/// SearchEngine provides text search functionality with support for:
/// - Case-sensitive/insensitive search
/// - Whole word matching
/// - Regular expression search
/// - Forward/backward navigation
/// - Wrap around
/// - Match highlighting

#pragma once

// Phase 11.6: Removed text_buffer.h - using QTextDocument directly
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QTextCursor>
#include <QTextDocument>
#include <vector>

class QTextBlock;

namespace kalahari::editor {

// =============================================================================
// Search Options
// =============================================================================

/// @brief Configuration options for search operations
struct SearchOptions {
    bool caseSensitive = false;   ///< Match case exactly
    bool wholeWord = false;       ///< Match whole words only
    bool useRegex = false;        ///< Interpret search text as regex
    bool searchBackward = false;  ///< Search in reverse direction
    bool wrapAround = true;       ///< Wrap to start/end when reaching document boundary
};

// =============================================================================
// Search Match
// =============================================================================

/// @brief Represents a single search match in the document
struct SearchMatch {
    size_t start = 0;           ///< Absolute character position (0-based)
    size_t length = 0;          ///< Match length in characters
    int paragraph = 0;          ///< Paragraph index containing the match
    int paragraphOffset = 0;    ///< Character offset within the paragraph
    QString matchedText;        ///< The actual matched text

    /// @brief Check if match is valid
    /// @return true if match has non-zero length
    bool isValid() const { return length > 0; }

    /// @brief Get end position (exclusive)
    /// @return Absolute character position after the match
    size_t end() const { return start + length; }

    /// @brief Compare matches by position
    bool operator<(const SearchMatch& other) const { return start < other.start; }

    /// @brief Equality comparison
    bool operator==(const SearchMatch& other) const {
        return start == other.start && length == other.length;
    }
};

// =============================================================================
// Search Engine
// =============================================================================

/// @brief Search engine for text find/replace operations
///
/// Usage:
/// @code
/// SearchEngine engine;
/// engine.setDocument(document);
/// engine.setSearchText("hello");
/// engine.setOptions({.caseSensitive = false, .wholeWord = true});
///
/// // Find all matches
/// auto matches = engine.findAll();
///
/// // Navigate through matches
/// while (engine.nextMatch().isValid()) {
///     // Process current match
/// }
/// @endcode
class SearchEngine : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent QObject
    explicit SearchEngine(QObject* parent = nullptr);

    /// @brief Destructor
    ~SearchEngine() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Set the document to search in (Phase 11.6)
    /// @param document Pointer to QTextDocument (not owned)
    void setDocument(QTextDocument* document);

    /// @brief Get the current document
    /// @return Pointer to QTextDocument
    QTextDocument* document() const;

    /// @brief Set the search text
    /// @param text Text to search for
    void setSearchText(const QString& text);

    /// @brief Get the current search text
    /// @return Current search text
    QString searchText() const;

    /// @brief Set the replacement text (for replace operations)
    /// @param text Replacement text
    void setReplaceText(const QString& text);

    /// @brief Get the current replacement text
    /// @return Current replacement text
    QString replaceText() const;

    /// @brief Set search options
    /// @param options Search configuration
    void setOptions(const SearchOptions& options);

    /// @brief Get current search options
    /// @return Current search configuration
    SearchOptions options() const;

    // =========================================================================
    // Search Operations
    // =========================================================================

    /// @brief Find next match from given position
    /// @param fromPosition Absolute character position to start from
    /// @return Found match, or invalid match if not found
    SearchMatch findNext(size_t fromPosition);

    /// @brief Find previous match from given position
    /// @param fromPosition Absolute character position to start from
    /// @return Found match, or invalid match if not found
    SearchMatch findPrevious(size_t fromPosition);

    /// @brief Find all matches in the document
    /// @return Vector of all matches
    std::vector<SearchMatch> findAll();

    // =========================================================================
    // Navigation
    // =========================================================================

    /// @brief Get current match index (0-based)
    /// @return Current match index, or -1 if no match selected
    int currentMatchIndex() const;

    /// @brief Get total number of matches
    /// @return Total match count
    int totalMatchCount() const;

    /// @brief Navigate to next match
    /// @return Next match, or invalid match if none
    SearchMatch nextMatch();

    /// @brief Navigate to previous match
    /// @return Previous match, or invalid match if none
    SearchMatch previousMatch();

    /// @brief Get current match without navigation
    /// @return Current match, or invalid match if none
    SearchMatch currentMatch() const;

    /// @brief Set current match by index
    /// @param index Match index (0-based)
    /// @return true if index is valid
    bool setCurrentMatchIndex(int index);

    /// @brief Set where the search goes on from: the editor's selection, or its cursor
    ///
    /// A selection that is exactly a match makes that match the current one; anything
    /// else clears the current match, so nextMatch() and previousMatch() continue from
    /// here instead of from the first or the last match. The range follows later edits.
    /// @param start Absolute start of the selection, or the cursor position
    /// @param end Absolute end of the selection (equal to @p start without one)
    void setOrigin(size_t start, size_t end);

    // =========================================================================
    // Replace Operations
    // =========================================================================

    /// @brief Replace current match with replacement text and go on to the next match
    ///
    /// The edit goes straight into the document, so its native undo records it.
    /// @return true if replacement was made
    bool replaceCurrent();

    /// @brief Replace all matches with replacement text
    ///
    /// The edit goes straight into the document as one undo step.
    /// @return Number of replacements made
    int replaceAll();

    // =========================================================================
    // Highlight Access
    // =========================================================================

    /// @brief Get all cached matches for highlighting
    /// @return Reference to cached matches vector
    const std::vector<SearchMatch>& matches() const;

    /// @brief Clear search state and matches
    void clear();

    /// @brief Check if search is active (has search text)
    /// @return true if search text is non-empty
    bool isActive() const;

signals:
    /// @brief Emitted when the matches change, or which of them is the current one
    /// without a navigation (setOrigin())
    void matchesChanged();

    /// @brief Emitted when current match changes
    /// @param match The new current match
    void currentMatchChanged(const SearchMatch& match);

    /// @brief Emitted when search text changes
    /// @param text The new search text
    void searchTextChanged(const QString& text);

private:
    /// @brief Rebuild match cache from buffer
    void rebuildMatches();

    /// @brief Prepare the search text (or the regular expression) for searchBlocks()
    /// @return false if there is nothing to search for
    bool prepareSearch();

    /// @brief Append the matches in the paragraphs from @p block to @p last
    /// @param block First paragraph to search
    /// @param last Last paragraph to search
    /// @param found Receives the matches, in document order
    void searchBlocks(QTextBlock block, const QTextBlock& last,
                      std::vector<SearchMatch>& found) const;

    /// @brief Bring the matches up to date with an edit of the document
    ///
    /// Only the edited paragraphs are searched again, the matches after them move with the
    /// text (QTextDocument::contentsChange).
    void onContentsChange(int position, int charsRemoved, int charsAdded);

    /// @brief Index of the first match starting at or after @p position
    /// @return The match count if there is none
    int firstMatchFrom(size_t position) const;

    /// @brief Index of the match that is exactly the origin's selection, or -1
    int originMatchIndex() const;

    /// @brief Make the match at @p index the current one and announce it
    /// @return A copy of the match
    SearchMatch selectMatch(int index);

    /// @brief Find match at or after position
    /// @param fromPosition Start position for search
    /// @param forward true for forward search, false for backward
    /// @return Found match or invalid match
    SearchMatch findMatch(size_t fromPosition, bool forward);

    QTextDocument* m_document = nullptr;      ///< QTextDocument (not owned) - Phase 11.6
    QMetaObject::Connection m_documentEdits;  ///< Notices edits the matches did not follow
    QMetaObject::Connection m_documentChange; ///< Follows the edits with the matches
    QTextCursor m_origin;                     ///< Where the search goes on from (setOrigin())
    QString m_searchText;                     ///< Current search text
    QString m_replaceText;                    ///< Current replacement text
    SearchOptions m_options;                  ///< Current search options
    QString m_needle;                         ///< Search text as matched (plain search)
    QRegularExpression m_regex;               ///< Search text as matched (regex search)
    bool m_searchReady = false;               ///< m_needle or m_regex can be searched for
    std::vector<SearchMatch> m_matches;       ///< Cached matches
    int m_currentMatchIndex = -1;             ///< Current match index (-1 = none)
    bool m_matchesDirty = true;               ///< Matches need rebuild
    int m_characterCount = 0;                 ///< Document length the matches are for
    int m_blockCount = 0;                     ///< Paragraph count the matches are for
    bool m_changeReported = false;            ///< The document reported the latest edit's range
};

}  // namespace kalahari::editor
