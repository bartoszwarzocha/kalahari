/// @file annotations_panel.h
/// @brief The Annotations panel: the comments, TODOs and notes of a chapter or the book

#pragma once

#include <kalahari/gui/panels/annotation_entry.h>

#include <QHash>
#include <QWidget>

#include <array>
#include <vector>

class QAbstractButton;
class QAction;
class QButtonGroup;
class QComboBox;
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
/// order. A card is selected by a click, which also asks to go to its place; its text is
/// edited in place. The panel changes no annotation itself: it reports what the writer
/// does, and the cards show the annotations it is given next.
///
/// Keys in the panel: Up and Down select the previous or next card, Enter edits the
/// selected one, Delete removes it and Esc goes back to the text.
class AnnotationsPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit AnnotationsPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~AnnotationsPanel() override;

    /// @brief The annotations to list (the panel filters and orders them)
    ///
    /// Cards stay for annotations listed again; the one being edited stays even when the
    /// filters would hide it now.
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

    /// @brief The filters in use
    const AnnotationFilter& filter() const { return m_filter; }

    /// @brief Use other filters (also the search text and the kind buttons follow)
    void setFilter(const AnnotationFilter& filter);

    /// @brief The order in use
    AnnotationSort sort() const { return m_sort; }

    /// @brief Use another order
    void setSort(AnnotationSort sort);

    /// @brief The buttons under the list: previous and next TODO
    void setTodoActions(QAction* previous, QAction* next);

    /// @brief Select a card, scrolled into view (none: no card is selected)
    void selectAnnotation(const QString& key);

    /// @brief The key of the selected card (empty: none)
    QString selectedKey() const { return m_selectedKey; }

    /// @brief Edit the text of an annotation in its card
    ///
    /// Filters that would hide it are eased so that it shows.
    void editAnnotation(const QString& key);

    /// @brief The card of an annotation (nullptr: not shown)
    AnnotationCard* card(const QString& key) const;

    /// @brief Give the keys to the list of cards (Up, Down, Enter, Delete)
    void focusList();

    /// @brief Take colors from a theme: the kinds' colors and the cards' backgrounds
    void applyTheme(const core::Theme& theme);

signals:
    /// @brief A card was clicked or selected with the keys: go to the annotation's place
    void annotationActivated(const AnnotationEntry& entry);

    /// @brief Editing of an annotation's text started
    void editingStarted(const AnnotationEntry& entry);

    /// @brief An annotation's text changed while it is edited
    void textEdited(const AnnotationEntry& entry, const QString& text);

    /// @brief Editing of an annotation's text ended
    void editingFinished(const AnnotationEntry& entry);

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

private:
    /// @brief The cards for the entries that pass the filters, in order
    void rebuild();

    /// @brief The counts on the kind buttons
    void updateKindCounts();

    /// @brief What the panel says when it shows no card
    void updateEmptyText(bool anyShown);

    /// @brief A card's colors from the current theme
    void colorCard(AnnotationCard* card) const;

    /// @brief A new card, connected to the panel
    AnnotationCard* createCard();

    /// @brief Select the card before or after the selected one
    void selectNeighbour(int step);

    /// @brief The filters from the controls, then the cards again
    void readFilters();

    std::vector<AnnotationEntry> m_entries;
    QHash<QString, AnnotationCard*> m_cards;  ///< Cards shown, by their entries' keys
    QString m_selectedKey;
    AnnotationFilter m_filter;
    AnnotationSort m_sort = AnnotationSort::TextOrder;
    AnnotationScope m_scope = AnnotationScope::Chapter;
    bool m_documentAvailable = false;
    bool m_updatingControls = false;  ///< Controls set by the panel, not by the writer

    // Kind colors from the theme (comment, TODO, note) and the cards' background
    std::array<QColor, 3> m_kindColors;
    QColor m_cardBase;
    double m_cardTint = 0.06;

    QLineEdit* m_searchEdit{nullptr};
    std::array<QToolButton*, 3> m_kindButtons{};  ///< Comment, TODO, note
    QButtonGroup* m_stateGroup{nullptr};
    QButtonGroup* m_scopeGroup{nullptr};
    QAbstractButton* m_bookScopeButton{nullptr};
    QComboBox* m_dateCombo{nullptr};
    QComboBox* m_sortCombo{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    QWidget* m_listWidget{nullptr};
    QVBoxLayout* m_listLayout{nullptr};
    QLabel* m_emptyLabel{nullptr};
    QToolButton* m_previousTodoButton{nullptr};
    QToolButton* m_nextTodoButton{nullptr};
};

}  // namespace kalahari::gui
