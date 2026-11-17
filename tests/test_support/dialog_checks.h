// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file dialog_checks.h
/// @brief Checks of the windows of the tests: the keys of their tabs, their size on a small
///        screen, the font of another system, the windows in Polish

#pragma once

#include <QFont>
#include <QString>
#include <QTranslator>

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

/// @brief The windows in Polish while it lives: the translation the program ships
class Polish {
public:
    Polish();
    ~Polish();
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

}  // namespace kalahari::test
