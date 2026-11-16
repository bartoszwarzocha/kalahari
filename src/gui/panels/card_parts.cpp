// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_parts.cpp
/// @brief The small parts of the cards of the materials, the characters, the locations and the
/// items

#include "kalahari/gui/panels/card_parts.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionGroupBox>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui::card {

namespace {

/// Rows a list shows at least, with its empty sentence or one row
constexpr int MINIMUM_ROWS = 2;
/// Rows a list shows at most; the rest scroll
constexpr int MAXIMUM_ROWS = 8;

/// The margins of a group of a card: at its sides and bottom, and under its title
constexpr int GROUP_MARGIN = 11;
constexpr int GROUP_TOP_MARGIN = 6;
/// The space between the parts of a group
constexpr int GROUP_SPACING = 6;
/// The space between the buttons of a list
constexpr int BUTTON_SPACING = 2;

}  // namespace

void keepIcon(QAbstractButton* button, const QString& iconId, core::IconContext context) {
    auto& art = core::ArtProvider::getInstance();
    const auto update = [button, iconId, context]() {
        button->setIcon(core::ArtProvider::getInstance().getIcon(iconId, context));
    };
    update();
    QObject::connect(&art, &core::ArtProvider::resourcesChanged, button, update);
}

QToolButton* listButton(QWidget* parent, const char* iconId, const QString& tooltip,
                        const QString& name) {
    auto* button = new QToolButton(parent);
    button->setObjectName(name);
    button->setAutoRaise(true);
    keepIcon(button, QString::fromLatin1(iconId), core::IconContext::Menu);
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    return button;
}

QHBoxLayout* buttonRow(QToolButton* add, QToolButton* remove) {
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(BUTTON_SPACING);
    row->addStretch();
    row->addWidget(add);
    if (remove != nullptr) {
        row->addWidget(remove);
    }
    return row;
}

QLabel* hintLabel(QWidget* parent, const QString& text, const QString& name) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(name);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

int listHeight(const QAbstractItemView* view, int rows, int rowHeight, int header) {
    const int shown = std::clamp(rows, MINIMUM_ROWS, MAXIMUM_ROWS);
    const int frame = view->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    return (shown * rowHeight) + header + (2 * frame) + 2;
}

void fitRows(QTableWidget* table) {
    const int rowHeight = table->verticalHeader()->defaultSectionSize();
    const int header =
        table->horizontalHeader()->isHidden() ? 0 : table->horizontalHeader()->sizeHint().height();
    table->setFixedHeight(listHeight(table, table->rowCount(), rowHeight, header));
}

void cutOffChildren(QWidget* widget) {
    for (const QObject* child : widget->findChildren<QObject*>()) {
        child->disconnect(widget);
    }
}

bool isKeyEvent(const QEvent* event) {
    return event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride;
}

bool isListKey(const QKeyEvent* key, int which) {
    Qt::KeyboardModifiers modifiers = key->modifiers();
    modifiers.setFlag(Qt::KeypadModifier, false);
    return key->key() == which && modifiers == Qt::NoModifier;
}

bool isOpenKey(const QKeyEvent* key) {
    return isListKey(key, Qt::Key_Return) || isListKey(key, Qt::Key_Enter);
}

QTableWidget* cardTable(QWidget* parent, const char* name, const QString& accessibleName,
                        const QStringList& columns, bool editable) {
    auto* table = new QTableWidget(0, static_cast<int>(columns.size()), parent);
    table->setObjectName(QString::fromLatin1(name));
    table->setAccessibleName(accessibleName);
    table->setHorizontalHeaderLabels(columns);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(editable ? QAbstractItemView::SelectItems
                                         : QAbstractItemView::SelectRows);
    table->setEditTriggers(editTriggers(!editable));
    table->setWordWrap(false);
    return table;
}

QAbstractItemView::EditTriggers editTriggers(bool readOnly) {
    if (readOnly) {
        return QAbstractItemView::NoEditTriggers;
    }
    return QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
           QAbstractItemView::AnyKeyPressed;
}

bool isEditing(const QAbstractItemView* view) {
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && focus != view && view->isAncestorOf(focus);
}

QVBoxLayout* groupLayout(QGroupBox* group) {
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(GROUP_MARGIN, GROUP_TOP_MARGIN, GROUP_MARGIN, GROUP_MARGIN);
    layout->setSpacing(GROUP_SPACING);
    return layout;
}

QFormLayout* groupForm() {
    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(GROUP_SPACING);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
}

void placeAtTitle(QGroupBox* group, QWidget* button) {
    QStyleOptionGroupBox option;
    option.initFrom(group);
    option.text = group->title();
    option.textAlignment = group->alignment();
    option.subControls = QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxLabel;
    option.lineWidth = 1;
    const QStyle* style = group->style();
    const QRect label =
        style->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxLabel, group);
    const QRect frame =
        style->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxFrame, group);
    const QSize size = button->size();
    const QRect place(QPoint(frame.right() + 1 - size.width(),
                             std::max(0, label.center().y() + 1 - (size.height() / 2))),
                      size);
    button->move(QStyle::visualRect(group->layoutDirection(), group->rect(), place).topLeft());
    button->raise();
}

QString subtitleValue(const core::ProjectManager& book, const core::ProjectElement& card) {
    const core::KindRef kind = book.kindOf(card);
    const core::CardField* field = book.subtitleField(kind);
    const QString value = field != nullptr ? card.card.value(field->id) : QString();
    const core::CardFieldChoice* choice =
        field != nullptr && field->type == core::CardFieldType::Choice ? field->choice(value)
                                                                       : nullptr;
    return choice != nullptr ? choice->name.text(programLanguage()) : value;
}

}  // namespace kalahari::gui::card
