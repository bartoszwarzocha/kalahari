// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file own_fields_box.cpp
/// @brief The writer's own fields of an element of the Workshop

#include "kalahari/gui/widgets/own_fields_box.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/widgets/list_parts.h"

#include <QAbstractItemDelegate>
#include <QCompleter>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// Room around the table and its buttons, at the sides and below
constexpr int MARGIN = 11;
/// Room above them, under the title of the group
constexpr int TOP_MARGIN = 6;

/// The names of the fields: the names of the fields of the book suggested as they are typed
class FieldDelegate : public QStyledItemDelegate {
public:
    /// @param book The book whose names of fields it suggests
    FieldDelegate(const core::ProjectManager& book, QObject* parent)
        : QStyledItemDelegate(parent), m_book(book) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
        auto* line = qobject_cast<QLineEdit*>(editor);
        if (line != nullptr && index.column() == 0) {
            auto* completer = new QCompleter(m_book.fieldNames(), line);
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            completer->setFilterMode(Qt::MatchStartsWith);
            line->setCompleter(completer);
        }
        return editor;
    }

private:
    const core::ProjectManager& m_book;
};

}  // namespace

OwnFieldsBox::OwnFieldsBox(core::ProjectManager& book, const QString& namePrefix,
                           const QString& tableName, bool stretch, QWidget* parent)
    //: The writer's own fields of an element of the Workshop (a material, a card of a
    //: character): a name and a value each. In Polish: Własne pola
    : QGroupBox(tr("Own Fields"), parent),
      m_book(book),
      m_table(new QTableWidget(0, 2, this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(MARGIN, TOP_MARGIN, MARGIN, MARGIN);
    layout->setSpacing(4);
    //: In Polish: Dodaj pole (Insert)
    m_addButton = list_parts::listButton(this, "common.add", tr("Add Field (Insert)"),
                                         namePrefix + QStringLiteral("AddField"));
    //: In Polish: Usuń pole (Delete)
    m_removeButton = list_parts::listButton(this, "common.remove", tr("Delete Field (Delete)"),
                                            namePrefix + QStringLiteral("RemoveField"));
    layout->addLayout(list_parts::buttonRow(m_addButton, m_removeButton));
    // The keys go through the buttons, then the table, as they stand
    setTabOrder(m_addButton, m_removeButton);
    setTabOrder(m_removeButton, m_table);
    m_table->setObjectName(namePrefix + QStringLiteral("FieldsTable"));
    //: The columns of the own fields. In Polish: Nazwa
    m_table->setHorizontalHeaderLabels({tr("Name"),
                                        //: In Polish: Wartość
                                        tr("Value")});
    m_table->setAccessibleName(tableName);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked |
                             QAbstractItemView::EditKeyPressed |
                             QAbstractItemView::AnyKeyPressed);
    m_table->setItemDelegate(new FieldDelegate(m_book, m_table));
    m_table->setWordWrap(false);
    m_table->installEventFilter(this);
    layout->addWidget(m_table);
    //: In Polish: Brak pól. „+” albo Insert dodaje pole.
    m_emptyLabel = list_parts::hintLabel(this, tr("No fields. \"+\" or Insert adds a field."),
                                         namePrefix + QStringLiteral("NoFields"));
    layout->addWidget(m_emptyLabel);
    if (stretch) {
        layout->addStretch(1);
    }
    connect(m_addButton, &QToolButton::clicked, this, &OwnFieldsBox::addField);
    connect(m_removeButton, &QToolButton::clicked, this, &OwnFieldsBox::removeField);
    connect(m_table, &QTableWidget::itemChanged, this, [this]() {
        if (!m_updating) {
            saveFields();
        }
    });
    connect(m_table, &QTableWidget::currentCellChanged, this, &OwnFieldsBox::updateButtons);
    // A new field left without a name and a value goes
    connect(m_table->itemDelegate(), &QAbstractItemDelegate::closeEditor, this,
            [this](const QWidget*, QAbstractItemDelegate::EndEditHint hint) {
                if (hint != QAbstractItemDelegate::EditNextItem &&
                    hint != QAbstractItemDelegate::EditPreviousItem) {
                    dropEmptyRow();
                }
            });
    fitTable();
}

void OwnFieldsBox::dropEmptyRow() {
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount()) {
        return;
    }
    const QTableWidgetItem* name = m_table->item(row, 0);
    const QTableWidgetItem* value = m_table->item(row, 1);
    const bool noName = name == nullptr || name->text().trimmed().isEmpty();
    if (noName && (value == nullptr || value->text().isEmpty())) {
        m_table->removeRow(row);
        if (row < m_fields.size()) {
            m_fields.removeAt(row);
        }
        saveFields();
    }
}

void OwnFieldsBox::showFields(const QString& elementId, const QList<core::ElementField>& fields) {
    // The fields it shows: the cell being edited keeps what is typed in it
    if (elementId != m_elementId || fields != m_saved) {
        m_elementId = elementId;
        m_fields = fields;
        m_saved = fields;
        const bool wasUpdating = m_updating;
        m_updating = true;
        m_table->setRowCount(0);
        for (const core::ElementField& field : fields) {
            const int row = m_table->rowCount();
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(field.name));
            m_table->setItem(row, 1, new QTableWidgetItem(field.value));
        }
        m_updating = wasUpdating;
        fitTable();
    }
    const bool readOnly = m_book.isReadOnly();
    m_table->setEditTriggers(readOnly ? QAbstractItemView::NoEditTriggers
                                      : QAbstractItemView::DoubleClicked |
                                            QAbstractItemView::EditKeyPressed |
                                            QAbstractItemView::AnyKeyPressed);
    updateButtons();
}

void OwnFieldsBox::fitTable() {
    const int rowHeight = m_table->verticalHeader()->defaultSectionSize();
    const int header = m_table->horizontalHeader()->sizeHint().height();
    m_table->setFixedHeight(list_parts::listHeight(m_table, m_table->rowCount(), rowHeight,
                                                   header));
    m_table->setVisible(m_table->rowCount() > 0);
    m_emptyLabel->setVisible(m_table->rowCount() == 0);
}

void OwnFieldsBox::updateButtons() {
    const bool readOnly = m_book.isReadOnly();
    m_addButton->setEnabled(!readOnly);
    m_removeButton->setEnabled(!readOnly && m_table->currentRow() >= 0 &&
                               m_table->rowCount() > 0);
}

QList<core::ElementField> OwnFieldsBox::tableFields() const {
    QList<core::ElementField> fields;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        core::ElementField field = row < m_fields.size() ? m_fields.at(row) : core::ElementField{};
        const QTableWidgetItem* name = m_table->item(row, 0);
        const QTableWidgetItem* value = m_table->item(row, 1);
        field.name = name != nullptr ? name->text().trimmed() : QString();
        field.value = value != nullptr ? value->text() : QString();
        fields << field;
    }
    return fields;
}

void OwnFieldsBox::saveFields() {
    auto& pm = m_book;
    if (m_elementId.isEmpty() || pm.isReadOnly()) {
        return;
    }
    // A field without a name and a value is not saved
    QList<core::ElementField> fields = tableFields();
    fields.removeIf([](const core::ElementField& field) {
        return field.name.isEmpty() && field.value.isEmpty();
    });
    if (!pm.setFields(m_elementId, fields)) {
        //: In Polish: Nie zapisano pól
        dialogs::MessageDialog::warning(this, tr("Fields Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    m_saved = fields;
    m_fields = tableFields();
    updateButtons();
    emit changed(m_elementId);
}

void OwnFieldsBox::addField() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const bool wasUpdating = m_updating;
    m_updating = true;
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem());
    m_table->setItem(row, 1, new QTableWidgetItem());
    m_fields.append(core::ElementField{});
    m_updating = wasUpdating;
    fitTable();
    m_table->setFocus(Qt::OtherFocusReason);
    m_table->setCurrentCell(row, 0);
    m_table->editItem(m_table->item(row, 0));
    updateButtons();
}

void OwnFieldsBox::removeField() {
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount() || m_book.isReadOnly()) {
        return;
    }
    const QTableWidgetItem* name = m_table->item(row, 0);
    const QTableWidgetItem* value = m_table->item(row, 1);
    if (value != nullptr && !value->text().isEmpty()) {
        //: %1: the name of the field. In Polish: Usunąć pole „%1” razem z jego wartością?
        const QString question = tr("Delete the field \"%1\" with its value?")
                                     .arg(name != nullptr ? name->text() : QString());
        //: In Polish: Usuń pole
        if (!dialogs::MessageDialog::confirm(this, tr("Delete Field"), question,
                                             //: In Polish: &Usuń
                                             tr("&Delete"))) {
            return;
        }
    }
    const bool wasUpdating = m_updating;
    m_updating = true;
    m_table->removeRow(row);
    if (row < m_fields.size()) {
        m_fields.removeAt(row);
    }
    m_updating = wasUpdating;
    saveFields();
    fitTable();
    if (m_table->rowCount() > 0) {
        m_table->setCurrentCell(std::min(row, m_table->rowCount() - 1), 0);
        m_table->setFocus(Qt::OtherFocusReason);
    }
}

bool OwnFieldsBox::eventFilter(QObject* watched, QEvent* event) {
    // Insert and Delete of the table; the same keys of the window's commands wait while the
    // table has the keys. A cell being edited has them itself.
    if (watched == m_table &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        const auto* key = static_cast<QKeyEvent*>(event);
        const bool insert = list_parts::isListKey(key, Qt::Key_Insert);
        const bool remove = list_parts::isListKey(key, Qt::Key_Delete);
        if (insert || remove) {
            if (event->type() == QEvent::ShortcutOverride) {
                event->accept();
            } else if (insert) {
                addField();
            } else {
                removeField();
            }
            return true;
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

}  // namespace kalahari::gui
