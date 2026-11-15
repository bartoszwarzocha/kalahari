// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file own_fields_box.h
/// @brief The writer's own fields of an element of the Workshop: of a material, of a card of a
/// character, a location or an item

#pragma once

#include "kalahari/core/book_project.h"

#include <QGroupBox>
#include <QList>
#include <QString>

class QLabel;
class QTableWidget;
class QToolButton;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari {
namespace gui {

/// @brief The group "Own Fields": a name and a value each, in the writer's order
///
/// "+" or Insert adds one with the keys in its name, Tab goes to its value, F2 edits a cell,
/// "-" or Delete takes it out (asking when it has a value). The names of the fields of the
/// other elements of the book are suggested. A new field left without a name and a value goes.
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

    /// @brief Show the fields of element @p elementId; the cell being edited keeps what is
    /// typed in it only when nothing changed
    void showFields(const QString& elementId, const QList<core::ElementField>& fields);

    /// @brief The buttons follow the rows and a book open read-only
    void updateButtons();

    QTableWidget* table() const { return m_table; }
    QToolButton* addButton() const { return m_addButton; }
    QToolButton* removeButton() const { return m_removeButton; }

public slots:
    /// @brief Add a field at the end, with the keys in its name
    void addField();

    /// @brief Take out the current field, asking when it has a value
    void removeField();

signals:
    /// @brief The fields of the element were saved
    void changed(const QString& elementId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Save the fields of the table, in its order
    void saveFields();

    /// @brief The fields that the table shows
    QList<core::ElementField> tableFields() const;

    /// @brief The table as high as its rows, or the sentence without rows
    void fitTable();

    core::ProjectManager& m_book;
    QString m_elementId;
    QList<core::ElementField> m_fields;  ///< The fields as saved, with what the table hides
    bool m_updating = false;             ///< The table is being filled: no saving
    QTableWidget* m_table = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
};

}  // namespace gui
}  // namespace kalahari
