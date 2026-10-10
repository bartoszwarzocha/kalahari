/// @file test_fitting_scroll_area.cpp
/// @brief A scroll area as large as its content, which scrolls only on a small screen

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/widgets/fitting_scroll_area.h"

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWidget>

using namespace kalahari::gui;
using kalahari::gui::dialogs::KalahariDialog;

namespace {

/// A sentence that takes more lines in a narrow area
const QString LONG_MESSAGE = QStringLiteral(
    "The screen is small, so the panels on the right are hidden. View > Panels shows them "
    "again.");

}  // anonymous namespace

TEST_CASE("Fitting scroll area: as large as its content, it scrolls only when lower",
          "[gui][widgets]") {
    auto* content = new QWidget();
    auto* lines = new QVBoxLayout(content);
    for (int i = 0; i < 30; ++i) {
        lines->addWidget(new QLabel(QStringLiteral("Line %1").arg(i)));
    }
    // A child widget: the system keeps a window within the screen (macOS), not a child
    QWidget host;
    auto& area = *new FittingScrollArea(content, &host);

    CHECK(area.sizeHint() == content->sizeHint());
    CHECK(area.minimumSizeHint().height() < content->sizeHint().height());  // a few lines
    CHECK(area.minimumSizeHint().width() > content->minimumSizeHint().width());  // the bar

    area.resize(area.sizeHint());
    host.show();
    QApplication::processEvents();
    CHECK(area.verticalScrollBar()->maximum() == 0);

    area.resize(area.width(), area.sizeHint().height() / 2);
    QApplication::processEvents();
    CHECK(area.verticalScrollBar()->maximum() > 0);
}

TEST_CASE("Fitting scroll area: wrapped text takes more lines in a narrower area",
          "[gui][widgets]") {
    auto* label = new QLabel(LONG_MESSAGE);
    label->setWordWrap(true);
    FittingScrollArea area(label);

    CHECK(area.contentHeightForWidth(150) > area.contentHeightForWidth(800));
}

TEST_CASE("Fitting scroll area: a list gets lower before the content scrolls",
          "[gui][widgets]") {
    // A panel with a wrapped text over a list: in a lower area the list gives up its room,
    // and the text keeps all its lines
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    auto* label = new QLabel(LONG_MESSAGE);
    label->setWordWrap(true);
    layout->addWidget(label);
    auto* list = new QListWidget();
    for (int i = 0; i < 50; ++i) {
        list->addItem(QStringLiteral("Item %1").arg(i));
    }
    layout->addWidget(list);
    FittingScrollArea area(content);

    const int listMinimum = list->minimumSizeHint().height();
    const int listHint = list->sizeHint().height();
    REQUIRE(listMinimum < listHint);
    area.resize(250, label->heightForWidth(220) + layout->spacing() + listMinimum +
                         (listHint - listMinimum) / 2 + layout->contentsMargins().top() +
                         layout->contentsMargins().bottom());
    area.show();
    QApplication::processEvents();

    CHECK(list->height() < listHint);
    CHECK(list->height() >= listMinimum);
    CHECK(area.verticalScrollBar()->maximum() == 0);

    SECTION("lower than the content can be: it scrolls") {
        area.resize(250, listMinimum);
        QApplication::processEvents();
        CHECK(list->height() == listMinimum);
        CHECK(area.verticalScrollBar()->maximum() > 0);
    }
}

TEST_CASE("Fitting scroll area: a panel's area can be lower than a dialog's", "[gui][widgets]") {
    auto* content = new QWidget();
    auto* lines = new QVBoxLayout(content);
    for (int i = 0; i < 30; ++i) {
        lines->addWidget(new QLabel(QStringLiteral("Line %1").arg(i)));
    }
    FittingScrollArea area(content);
    const int lineSpacing = area.fontMetrics().lineSpacing();

    CHECK(area.minimumSizeHint().height() == 6 * lineSpacing);
    area.setMinimumLines(3);
    CHECK(area.minimumSizeHint().height() == 3 * lineSpacing);
    area.setMinimumLines(0);  // at least a line
    CHECK(area.minimumSizeHint().height() == lineSpacing);
}

TEST_CASE("Own dialogs: a wrapped text of the content leaves no gap above the buttons",
          "[gui][dialogs]") {
    // Regression: the content's height was measured at its own, narrower width, where the
    // sentence takes more lines than in the dialog
    KalahariDialog dialog;
    dialog.setHeading(QStringLiteral("Export"));
    auto* label = new QLabel(QStringLiteral(
        "The book is exported chapter by chapter, with its notes at the end of each chapter "
        "and its pictures in a folder next to the file."));
    label->setWordWrap(true);
    dialog.contentLayout()->addWidget(label);
    dialog.show();
    QApplication::processEvents();

    CHECK(label->height() == label->heightForWidth(label->width()));
    CHECK_FALSE(dialog.findChild<QScrollArea*>()->verticalScrollBar()->isVisible());
}

TEST_CASE("Own dialogs: in a lower dialog a list gets lower before the content scrolls",
          "[gui][dialogs]") {
    KalahariDialog dialog;
    dialog.setHeading(QStringLiteral("Types"));
    auto* label = new QLabel(QStringLiteral("Choose the type of the new element."));
    label->setWordWrap(true);
    dialog.contentLayout()->addWidget(label);
    auto* list = new QListWidget();
    for (int i = 0; i < 40; ++i) {
        list->addItem(QStringLiteral("Type %1").arg(i + 1));
    }
    dialog.contentLayout()->addWidget(list, 1);
    dialog.show();
    QApplication::processEvents();
    const int listHeight = list->height();
    REQUIRE(listHeight > list->minimumSizeHint().height());

    dialog.resize(dialog.width(), dialog.height() - (listHeight - list->minimumSizeHint().height()) / 2);
    QApplication::processEvents();
    CHECK(list->height() < listHeight);
    CHECK_FALSE(dialog.findChild<QScrollArea*>()->verticalScrollBar()->isVisible());
}

TEST_CASE("Own dialogs: a dialog as high as the screen keeps its title bar on the screen",
          "[gui][dialogs]") {
    // The frame the system adds (on Windows a title bar of about 31 px and a border of
    // 8 px) stays within the screen too
    KalahariDialog dialog;
    dialog.setHeading(QStringLiteral("Report"));
    for (int i = 0; i < 200; ++i) {
        dialog.contentLayout()->addWidget(new QLabel(QStringLiteral("Line %1").arg(i + 1)));
    }
    dialog.show();
    QApplication::processEvents();
    const QRect available = dialog.screen()->availableGeometry();

    CHECK(dialog.height() < dialog.sizeHint().height());
    INFO("frame " << dialog.frameGeometry().top() << ".." << dialog.frameGeometry().bottom()
                  << ", screen " << available.top() << ".." << available.bottom());
    CHECK(available.contains(dialog.frameGeometry()));
}
