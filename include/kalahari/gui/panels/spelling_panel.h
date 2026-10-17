/// @file spelling_panel.h
/// @brief The Spelling and Grammar panel: the misspelled words and grammar issues of the text
///        in front, as cards

#pragma once

#include <kalahari/core/kind_words.h>
#include <kalahari/editor/text_issue.h>
#include <kalahari/gui/panels/spelling_issue_list.h>

#include <QString>
#include <QWidget>

#include <array>
#include <optional>
#include <vector>

class QAbstractButton;
class QContextMenuEvent;
class QLabel;
class QPushButton;
class QToolButton;

namespace kalahari::core {
struct Theme;
}

namespace kalahari::gui {

/// @brief The Spelling and Grammar panel
///
/// Lists the issues it is given as cards (SpellingIssueList): the selected card shows the
/// whole sentence, the suggestions and the actions. The buttons of the kinds count the issues
/// of each kind and are its filters: off, the cards of the kind are hidden. A row under them
/// says how far the text is checked, or why it is not. The panel changes no text itself: it
/// reports what the writer chooses, and shows the issues it is given next.
///
/// Keys in the panel: Up and Down select the previous or next issue, Home and End the first
/// or last one, Page Up and Page Down an issue a page away; Enter presses the button with the
/// keys, else chooses the first suggestion; 1 to 5 choose the suggestion with that number;
/// the menu key or Shift+F10 asks for the menu of the issue (the one of its wave in the text);
/// Esc asks to go back to the text. The row at the bottom reminds the most important keys.
///
/// In a narrow panel (a small screen) the rows of buttons go on in the next row.
class SpellingPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief What the panel shows
    enum class Content {
        Issues,     ///< The issues of the text in front (the list, perhaps empty)
        NoText,     ///< No text in front: what to open
        NotChecked  ///< Neither the spelling nor the grammar is checked: how to turn them on
    };

    /// @brief Constructor
    /// @param parent Parent widget
    explicit SpellingPanel(QWidget* parent = nullptr);

    /// @brief Show the issues, what to open, or how to turn the checking on
    void setContent(Content content);

    /// @brief What the panel shows
    Content content() const { return m_content; }

    /// @brief The names of the texts, in the program's language, for what the panel says
    /// @param inFront The kind of the text in front ("No spelling or grammar issues in this
    ///        story"), the word "file" for a file outside the book; nullopt: nothing in front
    /// @param bookTexts The main texts of the open book, which the writer opens to see their
    ///        issues ("Open a story..."); nullopt: no book, or a book without them
    /// @param bookOpen Whether a book is open
    void setKindNames(const std::optional<core::KindWords>& inFront,
                      const std::optional<core::KindWords>& bookTexts, bool bookOpen);

    /// @brief The issues of the text in front, in the order of the text (the panel filters
    ///        them)
    ///
    /// The selection stays on the issue with the same key; when it is gone, the first issue
    /// listed at or after its place is selected, without signals.
    void setIssues(std::vector<SpellingPanelIssue> issues);

    /// @brief The issues it was given
    const std::vector<SpellingPanelIssue>& issues() const { return m_issues; }

    /// @brief The issues it lists, in their order: those of the kinds checked and shown
    const std::vector<SpellingPanelIssue>& shownIssues() const;

    /// @brief Whether a kind is checked now: its button counts its issues and is a filter;
    ///        a kind not checked has "–" on its button, which is off, and none of its issues
    ///        is listed
    void setKindChecked(editor::TextIssueKind kind, bool checked);

    /// @brief Whether a kind is checked (setKindChecked())
    bool isKindChecked(editor::TextIssueKind kind) const;

    /// @brief Show or hide the issues of a kind (its button)
    void setKindShown(editor::TextIssueKind kind, bool shown);

    /// @brief Whether the issues of a kind are shown (its button)
    bool isKindShown(editor::TextIssueKind kind) const;

    /// @brief The row under the buttons of the kinds: how far the text is checked, or why
    ///        it is not
    /// @param text What it says (empty: the row is hidden)
    /// @param problem A problem: the row has a warning sign
    void setStatus(const QString& text, bool problem);

    /// @brief What the row under the buttons of the kinds says
    QString statusText() const;

    /// @brief Whether the whole text is checked: an empty list then says there are no issues
    ///        in it, not that none is found yet
    void setComplete(bool complete);

    /// @brief The text is only read: the suggestions are greyed out (SpellingIssueCard)
    void setReadOnly(bool readOnly);

    /// @brief The fix buttons on every card, or only on the selected one
    void setButtonsOnEveryCard(bool every);

    /// @brief Where the suggestions for the misspelled words come from
    void setSuggestionProvider(SpellingIssueList::SuggestionProvider provider);

    /// @brief Ask the provider again for the suggestions (the dictionary changed)
    void forgetSuggestions();

    /// @brief What the row at the bottom says about the command going to the next issue
    /// @param keys Its keys, as the program shows them (empty: none)
    /// @param toPanel Whether it shows the next issue in the panel, else its menu at the word
    void setNextIssueKeys(const QString& keys, bool toPanel);

    /// @brief Select an issue (empty: none), without signals; its card is scrolled into view
    void selectIssue(const QString& key);

    /// @brief The key of the selected issue (empty: none)
    QString selectedKey() const;

    /// @brief The selected issue, if any
    std::optional<SpellingPanelIssue> selectedIssue() const;

    /// @brief The first issue listed that starts at or after a place of the text, if any
    std::optional<SpellingPanelIssue> issueFrom(int position) const;

    /// @brief Give the keys to the panel: to the list, else to the button that turns the
    ///        checking on, else to the panel itself
    void focusList();

    /// @brief Whether the keys are in the panel
    bool hasFocusInside() const;

    /// @brief Take colors from a theme: the kinds' colors (issueColor()), the cards and the
    ///        controls
    void applyTheme(const core::Theme& theme);

    /// @brief The list of the cards
    SpellingIssueList* list() const { return m_list; }

    /// @brief The button of a kind (its count and filter)
    QToolButton* kindButton(editor::TextIssueKind kind) const;

signals:
    /// @brief The writer selected an issue: a click on its card, the keys, Previous or Next
    void issueSelected(const kalahari::gui::SpellingPanelIssue& issue);

    /// @brief A suggestion was chosen for an issue: what to put in its place
    void suggestionChosen(const kalahari::gui::SpellingPanelIssue& issue,
                          const QString& suggestion);

    /// @brief "Ignore Word" of a misspelled word
    void ignoreWordRequested(const kalahari::gui::SpellingPanelIssue& issue);

    /// @brief "Add to Dictionary" of a misspelled word
    void addToDictionaryRequested(const kalahari::gui::SpellingPanelIssue& issue);

    /// @brief "Ignore Rule" of a grammar issue
    void ignoreRuleRequested(const kalahari::gui::SpellingPanelIssue& issue);

    /// @brief The menu of an issue was asked for: a right click on its card, the menu key,
    ///        Shift+F10
    /// @param issue The issue (selected first)
    /// @param globalPos Where the menu opens, on the screen
    void menuRequested(const kalahari::gui::SpellingPanelIssue& issue, const QPoint& globalPos);

    /// @brief The writer asked to go back to the text (Esc)
    void editorFocusRequested();

    /// @brief The writer asked to turn the checking on ("Turn On Checking")
    void turnOnCheckingRequested();

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    /// @brief The issues listed, the buttons of the kinds, the texts and the buttons, for
    ///        what the panel shows now
    void rebuild();

    /// @brief The counts on the buttons of the kinds
    void updateKindButtons();

    /// @brief What the panel says when it lists no card, and which parts it shows
    void updateParts();

    /// @brief The text of an empty panel
    QString emptyText() const;

    /// @brief Previous and Next, for the selection
    void updateFootButtons();

    /// @brief The row at the bottom, for where the keys are
    void updateHint();

    /// @brief The keys came into the panel or left it
    void updateKeysInside();

    /// @brief The icons of the buttons and of the warning, from the icon theme
    void updateIcons();

    /// @brief Select an issue listed, scrolled into view, and report it (issueSelected())
    void selectAndReport(int index);

    /// @brief Select the issue before or after the selected one (the first or the last
    ///        without a selection)
    void step(int steps);

    /// @brief Choose a suggestion of the selected issue (1 to 5: index 0 to 4)
    void chooseSuggestion(int index);

    /// @brief Ask for the menu of the selected issue, at its card
    void requestMenuOfSelected();

    /// @brief Report a suggestion chosen on a card
    void onSuggestionChosen(const QString& key, int index);

    /// @brief The issue listed with a key, or nullptr
    const SpellingPanelIssue* shownIssue(const QString& key) const;

    /// @brief The keys of the panel: they are its own, not the window's shortcuts
    static bool isPanelKey(const QKeyEvent* event);

    std::vector<SpellingPanelIssue> m_issues;
    Content m_content = Content::NoText;
    std::array<bool, 2> m_kindChecked{true, true};  ///< Spelling, grammar
    std::array<bool, 2> m_kindShown{true, true};
    bool m_complete = false;
    bool m_readOnly = false;
    bool m_keysInside = false;
    QString m_nextKeys;
    bool m_nextToPanel = true;
    std::optional<core::KindWords> m_inFront;
    std::optional<core::KindWords> m_bookTexts;
    bool m_bookOpen = false;
    bool m_updatingButtons = false;  ///< The kind buttons set by the panel, not by the writer

    std::array<QToolButton*, 2> m_kindButtons{};
    QWidget* m_kindRow{nullptr};
    QWidget* m_statusRow{nullptr};
    QLabel* m_statusIcon{nullptr};
    QLabel* m_statusLabel{nullptr};
    SpellingIssueList* m_list{nullptr};
    QWidget* m_emptyWidget{nullptr};
    QLabel* m_emptyLabel{nullptr};
    QPushButton* m_turnOnButton{nullptr};
    QWidget* m_footRow{nullptr};
    QToolButton* m_previousButton{nullptr};
    QToolButton* m_nextButton{nullptr};
    QLabel* m_hintLabel{nullptr};
};

}  // namespace kalahari::gui
