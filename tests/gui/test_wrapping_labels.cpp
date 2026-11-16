// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_wrapping_labels.cpp
/// @brief Labels and buttons that wrap on a narrow panel instead of being cut off

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/widgets/path_label.h"
#include "kalahari/gui/widgets/wrapping_button.h"
#include "kalahari/gui/widgets/wrapping_label.h"

#include <QChar>
#include <QDir>
#include <QFontMetrics>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

using namespace kalahari::gui;

namespace {

const QString LONG_TEXT =
    QStringLiteral("Open the last project on startup, with the chapters that were open");

/// A button's text with its access key
const QString BUTTON_TEXT = QStringLiteral("&Replace All the Matches in All the Texts");

/// A folder named with a long number, which has no place to end a line in it
const QString LONG_NAME = QStringLiteral("6e1ac65f-8806-5aba-876d-5f142cdd2d6b");
const QString PATH_WITH_LONG_NAME =
    QStringLiteral("/home/writer/%1/Example Novel.klh").arg(LONG_NAME);

/// The lines of a label @p height high
int linesOf(const QLabel& label, int height) {
    return qRound(height / QFontMetricsF(label.font(), &label).height());
}

} // namespace

TEST_CASE("WrappingLabel: one line where it has room, more where it has less",
          "[gui][layout]") {
    WrappingLabel label(LONG_TEXT);
    QLabel guessing(LONG_TEXT);
    guessing.setWordWrap(true);

    // The whole text in its line, which a word-wrapped QLabel does not like
    const QSize hint = label.sizeHint();
    CHECK(hint.width() >= label.fontMetrics().horizontalAdvance(LONG_TEXT));
    CHECK(hint.height() == label.heightForWidth(hint.width()));
    CHECK(guessing.sizeHint().height() > hint.height());

    // Narrower, it wraps, down to its longest word
    CHECK(label.heightForWidth(hint.width() / 2) > hint.height());
    CHECK(label.minimumSizeHint().width() < hint.width() / 2);

    // A rich text keeps the hint of QLabel
    WrappingLabel rich(QStringLiteral("<b>%1</b>").arg(LONG_TEXT));
    QLabel richGuessing(QStringLiteral("<b>%1</b>").arg(LONG_TEXT));
    richGuessing.setWordWrap(true);
    CHECK(rich.sizeHint() == richGuessing.sizeHint());
}

TEST_CASE("PathLabel: a long name in the path wraps too", "[gui][layout]") {
    PathLabel label(PATH_WITH_LONG_NAME);
    const int nameWidth = label.fontMetrics().horizontalAdvance(LONG_NAME);

    // The system's separators, after each a place to end a line
    const QChar zeroWidthSpace(0x200B);
    QString shown = QDir::toNativeSeparators(PATH_WITH_LONG_NAME);
    shown.replace(QDir::separator(), QString(QDir::separator()) + zeroWidthSpace);
    CHECK(label.text() == shown);

    // In one line where it has room
    const QSize hint = label.sizeHint();
    CHECK(hint.width() >= label.fontMetrics().horizontalAdvance(PATH_WITH_LONG_NAME));
    CHECK(linesOf(label, hint.height()) == 1);
    CHECK(linesOf(label, label.heightForWidth(hint.width())) == 1);

    // A word-wrapped QLabel is not narrower than the long name; this one is
    QLabel wordWrapped(label.text());
    wordWrapped.setWordWrap(true);
    CHECK(wordWrapped.minimumSizeHint().width() >= nameWidth);
    CHECK(label.minimumSizeHint().width() < nameWidth / 4);

    // Narrower than the name, the name wraps as well: the folders in a line, the name in two
    // or three, the file in one or two
    const int lines = linesOf(label, label.heightForWidth(nameWidth / 2));
    CHECK(lines >= 4);
    CHECK(lines <= 6);

    // Short names end their lines after a separator, not inside a name
    PathLabel shortNames(QStringLiteral("/aaaa/bbbb/cccc"));
    const int narrow =
        shortNames.fontMetrics().horizontalAdvance(QStringLiteral("/aaaa/bb")) + 1;
    CHECK(linesOf(shortNames, shortNames.heightForWidth(narrow)) == 3);
}

TEST_CASE("WrappingButton: one line where it has room, more where it has less",
          "[gui][layout]") {
    QWidget panel;
    auto* layout = new QVBoxLayout(&panel);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* button = new WrappingButton(BUTTON_TEXT, &panel);
    layout->addWidget(button);
    layout->addStretch(1);
    const QString shownText = QString(BUTTON_TEXT).remove(QLatin1Char('&'));

    // In one line where it has room, as wide as its text
    const QSize hint = button->sizeHint();
    CHECK(hint.width() >= button->fontMetrics().horizontalAdvance(shownText));
    CHECK(button->heightForWidth(hint.width()) == hint.height());

    // A push button is not narrower than its text; this one is as narrow as its longest word,
    // in more lines
    const int narrow = button->minimumSizeHint().width();
    CHECK(narrow < hint.width() / 2);
    CHECK(panel.minimumSizeHint().width() == narrow);
    CHECK(button->heightForWidth(narrow) > hint.height());

    // Laid out narrow, it shows its text in lines, with its access key; wide, in one line
    panel.show();
    panel.resize(narrow, 2 * panel.heightForWidth(narrow));
    layout->activate();
    REQUIRE(button->width() == narrow);
    const QStringList lines = button->text().split(QLatin1Char('\n'));
    CHECK(lines.size() > 1);
    CHECK(lines.join(QLatin1Char(' ')) == BUTTON_TEXT);
    CHECK(button->text().startsWith(QLatin1Char('&')));
    CHECK(button->height() == button->heightForWidth(narrow));
    CHECK(button->height() > hint.height());

    panel.resize(hint.width() + 40, panel.height());
    layout->activate();
    CHECK(button->text() == BUTTON_TEXT);
    CHECK(button->height() == hint.height());

    // A new text shows at once in the lines of the button's width
    panel.resize(narrow, panel.height());
    layout->activate();
    button->setFullText(QStringLiteral("OK"));
    CHECK(button->text() == QStringLiteral("OK"));
    CHECK(button->fullText() == QStringLiteral("OK"));
    button->setFullText(BUTTON_TEXT);
    CHECK(button->text().contains(QLatin1Char('\n')));
    CHECK(button->fullText() == BUTTON_TEXT);
}
