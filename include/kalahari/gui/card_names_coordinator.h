// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_names_coordinator.h
/// @brief The names of the cards of the Workshop in the book's dictionary: a new card gets
///        its name, a renamed card changes it, and a text creates a card from a name in it

#ifndef KALAHARI_GUI_CARD_NAMES_COORDINATOR_H
#define KALAHARI_GUI_CARD_NAMES_COORDINATOR_H

#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/name_declension.h>

#include <QObject>
#include <QString>

class QTabWidget;
class QWidget;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::gui {

class BookDictionaryCoordinator;

/// @brief Keeps the names of the cards of characters, locations and items in the book's
///        dictionary (core/card_names.h)
///
/// A new card takes over the name of the dictionary without a card that its title is, with
/// its forms, or gets a new one with the forms the program gives it; a renamed card gives its
/// name the new title. The window "Name" of the dictionary opens for the name of a card
/// (editName()), and a card adds other names to it (addOtherName()). The texts of the book
/// offer Create Card on the context menu of a misspelled word and of a selection
/// (editor::BookEditor::setCardKinds()): Add Element opens with the kind chosen and the name
/// in it (addCardRequested()).
class CardNamesCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param book The book of its window
    /// @param dictionary The book's dictionary of the window: its windows and its forms
    /// @param centralTabs The tabs in the middle of the window: their texts offer Create Card
    /// @param parent Parent object
    CardNamesCoordinator(core::ProjectManager& book, BookDictionaryCoordinator& dictionary,
                         QTabWidget* centralTabs, QObject* parent = nullptr);

    /// @brief What a name of card @p card is in the book's dictionary (core::nameKindOf())
    [[nodiscard]] static core::NameKind nameKindOf(const core::ProjectManager& book,
                                                   const core::ProjectElement& card);

public slots:
    /// @brief The window "Name" of the book's dictionary for the name of card @p cardId: its
    ///        own, else the one it takes over, else a new one
    /// @return Whether the name was put in the dictionary
    bool editName(const QString& cardId);

    /// @brief A new other name of card @p cardId, typed in the program's window for a text:
    ///        it gets the models and the forms the program gives it
    /// @param window The window the window for the text opens over
    /// @return Whether it was added
    bool addOtherName(const QString& cardId, QWidget* window);

signals:
    /// @brief Create Card on the context menu of a text: Add Element with kind @p kind chosen
    ///        and @p title in it
    void addCardRequested(const QString& title, const kalahari::core::KindRef& kind);

private:
    /// @brief A new card gets its name in the dictionary (ProjectManager::cardAdded())
    void nameCard(const QString& cardId);

    /// @brief A renamed card gives its name the new title (ProjectManager::elementRenamed())
    void retitleCard(const QString& cardId);

    /// @brief The text of @p page offers Create Card with the kinds of cards of the book
    void offerCards(QWidget* page);

    /// @brief Create Card of a text: the kind of reference @p kind with @p title
    void onCreateCardAsked(const QString& title, const QString& kind);

    core::ProjectManager& m_book;               ///< The book of its window
    BookDictionaryCoordinator& m_dictionary;    ///< Its windows and the forms of the words
    QTabWidget* m_centralTabs;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_CARD_NAMES_COORDINATOR_H
