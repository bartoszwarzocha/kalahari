// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file list_parts.cpp
/// @brief The parts of the small lists of the cards of the Workshop

#include "kalahari/gui/widgets/list_parts.h"

#include "kalahari/core/art_provider.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QStyle>
#include <QStyleOptionGroupBox>
#include <QToolButton>

#include <algorithm>

namespace kalahari::gui::list_parts {

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
    row->setSpacing(2);
    row->addStretch();
    row->addWidget(add);
    row->addWidget(remove);
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
    return shown * rowHeight + header + 2 * frame + 2;
}

bool isListKey(const QKeyEvent* key, int which) {
    return key->key() == which && (key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
}

bool isOpenKey(const QKeyEvent* key) {
    return isListKey(key, Qt::Key_Return) || isListKey(key, Qt::Key_Enter);
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

}  // namespace kalahari::gui::list_parts
