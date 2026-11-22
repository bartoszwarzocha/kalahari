// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_bar.cpp
/// @brief The commands of the bar over a tab in the middle of the window

#include "kalahari/gui/widgets/card_bar.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/gui/shortcut_rules.h"

#include <QAction>
#include <QToolButton>

namespace kalahari::gui {

QAction* CardBar::command(const QString& iconId, const QString& name, QKeyCombination keys,
                          QObject* parent) {
    QAction* action = core::ArtProvider::getInstance().createAction(iconId, name, parent);
    action->setToolTip(withKeys(name, keys));
    return action;
}

QToolButton* CardBar::button(QAction* command, const char* objectName, bool named,
                             QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(objectName));
    button->setDefaultAction(command);
    button->setAutoRaise(true);
    button->setToolButtonStyle(named ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
    // The keys stay in the tab after a click; Tab still reaches the button
    button->setFocusPolicy(Qt::TabFocus);
    return button;
}

QString CardBar::withKeys(const QString& name, QKeyCombination keys) {
    if (keys.key() == Qt::Key_unknown || keys.toCombined() == 0) {
        return name;
    }
    return QStringLiteral("%1 (%2)").arg(name, ShortcutRules::keysText(keys));
}

}  // namespace kalahari::gui
