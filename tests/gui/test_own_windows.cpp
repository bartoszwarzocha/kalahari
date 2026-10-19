/// @file test_own_windows.cpp
/// @brief The program shows only its own windows: no window of the system but those that
/// choose a file or a folder, and every dialog on the common base (KalahariDialog), also
/// About and the Icon Downloader

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/gui/dialogs/icon_downloader_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/version.h"

#include <QApplication>
#include <QDirIterator>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>

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
    CHECK(dialog.tabs()->count() == 3);

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
