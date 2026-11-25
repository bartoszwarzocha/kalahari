// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file dialog_checks.h
/// @brief Checks of the windows of the tests: the keys of their tabs and sections, their size
///        on a small screen, the font of another system, the windows in Polish

#pragma once

#include "kalahari_tests/polish.h"

#include <QFont>
#include <QString>

class QTabWidget;
class QWidget;

namespace kalahari::test {

/// @brief The page of the tabs that has @p widget; null for a widget outside the tabs
const QWidget* tabPageOf(const QWidget* widget);

/// @brief Whether @p widget shows on its tab, whichever tab is in front
bool onItsTab(const QWidget* widget);

/// @brief The texts with the same access key, or with a key that is not a Latin letter or
///        digit (Alt does not reach "ś"), on any tab of @p tabs in front: the titles of the
///        tabs shown, and the labels of the fields, the buttons and the groups that show on
///        that tab or outside the tabs; empty when each key is the only one
QString keyProblems(const QWidget& dialog, const QTabWidget& tabs);

/// @brief The texts with the same access key, or with a key that is not a Latin letter or
///        digit, in a window of sections (gui::CollapsibleSection): the headers and what is
///        outside the sections with each other and with the fields of each section; empty
///        when each key is the only one. The fields of two sections may share a key, as
///        Alt and the key go from one to the other
QString sectionKeyProblems(const QWidget& dialog);

/// @brief The window fits a small screen whole, so nothing in it scrolls: on Windows and
///        Linux 1366x768 at 150%, 911 pixels wide and 460 high. A Mac has no such scaling: its
///        smallest screen is 1366x768 at 100% (the 11-inch MacBook Air), 640 pixels high under
///        the menu bar, the title bar and the Dock, and its controls and spacing are larger
void checkFitsSmallScreen(const QWidget& dialog);

/// @brief The font of the program of @p pixels while it lives: the window fits with the font
///        of another system, whose letters are as high (Windows' Segoe UI has the lines of a
///        13 pixel font, macOS has 13 pixels)
class AppFontOfPixels {
public:
    explicit AppFontOfPixels(int pixels);
    ~AppFontOfPixels();
    AppFontOfPixels(const AppFontOfPixels&) = delete;
    AppFontOfPixels& operator=(const AppFontOfPixels&) = delete;

private:
    QFont m_font;
};

}  // namespace kalahari::test
