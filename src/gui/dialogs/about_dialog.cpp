/// @file about_dialog.cpp
/// @brief Implementation of AboutDialog

#include "kalahari/gui/dialogs/about_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QPainter>
#include <QFont>

using namespace kalahari::gui::dialogs;

// ============================================================================
// Constructor
// ============================================================================

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Kalahari Writer's IDE"));
    setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint |
                   Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);
    setFixedSize(600, 600);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Banner at top (above tabs)
    QPixmap bannerPixmap = createPlaceholderBanner(580, 100);
    QLabel* bannerLabel = new QLabel(this);
    bannerLabel->setPixmap(bannerPixmap);
    bannerLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(bannerLabel, 0, Qt::AlignCenter);
    mainLayout->addSpacing(10);

    // Tab widget
    QTabWidget* tabWidget = new QTabWidget(this);
    tabWidget->addTab(createAboutTab(), tr("About"));
    tabWidget->addTab(createComponentsTab(), tr("Third-Party Components"));
    tabWidget->addTab(createLicenseTab(), tr("License"));
    mainLayout->addWidget(tabWidget, 1);

    // Close button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

// ============================================================================
// Tab Creation Methods
// ============================================================================

QWidget* AboutDialog::createAboutTab() {
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    // Application name and version
    QLabel* appName = new QLabel(tr("Kalahari Writer's IDE 0.3.1-alpha (Qt6)"), widget);
    QFont nameFont = appName->font();
    nameFont.setPointSize(14);
    nameFont.setBold(true);
    appName->setFont(nameFont);
    appName->setAlignment(Qt::AlignCenter);
    layout->addWidget(appName);
    layout->addSpacing(10);

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
           "Built with C++20 and Qt6 6.5.0+.\n\n"
           "A comprehensive writing toolkit with project management,\n"
           "statistics tracking, and powerful export capabilities."), widget);
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(desc, 1);

    // Qt version
    QLabel* qtVersion = new QLabel(
        tr("Built with Qt %1").arg(qVersion()), widget);
    qtVersion->setAlignment(Qt::AlignCenter);
    layout->addWidget(qtVersion);
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
           "  Target Release: Q2-Q3 2026 (Kalahari 1.0)"), widget);
    QFont creditsFont = credits->font();
    creditsFont.setPointSize(8);
    credits->setFont(creditsFont);
    credits->setWordWrap(true);
    layout->addWidget(credits);

    // Copyright
    QLabel* copyright = new QLabel(tr("Copyright (c) 2025 Kalahari Project"), widget);
    QFont copyrightFont = copyright->font();
    copyrightFont.setPointSize(8);
    copyright->setFont(copyrightFont);
    copyright->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyright);

    widget->setLayout(layout);
    return widget;
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

    components += tr("Qt6 6.5.0+ (www.qt.io)\n");
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

// ============================================================================
// Helper Methods
// ============================================================================

QPixmap AboutDialog::createPlaceholderBanner(int width, int height) {
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::black);

    QPainter painter(&pixmap);
    painter.setPen(Qt::white);

    QFont font = painter.font();
    font.setPointSize(24);
    font.setBold(true);
    painter.setFont(font);

    // Calculate centered position
    QString text = "KALAHARI";
    QFontMetrics metrics(font);
    QRect textRect = metrics.boundingRect(text);

    int x = (width - textRect.width()) / 2;
    int y = (height + textRect.height()) / 2;

    painter.drawText(x, y, text);

    return pixmap;
}
