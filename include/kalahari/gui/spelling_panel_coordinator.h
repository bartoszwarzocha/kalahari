/// @file spelling_panel_coordinator.h
/// @brief The Spelling and Grammar panel: the issues of the text in front, what the writer
///        does with them, and when the panel shows

#pragma once

#include <kalahari/gui/panels/spelling_issue_list.h>

#include <QObject>
#include <QString>

#include <array>
#include <atomic>
#include <optional>
#include <vector>

class QDockWidget;
class QTabWidget;
class QTimer;
class QToolButton;

namespace kalahari::editor {
class BookEditor;
struct CursorPosition;
}  // namespace kalahari::editor

namespace kalahari::gui {

class GrammarCoordinator;
class SpellingCoordinator;
class SpellingPanel;

namespace utils {
class DistractionFreeLayout;
}

/// @brief Coordinates the Spelling and Grammar panel with the text in front
///
/// Lists in the panel the misspelled words and grammar issues of the text in front, and says
/// how far it is checked, or why it is not (SpellingCoordinator::problem(),
/// GrammarCoordinator::problem()). The issue selected in the panel is selected in the text, and
/// a click on an issue in the text selects its card. What the writer chooses in the panel goes
/// to the text and to the checking: a suggestion takes the issue's place (a step of the undo
/// history), and Ignore Word, Add to Dictionary and Ignore Rule do what they do in the menu of
/// the wave; then the issue after it is selected. While the panel shows, the grammar of the
/// whole text is checked (the setting spellingPanel.grammarScope "whole").
///
/// Next Spelling or Grammar Issue (F7) shows the issue it finds in the panel (the setting
/// spellingPanel.nextIssue "panel"), and gives the panel the keys. A panel that was closed, or
/// whose tab was under another one, shows only while the issues are looked through: Esc in the
/// panel, or the first character typed in the text, hides it again, and the tab it covered
/// comes back on top. The pin of its title bar (the setting spellingPanel.pinned) keeps it
/// open; so does opening it from View > Panels. In Distraction-Free writing the panel always
/// shows only while the issues are looked through.
class SpellingPanelCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor: the coordinator shows the next issue for the spelling coordinator
    ///        (SpellingCoordinator::setNextIssueHandler())
    /// @param panel The panel
    /// @param dock The panel's dock
    /// @param centralTabs The tabs of the documents
    /// @param spelling The spelling as you type
    /// @param grammar The grammar as you type (may be null)
    /// @param parent Parent object
    SpellingPanelCoordinator(SpellingPanel* panel, QDockWidget* dock, QTabWidget* centralTabs,
                             SpellingCoordinator* spelling, GrammarCoordinator* grammar,
                             QObject* parent = nullptr);

    /// @brief Destructor: the spelling coordinator shows the next issue in its menu again
    ~SpellingPanelCoordinator() override;

    SpellingPanelCoordinator(const SpellingPanelCoordinator&) = delete;
    SpellingPanelCoordinator& operator=(const SpellingPanelCoordinator&) = delete;

    /// @brief The pin of the panel's title bar, which keeps it open
    void setPinButton(QToolButton* button);

    /// @brief The layout of Distraction-Free writing, where the panel shows only while the
    ///        issues are looked through
    void setDistractionFreeLayout(utils::DistractionFreeLayout* layout);

    /// @brief Show an issue Next Spelling or Grammar Issue found in the panel, and give the
    ///        panel the keys (the handler of SpellingCoordinator)
    /// @param editor The document in front, whose issue it selected
    /// @param found Whether it found one
    void showNextIssue(editor::BookEditor* editor, bool found);

    /// @brief Hide a panel shown only while the issues are looked through, or bring back the
    ///        tab it covered; nothing for a panel kept open
    ///
    /// Before the window's layout is saved, and before Distraction-Free writing hides it.
    void endTemporaryShow();

    /// @brief Whether the panel shows only while the issues are looked through
    bool isTemporaryShow() const { return m_temporary.active; }

    /// @brief Whether the panel is kept open (the pin)
    static bool isPinned();

    /// @brief List the issues of the text in front again now
    void refresh();

private slots:
    /// @brief The settings of the panel or of the checking changed (called by name, queued)
    void onSettingsChanged();

private:
    /// @brief A panel shown only while the issues are looked through
    struct TemporaryShow {
        bool active = false;
        bool wasHidden = false;           ///< The panel was closed
        QDockWidget* covered = nullptr;   ///< The tab that was on top in its group
    };

    // The text in front
    editor::BookEditor* currentEditor() const;
    void onCurrentTabChanged();
    void onCursorMoved(const editor::CursorPosition& position);
    void onContentChanged();
    void scheduleRefresh();

    /// @brief The writer's words changed (a word ignored or added to the dictionary): the text
    ///        is checked again, and the suggestions may be others
    void onWordsChanged();

    /// @brief The issues of an editor as the panel lists them
    /// @param spelling Whether the misspelled words are listed
    /// @param grammar Whether the grammar issues are listed
    std::vector<SpellingPanelIssue> issuesOf(editor::BookEditor& editor, bool spelling,
                                             bool grammar) const;

    /// @brief Whether a kind is checked through, kept while at most a paragraph or two is
    ///        checked again (as when typing), so the row under the counts does not flicker
    bool isDone(editor::TextIssueKind kind, int checked, int paragraphs);

    /// @brief The place of the editor's cursor in its whole text
    int cursorPlace(const editor::BookEditor& editor) const;

    // What the writer does in the panel
    void selectInEditor(const SpellingPanelIssue& issue);
    void selectFrom(int position);
    void onSuggestionChosen(const SpellingPanelIssue& issue, const QString& suggestion);
    void onIgnoreWord(const SpellingPanelIssue& issue, bool addToDictionary);
    void onIgnoreRule(const SpellingPanelIssue& issue);
    void onMenuRequested(const SpellingPanelIssue& issue, const QPoint& globalPos);
    void onEditorFocusRequested();
    void turnOnChecking();

    // When the panel shows
    void showPanel();
    void forgetDock(QObject* dock);  ///< A dock destroyed: not the covered tab any more
    void onDockVisibilityChanged(bool visible);
    void updateWholeTextGrammar();
    void updatePinButton();
    void updateNextIssueKeys();
    void applySettings();

    SpellingPanel* m_panel;
    QDockWidget* m_dock;
    QTabWidget* m_centralTabs;
    SpellingCoordinator* m_spelling;
    GrammarCoordinator* m_grammar;
    // Plain pointers, cleared when their objects go: clang-tidy's analyzer reports a QPointer
    // assigned as a use of freed memory
    editor::BookEditor* m_editor{nullptr};  ///< The editor in front, whose issues are listed
    QToolButton* m_pinButton{nullptr};
    QTimer* m_refreshTimer{nullptr};
    TemporaryShow m_temporary;
    bool m_shown = false;           ///< The panel can be seen (not closed, nor under a tab)
    bool m_stale = true;            ///< The list is old: refresh when the panel shows
    bool m_panelChange = false;     ///< The text is changed from the panel
    bool m_selecting = false;       ///< The panel's issue is being selected in the text
    bool m_hidingTemporary = false; ///< The panel shown for the issues is being hidden
    bool m_wordsChanged = false;    ///< The writer's words changed: the text is checked again
    bool m_distractionFree = false; ///< Distraction-Free writing is on
    std::array<bool, 2> m_done{};   ///< The spelling, the grammar checked through (isDone())
    int m_settingsListener{0};
    std::atomic<bool> m_settingsPending{false};  ///< onSettingsChanged() is queued
};

}  // namespace kalahari::gui
