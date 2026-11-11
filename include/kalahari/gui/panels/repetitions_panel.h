/// @file repetitions_panel.h
/// @brief The panel Repetitions: the words of the text in front, or of the whole book, that
///        come back too close, and the words used most often

#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

#include <utility>

class QButtonGroup;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QToolButton;
class QTreeWidget;

namespace kalahari::gui {

class RepetitionCardDelegate;

/// @brief A word that comes back too close, as a card of the panel
struct RepetitionCard {
    QString word;                      ///< The word: its base form
    QString detail;                    ///< "3 times in 19 words", with the forms and the chapter
    QString context;                   ///< The text around its words
    QList<std::pair<int, int>> marks;  ///< Its words in the context: start, length

    bool operator==(const RepetitionCard&) const = default;
};

/// @brief A word as the list of the most frequent ones shows it
struct FrequentWord {
    QString word;              ///< Its base form
    int count = 0;             ///< How many times it is used
    double perThousand = 0.0;  ///< Times in a thousand words of the text
    QString forms;             ///< Its forms with their counts: "powiedział 14, powiedziała 9"

    bool operator==(const FrequentWord&) const = default;
};

/// @brief The panel Repetitions
///
/// Two pages: "Close Together", a card for each word that comes back within the distance of
/// the settings, in the order of the text; "Most Frequent", the words used most often, with
/// their counts in a thousand words and their forms. The scope is the text in front (counted
/// again a moment after the writing stops) or the whole book (counted when Recount is
/// clicked). The panel changes no text: it reports what the writer chooses.
///
/// Keys in the lists: Up and Down choose a card or a word, which the text shows; Enter goes to
/// the text (to the words of the card; to the next use of the word); Delete skips the word of
/// the card from now on; the menu key or Shift+F10 opens the menu of the card; Ctrl+Tab
/// switches the pages; Esc goes back to the text.
class RepetitionsPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief What is counted
    enum class Scope {
        Chapter,  ///< The text in front
        Book      ///< All the texts of the book
    };

    /// @brief The pages of the panel
    enum class Page {
        Close,    ///< The words that come back too close
        Frequent  ///< The words used most often
    };

    /// @brief Constructor
    /// @param parent Parent widget
    explicit RepetitionsPanel(QWidget* parent = nullptr);

    /// @brief Choose the scope, without signals
    void setScope(Scope scope);

    /// @brief The scope chosen
    [[nodiscard]] Scope scope() const { return m_scope; }

    /// @brief Whether the whole book can be counted (a book is open)
    void setBookAvailable(bool available);

    /// @brief Show a page, without signals
    void setPage(Page page);

    /// @brief The page shown
    [[nodiscard]] Page page() const;

    /// @brief The words too close and the most frequent ones, with the lines that sum them up
    ///
    /// The card and the word chosen stay chosen while the lists have as many; the first ones
    /// are chosen otherwise. No signal.
    void setResults(const QList<RepetitionCard>& cards, const QList<FrequentWord>& words,
                    const QString& closeSummary, const QString& frequentSummary);

    /// @brief What the panel says in place of the lists (empty: the lists)
    void setMessage(const QString& message);

    /// @brief What the panel says in place of the lists
    [[nodiscard]] QString message() const;

    /// @brief The cards shown
    [[nodiscard]] const QList<RepetitionCard>& cards() const { return m_cards; }

    /// @brief The most frequent words shown
    [[nodiscard]] const QList<FrequentWord>& frequentWords() const { return m_words; }

    /// @brief The card chosen (-1: none)
    [[nodiscard]] int currentCard() const;

    /// @brief Choose a card, without signals; it is scrolled into view
    void setCurrentCard(int index);

    /// @brief The word chosen among the most frequent (-1: none)
    [[nodiscard]] int currentWord() const;

    /// @brief Choose a word, without signals
    void setCurrentWord(int index);

    /// @brief The color of the repeated words on the cards: the theme's, as in the text
    void setMarkColor(const QColor& color);

    /// @brief Give the keys to the list of the page shown
    void focusList();

    /// @brief Whether the keys are in the panel
    [[nodiscard]] bool hasFocusInside() const;

    /// @brief The list of the cards
    [[nodiscard]] QListWidget* cardList() const { return m_cardList; }

    /// @brief The list of the most frequent words
    [[nodiscard]] QTreeWidget* wordList() const { return m_wordList; }

    /// @brief Recount (the whole book)
    [[nodiscard]] QPushButton* recountButton() const { return m_recount; }

signals:
    /// @brief The writer chose another scope
    void scopeChanged(kalahari::gui::RepetitionsPanel::Scope scope);

    /// @brief The writer showed another page
    void pageChanged(kalahari::gui::RepetitionsPanel::Page page);

    /// @brief Recount was clicked
    void recountRequested();

    /// @brief The writer chose a card: the text shows its words
    void cardSelected(int index);

    /// @brief Go To on a card, or Enter: the cursor to its first word, the keys to the text
    void cardActivated(int index);

    /// @brief Skip This Word on a card, or Delete
    void skipWordRequested(int index);

    /// @brief The writer chose a word among the most frequent: the text shows its uses
    void wordSelected(int index);

    /// @brief Enter on a word: the next of its uses after the cursor
    void wordActivated(int index);

    /// @brief The writer asked to go back to the text (Esc)
    void editorFocusRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    /// @brief The texts of the summary and of the empty lists, for the page and the state
    void updateParts();

    /// @brief Open the menu of the card chosen, at it
    void showCardMenu(const QPoint& globalPos);

    Scope m_scope = Scope::Chapter;
    QList<RepetitionCard> m_cards;
    QList<FrequentWord> m_words;
    QString m_closeSummary;
    QString m_frequentSummary;
    QString m_message;
    QColor m_markColor{230, 130, 40};

    QButtonGroup* m_scopeGroup{nullptr};
    QToolButton* m_bookButton{nullptr};
    QPushButton* m_recount{nullptr};
    QTabWidget* m_tabs{nullptr};
    QLabel* m_closeSummaryLabel{nullptr};
    QLabel* m_frequentSummaryLabel{nullptr};
    QStackedWidget* m_closeStack{nullptr};
    QStackedWidget* m_frequentStack{nullptr};
    QLabel* m_closeEmpty{nullptr};
    QLabel* m_frequentEmpty{nullptr};
    QStackedWidget* m_pages{nullptr};  ///< The message, or the tabs
    QLabel* m_messageLabel{nullptr};
    QListWidget* m_cardList{nullptr};
    RepetitionCardDelegate* m_cardDelegate{nullptr};
    QTreeWidget* m_wordList{nullptr};
    bool m_updating = false;  ///< The lists are filled by the panel, not chosen by the writer
};

}  // namespace kalahari::gui
