// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_window_layouts.h
/// @brief The layout of the window of each book, kept for the next time the book opens in a window
/// of its own

#ifndef KALAHARI_GUI_BOOK_WINDOW_LAYOUTS_H
#define KALAHARI_GUI_BOOK_WINDOW_LAYOUTS_H

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <optional>

namespace kalahari::gui {

/// @brief The layout of a book window, as QMainWindow saves it
struct WindowLayout {
    QByteArray geometry;  ///< The place and the size of the window (QWidget::saveGeometry())
    QByteArray state;     ///< Its toolbars and panels (QMainWindow::saveState())
};

/// @brief The setting with the layouts kept for the books: an array of objects with the book (its
/// .klh file), the geometry and the state (base64), the book whose window closed last first
inline constexpr const char* BOOK_LAYOUTS_KEY = "window.books";

/// @brief How many books keep the layout of their window; the books left the longest ago lose it
inline constexpr std::size_t MAX_BOOK_LAYOUTS = 20;

/// @brief The layout kept for the book @p manifest (its .klh file), if it has one
[[nodiscard]] std::optional<WindowLayout> bookWindowLayout(const QString& manifest);

/// @brief Keep @p layout as the layout of the window of the book @p manifest, in place of the one
/// it had
void keepBookWindowLayout(const QString& manifest, const WindowLayout& layout);

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_BOOK_WINDOW_LAYOUTS_H
