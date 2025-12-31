/// @file find_replace_bar.h
/// @brief Inline find/replace bar widget (OpenSpec #00044 Task 9.6)
///
/// A compact horizontal bar for find/replace operations, similar to VS Code.
/// Features:
/// - Search input with match count display
/// - Toggle buttons for case, whole word, regex
/// - Navigation buttons (prev/next)
/// - Replace input with Replace/Replace All buttons
/// - Keyboard shortcuts for all operations

#pragma once

#include <QWidget>
#include <QString>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QUndoStack;
class QShortcut;

namespace kalahari::editor {
class SearchEngine;
struct SearchMatch;
}  // namespace kalahari::editor

namespace kalahari::gui {

/// @brief Inline find/replace bar widget
///
/// A compact horizontal bar that provides find/replace functionality:
/// - Row 1: Search input, option toggles (Aa, W, .*), navigation (up/down), match count, close
/// - Row 2: Replace input, Replace/Replace All buttons (toggleable visibility)
///
/// Layout:
/// @code
/// +-----------------------------------------------------------------+
/// | [Find input...] [Aa] [W] [.*]  [^] [v]  3 of 42  [X]           |
/// | [Replace input...] [Replace] [Replace All]                      |
/// +-----------------------------------------------------------------+
/// @endcode
///
/// Keyboard shortcuts:
/// - Enter: Find Next
/// - Shift+Enter: Find Previous
/// - Escape: Close bar
/// - Alt+C: Toggle case sensitive
/// - Alt+W: Toggle whole word
/// - Alt+R: Toggle regex
class FindReplaceBar : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit FindReplaceBar(QWidget* parent = nullptr);

    /// @brief Destructor
    ~FindReplaceBar() override;

    /// @brief Set the search engine to use
    /// @param engine Pointer to SearchEngine (not owned)
    void setSearchEngine(editor::SearchEngine* engine);

    /// @brief Set the undo stack for replace operations
    /// @param stack Pointer to QUndoStack (not owned)
    void setUndoStack(QUndoStack* stack);

    // Phase 11.8: Removed setFormatLayer - no longer needed (formatting in QTextCharFormat)

    /// @brief Show find-only mode (hide replace section)
    void showFind();

    /// @brief Show find+replace mode (show replace section)
    void showFindReplace();

    /// @brief Set the search text
    /// @param text Text to search for
    void setSearchText(const QString& text);

    /// @brief Get the current search text
    /// @return Current search text
    QString searchText() const;

    /// @brief Focus the search input field
    void focusSearchInput();

    /// @brief Check if replace mode is active
    /// @return true if replace section is visible
    bool isReplaceMode() const;

signals:
    /// @brief Emitted when user navigates to a match
    /// @param match The match to navigate to
    void navigateToMatch(const editor::SearchMatch& match);

    /// @brief Emitted when the bar is closed
    void closed();

    /// @brief Emitted when search text changes
    /// @param text The new search text
    void searchTextChanged(const QString& text);

protected:
    /// @brief Handle key press events
    /// @param event Key event
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    /// @brief Handle search text changes
    /// @param text New search text
    void onSearchTextChanged(const QString& text);

    /// @brief Find next match
    void onFindNext();

    /// @brief Find previous match
    void onFindPrevious();

    /// @brief Replace current match
    void onReplaceCurrent();

    /// @brief Replace all matches
    void onReplaceAll();

    /// @brief Handle matches changed from search engine
    void onMatchesChanged();

    /// @brief Handle search options changes
    void onOptionsChanged();

    /// @brief Close the bar
    void onClose();

private:
    /// @brief Setup UI components
    void setupUi();

    /// @brief Create keyboard shortcuts
    void createShortcuts();

    /// @brief Create signal/slot connections
    void createConnections();

    /// @brief Update the match count label
    void updateMatchCountLabel();

    /// @brief Update button enabled states
    void updateButtonStates();

    /// @brief Apply search options from toggle buttons
    void applySearchOptions();

    // =========================================================================
    // UI Components - Row 1 (Find)
    // =========================================================================

    QLineEdit* m_searchInput = nullptr;       ///< Search text input
    QLabel* m_matchCountLabel = nullptr;      ///< Match count display ("3 of 42")

    QToolButton* m_caseSensitiveBtn = nullptr;  ///< Case sensitive toggle (Aa)
    QToolButton* m_wholeWordBtn = nullptr;      ///< Whole word toggle (W)
    QToolButton* m_regexBtn = nullptr;          ///< Regex toggle (.*)

    QToolButton* m_prevBtn = nullptr;         ///< Previous match button
    QToolButton* m_nextBtn = nullptr;         ///< Next match button
    QToolButton* m_closeBtn = nullptr;        ///< Close bar button

    // =========================================================================
    // UI Components - Row 2 (Replace)
    // =========================================================================

    QWidget* m_replaceSection = nullptr;      ///< Container for replace widgets
    QLineEdit* m_replaceInput = nullptr;      ///< Replace text input
    QPushButton* m_replaceBtn = nullptr;      ///< Replace current button
    QPushButton* m_replaceAllBtn = nullptr;   ///< Replace all button

    // =========================================================================
    // External references (not owned)
    // =========================================================================

    editor::SearchEngine* m_searchEngine = nullptr;  ///< Search engine
    QUndoStack* m_undoStack = nullptr;               ///< Undo stack for replace
    // Phase 11.8: Removed m_formatLayer - no longer needed

    // =========================================================================
    // Keyboard shortcuts
    // =========================================================================

    QShortcut* m_escapeShortcut = nullptr;    ///< Escape to close
    QShortcut* m_findNextShortcut = nullptr;  ///< Enter to find next
    QShortcut* m_findPrevShortcut = nullptr;  ///< Shift+Enter to find previous
    QShortcut* m_toggleCaseShortcut = nullptr;   ///< Alt+C for case
    QShortcut* m_toggleWordShortcut = nullptr;   ///< Alt+W for whole word
    QShortcut* m_toggleRegexShortcut = nullptr;  ///< Alt+R for regex
};

}  // namespace kalahari::gui
