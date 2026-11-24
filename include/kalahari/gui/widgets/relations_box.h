// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file relations_box.h
/// @brief The relations of a card of a character, a location or an item with the other cards

#ifndef KALAHARI_GUI_WIDGETS_RELATIONS_BOX_H
#define KALAHARI_GUI_WIDGETS_RELATIONS_BOX_H

#include "kalahari/core/card_relations.h"

#include <QGroupBox>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QKeyEvent;
class QLabel;
class QMenu;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The group "Relations" of a card: what each other card is to it
///
/// A row has the name of the relation as the card has it, by the gender of the other card
/// ("brother"), and the other card with its color; the note of the relation is beside it on
/// the full card and in the tooltip. The full card has the rows in groups of the kinds of the
/// other cards. A relation is saved once, with the card it was added to; the other card shows
/// it by itself (core::CardRelations::of()).
///
/// "+" or Insert opens the window Relation for a new relation, F2 for the current one, and "-"
/// or Delete takes it out (asking when it has a note). Enter or a double-click shows the other
/// card (requestShowCard()); the menu of a row has it all too. A book open read-only shows the
/// relations and changes nothing.
class RelationsBox : public QGroupBox {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param full Whether it is on the full card: the rows in groups, with their notes
    /// @param parent Parent widget
    RelationsBox(core::ProjectManager& book, bool full, QWidget* parent = nullptr);
    /// @note What the list does while the box is destroyed does not reach the box
    ~RelationsBox() override;

    RelationsBox(const RelationsBox&) = delete;
    RelationsBox& operator=(const RelationsBox&) = delete;
    RelationsBox(RelationsBox&&) = delete;
    RelationsBox& operator=(RelationsBox&&) = delete;

    /// @brief Show the relations of card @p cardId
    void showCard(const QString& cardId);

    /// @brief Show the relations again as the book has them; the current row stays
    void refresh();

    /// @brief The relation of the current row; nullopt for none
    [[nodiscard]] std::optional<core::ShownRelation> currentRelation() const;

    /// @brief The relations, in groups on the full card
    [[nodiscard]] QTreeWidget* list() const { return m_list; }
    /// @brief "+": a new relation
    [[nodiscard]] QToolButton* addButton() const { return m_addButton; }
    /// @brief "-": the current relation goes
    [[nodiscard]] QToolButton* removeButton() const { return m_removeButton; }
    /// @brief The sentence shown without relations
    [[nodiscard]] QLabel* emptyLabel() const { return m_emptyLabel; }
    /// @brief The menu of a row, filled when it opens
    [[nodiscard]] QMenu* rowMenu() const { return m_menu; }

    /// @brief Show @p offer under the relations: what a module offers after a new relation
    /// (core::IWindowEditorProvider::relationAdded()); the offer closes itself
    void showOffer(QWidget* offer);

public slots:
    /// @brief Open the window Relation for a new relation of the card
    void addRelation();

    /// @brief Open the window Relation for the current relation
    void changeRelation();

    /// @brief Take out the current relation, asking when it has a note
    void removeRelation();

signals:
    /// @brief Show card @p cardId: the other card of a relation
    void requestShowCard(const QString& cardId);

    /// @brief Card @p cardId was made in the window Relation: the Navigator shows it
    void cardAdded(const QString& cardId);

    /// @brief Relation @p relationId of card @p cardId with card @p otherId was added in the
    /// window Relation: a module may offer something under the relations (showOffer())
    void relationAdded(kalahari::gui::RelationsBox* box, const QString& cardId,
                       const QString& otherId, const QString& relationId);

protected:
    /// @brief Insert, Delete, F2 and Enter of the list
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief A row of relation @p index of m_shown under @p parent (nullptr: at the top)
    void addRow(QTreeWidgetItem* parent, int index, const core::CardRelations& relations);

    /// @brief Rows of the relations in groups of the kinds of the other cards
    void addGroups(const core::CardRelations& relations);

    /// @brief The kinds of the groups: those of the Workshop in its order, then those of the
    /// other cards the program does not have
    [[nodiscard]] QStringList groupKinds(const core::CardRelations& relations) const;

    /// @brief What @p key does in the list: Insert adds a relation, Delete takes it out, F2
    /// changes it, Enter shows the other card; nothing for another key
    [[nodiscard]] std::function<void()> keyAction(const QKeyEvent* key);

    /// @brief Open the window Relation for @p edited, or for a new relation
    void openRelation(const std::optional<core::ShownRelation>& edited);

    /// @brief Fill the menu of the current row as it opens
    void fillMenu();

    /// @brief The list as high as its rows; the sentence without them
    void fitList();

    /// @brief The buttons follow the current row and a book open read-only
    void updateButtons();

    /// @brief Make the first row whose relation @p matches the current one
    void selectRow(const std::function<bool(const core::ShownRelation&)>& matches);

    QString m_cardId;
    bool m_full = false;
    QList<core::ShownRelation> m_shown;  ///< The relations of the card, as of() gives them
    QTreeWidget* m_list = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    QMenu* m_menu = nullptr;
    core::ProjectManager& m_book;  ///< The book of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_RELATIONS_BOX_H
