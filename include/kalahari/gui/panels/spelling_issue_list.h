/// @file spelling_issue_list.h
/// @brief The list of the Spelling and Grammar panel: a card for each misspelled word and
///        grammar issue, made only for the cards in view

#pragma once

#include <kalahari/editor/text_issue.h>
#include <kalahari/gui/panels/annotation_colors.h>

#include <QAbstractScrollArea>
#include <QFrame>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class QAbstractButton;
class QLabel;
class QWidget;

namespace kalahari::gui {

/// @brief A misspelled word or a grammar issue as the Spelling and Grammar panel lists it
struct SpellingPanelIssue {
    editor::TextIssue issue;  ///< What and where: its paragraph (block), place and text
    QString key;              ///< Tells it from the others of the list
    int position = 0;         ///< Its first character in the whole text
    int number = 0;           ///< Its paragraph among those with text, from 1 ("paragraph 3")
    QString paragraphText;    ///< The text of its paragraph, for the words around it

    bool operator==(const SpellingPanelIssue& other) const = default;
};

/// @brief The text before and after an issue that its card shows with it
struct IssueContext {
    QString before;
    QString after;
};

/// @brief A few words around an issue, for a card's one line: up to three words before it and
///        four after it, with "…" where its paragraph goes on
IssueContext shortContext(const QString& paragraph, int start, int length);

/// @brief The sentence of an issue: the text before it from the start of its sentence, and the
///        text after it to the end of its sentence
IssueContext sentenceContext(const QString& paragraph, int start, int length);

/// @brief The colors of the cards of a kind of issue
struct IssueCardColors {
    AnnotationCardColors card;  ///< As a card of an annotation, in a lighter tint
    QColor border;              ///< The frame and the first suggestion: the kind's color itself
    QColor onBorder;            ///< The text on the kind's color (the first suggestion's)

    bool operator==(const IssueCardColors& other) const = default;
};

/// @brief The colors of the cards of a kind of issue: every text readable on the card
/// @param kindColor The kind's color as shown on @p surface (issueColorOn()): the frame and the
///        bar of its cards
/// @param surface What the cards lie on: the panel's base
/// @param text The color of the text on it
IssueCardColors issueCardColors(const QColor& kindColor, const QColor& surface,
                                const QColor& text);

/// @brief The card of one issue
///
/// Folded, it has two lines: the kind with the paragraph, and a few words around the issue.
/// Unfolded (the selected card; every card when the panel shows the buttons on each), it also
/// has the message of a grammar issue, the whole sentence, the suggestions and the actions:
/// "Ignore Word" and "Add to Dictionary" for a misspelled word, "Ignore Rule" for a grammar
/// issue. The suggestions of the selected card are numbered: the keys 1 to 5 choose them.
class SpellingIssueCard : public QFrame {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit SpellingIssueCard(QWidget* parent = nullptr);

    /// @brief Show an issue
    void setIssue(const SpellingPanelIssue& issue);

    /// @brief The issue shown
    const SpellingPanelIssue& issue() const { return m_issue; }

    /// @brief Unfold the card (see the class) or fold it
    void setExpanded(bool expanded);

    /// @brief Whether the card is unfolded
    bool isExpanded() const { return m_expanded; }

    /// @brief The suggestions on its buttons, the likeliest first (none: "(No suggestions)")
    void setSuggestions(const QStringList& suggestions);

    /// @brief The suggestions on its buttons
    QStringList suggestions() const;

    /// @brief Selected: a stronger frame, numbered suggestions, and its buttons are reached
    ///        with Tab (those of the other cards only with the mouse)
    void setSelected(bool selected);

    /// @brief Whether it is selected
    bool isSelected() const { return m_selected; }

    /// @brief Show whether the keys are in the list, where they act on the selected card
    void setListFocused(bool focused);

    /// @brief Colors: of its kind (the frame, the first suggestion), and of the highlight (the
    ///        frame of the selected card while the keys are in the list, of a button with the
    ///        keys)
    void setColors(const IssueCardColors& colors, const QColor& highlight);

    /// @brief A text that is only read: the suggestions are greyed out and say why; ignoring
    ///        and the dictionary still work, as they do not change the text
    void setReadOnly(bool readOnly);

    /// @brief The buttons of the suggestions, in their order
    QList<QAbstractButton*> suggestionButtons() const;

    /// @brief The buttons of the actions, in their order
    QList<QAbstractButton*> actionButtons() const;

    /// @brief The buttons Tab goes through: the suggestions, then the actions
    QList<QAbstractButton*> buttons() const;

    /// @brief The text of the line of the words around the issue (folded) or of its sentence
    QString contextText() const;

signals:
    /// @brief The card was clicked (not on a button)
    void clicked();

    /// @brief A suggestion was chosen
    /// @param index Its place among the suggestions, from 0
    void suggestionChosen(int index);

    /// @brief "Ignore Word" was clicked
    void ignoreWordClicked();

    /// @brief "Add to Dictionary" was clicked
    void addToDictionaryClicked();

    /// @brief "Ignore Rule" was clicked
    void ignoreRuleClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    /// @brief The texts of the labels, for the issue, the fold and the width
    void updateTexts();

    /// @brief The line of the words around the issue, shortened to the label's width
    void updateShortContext();

    /// @brief The buttons of the suggestions and the actions, for the issue and the state
    void updateButtons();

    /// @brief The colors of the labels and the buttons
    void applyColors();

    /// @brief The fonts of the labels, from the card's font
    void updateFonts();

    SpellingPanelIssue m_issue;
    QStringList m_suggestions;
    bool m_expanded = false;
    bool m_selected = false;
    bool m_listFocused = false;
    bool m_readOnly = false;
    IssueCardColors m_colors;
    QColor m_highlight;

    QLabel* m_kindLabel{nullptr};
    QLabel* m_placeLabel{nullptr};
    QLabel* m_messageLabel{nullptr};
    QLabel* m_contextLabel{nullptr};
    QWidget* m_suggestionRow{nullptr};
    QLabel* m_noSuggestionsLabel{nullptr};
    QWidget* m_actionRow{nullptr};
    QList<QAbstractButton*> m_suggestionButtons;
    QList<QAbstractButton*> m_actionButtons;
};

/// @brief The cards of the issues, one under another
///
/// Only the cards in view and the selected one are made: a long chapter with hundreds of
/// issues takes no more time and memory than a short one. A card is folded, unless it is the
/// selected one or the list unfolds every card (setExpandedAll()).
///
/// The list does not take the keys itself: they go on to its panel, which moves the selection
/// (the arrows, Home, End) and acts on the selected card.
class SpellingIssueList : public QAbstractScrollArea {
    Q_OBJECT

public:
    /// @brief Gives the suggestions for a misspelled word (they take the dictionary a moment,
    ///        so they are asked only for the cards unfolded)
    using SuggestionProvider = std::function<QStringList(const SpellingPanelIssue&)>;

    /// @brief Constructor
    /// @param parent Parent widget
    explicit SpellingIssueList(QWidget* parent = nullptr);

    /// @brief The issues to list, in their order; the selection stays on the issue with the
    ///        same key, else none is selected
    void setIssues(std::vector<SpellingPanelIssue> issues);

    /// @brief The issues listed
    const std::vector<SpellingPanelIssue>& issues() const { return m_issues; }

    /// @brief The number of the issues listed
    int count() const { return static_cast<int>(m_issues.size()); }

    /// @brief The index of the issue with a key (-1: none)
    int indexOf(const QString& key) const;

    /// @brief Select the issue with a key (empty: none), without signals; its card is
    ///        scrolled into view
    void setSelectedKey(const QString& key);

    /// @brief The key of the selected issue (empty: none)
    QString selectedKey() const { return m_selectedKey; }

    /// @brief The index of the selected issue (-1: none)
    int selectedIndex() const;

    /// @brief Unfold every card (the buttons on each card), or only the selected one
    void setExpandedAll(bool all);

    /// @brief Whether every card is unfolded
    bool isExpandedAll() const { return m_expandedAll; }

    /// @brief The text is only read: the cards grey out their suggestions
    void setReadOnly(bool readOnly);

    /// @brief Show whether the keys are in the panel, where they act on the selected card:
    ///        its frame is then in the highlight's color
    void setKeysInside(bool inside);

    /// @brief The colors of the cards of each kind and of the highlight
    void setColors(const IssueCardColors& spelling, const IssueCardColors& grammar,
                   const QColor& highlight);

    /// @brief Where the suggestions for misspelled words come from (none: no suggestions)
    void setSuggestionProvider(SuggestionProvider provider);

    /// @brief The suggestions for an issue: those of a grammar issue, those of the provider
    ///        for a misspelled word (asked once, then kept while the issue is listed)
    QStringList suggestionsFor(const SpellingPanelIssue& issue);

    /// @brief Ask the provider again for the suggestions of the cards (the dictionary changed)
    void forgetSuggestions();

    /// @brief The card of an issue, when it is in view (nullptr: not made)
    SpellingIssueCard* card(const QString& key) const;

    /// @brief The card of the selected issue (nullptr: none, or not made)
    SpellingIssueCard* selectedCard() const;

    /// @brief The key of the issue whose card is at a point of the viewport (empty: none)
    QString keyAt(const QPoint& viewportPoint) const;

    /// @brief The issue about a page (the height of the view) before or after the selected
    ///        one; the first or last issue at the ends
    /// @param pages -1: a page up, 1: a page down
    int pageStepFrom(int index, int pages) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// @brief A card was clicked (not on a button)
    void cardClicked(const QString& key);

    /// @brief A suggestion of a card was chosen
    void suggestionChosen(const QString& key, int index);

    /// @brief "Ignore Word" of a card was clicked
    void ignoreWordClicked(const QString& key);

    /// @brief "Add to Dictionary" of a card was clicked
    void addToDictionaryClicked(const QString& key);

    /// @brief "Ignore Rule" of a card was clicked
    void ignoreRuleClicked(const QString& key);

    /// @brief The card of the selected issue was made or changed (its buttons are others)
    void selectedCardChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;

private:
    /// @brief The heights of the folded cards and of the unfolded ones not measured yet,
    ///        after the font or the style changed
    void measureFolded();

    /// @brief Whether the card of an issue is unfolded
    bool isExpanded(int index) const;

    /// @brief The height of the card of an issue: measured, or estimated until it is made
    int heightOf(int index) const;

    /// @brief The issues whose cards are made: those in view, and the selected one
    std::vector<int> indexesToShow() const;

    /// @brief The top of each card, the height of the whole list and the scroll bar
    void updatePositions();

    /// @brief Make the cards in view (and the selected one), place them, and put away those
    ///        out of view
    void placeCards();

    /// @brief A card for an issue: the one it has, else one put away, else a new one
    SpellingIssueCard* cardFor(int index);

    /// @brief Show an issue on a card, folded or not, with its suggestions
    void fillCard(SpellingIssueCard* card, int index);

    /// @brief Scroll the selected card into view
    void scrollToSelected();

    /// @brief The width of a card
    int cardWidth() const;

    std::vector<SpellingPanelIssue> m_issues;
    std::vector<int> m_tops;  ///< The top of each card; one more: the bottom of the list
    QString m_selectedKey;
    bool m_expandedAll = false;
    bool m_readOnly = false;
    bool m_keysInside = false;
    bool m_placing = false;         ///< Placing the cards just now
    int m_foldedHeight = 0;         ///< The height of a folded card
    int m_expandedEstimate = 0;     ///< The height an unfolded card is taken to have, unmeasured
    int m_measuredWidth = -1;       ///< The width the heights were measured at
    IssueCardColors m_spellingColors;
    IssueCardColors m_grammarColors;
    QColor m_highlight;
    SuggestionProvider m_suggestionProvider;
    QHash<QString, QStringList> m_suggestions;     ///< Of the misspelled words, by key
    QHash<QString, int> m_expandedHeights;         ///< Of the unfolded cards measured, by key
    QHash<QString, SpellingIssueCard*> m_shown;    ///< The cards in use, by their issues' keys
    QList<SpellingIssueCard*> m_spare;             ///< Cards put away, for other issues
    // Plain: only compared, and the cards live as long as the list (put away, never deleted);
    // clang-tidy's analyzer reports a QPointer assigned as a use of freed memory
    SpellingIssueCard* m_lastSelectedCard{nullptr};  ///< For selectedCardChanged()
};

}  // namespace kalahari::gui
