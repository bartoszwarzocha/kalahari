// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file relation_dialog.h
/// @brief Relation: a relation of a card of a character, a location or an item with another
/// card

#ifndef KALAHARI_GUI_DIALOGS_RELATION_DIALOG_H
#define KALAHARI_GUI_DIALOGS_RELATION_DIALOG_H

#include "kalahari/core/card_relations.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

#include <optional>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QTreeWidgetItem;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::gui::dialogs {

class RowList;

/// @brief What the writer chose in the window Relation
struct RelationChoice {
    QString otherId;      ///< The other card; empty: a new card (newTitle, newKind)
    QString newTitle;     ///< The title of the new card
    QString newKind;      ///< The kind of the new card: "kalahari.base:character"
    QString relationId;   ///< The relation; empty: a relation the writer names (name,
                          ///< inverseName)
    QString name;         ///< The writer's relation: what the other card is to the card
    QString inverseName;  ///< The writer's relation: what the card is to the other card
    QString note;         ///< The note of the relation; empty: none
};

/// @brief Relation: the other card, the relation and its note
///
/// - With whom or what: the cards of the book in groups of their kinds, without the card
///   itself; typing filters them by their names, the forms of the names and their other
///   names in the book's dictionary ("janem" finds Jan Wolski). At the end, for a name that no card of a kind has,
///   "Create character "x"...", one for each kind of cards: the card is made with the
///   relation.
/// - The relation: those between the kinds of the two cards (core::CardRelations::between()),
///   with their groups, then "Own Relation...", which shows a name for each card; the writer's
///   relation is kept in the book for the next relations.
/// - On both cards: how each card shows the relation, its name by the gender of the other one;
///   when the program speaks the language of the book, with the name of the card declined
///   ("On the card of Jana Wolskiego" in Polish).
/// - A note, shown beside the relation on both cards.
///
/// The filter has the keys: the letters filter, the arrows choose in the list, Tab goes to the
/// relation and the note, Enter saves and Esc closes. On a small screen only the list
/// scrolls.
class RelationDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param cardId The card the relation is of
    /// @param edited The relation of the card it changes (core::CardRelations::of()); nullopt:
    ///        a new one
    /// @param parent Parent widget
    RelationDialog(const core::ProjectManager& book, const QString& cardId,
                   const std::optional<core::ShownRelation>& edited, QWidget* parent = nullptr);

    /// @brief What the writer chose
    [[nodiscard]] RelationChoice choice() const;

    /// @brief Make the item of card @p cardId the current one
    void setCurrentCard(const QString& cardId);

    /// @brief Choose relation @p relationId among those offered for the current card
    void setCurrentRelation(const QString& relationId);

    /// @brief The filter above the list of the cards
    [[nodiscard]] QLineEdit* filterEdit() const { return m_filter; }
    /// @brief The cards in groups, and the cards it can create
    [[nodiscard]] RowList* cardList() const { return m_cards; }
    /// @brief The relations between the two cards
    [[nodiscard]] QComboBox* relationCombo() const { return m_relation; }
    /// @brief The writer's relation: what the other card is to the card
    [[nodiscard]] QLineEdit* nameEdit() const { return m_name; }
    /// @brief The writer's relation: what the card is to the other card
    [[nodiscard]] QLineEdit* inverseEdit() const { return m_inverse; }
    /// @brief How each card shows the relation
    [[nodiscard]] QLabel* previewLabel() const { return m_preview; }
    /// @brief The note of the relation
    [[nodiscard]] QLineEdit* noteEdit() const { return m_note; }

protected:
    /// @brief The arrows of the filter choose in the list; the letters typed in the list go to
    /// the filter
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The cards of the book in groups of their kinds
    void fillCards();

    /// @brief Show the cards whose names have @p text, and the cards it can create named so
    void filter(const QString& text);

    /// @brief Whether card item @p item has @p wanted in its title, or in a form of its name
    ///        or of its other names in the book's dictionary
    [[nodiscard]] bool matches(const QTreeWidgetItem* item, const QString& wanted) const;

    /// @brief The genitive of the name of card @p cardId titled @p title, of kind @p kind
    ///        (or of the card to create), as the book's dictionary has it or the program
    ///        gives it; empty when the program speaks another language than the book
    [[nodiscard]] QString genitiveOf(const QString& cardId, const QString& title,
                                     const QString& kind) const;

    /// @brief The items that create a card named @p title, for each kind of cards that has
    /// no card of that name
    void addCreateItems(const QString& title);

    /// @brief The relations between the card and the current item of the list
    void fillRelations();

    /// @brief The names of the writer's relation, under the relations, when it is chosen
    void showOwnNames();

    /// @brief How each card shows the relation chosen
    void showPreview();

    /// @brief The button that saves follows what is chosen and a book open read-only
    void updateAccept();

    /// @brief The kind of the other card ("kalahari.base:location"); empty: none chosen
    [[nodiscard]] QString otherKind() const;

    /// @brief The title of the other card, or of the card to create
    [[nodiscard]] QString otherTitle() const;

    /// @brief The other card; nullptr for none, or a card to create
    [[nodiscard]] const core::ProjectElement* otherCard() const;

    /// @brief Whether "Own Relation..." is chosen
    [[nodiscard]] bool ownChosen() const;

    /// @brief The first item of the list that is shown and is a card or a card to create
    [[nodiscard]] QTreeWidgetItem* firstChoice() const;

    const core::ProjectManager& m_book;  ///< The book of its window
    core::CardRelations m_relations;
    const core::ProjectElement* m_card = nullptr;
    QString m_kind;  ///< The kind of the card: "kalahari.base:character"
    /// The declension of the language of the book, when the program speaks it: the preview
    /// declines the names of the cards
    std::optional<core::NameDeclension> m_declension;
    QLineEdit* m_filter = nullptr;
    RowList* m_cards = nullptr;
    QComboBox* m_relation = nullptr;
    QWidget* m_ownNames = nullptr;
    QFormLayout* m_ownForm = nullptr;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_inverse = nullptr;
    QLabel* m_preview = nullptr;
    QLineEdit* m_note = nullptr;
};

/// @brief Open the window Relation for card @p cardId of @p book, for a new relation or to
/// change @p edited, and save what the writer chose (saveRelation())
/// @return The other card; empty when the window was closed or nothing was saved
QString editRelation(core::ProjectManager& book, const QString& cardId,
                     const std::optional<core::ShownRelation>& edited, QWidget* parent);

/// @brief Save relation @p choice of card @p cardId of @p book, in place of @p edited when it
/// changes one: the new card first, then the writer's relation, then the relation itself; a
/// step that cannot be saved is said so, and the steps after it are not made
/// @return The other card; empty when the relation was not saved
QString saveRelation(core::ProjectManager& book, const QString& cardId,
                     const RelationChoice& choice,
                     const std::optional<core::ShownRelation>& edited, QWidget* parent);

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_RELATION_DIALOG_H
