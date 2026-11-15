// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_parts.h
/// @brief The small parts of the cards of the materials, the characters, the locations and the
/// items: the buttons of their lists, the grey sentences, the heights of their lists, the keys
/// their lists take, their tables, the layouts of their groups and the gear at a group's title

#ifndef KALAHARI_GUI_PANELS_CARD_PARTS_H
#define KALAHARI_GUI_PANELS_CARD_PARTS_H

#include "kalahari/core/art_provider.h"

#include <QAbstractItemView>
#include <QString>
#include <QStringList>

class QAbstractButton;
class QFormLayout;
class QGroupBox;
class QHBoxLayout;
class QKeyEvent;
class QLabel;
class QTableWidget;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::gui::card {

/// @brief Give @p button icon @p iconId, also after a change of the theme or of the icons
void keepIcon(QAbstractButton* button, const QString& iconId, core::IconContext context);

/// @brief A small button of a list: "+" or "-", with @p tooltip as its name for the screen
/// readers too and the object name @p name
[[nodiscard]] QToolButton* listButton(QWidget* parent, const char* iconId,
                                      const QString& tooltip, const QString& name);

/// @brief A row of "+" and "-" at the right, over a list; @p remove may be nullptr
[[nodiscard]] QHBoxLayout* buttonRow(QToolButton* add, QToolButton* remove);

/// @brief A grey sentence of a list, which wraps, with the object name @p name
[[nodiscard]] QLabel* hintLabel(QWidget* parent, const QString& text, const QString& name);

/// @brief Height of @p rows rows of @p view, two to eight of them, with the header
[[nodiscard]] int listHeight(const QAbstractItemView* view, int rows, int rowHeight, int header);

/// @brief Give @p table the height of its rows, two to eight of them, with its header
void fitRows(QTableWidget* table);

/// @brief Nothing of the children of @p widget reaches it any more. QWidget's destructor
/// deletes them after the class of @p widget is gone, and a field with the keys finishes its
/// editing then.
void cutOffChildren(QWidget* widget);

/// @brief Whether @p key is @p which without modifiers: Insert or Delete of a list
[[nodiscard]] bool isListKey(const QKeyEvent* key, int which);

/// @brief Whether @p key is Enter without modifiers, which opens the current row of a list also
/// on macOS, where a list edits it
[[nodiscard]] bool isOpenKey(const QKeyEvent* key);

/// @brief A table of a card: one row selected; an @p editable one edits its cells with F2, a
/// double-click or by typing
[[nodiscard]] QTableWidget* cardTable(QWidget* parent, const char* name,
                                      const QString& accessibleName, const QStringList& columns,
                                      bool editable);

/// @brief The keys of a table that edits: none in a book open read-only
[[nodiscard]] QAbstractItemView::EditTriggers editTriggers(bool readOnly);

/// @brief Whether a cell of @p view is being edited: its editor has the keys
[[nodiscard]] bool isEditing(const QAbstractItemView* view);

/// @brief The layout of group @p group of a card, with its margins
[[nodiscard]] QVBoxLayout* groupLayout(QGroupBox* group);

/// @brief A form of a group of a card: the labels at the left of their fields
[[nodiscard]] QFormLayout* groupForm();

/// @brief What card @p card of @p book shows beside its kind: the value of the field that
/// stands there (core::ElementKind::subtitleField()), a value to choose by its name in the
/// program's language; empty when it has none
[[nodiscard]] QString subtitleValue(const core::ProjectManager& book,
                                    const core::ProjectElement& card);

/// @brief Put @p button, a child of @p group, at the right end of the line of its title, in
/// its middle (at the left end from right to left): the gear that opens the settings of the
/// group. Called again whenever the group changes its size.
void placeAtTitle(QGroupBox* group, QWidget* button);

}  // namespace kalahari::gui::card

#endif  // KALAHARI_GUI_PANELS_CARD_PARTS_H
