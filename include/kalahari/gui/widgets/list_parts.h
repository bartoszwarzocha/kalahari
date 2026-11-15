// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file list_parts.h
/// @brief The parts of the small lists of the cards of the Workshop (of materials and of
/// characters, locations and items): their "+" and "-" buttons, their grey sentences, their
/// heights and their keys

#pragma once

#include "kalahari/core/art_provider.h"

#include <QString>

class QAbstractButton;
class QAbstractItemView;
class QHBoxLayout;
class QKeyEvent;
class QLabel;
class QToolButton;
class QWidget;

namespace kalahari {
namespace gui {
namespace list_parts {

/// @brief Rows of a list shown at least and at most; the others scroll
constexpr int MINIMUM_ROWS = 2;
constexpr int MAXIMUM_ROWS = 8;

/// @brief Give @p button icon @p iconId, also after a change of the theme or of the icons
void keepIcon(QAbstractButton* button, const QString& iconId, core::IconContext context);

/// @brief A small button of a list, "+" or "-", with @p tooltip and the object name @p name
QToolButton* listButton(QWidget* parent, const char* iconId, const QString& tooltip,
                        const QString& name);

/// @brief A row of "+" and "-" at the right, over a list
QHBoxLayout* buttonRow(QToolButton* add, QToolButton* remove);

/// @brief A grey sentence of a list, which wraps, with the object name @p name
QLabel* hintLabel(QWidget* parent, const QString& text, const QString& name);

/// @brief Height of @p rows rows of @p view, between MINIMUM_ROWS and MAXIMUM_ROWS, with the
/// header
int listHeight(const QAbstractItemView* view, int rows, int rowHeight, int header);

/// @brief Whether @p key is @p which without modifiers: Insert or Delete of a list, which its
/// own handler takes
bool isListKey(const QKeyEvent* key, int which);

/// @brief Whether @p key is Enter without modifiers, which opens the current row of a list
/// also on macOS, where a list edits it
bool isOpenKey(const QKeyEvent* key);

}  // namespace list_parts
}  // namespace gui
}  // namespace kalahari
