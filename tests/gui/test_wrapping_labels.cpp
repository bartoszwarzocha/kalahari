/// @file test_wrapping_labels.cpp
/// @brief Labels that wrap on a narrow panel instead of being cut off

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/widgets/path_label.h"
#include "kalahari/gui/widgets/wrapping_label.h"

#include <QChar>
#include <QDir>
#include <QFontMetrics>
#include <QLabel>
#include <QString>

using namespace kalahari::gui;

namespace {

const QString LONG_TEXT =
    QStringLiteral("Open the last project on startup, with the chapters that were open");

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
