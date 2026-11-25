// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file about_dialog.cpp
/// @brief Implementation of AboutDialog

#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/core/resource_paths.h"
#include "kalahari/core/theme.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/widgets/fitting_scroll_area.h"
#include "kalahari/build_info.h"
#include "kalahari/version.h"
#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QLocale>
#include <QPair>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QSysInfo>
#include <QTabWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

using namespace kalahari::gui::dialogs;

namespace {

/// @brief The address of the authors of the program (NOTICE.md, additional term 1)
constexpr const char* AUTHOR_ADDRESS = "https://bwdsp.com.pl/";

/// @brief The name of the authors of the program, beside their logo
constexpr const char* COMPANY_NAME = "BW Digital Systems & Publishing";

/// @brief The full logo of the authors, its black letters for a light background and its white
/// ones for a dark one (resources/images, in the program by src/CMakeLists.txt)
constexpr const char* COMPANY_LOGO_BLACK = ":/images/bwdsp_logo_black.png";
constexpr const char* COMPANY_LOGO_WHITE = ":/images/bwdsp_logo_white.png";

/// @brief The name of the person who had the idea of the program and made its architecture
constexpr const char* AUTHOR_NAME = "Bartosz W. Warzocha";

/// @brief How wide the logo of the authors is in the heading, in logical pixels; its height
/// keeps the proportions of the image
constexpr int COMPANY_LOGO_WIDTH = 150;

/// @brief The logo of Kalahari before the name in the heading, in logical pixels
constexpr int PROGRAM_LOGO_SIZE = 52;

/// @brief The line between the name of the program and the logo of the authors
constexpr int HEADING_LINE_HEIGHT = 44;

/// @brief The About tab: its margins, the room between its parts, within a part, and between
/// the columns of its rows
constexpr int TAB_MARGIN = 16;
constexpr int PART_SPACING = 18;
constexpr int PART_INNER_SPACING = 8;
constexpr int ROW_SPACING = 5;
constexpr int COLUMN_SPACING = 16;

/// @brief Sizes of the texts of the About tab against the dialog's font: what Kalahari is, the
/// headings of the parts and the legal notices
constexpr qreal TAGLINE_SCALE = 1.15;
constexpr qreal PART_HEADING_SCALE = 0.88;
constexpr qreal LEGAL_SCALE = 0.92;

/// @brief The headings of the parts, in capitals, a little spaced (percent of the normal)
constexpr qreal PART_HEADING_LETTER_SPACING = 106.0;

/// @brief The size About opens at where the screen has room for it
constexpr int DIALOG_WIDTH = 600;
constexpr int DIALOG_HEIGHT = 640;

/// @brief How much of the color of text the line in the heading has, on the background of
/// text fields: seen on a light and on a dark theme
constexpr double HEADING_LINE_SHARE = 0.2;

/// @brief @p font scaled by @p scale
QFont scaledFont(QFont font, qreal scale, QFont::Weight weight = QFont::Normal) {
    font.setPointSizeF(font.pointSizeF() * scale);
    font.setWeight(weight);
    return font;
}

/// @brief A horizontal line in the color of the dialog's lines
QFrame* horizontalLine(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFixedHeight(1);
    line->setAutoFillBackground(true);
    line->setBackgroundRole(QPalette::Mid);
    return line;
}

/// @brief A background darker than this lightness (0–255) is dark
constexpr int DARK_LIGHTNESS = 128;

/// @brief The text of the licence in the program (src/CMakeLists.txt, qt_add_resources)
constexpr const char* LICENSE_RESOURCE = ":/licenses/LICENSE";

/// @brief Where the text of the licence is when the program does not carry it
constexpr const char* LICENSE_ADDRESS = "https://www.gnu.org/licenses/gpl-3.0.html";

/// @brief A text of the build, or "–" where the build does not know it (no git, no branch)
QString orDash(const char* text) {
    const QString value = QString::fromUtf8(text);
    return value.isEmpty() ? QStringLiteral("–") : value;
}

/// @brief What the program was built from: a label and its value for each line
QList<QPair<QString, QString>> buildRows() {
    namespace build = kalahari::build;
    QString commit = orDash(build::commit());
    if (build::modified()) {
        //: After the hash of the commit in About: the program was built with changes of the code
        //: not yet committed. In Polish: %1 (z niezatwierdzonymi zmianami)
        commit = AboutDialog::tr("%1 (with changes not committed)").arg(commit);
    }
    const QDateTime built =
        QDateTime::fromString(QString::fromLatin1(build::builtAt()), Qt::ISODate);
    QString qt = QString::fromLatin1(qVersion());
    if (qt != QLatin1String(QT_VERSION_STR)) {
        //: The version of Qt in About: %1 the one the program runs with, %2 the one it was built
        //: with. In Polish: %1 (kompilacja z %2)
        qt = AboutDialog::tr("%1 (built with %2)").arg(qt, QStringLiteral(QT_VERSION_STR));
    }
    return {
        //: In Polish: Wersja:
        {AboutDialog::tr("Version:"), QString::fromLatin1(kalahari::VERSION_STRING)},
        //: The commit of git the program was built from. In Polish: Commit:
        {AboutDialog::tr("Commit:"), commit},
        //: The branch of git of that commit. In Polish: Gałąź:
        {AboutDialog::tr("Branch:"), orDash(build::branch())},
        //: The date and the time of the build. In Polish: Skompilowano:
        {AboutDialog::tr("Built:"),
         built.isValid() ? QLocale().toString(built.toLocalTime(), QLocale::ShortFormat)
                         : QStringLiteral("–")},
        //: Debug or Release. In Polish: Konfiguracja:
        {AboutDialog::tr("Configuration:"), orDash(build::configuration())},
        //: The version of Qt. In Polish: Qt:
        {AboutDialog::tr("Qt:"), qt},
        //: The operating system and its processor. In Polish: System:
        {AboutDialog::tr("System:"), QStringLiteral("%1 (%2)").arg(
                                         QSysInfo::prettyProductName(),
                                         QSysInfo::currentCpuArchitecture())},
        //: In Polish: Kompilator:
        {AboutDialog::tr("Compiler:"), QString::fromLatin1(kalahari::COMPILER)},
    };
}

/// @brief What the Build tab shows, short: the commit and its branch, when and how it was built,
/// the system and the compiler (the version and Qt are in the heading)
QList<QPair<QString, QString>> shortBuildRows() {
    namespace build = kalahari::build;
    QString commit = orDash(build::commit());
    if (build::modified()) {
        commit = AboutDialog::tr("%1 (with changes not committed)").arg(commit);
    }
    const QString branch = QString::fromUtf8(build::branch());
    if (!branch.isEmpty()) {
        //: The commit and its branch on the Build tab of About. In Polish: %1, gałąź %2
        commit = AboutDialog::tr("%1, branch %2").arg(commit, branch);
    }
    const QDateTime built =
        QDateTime::fromString(QString::fromLatin1(build::builtAt()), Qt::ISODate);
    const QString when = built.isValid()
                             ? QLocale().toString(built.toLocalTime(), QLocale::ShortFormat)
                             : QStringLiteral("–");
    return {
        {AboutDialog::tr("Commit:"), commit},
        //: When and how (Debug or Release) on the Build tab of About. In Polish: %1, %2
        {AboutDialog::tr("Built:"),
         AboutDialog::tr("%1, %2").arg(when, orDash(build::configuration()))},
        {AboutDialog::tr("System:"),
         //: The system, its processor and the compiler on the Build tab of About. In Polish:
         //: %1 (%2), %3
         AboutDialog::tr("%1 (%2), %3")
             .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(),
                  QString::fromLatin1(kalahari::COMPILER))},
    };
}

}  // namespace

// ============================================================================
// Constructor
// ============================================================================

AboutDialog::AboutDialog(QWidget* parent)
    : KalahariDialog(parent)
{
    //: The name of the program in the heading of About, beside its logo. In Polish: Kalahari
    //: Writer's IDE
    setHeading(tr("Kalahari Writer's IDE"),
               //: The heading of the About window. %1: the version of the program, %2: the
               //: version of Qt it runs with. In Polish: Wersja %1, Qt %2
               tr("Version %1, Qt %2")
                   .arg(QString::fromLatin1(kalahari::VERSION_STRING),
                        QString::fromLatin1(qVersion())));
    // The window keeps its title; the heading names the program beside its logo
    setWindowTitle(tr("About Kalahari Writer's IDE"));
    addHeadingLogos();

    // Tab widget
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createAboutTab(), tr("About"));
    //: The tab of About with what the program was built from. In Polish: Kompilacja
    m_tabs->addTab(createBuildTab(), tr("Build"));
    m_tabs->addTab(createComponentsTab(), tr("Third-Party Components"));
    m_tabs->addTab(createLicenseTab(), tr("License"));
    contentLayout()->addWidget(m_tabs, 1);

    // It only informs: Close, which Enter and Esc press
    setAcceptText(tr("Close"));
    cancelButton()->hide();

    updateMutedColors();

    // Lower where the screen is, e.g. 1366x768 at 150%: the tabs scroll
    kalahari::gui::utils::resizeWithinScreen(this, QSize(DIALOG_WIDTH, DIALOG_HEIGHT));
}

// ============================================================================
// Tab Creation Methods
// ============================================================================

QString AboutDialog::buildInformation() {
    QStringList lines{QStringLiteral("Kalahari Writer's IDE")};
    for (const auto& [label, value] : buildRows()) {
        lines << label + QLatin1Char(' ') + value;
    }
    return lines.join(QLatin1Char('\n'));
}

QString AboutDialog::copyrightYears(int year) {
    // The first year of the program; a clock set earlier changes nothing
    constexpr int FIRST_YEAR = 2025;
    return year > FIRST_YEAR ? QStringLiteral("%1–%2").arg(FIRST_YEAR).arg(year)
                             : QString::number(FIRST_YEAR);
}

QString AboutDialog::copyright() {
    //: The copyright of About and of its License tab. %1: the years, "2025–2026". In Polish:
    //: Copyright © %1 Bartosz W. Warzocha
    return tr("Copyright © %1 Bartosz W. Warzocha")
        .arg(copyrightYears(QDate::currentDate().year()));
}

QString AboutDialog::authorAddress() {
    return QString::fromLatin1(AUTHOR_ADDRESS);
}

QString AboutDialog::attribution() {
    //: The authors of the program on the About tab and the License tab. Every copy of Kalahari
    //: must keep it (NOTICE.md, additional term 1): keep the name of the company and the
    //: address. %1: https://bwdsp.com.pl/. In Polish: Realizacja programu Kalahari: BW Digital
    //: Systems & Publishing, %1
    return tr("Kalahari – created by BW Digital Systems & Publishing, %1").arg(authorAddress());
}

QString AboutDialog::companyLogo(bool darkBackground) {
    return QString::fromLatin1(darkBackground ? COMPANY_LOGO_WHITE : COMPANY_LOGO_BLACK);
}

void AboutDialog::addHeadingLogos() {
    // Kalahari: the lion with the book and the quill, drawn from the whole image so that it
    // is sharp at any scaling; without it (an incomplete installation) the icon of About
    m_programLogo = new QLabel(this);
    m_programLogo->setObjectName(QStringLiteral("aboutProgramLogo"));
    m_programLogo->setFixedSize(PROGRAM_LOGO_SIZE, PROGRAM_LOGO_SIZE);
    m_programLogo->setScaledContents(true);
    const QPixmap lion(kalahari::core::ResourcePaths::getInstance().getResourcesDir() +
                       QStringLiteral("/images/app_logo.png"));
    m_programLogo->setPixmap(lion);
    addHeadingWidget(m_programLogo, true);
    if (lion.isNull()) {
        m_programLogo->hide();
        setHeadingIcon(QStringLiteral("help.about"));
    }

    // The authors at the right end, apart from the name of the program
    m_headingLine = new QFrame(this);
    m_headingLine->setFixedSize(1, HEADING_LINE_HEIGHT);
    m_headingLine->setAutoFillBackground(true);
    addHeadingWidget(m_headingLine);

    m_companyLogo = new QLabel(this);
    m_companyLogo->setObjectName(QStringLiteral("aboutCompanyLogo"));
    m_companyLogo->setAccessibleName(QString::fromLatin1(COMPANY_NAME));
    m_companyLogo->setToolTip(QStringLiteral("%1 – %2").arg(QString::fromLatin1(COMPANY_NAME),
                                                           authorAddress()));
    m_companyLogo->setScaledContents(true);
    m_companyLogo->setCursor(Qt::PointingHandCursor);
    m_companyLogo->installEventFilter(this);
    addHeadingWidget(m_companyLogo);
    updateCompanyLogo();
}

bool AboutDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_companyLogo && event->type() == QEvent::MouseButtonRelease) {
        QDesktopServices::openUrl(QUrl(authorAddress()));
        return true;
    }
    return KalahariDialog::eventFilter(watched, event);
}

QLabel* AboutDialog::mutedLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    m_mutedLabels.append(label);
    return label;
}

void AboutDialog::updateMutedColors() {
    // The color of the notes of the program, readable on the window
    const QColor muted = SettingsPage::noteColorOn(palette().color(QPalette::Window));
    for (QLabel* label : m_mutedLabels) {
        QPalette colors = label->palette();
        colors.setColor(QPalette::WindowText, muted);
        label->setPalette(colors);
    }
}

void AboutDialog::updateCompanyLogo() {
    if (m_companyLogo == nullptr) {
        return;
    }
    // The background of the heading, the one of text fields: the window has the palette of a
    // new theme before its heading does, when it hears of the change
    const QColor background = palette().color(QPalette::Base);
    if (m_headingLine != nullptr) {
        QPalette colors = m_headingLine->palette();
        colors.setColor(m_headingLine->backgroundRole(),
                        kalahari::core::mixedColor(background, palette().color(QPalette::Text),
                                                   HEADING_LINE_SHARE));
        m_headingLine->setPalette(colors);
    }
    const QPixmap logo(companyLogo(background.lightness() < DARK_LIGHTNESS));
    if (logo.isNull()) {
        m_companyLogo->hide();
        return;
    }
    // Drawn from the whole image at the size of the label, sharp at any scaling of the screen
    m_companyLogo->setFixedSize(COMPANY_LOGO_WIDTH,
                                COMPANY_LOGO_WIDTH * logo.height() / logo.width());
    m_companyLogo->setPixmap(logo);
    m_companyLogo->show();
}

void AboutDialog::changeEvent(QEvent* event) {
    KalahariDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) {
        updateCompanyLogo();
        updateMutedColors();
    }
}

QString AboutDialog::licenseText() {
    QFile file(QString::fromLatin1(LICENSE_RESOURCE));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QWidget* AboutDialog::createBuildTab() {
    auto* widget = new QWidget();
    widget->setObjectName(QStringLiteral("aboutBuildTab"));
    auto* layout = new QVBoxLayout(widget);
    auto* form = new QFormLayout();
    for (const auto& [label, value] : shortBuildRows()) {
        auto* text = new QLabel(value, widget);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        text->setWordWrap(true);
        form->addRow(label, text);
    }
    layout->addLayout(form);

    auto* row = new QHBoxLayout();
    //: Puts what the program was built from on the clipboard, for a report of a fault. In
    //: Polish: &Kopiuj informacje o kompilacji
    m_copy = new QPushButton(tr("&Copy Build Information"), widget);
    m_copied = new QLabel(widget);
    row->addWidget(m_copy);
    row->addWidget(m_copied, 1);
    layout->addLayout(row);
    layout->addStretch(1);
    connect(m_copy, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(buildInformation());
        //: After Copy Build Information in About. In Polish: Skopiowano do schowka.
        m_copied->setText(tr("Copied to the clipboard."));
    });
    return widget;
}

QWidget* AboutDialog::createAboutTab() {
    // Parts apart, each with its heading over a line: what Kalahari is, Authors, Thanks and
    // Project; under a line at the bottom the copyright and the licence. The name, the version
    // and the logos are in the heading
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(TAB_MARGIN, TAB_MARGIN, TAB_MARGIN, TAB_MARGIN);
    layout->setSpacing(PART_SPACING);

    // What Kalahari is
    auto* intro = new QVBoxLayout();
    intro->setSpacing(PART_INNER_SPACING - 2);
    //: The first line of the About tab. In Polish: Środowisko pracy pisarza dla systemów
    //: Windows, macOS i Linux
    auto* tagline =
        new QLabel(tr("Cross-platform Writer's IDE for Windows, macOS, and Linux"), widget);
    tagline->setFont(scaledFont(font(), TAGLINE_SCALE, QFont::DemiBold));
    tagline->setWordWrap(true);
    intro->addWidget(tagline);
    //: Under the first line of the About tab. In Polish: Kalahari to nowoczesne środowisko
    //: pracy dla autorów książek: zarządzanie projektem, statystyki i rozbudowany eksport.
    auto* description = new QLabel(
        tr("Kalahari is a modern writing environment for book authors: project management, "
           "statistics and powerful export."),
        widget);
    description->setWordWrap(true);
    intro->addWidget(description);
    layout->addLayout(intro);

    // A part: its heading in capitals over a line, then its rows
    const auto addPart = [this, widget, layout](const QString& heading) {
        auto* part = new QVBoxLayout();
        part->setSpacing(PART_INNER_SPACING);
        auto* title = mutedLabel(heading, widget);
        QFont titleFont = scaledFont(font(), PART_HEADING_SCALE, QFont::DemiBold);
        titleFont.setCapitalization(QFont::AllUppercase);
        titleFont.setLetterSpacing(QFont::PercentageSpacing, PART_HEADING_LETTER_SPACING);
        title->setFont(titleFont);
        auto* heads = new QVBoxLayout();
        heads->setSpacing(PART_INNER_SPACING - 3);
        heads->addWidget(title);
        heads->addWidget(horizontalLine(widget));
        part->addLayout(heads);
        layout->addLayout(part);
        return part;
    };
    // Rows of a label in the muted color and its value
    const auto addRows = [](QVBoxLayout* part) {
        auto* rows = new QGridLayout();
        rows->setHorizontalSpacing(COLUMN_SPACING);
        rows->setVerticalSpacing(ROW_SPACING);
        rows->setColumnStretch(1, 1);
        part->addLayout(rows);
        return rows;
    };
    const auto addRow = [this, widget](QGridLayout* rows, const QString& label, QLabel* value) {
        const int row = rows->rowCount();
        rows->addWidget(mutedLabel(label, widget), row, 0, Qt::AlignTop);
        value->setWordWrap(true);
        rows->addWidget(value, row, 1, Qt::AlignTop);
    };

    // Authors: the idea and the overall architecture, and the company that created the
    // program, as the licence requires every copy to show (NOTICE.md, additional term 1); the
    // address is a link to its page
    //: The heading of a part of the About tab. In Polish: Twórcy
    QGridLayout* authors = addRows(addPart(tr("Authors")));
    auto* author = new QLabel(widget);
    author->setTextFormat(Qt::RichText);
    author->setText(QStringLiteral("<b>%1</b>").arg(QString::fromLatin1(AUTHOR_NAME)));
    //: In Authors on the About tab. In Polish: Pomysł i architektura całości
    addRow(authors, tr("Idea and overall architecture"), author);
    const QString address = authorAddress();
    auto* company = new QLabel(widget);
    company->setObjectName(QStringLiteral("aboutAttribution"));
    company->setTextFormat(Qt::RichText);
    company->setText(QStringLiteral("<b>%1</b>, <a href=\"%2\">%2</a>")
                         .arg(QString::fromLatin1(COMPANY_NAME).toHtmlEscaped(), address));
    company->setOpenExternalLinks(true);
    //: In Authors on the About tab, before the name of the company and its address: the row
    //: reads as the attribution every copy keeps (NOTICE.md, additional term 1), so it says
    //: what the beginning of "Kalahari – created by BW Digital Systems & Publishing, %1"
    //: says. In Polish: Realizacja programu Kalahari
    addRow(authors, tr("Kalahari – created by"), company);

    // Thanks, a line for each
    //: The heading of a part of the About tab. In Polish: Podziękowania
    QGridLayout* thanks = addRows(addPart(tr("Thanks")));
    thanks->setHorizontalSpacing(PART_INNER_SPACING);
    thanks->setVerticalSpacing(ROW_SPACING - 2);
    const QStringList thanked = {
        //: In Thanks on the About tab. In Polish: Anthropic – za pomoc Claude AI
        tr("Anthropic – for the help of Claude AI"),
        //: In Thanks on the About tab. In Polish: zespołowi Qt – za wieloplatformową
        //: bibliotekę interfejsu
        tr("the Qt team – for the cross-platform GUI framework"),
        //: In Thanks on the About tab. In Polish: społeczności open source – za znakomite
        //: biblioteki
        tr("the open source community – for exceptional libraries"),
    };
    for (const QString& line : thanked) {
        const int row = thanks->rowCount();
        thanks->addWidget(mutedLabel(QStringLiteral("–"), widget), row, 0, Qt::AlignTop);
        auto* text = new QLabel(line, widget);
        text->setWordWrap(true);
        thanks->addWidget(text, row, 1, Qt::AlignTop);
    }

    // Project
    //: The heading of a part of the About tab. In Polish: Projekt
    QGridLayout* project = addRows(addPart(tr("Project")));
    //: In Project on the About tab. In Polish: Technologie
    addRow(project, tr("Technologies"),
           new QLabel(QStringLiteral("C++20, Qt 6, CMake, vcpkg"), widget));

    // Under a line at the bottom: the copyright, the years up to this one, and that Kalahari is
    // free software without warranty, the legal notices the GNU GPL asks of a program with a
    // user interface (its sections 0 and 5 d)
    layout->addStretch(1);
    auto* legal = new QVBoxLayout();
    legal->setSpacing(3);
    legal->addWidget(horizontalLine(widget));
    legal->addSpacing(PART_INNER_SPACING + 2);
    const QFont legalFont = scaledFont(font(), LEGAL_SCALE);
    auto* copyrightLabel = mutedLabel(copyright(), widget);
    copyrightLabel->setObjectName(QStringLiteral("aboutCopyright"));
    copyrightLabel->setFont(legalFont);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    legal->addWidget(copyrightLabel);
    //: Under the copyright on the About tab. In Polish: Kalahari to wolne oprogramowanie na
    //: licencji GNU General Public License w wersji 3, bez żadnej gwarancji. Jej tekst i warunki
    //: dodatkowe są na karcie Licencja.
    auto* noticeLabel = mutedLabel(
        tr("Kalahari is free software under the GNU General Public License version 3, with no "
           "warranty. Its text and the additional terms are on the License tab."),
        widget);
    noticeLabel->setObjectName(QStringLiteral("aboutLicenseNotice"));
    noticeLabel->setFont(legalFont);
    noticeLabel->setAlignment(Qt::AlignCenter);
    noticeLabel->setWordWrap(true);
    legal->addWidget(noticeLabel);
    layout->addLayout(legal);

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
    //: The licence of Qt in Kalahari. In Polish:   Licencja: LGPL v3
    components += tr("  License: LGPL v3\n\n");

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
    //: Hunspell offers three licences; Kalahari uses it under the LGPL. In Polish:
    //:   Licencja: LGPL 2.1 lub nowsza (wybrana spośród MPL 1.1 / GPL 2.0+ / LGPL 2.1+)
    components += tr("  License: LGPL 2.1 or later (of MPL 1.1 / GPL 2.0+ / LGPL 2.1+)\n\n");

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

    // Read-only: the notice of the licence, the additional terms and the text of the licence
    QTextEdit* textEdit = new QTextEdit(widget);
    textEdit->setObjectName(QStringLiteral("aboutLicenseText"));
    textEdit->setReadOnly(true);
    textEdit->setWordWrapMode(QTextOption::WordWrap);

    const QString paragraph = QStringLiteral("\n\n");
    QString license = QStringLiteral("Kalahari Writer's IDE\n") + copyright() +
                      QLatin1Char('\n') + attribution() + paragraph;
    //: The notice of the GNU GPL on the License tab of About. In Polish: Kalahari jest wolnym
    //: oprogramowaniem: można je rozpowszechniać i zmieniać na warunkach Powszechnej Licencji
    //: Publicznej GNU (GNU General Public License) w wersji 3, opublikowanej przez Free Software
    //: Foundation, z poniższymi warunkami dodatkowymi, na które pozwala jej sekcja 7.
    license += tr("Kalahari is free software: you can redistribute it and/or modify it under the "
                  "terms of the GNU General Public License version 3 as published by the Free "
                  "Software Foundation, with the additional terms below, which its section 7 "
                  "permits.") +
               paragraph;
    //: In Polish: Program Kalahari jest rozpowszechniany w nadziei, że będzie użyteczny, ale BEZ
    //: JAKIEJKOLWIEK GWARANCJI, nawet domyślnej gwarancji PRZYDATNOŚCI HANDLOWEJ albo
    //: PRZYDATNOŚCI DO OKREŚLONYCH ZASTOSOWAŃ. Szczegóły podaje tekst licencji GNU General
    //: Public License poniżej.
    license += tr("Kalahari is distributed in the hope that it will be useful, but WITHOUT ANY "
                  "WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR "
                  "A PARTICULAR PURPOSE. See the GNU General Public License below for more "
                  "details.") +
               paragraph;
    //: A heading on the License tab of About. In Polish: Warunki dodatkowe (sekcja 7 licencji
    //: GNU GPL)
    license += tr("Additional terms (section 7 of the GNU GPL)") + paragraph;
    //: The first additional term of the licence (NOTICE.md). %1: the authors, as attribution()
    //: writes them. In Polish: 1. Autorstwo (sekcja 7 b): każda rozpowszechniana kopia programu
    //: Kalahari i każde dzieło na nim oparte zachowuje w swoim oknie „O programie” informację
    //: „%1”, bez zmian i wyraźnie widoczną; może ona być w języku interfejsu.
    license += tr("1. Author attribution (section 7 b): every copy of Kalahari, and every work "
                  "based on it, that is distributed keeps the attribution \"%1\" in its About "
                  "window, unchanged and clearly visible; it may be in the language of the user "
                  "interface.")
                   .arg(attribution()) +
               paragraph;
    //: The second additional term of the licence (NOTICE.md). In Polish: 2. Nazwy i logo
    //: (sekcja 7 e): licencja nie daje praw z zakresu znaków towarowych do nazwy „Kalahari” ani
    //: do jej logo, ani do nazwy i logo BW Digital Systems & Publishing. Zmienioną wersję
    //: rozpowszechnia się pod własną nazwą.
    license += tr("2. The names and the logos (section 7 e): the licence grants no rights under "
                  "trademark law to the name \"Kalahari\" or to its logo, nor to the name or the "
                  "logo of BW Digital Systems & Publishing. A modified version is distributed "
                  "under a name of its own.") +
               paragraph;
    //: In Polish: Pełna treść warunków jest w pliku NOTICE.md, dołączonym do programu.
    license += tr("The full text of the terms is in the file NOTICE.md, which comes with the "
                  "program.") +
               paragraph;

    const QString text = licenseText();
    if (text.isEmpty()) {
        //: When the program carries no text of the licence. %1: its address. In Polish: Tekst
        //: licencji GNU General Public License w wersji 3: %1
        license += tr("The text of the GNU General Public License version 3: %1")
                       .arg(QString::fromLatin1(LICENSE_ADDRESS));
    } else {
        //: Before the English text of the GNU GPL. In Polish: Licencja GNU General Public
        //: License w wersji 3 – oryginalny tekst angielski, jedyny wiążący prawnie:
        license += tr("The GNU General Public License version 3 – the original English text, the "
                      "only one that is legally binding:") +
                   paragraph + text;
    }

    textEdit->setPlainText(license);
    layout->addWidget(textEdit);

    widget->setLayout(layout);
    return widget;
}
