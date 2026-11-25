// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file card_list.h
/// @brief The lists of the cards of the characters, the locations and the items on their tabs:
/// the columns of a kind of cards and what a card shows in them

#ifndef KALAHARI_GUI_CARD_LIST_H
#define KALAHARI_GUI_CARD_LIST_H

#include "kalahari/gui/material_list.h"

#include <QCoreApplication>
#include <QList>
#include <QString>

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
struct CardField;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The list of a kind of cards of the Workshop (characters, locations, items), as the
/// lists of the materials have it (MaterialList, MaterialListPanel)
///
/// A list of cards is named by its kind with its package ("kalahari.base:character"). Its
/// columns are the name, the fields of the kind (those the kind has on its list shown at
/// first, CardField::inList), In Short, the number of the chapters the card is in and of its
/// relations, then the fields of the writer's that its cards have, hidden at first. The search
/// finds a card by its name, by the forms and the other names of its name in the book's
/// dictionary, and by all its fields. The tiles show its portrait with its color, its name, its
/// kind with the field beside it ("Character · main") and In Short.
class CardList {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::CardList)

public:
    /// @name Ids of the columns besides those of MaterialList (TITLE, DESCRIPTION, FIELD_PREFIX)
    /// @{
    static constexpr const char* FIELD_PREFIX = "cardField:";  ///< A field of the kind, by id
    static constexpr const char* CHAPTERS = "chapters";        ///< The chapters it is in
    static constexpr const char* RELATIONS = "relations";      ///< Its relations
    /// @}

    /// @brief Whether @p list is a list of cards: a kind of cards (core::ElementKind::isCard())
    [[nodiscard]] static bool isCardList(const QString& list);

    /// @brief The columns of list @p list of the book open in @p book
    [[nodiscard]] static QList<MaterialColumn> columnsOf(const core::ProjectManager& book,
                                                         const QString& list);

    /// @brief The rows of the cards of @p list in the book open in @p book, in the order of the
    /// Workshop
    [[nodiscard]] static QList<MaterialRow> rowsOf(const core::ProjectManager& book,
                                                   const QString& list);

    /// @brief The id of the field of column @p columnId (FIELD_PREFIX); empty for another
    /// column
    [[nodiscard]] static QString fieldOf(const QString& columnId);

    /// @brief What field @p field of card @p card shows: the name of its value of a list in
    /// the program's language, the name of the card it names, else its value
    [[nodiscard]] static QString textOf(const core::ProjectManager& book,
                                        const core::ProjectElement& card,
                                        const core::CardField& field);

    /// @brief Give card @p cardId the text @p text in column @p columnId: a field of its kind
    /// (core::ProjectManager::setCardFieldText(), the id of a value of a list) or In Short
    /// @return false when the column is neither or the book cannot save it
    static bool setText(core::ProjectManager& book, const QString& cardId,
                        const QString& columnId, const QString& text);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_CARD_LIST_H
