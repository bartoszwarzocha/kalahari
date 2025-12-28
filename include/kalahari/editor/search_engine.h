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

#include <kalahari/editor/text_buffer.h>
#include <QObject>
#include <QString>
#include <QTextDocument>
#include <vector>

class QUndoStack;

namespace kalahari::editor {

class FormatLayer;

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
/// engine.setBuffer(&buffer);
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

    /// @brief Set the text buffer to search in
    /// @param buffer Pointer to TextBuffer (not owned)
    void setBuffer(TextBuffer* buffer);

    /// @brief Get the current text buffer
    /// @return Pointer to TextBuffer
    TextBuffer* buffer() const;

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

    // =========================================================================
    // Replace Operations (stubs for Task 9.5)
    // =========================================================================

    /// @brief Replace current match with replacement text
    /// @param undoStack Undo stack for operation (required)
    /// @param formatLayer Format layer for the buffer (required)
    /// @return true if replacement was made
    bool replaceCurrent(QUndoStack* undoStack, FormatLayer* formatLayer);

    /// @brief Replace all matches with replacement text
    /// @param undoStack Undo stack for operation (required)
    /// @param formatLayer Format layer for the buffer (required)
    /// @return Number of replacements made
    int replaceAll(QUndoStack* undoStack, FormatLayer* formatLayer);

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
    /// @brief Emitted when matches list changes
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

    /// @brief Build a SearchMatch from position and length
    /// @param start Absolute start position
    /// @param length Match length
    /// @return Populated SearchMatch
    SearchMatch buildMatch(size_t start, size_t length) const;

    /// @brief Build Qt find flags from current options
    /// @return QTextDocument::FindFlags
    QTextDocument::FindFlags buildFindFlags() const;

    /// @brief Find match at or after position
    /// @param fromPosition Start position for search
    /// @param forward true for forward search, false for backward
    /// @return Found match or invalid match
    SearchMatch findMatch(size_t fromPosition, bool forward);

    /// @brief Update current match index to match containing position
    /// @param position Absolute character position
    void updateCurrentMatchForPosition(size_t position);

    TextBuffer* m_buffer = nullptr;          ///< Text buffer (not owned)
    QString m_searchText;                    ///< Current search text
    QString m_replaceText;                   ///< Current replacement text
    SearchOptions m_options;                 ///< Current search options
    std::vector<SearchMatch> m_matches;      ///< Cached matches
    int m_currentMatchIndex = -1;            ///< Current match index (-1 = none)
    bool m_matchesDirty = true;              ///< Matches need rebuild
};

}  // namespace kalahari::editor
