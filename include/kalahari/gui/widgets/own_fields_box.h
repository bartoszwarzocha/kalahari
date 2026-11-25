// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file own_fields_box.h
/// @brief The writer's own fields of an element of the Workshop: of a material, of a card of a
/// character, a location or an item

#ifndef KALAHARI_GUI_WIDGETS_OWN_FIELDS_BOX_H
#define KALAHARI_GUI_WIDGETS_OWN_FIELDS_BOX_H

#include "kalahari/core/book_project.h"

#include <QGroupBox>
#include <QList>
#include <QString>

class QLabel;
class QMenu;
class QPoint;
class QTableWidget;
class QToolButton;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The group "Own Fields": a name and a value each, in the writer's order
///
/// "+" or Insert adds one with the keys in its name, Tab goes to its value, F2 edits a cell,
/// "-" or Delete takes it out (asking when it has a value); the menu of the table has the
/// commands too, and on a card Add to All Cards of This Kind (setAddToKind()). The names of
/// the fields of the other elements of the book are suggested. A new field left without a name
/// and a value goes.
/// Every change is saved at once in the .klh file (core::ProjectManager::setFields()); a book
/// open read-only shows the fields and changes nothing.
class OwnFieldsBox : public QGroupBox {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param namePrefix Start of the object names of its parts: "material" names the table
    ///        "materialFieldsTable", the buttons "materialAddField" and "materialRemoveField"
    /// @param tableName The name of the table for screen readers: "Own fields of the material"
    /// @param stretch Whether a stretch under the table takes the rest of its height
    /// @param parent Parent widget
    OwnFieldsBox(core::ProjectManager& book, const QString& namePrefix, const QString& tableName,
                 bool stretch, QWidget* parent = nullptr);

    /// @brief Show the fields of element @p elementId; when they are those it shows or saved
    /// last, the cell being edited keeps what is typed in it
    void showFields(const QString& elementId, const QList<core::ElementField>& fields);

    /// @brief The buttons follow the rows and a book open read-only
    void updateButtons();

    /// @brief The table of the fields: a name and a value in each row
    [[nodiscard]] QTableWidget* table() const { return m_table; }
    /// @brief "+": a new field
    [[nodiscard]] QToolButton* addButton() const { return m_addButton; }
    /// @brief "-": the current field goes
    [[nodiscard]] QToolButton* removeButton() const { return m_removeButton; }

    /// @brief The menu of the table, filled when it opens
    [[nodiscard]] QMenu* menu() const { return m_menu; }

    /// @brief Whether the menu of the table has Add to All Cards of This Kind: the fields of a
    /// card (by default it has not)
    void setAddToKind(bool on) { m_addToKind = on; }

public slots:
    /// @brief Add a field at the end, with the keys in its name
    void addField();

    /// @brief Take out the current field, asking when it has a value
    void removeField();

    /// @brief Make the current field a field of all the cards of the kind of the card, which
    /// take the values of their own fields of its name
    /// (core::ProjectManager::addFieldToKind())
    void addToKind();

signals:
    /// @brief The fields of the element were saved
    void changed(const QString& elementId);

protected:
    /// @brief The keys of the table: Insert, Delete, Tab from a name to its value
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Save the fields of the table, in its order
    void saveFields();

    /// @brief The current row goes when it has neither a name nor a value: a new field left
    /// as it was added
    void dropEmptyRow();

    /// @brief The fields that the table shows
    [[nodiscard]] QList<core::ElementField> tableFields() const;

    /// @brief The table as high as its rows, or the sentence without rows
    void fitTable();

    /// @brief Fill the menu of the table and open it at @p pos, in the table's viewport
    void showMenu(const QPoint& pos);

    core::ProjectManager& m_book;
    QString m_elementId;
    QList<core::ElementField> m_fields;  ///< The fields of the rows, with what the table hides
    QList<core::ElementField> m_saved;   ///< The fields of the book, as shown or saved last
    bool m_updating = false;             ///< The table is being filled: no saving
    bool m_addToKind = false;            ///< The menu has Add to All Cards of This Kind
    QTableWidget* m_table = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    QMenu* m_menu = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_OWN_FIELDS_BOX_H
