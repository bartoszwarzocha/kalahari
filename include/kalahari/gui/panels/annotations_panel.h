/// @file annotations_panel.h
/// @brief The Annotations panel: the comments, to-dos and notes of a chapter or the book

#pragma once

#include <kalahari/core/kind_words.h>
#include <kalahari/gui/panels/annotation_colors.h>
#include <kalahari/gui/panels/annotation_entry.h>

#include <QHash>
#include <QWidget>

#include <array>
#include <functional>
#include <optional>
#include <vector>

class QAbstractButton;
class QAction;
class QButtonGroup;
class QComboBox;
class QContextMenuEvent;
class QLabel;
class QLineEdit;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace kalahari::core {
struct Theme;
}

namespace kalahari::gui {

class AnnotationCard;

/// @brief The Annotations panel
///
/// Lists the annotations it is given as cards, with a search in their text, filters of
/// kind, state and date, the scope (the chapter in front or the whole book) and the
/// order. The panel is for looking through the annotations: a click on a card selects it
/// and asks to go to its place; editing one is asked for, and its text is written in the
/// frame at its place in the text. The panel changes no annotation itself: it reports what
/// the writer does, and the cards show the annotations it is given next.
///
/// Keys in the list: Up and Down select the previous or next card (and go to its place),
/// Home and End the first or last one, Enter or F2 edits the selected one, Space marks it
/// done (or brings it back), Delete removes it, the menu key or Shift+F10 opens its menu.
/// Esc goes back to the text from anywhere in the panel; Down in the search goes to the
/// list.
class AnnotationsPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit AnnotationsPanel(QWidget* parent = nullptr);

    /// @brief The annotations to list (the panel filters and orders them)
    ///
    /// Cards stay for annotations listed again.
    void setEntries(const std::vector<AnnotationEntry>& entries);

    /// @brief The annotations it was given
    const std::vector<AnnotationEntry>& entries() const { return m_entries; }

    /// @brief The annotations it shows, in their order
    std::vector<AnnotationEntry> shownEntries() const;

    /// @brief Whose annotations it lists
    AnnotationScope scope() const { return m_scope; }

    /// @brief List the annotations of the chapter in front or of the book
    void setScope(AnnotationScope scope);

    /// @brief Whether the whole book can be listed (a book is open)
    void setBookScopeAvailable(bool available);

    /// @brief Whether a chapter or document is in front (what the empty panel says)
    void setDocumentAvailable(bool available);

    /// @brief The names of what the panel lists, in the program's language: the button of the
    /// text in front and what the empty panel says ("Story", "This story has no annotations")
    /// @param inFront Kind of the element of the book in front; nullopt: a file outside the
    ///        book, or nothing
    /// @param bookTexts The main texts of the open book, which the writer opens to see their
    ///        annotations ("Open a story..."); nullopt: no book, or a book without them
    /// @param bookOpen Whether a book is open
    void setKindNames(const std::optional<core::KindWords>& inFront,
                      const std::optional<core::KindWords>& bookTexts, bool bookOpen);

    /// @brief The filters in use
    const AnnotationFilter& filter() const { return m_filter; }

    /// @brief Use other filters (also the search text and the kind buttons follow)
    void setFilter(const AnnotationFilter& filter);

    /// @brief The order in use
    AnnotationSort sort() const { return m_sort; }

    /// @brief Use another order
    void setSort(AnnotationSort sort);

    /// @brief The buttons under the list: previous and next to-do
    void setTodoActions(QAction* previous, QAction* next);

    /// @brief Select a card, scrolled into view (none: no card is selected)
    void selectAnnotation(const QString& key);

    /// @brief Select an annotation's card, easing the filters that would hide it
    void revealAnnotation(const QString& key);

    /// @brief The key of the selected card (empty: none)
    QString selectedKey() const { return m_selectedKey; }

    /// @brief The card of an annotation (nullptr: not shown)
    AnnotationCard* card(const QString& key) const;

    /// @brief Give the keys to the list of cards
    /// @param preferredKey The card to select when it is shown; else the selected card stays,
    ///        and without one the first card is selected
    void focusList(const QString& preferredKey = QString());

    /// @brief Whether the keys are in the panel
    bool hasFocusInside() const;

    /// @brief Take colors from a theme: the kinds' colors, the cards and the controls
    void applyTheme(const core::Theme& theme);

    /// @brief The colors of the cards of a kind, on the current theme
    const AnnotationCardColors& cardColors(editor::AnnotationKind kind) const;

signals:
    /// @brief A card was clicked or selected with the keys: go to the annotation's place
    void annotationActivated(const AnnotationEntry& entry);

    /// @brief Editing an annotation was asked for (Enter, F2, a double click, Edit)
    void editRequested(const AnnotationEntry& entry);

    /// @brief An annotation was done (resolved) or brought back
    void doneToggled(const AnnotationEntry& entry, bool done);

    /// @brief Removing an annotation was asked for
    void deleteRequested(const AnnotationEntry& entry);

    /// @brief The writer asked to go back to the text (Esc)
    void editorFocusRequested();

    /// @brief The scope was changed by the writer
    void scopeChanged(AnnotationScope scope);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The cards for the entries that pass the filters, in order
    void rebuild();

    /// @brief The counts on the kind buttons
    void updateKindCounts();

    /// @brief What the panel says when it shows no card
    void updateEmptyText(bool anyShown);

    /// @brief The button of the text in front: its kind ("Story"), "File", or what the writer
    /// opens in the book
    void updateScopeText();

    /// @brief A card's colors from the current theme
    void colorCard(AnnotationCard* card) const;

    /// @brief A new card, connected to the panel
    AnnotationCard* createCard();

    /// @brief Select the card before or after the selected one, or the first or last
    void selectNeighbour(int step);
    void selectEdge(bool last);

    /// @brief Do something to the selected card's annotation
    ///
    /// When its card goes (the annotation removed, or done and no longer shown), the card
    /// after it is selected (before it, for the last one).
    void actOnSelected(const std::function<void(const AnnotationEntry&)>& action);

    /// @brief The keys of the list (see the class)
    bool listKeyPressed(const QKeyEvent* event);

    /// @brief Show on the cards whether the keys are in the list
    void showListFocus(bool focused);

    /// @brief The menu of a card: the one under the mouse, or the selected one for the keys
    bool showCardMenu(const QContextMenuEvent* event);

    /// @brief The filters from the controls, then the cards again
    void readFilters();

    std::vector<AnnotationEntry> m_entries;
    QHash<QString, AnnotationCard*> m_cards;  ///< Cards shown, by their entries' keys
    QString m_selectedKey;
    AnnotationFilter m_filter;
    AnnotationSort m_sort = AnnotationSort::TextOrder;
    AnnotationScope m_scope = AnnotationScope::Chapter;
    bool m_documentAvailable = false;
    std::optional<core::KindWords> m_inFront;    ///< Kind of the element in front, if any
    std::optional<core::KindWords> m_bookTexts;  ///< Main texts of the open book, if any
    bool m_bookOpen = false;
    bool m_updatingControls = false;  ///< Controls set by the panel, not by the writer

    // Colors from the theme: the cards of each kind (comment, to-do, note), the controls
    std::array<AnnotationCardColors, 3> m_cardColors;
    QColor m_base;
    QColor m_text;
    QColor m_highlight;
    QColor m_highlightedText;

    QLineEdit* m_searchEdit{nullptr};
    std::array<QToolButton*, 3> m_kindButtons{};  ///< Comment, to-do, note
    QButtonGroup* m_stateGroup{nullptr};
    QButtonGroup* m_scopeGroup{nullptr};
    QAbstractButton* m_textScopeButton{nullptr};  ///< The text in front: "Chapter", "Story"
    QAbstractButton* m_bookScopeButton{nullptr};
    QComboBox* m_dateCombo{nullptr};
    QComboBox* m_sortCombo{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    QWidget* m_listWidget{nullptr};
    QVBoxLayout* m_listLayout{nullptr};
    QLabel* m_emptyLabel{nullptr};
    QLabel* m_todoLabel{nullptr};
    QToolButton* m_previousTodoButton{nullptr};
    QToolButton* m_nextTodoButton{nullptr};
};

}  // namespace kalahari::gui
