// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file about_dialog.h
/// @brief About dialog for Kalahari Writer's IDE

#ifndef KALAHARI_GUI_DIALOGS_ABOUT_DIALOG_H
#define KALAHARI_GUI_DIALOGS_ABOUT_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

class QLabel;
class QPushButton;
class QTabWidget;

namespace kalahari::gui::dialogs {

/// @brief About: the version of the program, its authors, the third-party components and the
/// license
///
/// The heading of the program's own dialogs names the program, its version and the version of
/// Qt it runs with; under it four tabs: About, Build, Third-Party Components and License. The
/// Build tab has, short, what the program was built from (the commit and its branch, when,
/// Debug or Release, the system and the compiler); Copy Build Information puts all of it on the
/// clipboard. The copyright has the years up to this one. Close (or Esc) closes the dialog.
///
/// @see MainWindow::onAbout()
class AboutDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Constructs About dialog
    /// @param parent Parent widget (typically MainWindow)
    explicit AboutDialog(QWidget* parent = nullptr);

    /// @brief The tabs: About, Build, Third-Party Components, License
    [[nodiscard]] QTabWidget* tabs() const { return m_tabs; }

    /// @brief What the program was built from, a line for each: the version, the commit, its
    /// branch, when, the configuration, Qt, the system and the compiler
    ///
    /// The text Copy Build Information puts on the clipboard, for a report of a fault.
    [[nodiscard]] static QString buildInformation();

    /// @brief Copy Build Information
    [[nodiscard]] QPushButton* copyButton() const { return m_copy; }

    /// @brief The years of the copyright in @p year: "2025–2026"; "2025" up to 2025 (also for a
    /// clock set earlier)
    [[nodiscard]] static QString copyrightYears(int year);

    /// @brief "Copyright © 2025–…", up to the year of the system's clock
    [[nodiscard]] static QString copyright();

private:
    /// @brief Creates "About" tab content
    /// @return Widget containing application info and credits
    QWidget* createAboutTab();

    /// @brief Creates "Third-Party Components" tab content
    /// @return Widget containing component attribution list
    QWidget* createComponentsTab();

    /// @brief Creates "License" tab content
    /// @return Widget containing full MIT license text
    QWidget* createLicenseTab();

    /// @brief The Build tab: what the program was built from, short, and Copy Build Information
    QWidget* createBuildTab();

    QTabWidget* m_tabs = nullptr;
    QPushButton* m_copy = nullptr;
    QLabel* m_copied = nullptr;  ///< "Copied to the clipboard." after Copy
};

} // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_ABOUT_DIALOG_H
