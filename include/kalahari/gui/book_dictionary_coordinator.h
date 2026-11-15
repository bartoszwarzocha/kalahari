// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_dictionary_coordinator.h
/// @brief The book's dictionary on its tab, its windows and the commands that change it

#ifndef KALAHARI_GUI_BOOK_DICTIONARY_COORDINATOR_H
#define KALAHARI_GUI_BOOK_DICTIONARY_COORDINATOR_H

#include "kalahari/gui/panels/book_dictionary_panel.h"

#include <kalahari/core/name_declension.h>

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QStatusBar;
class QTabWidget;
class QWidget;

namespace kalahari::core {
class BookDictionary;
class ProjectManager;
struct DictionaryName;
struct SpellingRule;
}  // namespace kalahari::core

namespace kalahari::gui {

class CommandRegistry;
class SpellingCoordinator;

/// @brief Shows the book's dictionary on a tab in the middle of the window and changes it
///        with its windows
///
/// Book > Book Dictionary... (no keys by default) and "Book Dictionary" at the top of the
/// Workshop in the Navigator open the page of the dictionary (BookDictionaryPanel) on a tab of
/// its own, one for the book, or bring it to the front; it follows the dictionary and closes
/// with the book. Add Name..., Edit... of a name, and Add Name... on the context menu of a
/// misspelled word or on its card in the Spelling and Grammar panel (with the word in it),
/// open the window of a name (dialogs::NameDialog), in the language of the book; those of a
/// rule open the window of a rule (dialogs::RuleDialog). Remove asks first, in the program's
/// own window. Move to Your Words moves words of the book to the writer's own words. The
/// dictionary of a book open read-only is shown, but not changed.
///
/// A card of the Workshop (once the program has them) opens the window of its name with
/// editCardName().
class BookDictionaryCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Book > Book Dictionary...
    static constexpr const char* COMMAND_ID = "book.dictionary";

    /// @brief Constructor
    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param centralTabs The tabs in the middle of the window
    /// @param spelling The spelling as you type: its dictionary gives the forms of the words
    ///        (may be null: the models alone)
    /// @param statusBar Where it says what could not be done (may be null)
    /// @param parent Parent object
    BookDictionaryCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                              QTabWidget* centralTabs, SpellingCoordinator* spelling,
                              QStatusBar* statusBar, QObject* parent = nullptr);

    /// @brief Destructor: the command has no callback any more
    ~BookDictionaryCoordinator() override;

    BookDictionaryCoordinator(const BookDictionaryCoordinator&) = delete;
    BookDictionaryCoordinator& operator=(const BookDictionaryCoordinator&) = delete;
    BookDictionaryCoordinator(BookDictionaryCoordinator&&) = delete;
    BookDictionaryCoordinator& operator=(BookDictionaryCoordinator&&) = delete;

    /// @brief Give Book > Book Dictionary... its callback
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief Show the dictionary of the open book on its tab, in front
    /// @param keys The list of the part shown gets the keys
    /// @return The page; none without an open book
    BookDictionaryPanel* openDictionary(bool keys = true);

    /// @brief The page of the dictionary on a tab, or nullptr
    [[nodiscard]] BookDictionaryPanel* panel() const;

    /// @brief The page of the dictionary when its tab is in front, else nullptr
    [[nodiscard]] BookDictionaryPanel* currentPanel() const;

public slots:
    /// @brief The window of a new name, with @p text in it and its forms suggested
    /// @return Whether the name was added
    bool addName(const QString& text = QString());

    /// @brief The window of the name with id @p nameId
    /// @return Whether it was changed
    bool editName(const QString& nameId);

    /// @brief The window of the name of a card of the Workshop: the name of the card, else
    ///        one without a card whose name is @p cardTitle (the card takes it over), else a
    ///        new one with the forms suggested; the name keeps the card's id
    /// @return Whether the name was put in the dictionary
    bool editCardName(const QString& cardId, const QString& cardTitle, core::NameKind kind);

    /// @brief The window of a new spelling rule
    /// @return Whether the rule was added
    bool addRule();

    /// @brief The window of the spelling rule with id @p ruleId
    /// @return Whether it was changed
    bool editRule(const QString& ruleId);

    /// @brief Remove names, rules or words, after asking
    /// @return Whether they were removed
    bool removeEntries(kalahari::gui::BookDictionaryPanel::Part part, const QStringList& ids);

    /// @brief Add a word of the book
    /// @return false when it could not be added (the status bar says why)
    bool addWord(const QString& word);

    /// @brief Move words of the book to the writer's own words (every book)
    /// @return false when they could not be moved (the status bar says why)
    bool moveToUserWords(const QStringList& words);

signals:
    /// @brief The gear of the page was clicked: Settings > Editor > Spelling
    void settingsRequested();

private:
    /// @brief Give the page what it asks for
    void connectPanel(BookDictionaryPanel* panel);

    /// @brief The icon and the text of the tab of the page
    static void updateTab(BookDictionaryPanel* panel);

    /// @brief Show the window of a name, and put it in the dictionary when accepted
    bool runNameDialog(const core::DictionaryName& name);

    /// @brief Show the window of a rule, and put it in the dictionary when accepted
    bool runRuleDialog(const core::SpellingRule& rule);

    /// @brief Change the book's dictionary; the status bar says when it cannot be changed
    bool change(const std::function<void(core::BookDictionary&)>& change);

    /// @brief The models of the language of the book
    [[nodiscard]] core::NameDeclension declension() const;

    /// @brief The forms the spelling dictionary of the chapters knows, when it is of the
    ///        language of the book
    [[nodiscard]] core::NameDeclension::DictionaryForms dictionaryForms() const;

    /// @brief Show a short message in the status bar
    void showMessage(const QString& message);

    /// @brief The window the windows open over
    [[nodiscard]] QWidget* window() const;

    QTabWidget* m_centralTabs;
    SpellingCoordinator* m_spelling;
    QStatusBar* m_statusBar;
    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;   ///< The commands of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_BOOK_DICTIONARY_COORDINATOR_H
