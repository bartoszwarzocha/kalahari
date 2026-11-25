// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_dictionary_panel.h
/// @brief The book's dictionary on its tab in the middle of the window: its names, its spelling
///        rules and its words

#ifndef KALAHARI_GUI_PANELS_BOOK_DICTIONARY_PANEL_H
#define KALAHARI_GUI_PANELS_BOOK_DICTIONARY_PANEL_H

#include <QStringList>
#include <QWidget>

#include <array>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTabBar;
class QToolButton;
class QTreeWidget;

namespace kalahari::core {
class BookDictionary;
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The page of the book's dictionary (core::ProjectManager::bookDictionary())
///
/// Three tabs, each with its count: "Names" (the characters, places, things and other proper
/// names of the book with their forms and other names), "Spelling Rules" (how the author wants
/// words written) and "Book Words" (words right in this book only). The search finds the names
/// also by their forms and other names ("annę" and "ania" find "Anna Wolska"). The buttons ask
/// for what the window does with the dictionary (BookDictionaryCoordinator): Add..., Edit...,
/// Remove, and for the words Add and Move to Your Words; in a book open read-only they are
/// greyed out, saying why. The gear opens Settings > Editor > Spelling.
///
/// Keys: Ctrl+Tab and Ctrl+Shift+Tab go to the next and the previous tab; Find (Ctrl+F) goes
/// to the search (focusSearch()); in a list, Insert adds, Enter (and a double click) edits and
/// Delete removes.
class BookDictionaryPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief The parts of the dictionary, in the order of the tabs
    enum class Part {
        Names,  ///< The names: of the characters, places, items and other things
        Rules,  ///< The spelling rules of the author
        Words   ///< The words right in this book only
    };

    /// @brief How many parts there are
    static constexpr int PART_COUNT = 3;

    /// @brief The page of the dictionary of a book
    /// @param book The book of its window
    /// @param parent Parent widget
    explicit BookDictionaryPanel(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Fill the lists again from the book's dictionary, keeping the rows chosen
    void refresh();

    /// @brief Show a part (its tab)
    void showPart(Part part);

    /// @brief The part shown
    [[nodiscard]] Part part() const;

    /// @brief The search with the keys, its text selected
    void focusSearch();

    /// @brief The list of the part shown with the keys
    void focusList();

    /// @brief The ids of the rows chosen in the list of the part shown: of the names, of the
    ///        rules, or the words
    [[nodiscard]] QStringList selectedIds() const;

    /// @brief Choose the row of @p id in the list of the part shown; false when it is not
    ///        listed
    bool selectId(const QString& id);

    /// @brief The list of a part
    [[nodiscard]] QTreeWidget* list(Part part) const;

    /// @brief The search
    [[nodiscard]] QLineEdit* searchField() const { return m_search; }

    /// @brief The field of a new word of the book
    [[nodiscard]] QLineEdit* newWordField() const { return m_newWord; }

    /// @brief The tabs of the parts
    [[nodiscard]] QTabBar* tabs() const { return m_tabs; }

signals:
    /// @brief Add... was asked for (Insert): a name or a rule
    void addRequested(kalahari::gui::BookDictionaryPanel::Part part);

    /// @brief Edit... was asked for (Enter, a double click): a name or a rule
    void editRequested(kalahari::gui::BookDictionaryPanel::Part part, const QString& id);

    /// @brief Remove was asked for (Delete)
    void removeRequested(kalahari::gui::BookDictionaryPanel::Part part, const QStringList& ids);

    /// @brief Add of the field of a new word was asked for
    void addWordRequested(const QString& word);

    /// @brief Move to Your Words was asked for: the words go to the writer's own words
    void moveToUserWordsRequested(const QStringList& words);

    /// @brief The gear was clicked: Settings > Editor > Spelling
    void settingsRequested();

protected:
    /// @brief Insert and Delete in a list, Enter in the field of a new word
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void createLayout();
    void createKeys();
    void refreshIcons();

    /// @brief Fill the list of a part from the dictionary, with the rows the search finds
    void fillNames(const core::BookDictionary& dictionary);
    void fillRules(const core::BookDictionary& dictionary);
    void fillWords(const core::BookDictionary& dictionary);

    /// @brief The counts on the tabs
    void updateTabs(const core::BookDictionary& dictionary);

    /// @brief The texts and the states of the buttons, and the note, for the part shown
    void updateButtons();

    /// @brief Ask for Add... of the part shown, or the field of a new word
    void requestAdd();

    /// @brief Ask for Edit... of the row chosen
    void requestEdit();

    /// @brief Ask for Remove of the rows chosen
    void requestRemove();

    /// @brief Ask for Add of the field of a new word
    void requestAddWord();

    /// @brief The list of the part shown
    [[nodiscard]] QTreeWidget* currentList() const;

    /// @brief Whether the dictionary cannot be changed: the book is open read-only
    [[nodiscard]] bool isReadOnly() const;

    core::ProjectManager& m_book;  ///< The book of its window

    QLabel* m_icon = nullptr;
    QLabel* m_subtitle = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QTabBar* m_tabs = nullptr;
    QLineEdit* m_search = nullptr;
    QStackedWidget* m_stack = nullptr;
    std::array<QTreeWidget*, PART_COUNT> m_lists{};  ///< By part
    QLineEdit* m_newWord = nullptr;
    QPushButton* m_addWordButton = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_moveButton = nullptr;
    QLabel* m_note = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_BOOK_DICTIONARY_PANEL_H
