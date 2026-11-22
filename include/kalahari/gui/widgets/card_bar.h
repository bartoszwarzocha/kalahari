// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_bar.h
/// @brief The commands of the bar over a tab in the middle of the window, in one look

#ifndef KALAHARI_GUI_WIDGETS_CARD_BAR_H
#define KALAHARI_GUI_WIDGETS_CARD_BAR_H

#include <QKeyCombination>
#include <QString>

class QAction;
class QObject;
class QToolButton;
class QWidget;

namespace kalahari::gui {

/// @brief The commands of the bar over a tab in the middle of the window (a timeline, ...)
///
/// A command is an action with the icon of the program (it follows the theme), its name and,
/// in its tooltip, its keys: "Delete the Event (Delete)". Its button is flat until pointed at;
/// the main commands of a tab have their name beside the icon. The keys work in the tab
/// itself, and the same commands are in its menus.
class CardBar {
public:
    /// @brief A command
    /// @param iconId The icon (core::ArtProvider)
    /// @param name Its name, as the menus of the tab have it
    /// @param keys Its keys in the tab (none: Qt::Key_unknown)
    /// @param parent The owner of the action
    static QAction* command(const QString& iconId, const QString& name, QKeyCombination keys,
                            QObject* parent);

    /// @brief The button of a command
    /// @param named Its name beside the icon (a main command of the tab)
    static QToolButton* button(QAction* command, const char* objectName, bool named,
                               QWidget* parent);

    /// @brief "Name (keys)", or the name alone without keys
    [[nodiscard]] static QString withKeys(const QString& name, QKeyCombination keys);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_CARD_BAR_H
