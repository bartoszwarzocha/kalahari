/// @file about_dialog.cpp
/// @brief Implementation of AboutDialog

#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/widgets/fitting_scroll_area.h"
#include "kalahari/version.h"
#include <QVBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>

using namespace kalahari::gui::dialogs;

// ============================================================================
// Constructor
// ============================================================================

AboutDialog::AboutDialog(QWidget* parent)
    : KalahariDialog(parent)
{
    setHeading(tr("About Kalahari Writer's IDE"),
               //: The heading of the About window. %1: the version of the program, %2: the
               //: version of Qt it runs with. In Polish: Wersja %1, Qt %2
               tr("Version %1, Qt %2")
                   .arg(QString::fromLatin1(kalahari::VERSION_STRING),
                        QString::fromLatin1(qVersion())));
    setHeadingIcon(QStringLiteral("help.about"));

    // Tab widget
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createAboutTab(), tr("About"));
    m_tabs->addTab(createComponentsTab(), tr("Third-Party Components"));
    m_tabs->addTab(createLicenseTab(), tr("License"));
    contentLayout()->addWidget(m_tabs, 1);

    // It only informs: Close, which Enter and Esc press
    setAcceptText(tr("Close"));
    cancelButton()->hide();

    // Lower where the screen is, e.g. 1366x768 at 150%: the tabs scroll
    kalahari::gui::utils::resizeWithinScreen(this, QSize(600, 600));
}

// ============================================================================
// Tab Creation Methods
// ============================================================================

QWidget* AboutDialog::createAboutTab() {
    // The name, the version and the version of Qt are in the heading
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    // Platform info
    QLabel* platform = new QLabel(
        tr("Cross-platform Writer's IDE for Windows, macOS, and Linux"), widget);
    platform->setAlignment(Qt::AlignCenter);
    platform->setWordWrap(true);
    layout->addWidget(platform);
    layout->addSpacing(10);

    // Description
    QLabel* desc = new QLabel(
        tr("Kalahari is a modern writing environment designed for book authors.\n"
           "Built with C++20 and Qt 6.\n\n"
           "A comprehensive writing toolkit with project management,\n"
           "statistics tracking, and powerful export capabilities."), widget);
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(desc);
    layout->addSpacing(10);

    // Credits section
    QLabel* credits = new QLabel(
        tr("Kalahari Writer's IDE - Development Team\n\n"
           "Project Vision & Architecture:\n"
           "  Bartosz Warzocha (bartosz.warzocha@gmail.com)\n"
           "Technology Stack:\n"
           "  C++20, Qt6 6.9+, CMake, vcpkg\n\n"
           "Special Thanks:\n"
           "  - Anthropic for Claude AI assistance\n"
           "  - Qt team for excellent cross-platform GUI framework\n"
           "  - Open source community for exceptional libraries\n\n"
           "Project Timeline:\n"
           "  Start: 2025-11\n"
           "  Target Release: February 2027 (Kalahari 1.0)"), widget);
    credits->setWordWrap(true);
    layout->addWidget(credits);
    layout->addStretch(1);

    // Copyright
    QLabel* copyright = new QLabel(tr("Copyright (c) 2025 Kalahari Project"), widget);
    copyright->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyright);

    // The texts scroll in a low window, e.g. on 1366x768 at 150%
    auto* area = new kalahari::gui::FittingScrollArea(widget);
    area->viewport()->setAutoFillBackground(false);
    return area;
}

QWidget* AboutDialog::createComponentsTab() {
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    // Read-only text edit with component list
    QTextEdit* textEdit = new QTextEdit(widget);
    textEdit->setReadOnly(true);
    textEdit->setWordWrapMode(QTextOption::WordWrap);

    // Build component list
    QString components;
    components += tr("Kalahari uses the following third-party components:\n\n");

    components += tr("Qt 6 (www.qt.io)\n");
    components += tr("  Cross-platform GUI framework\n");
    components += tr("  License: LGPL v3 / Commercial\n\n");

    components += tr("nlohmann_json (github.com/nlohmann/json)\n");
    components += tr("  JSON for Modern C++\n");
    components += tr("  License: MIT License\n\n");

    components += tr("spdlog (github.com/gabime/spdlog)\n");
    components += tr("  Fast C++ logging library\n");
    components += tr("  License: MIT License\n\n");

    components += tr("libzip (libzip.org)\n");
    components += tr("  C library for reading, creating, and modifying zip archives\n");
    components += tr("  License: BSD 3-Clause License\n\n");

    components += tr("Catch2 (github.com/catchorg/Catch2)\n");
    components += tr("  Modern C++ test framework\n");
    components += tr("  License: Boost Software License 1.0\n\n");

    components += tr("pybind11 (github.com/pybind/pybind11)\n");
    components += tr("  Seamless C++/Python interoperability\n");
    components += tr("  License: BSD 3-Clause License\n\n");

    components += tr("Python 3.11 (www.python.org)\n");
    components += tr("  Embedded Python interpreter for plugins\n");
    components += tr("  License: Python Software Foundation License\n\n");

    components += tr("Hunspell (hunspell.github.io)\n");
    components += tr("  Spell checking library\n");
    components += tr("  License: MPL 1.1 / GPL 2.0 / LGPL 2.1\n\n");

    components += tr("Polish spelling dictionary (sjp.pl, Marek Futrega)\n");
    components += tr("  From the LibreOffice dictionaries\n");
    components += tr("  License: Apache License 2.0\n\n");

    components += tr("English spelling dictionary (SCOWL, Kevin Atkinson)\n");
    components += tr("  Spell Checker Oriented Word Lists\n");
    components += tr("  License: SCOWL license (BSD-like)\n\n");

    components += tr("vcpkg (github.com/microsoft/vcpkg)\n");
    components += tr("  C++ package manager\n");
    components += tr("  License: MIT License\n");

    textEdit->setText(components);
    layout->addWidget(textEdit);

    widget->setLayout(layout);
    return widget;
}

QWidget* AboutDialog::createLicenseTab() {
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    // Read-only text edit with MIT license
    QTextEdit* textEdit = new QTextEdit(widget);
    textEdit->setReadOnly(true);
    textEdit->setWordWrapMode(QTextOption::WordWrap);

    QString license;
    license += tr("MIT License\n\n");
    license += tr("Copyright (c) 2025 Kalahari Project\n\n");
    license += tr("Permission is hereby granted, free of charge, to any person obtaining a copy\n");
    license += tr("of this software and associated documentation files (the \"Software\"), to deal\n");
    license += tr("in the Software without restriction, including without limitation the rights\n");
    license += tr("to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
    license += tr("copies of the Software, and to permit persons to whom the Software is\n");
    license += tr("furnished to do so, subject to the following conditions:\n\n");
    license += tr("The above copyright notice and this permission notice shall be included in all\n");
    license += tr("copies or substantial portions of the Software.\n\n");
    license += tr("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
    license += tr("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
    license += tr("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
    license += tr("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
    license += tr("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
    license += tr("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n");
    license += tr("SOFTWARE.\n\n");
    license += tr("Note: The \"Kalahari\" name and branding are trademarked.");

    textEdit->setText(license);
    layout->addWidget(textEdit);

    widget->setLayout(layout);
    return widget;
}
