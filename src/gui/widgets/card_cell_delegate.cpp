// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_cell_delegate.cpp
/// @brief The cells of the table of a list of cards that F2 edits

#include "kalahari/gui/widgets/card_cell_delegate.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/card_list.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/panels/material_list_panel.h"

#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTimer>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// Lines of a longer text and of In Short while they are edited
constexpr int LONG_TEXT_LINES = 4;
/// Characters across an edited longer text at least: a short sentence
constexpr int LONG_TEXT_CHARACTERS = 40;
/// The value of a field with a list that has none, as the table shows it
QString noValue() {
    return QStringLiteral("–");
}

/// The text @p editor holds: the id of the value chosen, the name typed or chosen, the text
QString textOf(const QWidget* editor) {
    if (const auto* box = qobject_cast<const QComboBox*>(editor)) {
        return box->isEditable() ? box->currentText().trimmed() : box->currentData().toString();
    }
    if (const auto* text = qobject_cast<const QPlainTextEdit*>(editor)) {
        return text->toPlainText().trimmed();
    }
    const auto* line = qobject_cast<const QLineEdit*>(editor);
    return line != nullptr ? line->text().trimmed() : QString();
}

/// The editor of field @p field with a list: its values, "–" for none
QWidget* choiceEditor(QWidget* parent, const core::CardField& field) {
    auto* box = new QComboBox(parent);
    box->setObjectName(QStringLiteral("cardCellChoice"));
    box->addItem(noValue(), QString());
    for (const core::CardFieldChoice& choice : field.choices) {
        box->addItem(choice.name.text(programLanguage()), choice.id);
    }
    return box;
}

/// The editor of field @p field of card @p cardId of @p book that names a card: the names of the
/// cards of its kind to choose, or a name to type
QWidget* cardEditor(QWidget* parent, const core::ProjectManager& book, const core::CardField& field,
                    const QString& cardId) {
    auto* box = new QComboBox(parent);
    box->setObjectName(QStringLiteral("cardCellCard"));
    box->setEditable(true);
    box->setInsertPolicy(QComboBox::NoInsert);
    const core::ProjectElement* card = book.findElement(cardId);
    const core::KindRef kind = card != nullptr ? book.kindOf(*card) : core::KindRef{};
    box->addItems(book.cardTitles(kind.package != nullptr ? kind.package->id : QString(),
                                  field.cardKind, cardId));
    return box;
}

/// The editor of a longer text and of In Short: a few lines
QWidget* longTextEditor(QWidget* parent) {
    auto* text = new QPlainTextEdit(parent);
    text->setObjectName(QStringLiteral("cardCellText"));
    text->setTabChangesFocus(true);
    //: The editor of a longer text of a card in the list of cards, empty. In Polish: Enter
    //: zapisuje, Shift+Enter zaczyna nowy wiersz
    text->setPlaceholderText(CardList::tr("Enter saves, Shift+Enter starts a new line"));
    text->setAccessibleDescription(text->placeholderText());
    return text;
}

}  // namespace

CardCellDelegate::CardCellDelegate(core::ProjectManager& book, QObject* parent)
    : QStyledItemDelegate(parent), m_book(book) {}

QWidget* CardCellDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const {
    Q_UNUSED(option);
    const core::CardField* field = fieldAt(index);
    if (field != nullptr && field->type == core::CardFieldType::Choice) {
        return choiceEditor(parent, *field);
    }
    if (field != nullptr && field->type == core::CardFieldType::Card) {
        return cardEditor(parent, m_book, *field, index.data(MaterialListPanel::ID_ROLE).toString());
    }
    if (field == nullptr || field->type == core::CardFieldType::LongText) {
        return longTextEditor(parent);
    }
    auto* line = new QLineEdit(parent);
    line->setObjectName(QStringLiteral("cardCellLine"));
    line->setFrame(false);
    return line;
}

void CardCellDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    const QString value = valueAt(index);
    if (auto* box = qobject_cast<QComboBox*>(editor)) {
        if (box->isEditable()) {
            box->setEditText(value);
        } else {
            box->setCurrentIndex(std::max(0, box->findData(value)));
        }
    } else if (auto* text = qobject_cast<QPlainTextEdit*>(editor)) {
        text->setPlainText(value);
        text->selectAll();
    } else if (auto* line = qobject_cast<QLineEdit*>(editor)) {
        line->setText(value);
        line->selectAll();
    }
}

void CardCellDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                    const QModelIndex& index) const {
    Q_UNUSED(model);
    const QString text = textOf(editor);
    if (text == valueAt(index).trimmed()) {
        return;
    }
    // The list is filled again when the card changes: once the editor has closed
    const QString cardId = index.data(MaterialListPanel::ID_ROLE).toString();
    const QString column = m_columns.value(index.column());
    QTimer::singleShot(0, this, [this, cardId, column, text,
                                 window = QPointer<QWidget>(editor->window())]() {
        if (!CardList::setText(m_book, cardId, column, text)) {
            //: In Polish: Nie zapisano karty
            dialogs::MessageDialog::warning(window, CardList::tr("Card Not Saved"),
                                            CardList::tr("Failed to save changes."));
        }
    });
}

void CardCellDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const {
    if (qobject_cast<QPlainTextEdit*>(editor) == nullptr || editor->parentWidget() == nullptr) {
        QStyledItemDelegate::updateEditorGeometry(editor, option, index);
        return;
    }
    // A few lines from the top of the cell, at least a short sentence across, inside the table
    const QFontMetrics metrics(option.font);
    QRect area = option.rect;
    area.setWidth(std::max(area.width(), metrics.averageCharWidth() * LONG_TEXT_CHARACTERS));
    area.setHeight(std::max(area.height(), metrics.lineSpacing() * LONG_TEXT_LINES +
                                               2 * editor->contentsMargins().top() +
                                               metrics.descent()));
    const QRect inside = editor->parentWidget()->rect();
    area.moveRight(std::min(area.right(), inside.right()));
    area.moveBottom(std::min(area.bottom(), inside.bottom()));
    area.moveLeft(std::max(area.left(), inside.left()));
    area.moveTop(std::max(area.top(), inside.top()));
    editor->setGeometry(area);
}

bool CardCellDelegate::eventFilter(QObject* watched, QEvent* event) {
    auto* text = qobject_cast<QPlainTextEdit*>(watched);
    if (text != nullptr && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<const QKeyEvent*>(event);
        const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
        if (enter && key->modifiers() == Qt::NoModifier) {
            emit commitData(text);
            emit closeEditor(text, QAbstractItemDelegate::NoHint);
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter(watched, event);
}

const core::CardField* CardCellDelegate::fieldAt(const QModelIndex& index) const {
    const QString fieldId = CardList::fieldOf(m_columns.value(index.column()));
    const core::ProjectElement* card =
        fieldId.isEmpty() ? nullptr
                          : m_book.findElement(index.data(MaterialListPanel::ID_ROLE).toString());
    const core::KindRef kind = card != nullptr ? m_book.kindOf(*card) : core::KindRef{};
    return kind ? kind.kind->field(fieldId) : nullptr;
}

QString CardCellDelegate::valueAt(const QModelIndex& index) const {
    const QString cardId = index.data(MaterialListPanel::ID_ROLE).toString();
    const core::ProjectElement* card = m_book.findElement(cardId);
    if (card == nullptr) {
        return {};
    }
    if (m_columns.value(index.column()) == QLatin1String(MaterialList::DESCRIPTION)) {
        return card->description;
    }
    const core::CardField* field = fieldAt(index);
    if (field == nullptr) {
        return {};
    }
    return field->type == core::CardFieldType::Card ? m_book.cardFieldText(cardId, field->id)
                                                     : card->card.value(field->id);
}

}  // namespace kalahari::gui
