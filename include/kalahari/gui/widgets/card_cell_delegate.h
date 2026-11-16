// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_cell_delegate.h
/// @brief The cells of the table of a list of cards that F2 edits: the fields of a card and
/// In Short

#pragma once

#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>

namespace kalahari::core {
class ProjectManager;
struct CardField;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief Edits a cell of the table of a list of cards (CardList) in place, as F2 opens it
///
/// A field of a text has a line, a field with a list its values to choose ("–" for none), a
/// field that names a card the names of the cards of its kind to choose or a name to type, and
/// a longer text and In Short a few lines. Enter and the keys or the mouse that leave the cell
/// keep what was typed, Shift+Enter starts a new line of a longer text and Esc leaves the card
/// as it was. The card gets the text (CardList::setText()) once the editor has closed; a book
/// that cannot save it says so.
class CardCellDelegate : public QStyledItemDelegate {
public:
    /// @param book The book of the window of the list
    /// @param parent Its owner, the table
    explicit CardCellDelegate(core::ProjectManager& book, QObject* parent = nullptr);

    /// @brief The columns of the table, by their place in its model (MaterialColumn::id)
    void setColumns(const QStringList& columns) { m_columns = columns; }

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;

protected:
    /// @brief Enter keeps a longer text, which the view would take as a new line
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The field of the kind of the card of @p index that its column shows; nullptr for
    /// In Short and the other columns
    [[nodiscard]] const core::CardField* fieldAt(const QModelIndex& index) const;

    /// @brief The text of the cell of @p index as its editor starts with it: the id of a value
    /// to choose, the name of a card a field names, else the text of the card
    [[nodiscard]] QString valueAt(const QModelIndex& index) const;

    QStringList m_columns;
    core::ProjectManager& m_book;  ///< The book of its window
};

}  // namespace kalahari::gui
