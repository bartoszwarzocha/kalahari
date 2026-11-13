/// @file search_panel.h
/// @brief The Search panel: Find in Book, the matches in all the texts of the book grouped by
///        text, and Replace All in Book

#pragma once

#include <kalahari/editor/search_engine.h>

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

#include <optional>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari::gui {

/// @brief A match as the panel shows it: the line around it, on one line
struct SearchPanelHit {
    QString context;  ///< The text around the match
    int start = 0;    ///< Where the match is in the context
    int length = 0;   ///< Its length there
    QString note;     ///< The note it is in ("footnote 2"); empty: in the text itself

    bool operator==(const SearchPanelHit&) const = default;
};

/// @brief A text with matches, as the panel lists it
struct SearchPanelText {
    QString key;                  ///< What the text is (its element): the same after a refresh
    QString title;                ///< Its title
    int count = 0;                ///< Its matches (hits may list fewer)
    QList<SearchPanelHit> hits;   ///< The matches listed, in text order

    bool operator==(const SearchPanelText&) const = default;
};

/// @brief The Search panel
///
/// The field of what to find with the options of the find bar (Match Case, Whole Word, Regular
/// Expression, with the same keys), the field of what the matches become and Replace All in
/// Book, a line that says what was found, and the matches in a tree: each text with its count,
/// under it its matches with the text around them. The panel searches nothing itself: it asks
/// for a search a moment after the typing stops (at once on Enter or a change of an option)
/// and shows the matches it is given.
///
/// Keys: Down in the field goes to the first match; in the tree Up and Down choose a match,
/// which an open text shows, Enter goes to it in the text (opened if it is not) with the keys,
/// a click goes to it with the keys staying in the panel; Esc goes back to the text.
class SearchPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief A match of the tree: the index of its text and its index there
    struct HitIndex {
        int text = -1;
        int hit = -1;

        bool operator==(const HitIndex&) const = default;
    };

    /// @brief How long after the last key the search is asked for, in milliseconds
    static constexpr int SEARCH_DELAY_MS = 300;

    /// @brief Constructor
    /// @param parent Parent widget
    explicit SearchPanel(QWidget* parent = nullptr);
    ~SearchPanel() override;

    /// @brief What to find
    [[nodiscard]] QString searchText() const;

    /// @brief Put a text in the field of what to find, without asking for a search
    void setSearchText(const QString& text);

    /// @brief What the matches become
    [[nodiscard]] QString replaceText() const;

    /// @brief The options of the search (Match Case, Whole Word, Regular Expression)
    [[nodiscard]] editor::SearchOptions options() const;

    /// @brief Set the options, without asking for a search
    void setOptions(const editor::SearchOptions& options);

    /// @brief The keys into the field of what to find, its text selected
    void focusSearchField();

    /// @brief What the line above the tree says (what was found, how far the search is)
    void setStatus(const QString& status);

    /// @brief What the line above the tree says
    [[nodiscard]] QString status() const;

    /// @brief The texts with matches, in the order of the book
    ///
    /// The match chosen stays chosen when its text is listed again and has a match with its
    /// index; the texts folded stay folded. No signal is emitted.
    void setTexts(const QList<SearchPanelText>& texts);

    /// @brief The texts listed (setTexts())
    [[nodiscard]] const QList<SearchPanelText>& texts() const { return m_texts; }

    /// @brief The match chosen in the tree; none: a text or nothing is chosen
    [[nodiscard]] std::optional<HitIndex> currentHit() const;

    /// @brief Choose a match in the tree, without signals
    void setCurrentHit(const HitIndex& hit);

    /// @brief Whether Replace All in Book can be pressed, and why it cannot (its tooltip)
    void setReplaceEnabled(bool enabled, const QString& whyNot = QString());

    /// @brief The color the matches are marked with in the tree (that of the search's hits)
    void setMarkColor(const QColor& color);

    /// @brief The tree of the matches (tests)
    [[nodiscard]] QTreeWidget* tree() const { return m_tree; }

signals:
    /// @brief What to find or the options changed: the matches are wanted again
    void searchRequested();

    /// @brief A match was chosen in the tree (Up, Down, a click): an open text may show it
    void hitChosen(int text, int hit);

    /// @brief A match is to be shown in its text, opened if it is not: by a click with the
    ///        keys staying in the panel (@p keys false), by Enter or a double click with them
    void hitActivated(int text, int hit, bool keys);

    /// @brief Replace All in Book was pressed
    void replaceAllRequested();

    /// @brief Esc: the keys back to the text
    void editorFocusRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The match of an item of the tree; none for a text
    [[nodiscard]] std::optional<HitIndex> hitOf(const QTreeWidgetItem* item) const;

    /// @brief The item of a match (nullptr: not listed)
    [[nodiscard]] QTreeWidgetItem* itemOf(const HitIndex& hit) const;

    /// @brief Ask for the search now (Enter, an option), not after the delay
    void searchNow();

    QLineEdit* m_searchField = nullptr;
    QToolButton* m_caseButton = nullptr;
    QToolButton* m_wordButton = nullptr;
    QToolButton* m_regexButton = nullptr;
    QLineEdit* m_replaceField = nullptr;
    QPushButton* m_replaceButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTreeWidget* m_tree = nullptr;
    QTimer* m_searchTimer = nullptr;
    QList<SearchPanelText> m_texts;
    bool m_quiet = false;  ///< Changes made by the program: no signals
    class HitDelegate;
    HitDelegate* m_delegate = nullptr;
};

}  // namespace kalahari::gui
