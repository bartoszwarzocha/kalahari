// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file about_dialog.h
/// @brief About dialog for Kalahari Writer's IDE

#ifndef KALAHARI_GUI_DIALOGS_ABOUT_DIALOG_H
#define KALAHARI_GUI_DIALOGS_ABOUT_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QList>

class QEvent;
class QFrame;
class QLabel;
class QPushButton;
class QTabWidget;

namespace kalahari::gui::dialogs {

/// @brief About: the version of the program, its authors, the third-party components and the
/// license
///
/// The heading of the program's own dialogs has the logo of Kalahari, the name of the program,
/// its version and the version of Qt it runs with and, at its right end apart from them, the
/// full logo of BW Digital Systems & Publishing (its black letters on a light theme, the white
/// ones on a dark one; a click opens the company's page); under it four tabs: About, Build,
/// Third-Party Components and License. The About tab has parts apart: what Kalahari is; Authors
/// (the idea and the overall architecture, and the company that created the program, as the
/// licence requires of every copy: NOTICE.md, additional term 1); Thanks; Project; under a line
/// the copyright and that Kalahari is free software without warranty. The Build tab has, short,
/// what the program was built from (the commit and its branch, when, Debug or Release, the
/// system and the compiler); Copy Build Information puts all of it on the clipboard. The
/// copyright has the years up to this one. The License tab has the notice of the GNU GPL version
/// 3, the additional terms and the text of the licence (LICENSE, embedded in the program as
/// :/licenses/LICENSE). Close (or Esc) closes the dialog.
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

    /// @brief "Copyright © 2025–… Bartosz W. Warzocha", up to the year of the system's clock
    [[nodiscard]] static QString copyright();

    /// @brief The authors of the program, as every copy and every work based on it keeps them
    /// (NOTICE.md, additional term 1): "Kalahari – created by BW Digital Systems & Publishing,
    /// https://bwdsp.com.pl/", in the language of the user interface
    [[nodiscard]] static QString attribution();

    /// @brief The address in attribution(), a link on the About tab
    [[nodiscard]] static QString authorAddress();

    /// @brief The full logo of BW Digital Systems & Publishing for a background: the one with
    /// white letters on a dark background (@p darkBackground), the one with black letters on a
    /// light one; a path of the program's resources
    [[nodiscard]] static QString companyLogo(bool darkBackground);

    /// @brief The logo of the authors, at the right end of the heading
    [[nodiscard]] QLabel* companyLogoLabel() const { return m_companyLogo; }

    /// @brief The logo of Kalahari before the name in the heading; hidden where the program
    /// has not its image (then the heading has the icon of About)
    [[nodiscard]] QLabel* programLogoLabel() const { return m_programLogo; }

    /// @brief The text of the GNU GPL version 3 the program carries (LICENSE); empty if the
    /// program has none
    [[nodiscard]] static QString licenseText();

protected:
    /// @brief Another theme (its palette): the logo of the authors for its background and the
    /// muted color of the labels of the About tab
    void changeEvent(QEvent* event) override;

    /// @brief A click on the logo of the authors opens their page
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The logo of the authors for the background of the heading, light or dark
    void updateCompanyLogo();

    /// @brief The logos in the heading: Kalahari before the name, the authors at the right end
    void addHeadingLogos();

    /// @brief The labels, the headings of the parts and the legal notices of the About tab in
    /// the muted color of notes, readable on the window
    void updateMutedColors();

    /// @brief A label of the About tab in the muted color (updateMutedColors())
    QLabel* mutedLabel(const QString& text, QWidget* parent);

    /// @brief Creates "About" tab content
    /// @return Widget containing application info and credits
    QWidget* createAboutTab();

    /// @brief Creates "Third-Party Components" tab content
    /// @return Widget containing component attribution list
    QWidget* createComponentsTab();

    /// @brief Creates "License" tab content
    /// @return Widget with the notice of the GNU GPL version 3, the additional terms and the
    /// text of the licence
    QWidget* createLicenseTab();

    /// @brief The Build tab: what the program was built from, short, and Copy Build Information
    QWidget* createBuildTab();

    QTabWidget* m_tabs = nullptr;
    QPushButton* m_copy = nullptr;
    QLabel* m_copied = nullptr;  ///< "Copied to the clipboard." after Copy
    QLabel* m_companyLogo = nullptr;  ///< The logo of BW Digital Systems & Publishing
    QLabel* m_programLogo = nullptr;  ///< The logo of Kalahari
    QFrame* m_headingLine = nullptr;  ///< Between the name of the program and the logo
    QList<QLabel*> m_mutedLabels;     ///< The labels in the muted color
};

} // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_ABOUT_DIALOG_H
