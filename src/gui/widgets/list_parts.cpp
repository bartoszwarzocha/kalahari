// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file list_parts.cpp
/// @brief The parts of the small lists of the cards of the Workshop

#include "kalahari/gui/widgets/list_parts.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QStyle>
#include <QToolButton>

#include <algorithm>

namespace kalahari {
namespace gui {
namespace list_parts {

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

}  // namespace list_parts
}  // namespace gui
}  // namespace kalahari
