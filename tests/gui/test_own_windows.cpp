// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_own_windows.cpp
/// @brief The program shows only its own windows: no window of the system but those that
/// choose a file or a folder, and every dialog on the common base (KalahariDialog), also
/// About and the Icon Downloader

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/gui/dialogs/icon_downloader_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/new_file_dialog.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/gui/dialogs/toolbar_manager_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/build_info.h"
#include "kalahari/version.h"

#include "kalahari_tests/gui/document_window.h"

#include <QApplication>
#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QLayout>
#include <QMainWindow>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QString>
#include <QStringList>
#include <QSysInfo>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTranslator>

#include <algorithm>
#include "kalahari_tests/test_support/window_parts.h"

using kalahari::gui::IconDownloaderDialog;
using kalahari::gui::dialogs::AboutDialog;
using kalahari::gui::dialogs::KalahariDialog;
using kalahari::gui::dialogs::MessageDialog;

namespace {

/// @brief The windows of Qt that the program does not show: it has its own in their place
/// (MessageDialog, TextInputDialog, ColorDialog, ProgressDialog). The windows that choose a
/// file or a folder (QFileDialog) stay the system's.
const QStringList SYSTEM_WINDOWS = {
    QStringLiteral("QMessageBox"),       QStringLiteral("QInputDialog"),
    QStringLiteral("QColorDialog"),      QStringLiteral("QFontDialog"),
    QStringLiteral("QProgressDialog"),   QStringLiteral("QErrorMessage"),
    QStringLiteral("QWizard"),           QStringLiteral("QWizardPage"),
    QStringLiteral("QPrintDialog"),      QStringLiteral("QAbstractPrintDialog"),
    QStringLiteral("QPageSetupDialog"),  QStringLiteral("QPrintPreviewDialog"),
};

/// @brief The headers and sources of the program
QStringList programFiles() {
    QStringList files;
    for (const char* folder : {"/src", "/include"}) {
        QDirIterator it(QStringLiteral(KALAHARI_SOURCE_DIR) + QLatin1String(folder),
                        {QStringLiteral("*.h"), QStringLiteral("*.cpp")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            files << it.next();
        }
    }
    files.sort();
    return files;
}

/// @brief A character of a name in C++
bool isNameChar(QChar c) {
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

/// @brief The code of @p text without its comments and the contents of its strings and
/// characters, each of their characters a space (the lines stay where they are): a comment
/// that says "in place of QMessageBox" and the name of a translation context are no use of
/// the class
QString codeOnly(const QString& text) {
    QString code = text;
    const auto blank = [&code](qsizetype from, qsizetype to) {
        for (qsizetype i = from; i < to && i < code.size(); ++i) {
            if (code[i] != QLatin1Char('\n')) {
                code[i] = QLatin1Char(' ');
            }
        }
    };
    qsizetype i = 0;
    while (i < text.size()) {
        const QChar c = text[i];
        const QChar next = i + 1 < text.size() ? text[i + 1] : QChar();
        if (c == QLatin1Char('/') && next == QLatin1Char('/')) {
            qsizetype end = text.indexOf(QLatin1Char('\n'), i);
            end = end < 0 ? text.size() : end;
            blank(i, end);
            i = end;
        } else if (c == QLatin1Char('/') && next == QLatin1Char('*')) {
            qsizetype end = text.indexOf(QStringLiteral("*/"), i + 2);
            end = end < 0 ? text.size() : end + 2;
            blank(i, end);
            i = end;
        } else if (c == QLatin1Char('"')) {
            // A raw string, R"delimiter( ... )delimiter", ends only at its delimiter
            qsizetype name = i;
            while (name > 0 && isNameChar(text[name - 1])) {
                --name;
            }
            static const QStringList RAW = {QStringLiteral("R"), QStringLiteral("u8R"),
                                            QStringLiteral("uR"), QStringLiteral("UR"),
                                            QStringLiteral("LR")};
            const qsizetype open = text.indexOf(QLatin1Char('('), i);
            if (RAW.contains(text.mid(name, i - name)) && open > i) {
                const QString close =
                    QLatin1Char(')') + text.mid(i + 1, open - i - 1) + QLatin1Char('"');
                qsizetype end = text.indexOf(close, open);
                end = end < 0 ? text.size() : end + close.size();
                blank(i + 1, end - 1);
                i = end;
                continue;
            }
            qsizetype end = i + 1;
            while (end < text.size() && text[end] != QLatin1Char('"') &&
                   text[end] != QLatin1Char('\n')) {
                end += text[end] == QLatin1Char('\\') ? 2 : 1;
            }
            blank(i + 1, end);
            i = end + 1;
        } else if (c == QLatin1Char('\'') && (i == 0 || !isNameChar(text[i - 1]))) {
            // A character; after a digit the quote separates the digits of a number
            qsizetype end = i + 1;
            while (end < text.size() && text[end] != QLatin1Char('\'') &&
                   text[end] != QLatin1Char('\n')) {
                end += text[end] == QLatin1Char('\\') ? 2 : 1;
            }
            blank(i + 1, end);
            i = end + 1;
        } else {
            ++i;
        }
    }
    return code;
}

/// @brief The places in the program where @p pattern matches its code, as
/// "src/file.cpp:12: text of the line"
QStringList placesOf(const QRegularExpression& pattern) {
    QStringList places;
    const QString root = QStringLiteral(KALAHARI_SOURCE_DIR "/");
    for (const QString& path : programFiles()) {
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(file.readAll());
        const QString code = codeOnly(text);
        QRegularExpressionMatchIterator it = pattern.globalMatch(code);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const qsizetype start = match.capturedStart();
            const qsizetype lineStart = text.lastIndexOf(QLatin1Char('\n'), start) + 1;
            qsizetype lineEnd = text.indexOf(QLatin1Char('\n'), start);
            lineEnd = lineEnd < 0 ? text.size() : lineEnd;
            const qsizetype line = text.left(start).count(QLatin1Char('\n')) + 1;
            places << QStringLiteral("%1:%2: %3")
                          .arg(QString(path).remove(0, root.size()))
                          .arg(line)
                          .arg(text.mid(lineStart, lineEnd - lineStart).trimmed());
        }
    }
    return places;
}

/// @brief Whether a label of @p dialog shows @p text
bool showsText(const QDialog& dialog, const QString& text) {
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->isVisibleTo(&dialog) && label->text().contains(text)) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

TEST_CASE("Own windows: comments and strings are no use of a window", "[gui][dialogs]") {
    const auto check = [](const QString& text, bool used) {
        INFO(text.toStdString());
        const QString code = codeOnly(text);
        // The lines stay where they are, for the places the tests name
        CHECK(code.size() == text.size());
        CHECK(code.count(QLatin1Char('\n')) == text.count(QLatin1Char('\n')));
        CHECK(code.contains(QStringLiteral("QMessageBox")) == used);
        CHECK(code.contains(QStringLiteral("end")));
    };
    check(QStringLiteral("#include <QMessageBox>\nend"), true);
    check(QStringLiteral("// in place of QMessageBox\nend"), false);
    check(QStringLiteral("/* in place of\n   QMessageBox */ end"), false);
    check(QStringLiteral("QT_TRANSLATE_NOOP(\"QMessageBox\", \"OK\"); end"), false);
    // A quote in a string does not end it, and a raw string ends only at its delimiter
    check(QStringLiteral("tr(\"a \\\" QMessageBox\"); end"), false);
    check(QStringLiteral("R\"x(\" QMessageBox)\" QMessageBox)x\"; end"), false);
    // A character with a quote in it is no string; a quote between digits is no character
    check(QStringLiteral("'\"'; QMessageBox::warning(); end"), true);
    check(QStringLiteral("1'000; QMessageBox::warning(); end"), true);
}

TEST_CASE("Own windows: no window of the system but those that choose a file or a folder",
          "[gui][dialogs]") {
    REQUIRE_FALSE(programFiles().isEmpty());
    const QRegularExpression systemWindow(
        QStringLiteral("\\b(%1)\\b").arg(SYSTEM_WINDOWS.join(QLatin1Char('|'))));
    const QStringList places = placesOf(systemWindow);
    INFO("Use the program's own windows (MessageDialog, TextInputDialog, ColorDialog, "
         "ProgressDialog) in place of:\n"
         << places.join(QLatin1Char('\n')).toStdString());
    CHECK(places.isEmpty());
}

TEST_CASE("Own windows: every dialog of the program is built on its common base",
          "[gui][dialogs]") {
    REQUIRE_FALSE(programFiles().isEmpty());

    // A class whose bases name QDialog: only the base itself
    const QRegularExpression dialogClass(
        QStringLiteral("\\b(?:class|struct)\\s+(\\w+)\\s*(?:final\\s*)?:[^{;]*\\bQDialog\\b"));
    QStringList classes = placesOf(dialogClass);
    classes.removeIf([](const QString& place) {
        return place.startsWith(QStringLiteral("include/kalahari/gui/dialogs/kalahari_dialog.h:"));
    });
    INFO("Derive the dialog from dialogs::KalahariDialog:\n"
         << classes.join(QLatin1Char('\n')).toStdString());
    CHECK(classes.isEmpty());

    // A window made of a plain QDialog
    const QStringList plain =
        placesOf(QRegularExpression(QStringLiteral("\\bnew\\s+QDialog\\b|\\bQDialog\\s+\\w+\\s*[({;]")));
    INFO("Make the window a dialogs::KalahariDialog:\n"
         << plain.join(QLatin1Char('\n')).toStdString());
    CHECK(plain.isEmpty());
}

TEST_CASE("Own windows: About has the heading of the program's dialogs and only Close",
          "[gui][dialogs]") {
    AboutDialog dialog;
    CHECK(qobject_cast<KalahariDialog*>(&dialog) != nullptr);
    CHECK(dialog.windowTitle() == QStringLiteral("About Kalahari Writer's IDE"));
    CHECK(showsText(dialog, QString::fromLatin1(kalahari::VERSION_STRING)));
    CHECK(showsText(dialog, QStringLiteral("Qt %1").arg(QString::fromLatin1(qVersion()))));
    REQUIRE(dialog.tabs() != nullptr);
    CHECK(dialog.tabs()->count() == 4);

    // Close is the only button: Enter presses it, and Esc closes the dialog too
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Close"));
    CHECK(dialog.acceptButton()->isVisibleTo(&dialog));
    CHECK(dialog.acceptButton()->isDefault());
    CHECK_FALSE(dialog.cancelButton()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.applyButton()->isVisibleTo(&dialog));
    dialog.show();
    QApplication::processEvents();
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&dialog, &escape);
    CHECK_FALSE(dialog.isVisible());
}

TEST_CASE("Own windows: About shows what the program was built from, and copies it",
          "[gui][dialogs]") {
    namespace build = kalahari::build;

    // The build knows when and how it was built; the commit and its branch where there is git
    const QDateTime built = QDateTime::fromString(QString::fromLatin1(build::builtAt()),
                                                  Qt::ISODate);
    REQUIRE(built.isValid());
    CHECK(built.timeSpec() == Qt::UTC);
    CHECK(built <= QDateTime::currentDateTimeUtc());
    const QString configuration = QString::fromLatin1(build::configuration());
    CHECK_FALSE(configuration.isEmpty());

    AboutDialog dialog;
    REQUIRE(dialog.tabs() != nullptr);
    // The build on a tab of its own after About, not on the About tab (test of 24.11)
    CHECK(dialog.tabs()->currentIndex() == 0);
    CHECK(dialog.tabs()->tabText(0) == QStringLiteral("About"));
    CHECK(dialog.tabs()->tabText(1) == QStringLiteral("Build"));
    // When and how it was built, as the Build tab writes it ("Release" alone is also in
    // "Target Release" on the About tab)
    const QString builtAs = QStringLiteral("%1, %2").arg(
        QLocale().toString(built.toLocalTime(), QLocale::ShortFormat), configuration);
    CHECK_FALSE(showsText(dialog, builtAs));
    dialog.tabs()->setCurrentIndex(1);
    CHECK(showsText(dialog, builtAs));
    const QString commit = QString::fromLatin1(build::commit());
    if (!commit.isEmpty()) {
        CHECK(showsText(dialog, commit));
        CHECK(showsText(dialog, build::modified()
                                    ? QStringLiteral("%1 (with changes not committed)").arg(commit)
                                    : commit));
    }
    const QString branch = QString::fromUtf8(build::branch());
    if (!branch.isEmpty()) {
        CHECK(showsText(dialog, branch));
    }

    // Short: three lines, the version and Qt are in the heading (test of 24.11)
    const auto* tab = dialog.findChild<QWidget*>(QStringLiteral("aboutBuildTab"));
    REQUIRE(tab != nullptr);
    const auto* form = tab->findChild<QFormLayout*>();
    REQUIRE(form != nullptr);
    CHECK(form->rowCount() == 3);
    CHECK(showsText(dialog, QSysInfo::prettyProductName()));
    CHECK(showsText(dialog, QString::fromLatin1(kalahari::COMPILER)));

    // The text to copy: a line for each, the same as the dialog shows
    const QStringList lines = AboutDialog::buildInformation().split(QLatin1Char('\n'));
    REQUIRE(lines.size() == 9);
    CHECK(lines.at(0) == QStringLiteral("Kalahari Writer's IDE"));
    CHECK(lines.at(1) == QStringLiteral("Version: %1").arg(QString::fromLatin1(
                                                                kalahari::VERSION_STRING)));
    CHECK(lines.at(2).startsWith(QStringLiteral("Commit: %1").arg(commit.isEmpty()
                                                                      ? QStringLiteral("–")
                                                                      : commit)));
    CHECK(lines.at(5) == QStringLiteral("Configuration: %1").arg(configuration));

    // Copy Build Information: on the clipboard, and the dialog says so
    REQUIRE(dialog.copyButton() != nullptr);
    CHECK(dialog.copyButton()->text() == QStringLiteral("&Copy Build Information"));
    QGuiApplication::clipboard()->clear();
    dialog.copyButton()->click();
    CHECK(QGuiApplication::clipboard()->text() == AboutDialog::buildInformation());
    CHECK(showsText(dialog, QStringLiteral("Copied to the clipboard.")));

    // Opened, Enter still presses Close. The field of the dialog that has the keys, also
    // where the system does not make the dialog the active window (macOS on CI)
    dialog.show();
    QApplication::processEvents();
    QWidget* focused = dialog.focusWidget();
    REQUIRE(focused != nullptr);
    CHECK(focused != dialog.copyButton());
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(focused, &enter);
    CHECK_FALSE(dialog.isVisible());
}

TEST_CASE("Own windows: About has the copyright sign and the years up to this one",
          "[gui][dialogs]") {
    // 2025 and the year of the clock; a clock set before 2025 changes nothing (test of 24.11)
    CHECK(AboutDialog::copyrightYears(2026) == QStringLiteral("2025–2026"));
    CHECK(AboutDialog::copyrightYears(2031) == QStringLiteral("2025–2031"));
    CHECK(AboutDialog::copyrightYears(2025) == QStringLiteral("2025"));
    CHECK(AboutDialog::copyrightYears(2019) == QStringLiteral("2025"));
    // The holder of the copyright (NOTICE.md), not a team or a project
    const QString copyright =
        QStringLiteral("Copyright © %1 Bartosz W. Warzocha")
            .arg(AboutDialog::copyrightYears(QDate::currentDate().year()));
    CHECK(AboutDialog::copyright() == copyright);

    AboutDialog dialog;
    const auto* label = dialog.findChild<QLabel*>(QStringLiteral("aboutCopyright"));
    REQUIRE(label != nullptr);
    CHECK(label->text() == copyright);
    CHECK_FALSE(label->text().contains(QStringLiteral("(c)")));
    // The License tab says the same
    bool inLicense = false;
    for (const QTextEdit* text : dialog.findChildren<QTextEdit*>()) {
        inLicense = inLicense || text->toPlainText().contains(copyright);
        CHECK_FALSE(text->toPlainText().contains(QStringLiteral("Copyright (c)")));
    }
    CHECK(inLicense);
}

TEST_CASE("Own windows: About keeps the authors and the notices of the GNU GPL",
          "[gui][dialogs][license]") {
    // The attribution every copy must keep (NOTICE.md, additional term 1)
    const QString attribution = QStringLiteral(
        "Kalahari – created by BW Digital Systems & Publishing, https://bwdsp.com.pl/");
    CHECK(AboutDialog::authorAddress() == QStringLiteral("https://bwdsp.com.pl/"));
    CHECK(AboutDialog::attribution() == attribution);

    AboutDialog dialog;
    REQUIRE(dialog.tabs() != nullptr);
    REQUIRE(dialog.tabs()->count() == 4);
    CHECK(dialog.tabs()->tabText(3) == QStringLiteral("License"));

    // The About tab: the authors with their page as a link, above the copyright, and that
    // Kalahari is free software without warranty
    const auto* authors = dialog.findChild<QLabel*>(QStringLiteral("aboutAttribution"));
    REQUIRE(authors != nullptr);
    CHECK(authors->isVisibleTo(&dialog));
    CHECK(authors->openExternalLinks());
    CHECK(authors->text().contains(
        QStringLiteral("<a href=\"https://bwdsp.com.pl/\">https://bwdsp.com.pl/</a>")));
    CHECK(authors->text().contains(QStringLiteral("BW Digital Systems &amp; Publishing")));
    const auto* notice = dialog.findChild<QLabel*>(QStringLiteral("aboutLicenseNotice"));
    REQUIRE(notice != nullptr);
    CHECK(notice->isVisibleTo(&dialog));
    CHECK(notice->text().contains(QStringLiteral("GNU General Public License version 3")));
    CHECK(notice->text().contains(QStringLiteral("no warranty")));
    CHECK_FALSE(showsText(dialog, QStringLiteral("MIT")));

    // The License tab: the notice, no warranty, the additional terms and the whole licence
    const QString text = AboutDialog::licenseText();
    REQUIRE_FALSE(text.isEmpty());
    CHECK(text.contains(QStringLiteral("GNU GENERAL PUBLIC LICENSE")));
    CHECK(text.contains(QStringLiteral("Version 3, 29 June 2007")));
    CHECK(text.contains(QStringLiteral("END OF TERMS AND CONDITIONS")));
    // The text the program carries is the file of the repository, unchanged
    QFile file(QStringLiteral(KALAHARI_SOURCE_DIR "/LICENSE"));
    REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    CHECK(text == QString::fromUtf8(file.readAll()));

    const auto* license = dialog.findChild<QTextEdit*>(QStringLiteral("aboutLicenseText"));
    REQUIRE(license != nullptr);
    CHECK(license->isReadOnly());
    const QString shown = license->toPlainText();
    CHECK(shown.contains(AboutDialog::copyright()));
    CHECK(shown.contains(attribution));
    CHECK(shown.contains(QStringLiteral("under the terms of the GNU General Public License "
                                        "version 3")));
    CHECK(shown.contains(QStringLiteral("WITHOUT ANY WARRANTY")));
    CHECK(shown.contains(QStringLiteral("Additional terms (section 7 of the GNU GPL)")));
    CHECK(shown.contains(QStringLiteral("keeps the attribution \"%1\"").arg(attribution)));
    CHECK(shown.contains(QStringLiteral("the name \"Kalahari\"")));
    CHECK(shown.contains(QStringLiteral("NOTICE.md")));
    CHECK(shown.endsWith(text));
    CHECK_FALSE(shown.contains(QStringLiteral("MIT License")));
    CHECK_FALSE(shown.contains(QStringLiteral("Permission is hereby granted")));

    // The Third-Party Components tab: Hunspell under the LGPL, the licence compatible with
    // the GPL, not under the MPL 1.1
    bool hunspell = false;
    for (const QTextEdit* edit : dialog.findChildren<QTextEdit*>()) {
        hunspell = hunspell || edit->toPlainText().contains(QStringLiteral(
                                   "License: LGPL 2.1 or later (of MPL 1.1 / GPL 2.0+ / "
                                   "LGPL 2.1+)"));
    }
    CHECK(hunspell);
}

TEST_CASE("Own windows: About shows the full logo of the authors for a light and a dark theme",
          "[gui][dialogs][license]") {
    // Both logos are in the program, the full ones ("BW", "Digital Systems & Publishing",
    // "Est 2013"): of one size, much wider than high
    const QPixmap black(AboutDialog::companyLogo(false));
    const QPixmap white(AboutDialog::companyLogo(true));
    REQUIRE_FALSE(black.isNull());
    REQUIRE_FALSE(white.isNull());
    CHECK(black.size() == QSize(2358, 631));
    CHECK(white.size() == black.size());
    CHECK(black.toImage() != white.toImage());

    AboutDialog dialog;
    QLabel* logo = dialog.companyLogoLabel();
    REQUIRE(logo != nullptr);
    CHECK(logo->objectName() == QStringLiteral("aboutCompanyLogo"));
    CHECK(logo->isVisibleTo(&dialog));
    CHECK(logo->accessibleName() == QStringLiteral("BW Digital Systems & Publishing"));
    // The whole logo, in its proportions, above the authors
    CHECK(logo->width() == 240);
    CHECK(logo->height() == 240 * 631 / 2358);
    CHECK(logo->hasScaledContents());
    const auto* authors = dialog.findChild<QLabel*>(QStringLiteral("aboutAttribution"));
    REQUIRE(authors != nullptr);
    REQUIRE(logo->parentWidget() == authors->parentWidget());
    const QLayout* layout = logo->parentWidget()->layout();
    REQUIRE(layout != nullptr);
    CHECK(layout->indexOf(logo) < layout->indexOf(authors));

    // A light theme: the black letters; a dark one: the white ones; and back, as the theme
    // changes while the window is open
    const auto shownOn = [&dialog, logo](const char* window) {
        QPalette palette = dialog.palette();
        palette.setColor(QPalette::Window, QColor(QString::fromLatin1(window)));
        dialog.setPalette(palette);
        return logo->pixmap().toImage();
    };
    CHECK(shownOn("#f0f0f0") == black.toImage());
    CHECK(shownOn("#2b2b2b") == white.toImage());
    CHECK(shownOn("#ffffff") == black.toImage());
}

TEST_CASE("Own windows: in Polish About keeps the name and the address of the authors",
          "[gui][dialogs][license]") {
    QTranslator polish;
    REQUIRE(polish.load(QStringLiteral(":/i18n/kalahari_pl.qm")));
    QCoreApplication::installTranslator(&polish);
    // The attribution may be in the language of the user interface, with the name of the
    // company and its address unchanged (NOTICE.md, additional term 1)
    const QString attribution = AboutDialog::attribution();
    CHECK(attribution == QStringLiteral(
                             "Autor programu Kalahari: BW Digital Systems & Publishing, "
                             "https://bwdsp.com.pl/"));
    CHECK(AboutDialog::copyright().endsWith(QStringLiteral("Bartosz W. Warzocha")));
    {
        AboutDialog dialog;
        CHECK(dialog.tabs()->tabText(3) == QStringLiteral("Licencja"));
        const auto* license = dialog.findChild<QTextEdit*>(QStringLiteral("aboutLicenseText"));
        REQUIRE(license != nullptr);
        const QString shown = license->toPlainText();
        CHECK(shown.contains(attribution));
        CHECK(shown.contains(QStringLiteral("Warunki dodatkowe (sekcja 7 licencji GNU GPL)")));
        CHECK(shown.contains(QStringLiteral("BEZ JAKIEJKOLWIEK GWARANCJI")));
        // The licence itself only in English, the text that is legally binding
        CHECK(shown.endsWith(AboutDialog::licenseText()));
    }
    QCoreApplication::removeTranslator(&polish);
}

TEST_CASE("Own windows: the Icon Downloader stays open after Download", "[gui][dialogs]") {
    IconDownloaderDialog dialog;
    CHECK(qobject_cast<KalahariDialog*>(&dialog) != nullptr);
    CHECK(dialog.windowTitle() == QStringLiteral("Icon Downloader"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Download"));
    CHECK(dialog.cancelButton()->text() == QStringLiteral("Close"));
    dialog.show();
    QApplication::processEvents();

    // Download without a URL says what is missing, in the program's own message
    QStringList said;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&said]() {
        auto* message = qobject_cast<MessageDialog*>(QApplication::activeModalWidget());
        if (message && !message->property("handled").toBool()) {
            message->setProperty("handled", true);
            said << message->windowTitle();
            QMetaObject::invokeMethod(message->acceptButton(), "click", Qt::QueuedConnection);
        }
    });
    timer.start();
    dialog.acceptButton()->click();
    timer.stop();
    CHECK(said == QStringList{QStringLiteral("Invalid Input")});
    CHECK(dialog.isVisible());
    CHECK(dialog.result() != QDialog::Accepted);

    // Close closes it
    dialog.cancelButton()->click();
    CHECK_FALSE(dialog.isVisible());
}

TEST_CASE("Own windows: a window that got the heading is higher by it", "[gui][dialogs]") {
    using namespace kalahari;
    gui::registerAllCommands(kalahari::test::commands(), gui::CommandCallbacks{});
    gui::registerAllIcons();
    test::OpenNovel book(QStringLiteral("Rain"));
    QMainWindow window;
    gui::ToolbarManager toolbars(kalahari::test::commands(), &window);
    toolbars.createToolbars(test::commands());

    // The default size the window had before its heading, higher by the heading and the line
    // under it at that width; a smaller screen gets a lower window.
    // The room is the one the window was made with, read before the event loop runs: Qt learns
    // of a change of the screen only there, and on the macOS runner the screen known at the
    // start is a few pixels lower (681 or 683 of 684) until the system tells of the change, so
    // the first window got less room than the screen has once it is shown
    const auto check = [](KalahariDialog& dialog, const QSize& before) {
        const QSize room = dialog.screen()->availableGeometry().size() * 0.9;
        dialog.show();
        QApplication::processEvents();
        const auto* heading = dialog.findChild<QFrame*>(QStringLiteral("kalahariDialogHeading"));
        REQUIRE(heading != nullptr);
        const int higher = before.height() + heading->heightForWidth(before.width()) + 1;
        INFO(dialog.windowTitle().toStdString() << ": the screen had room for "
             << room.height() << " pixels");
        CHECK(dialog.height() == std::min(higher, room.height()));
    };

    gui::dialogs::NewItemDialog newBook(gui::dialogs::NewItemMode::Project);
    check(newBook, QSize(850, 620));
    gui::dialogs::NewFileDialog newFile(
        gui::dialogs::NewFileDialog::openBook(kalahari::test::book()), book.pm.bookTypes());
    check(newFile, QSize(880, 740));
    gui::dialogs::ToolbarManagerDialog toolbarManager(&toolbars, &window);
    check(toolbarManager, QSize(1000, 650));
    IconDownloaderDialog iconDownloader;
    check(iconDownloader, QSize(600, 650));
}
