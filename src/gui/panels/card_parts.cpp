// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_parts.cpp
/// @brief The small parts of the cards of the materials

#include "kalahari/gui/panels/card_parts.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QStyle>
#include <QTableWidget>
#include <QToolButton>

#include <algorithm>

namespace kalahari::gui::card {

namespace {

/// Rows a list shows at least, with its empty sentence or one row
constexpr int MINIMUM_ROWS = 2;
/// Rows a list shows at most; the rest scroll
constexpr int MAXIMUM_ROWS = 8;

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
                        const char* name) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setAutoRaise(true);
    keepIcon(button, QString::fromLatin1(iconId), core::IconContext::Menu);
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    return button;
}

QHBoxLayout* buttonRow(QToolButton* add, QToolButton* remove) {
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(2);
    row->addStretch();
    row->addWidget(add);
    if (remove != nullptr) {
        row->addWidget(remove);
    }
    return row;
}

QLabel* hintLabel(QWidget* parent, const QString& text, const char* name) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(name));
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

int listHeight(const QAbstractItemView* view, int rows, int rowHeight, int header) {
    const int shown = std::clamp(rows, MINIMUM_ROWS, MAXIMUM_ROWS);
    const int frame = view->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    return shown * rowHeight + header + 2 * frame + 2;
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

bool isListKey(const QKeyEvent* key, int which) {
    return key->key() == which && (key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
}

bool isOpenKey(const QKeyEvent* key) {
    return isListKey(key, Qt::Key_Return) || isListKey(key, Qt::Key_Enter);
}

}  // namespace kalahari::gui::card
