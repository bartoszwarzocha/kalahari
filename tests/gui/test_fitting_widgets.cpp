/// @file test_fitting_widgets.cpp
/// @brief Widgets that fit a small screen: items that go on in more rows, and bars whose
/// message takes more lines

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/info_bar.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"

#include <QLayoutItem>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QWidget>

using namespace kalahari::gui;

namespace {

/// A widget of a fixed size in @p parent, as a button in a row
QWidget* box(QWidget* parent, int width, int height = 20) {
    auto* widget = new QWidget(parent);
    widget->setFixedSize(width, height);
    return widget;
}

/// A sentence that takes more lines in a narrow widget
const QString LONG_MESSAGE = QStringLiteral(
    "The screen is small, so the panels on the right are hidden. View > Panels shows them "
    "again.");

}  // anonymous namespace

// =============================================================================
// FlowLayout
// =============================================================================

TEST_CASE("Flow layout: items side by side, in more rows when narrow", "[gui][widgets]") {
    QWidget panel;
    auto* layout = new FlowLayout(&panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    QWidget* first = box(&panel, 100);
    QWidget* second = box(&panel, 100);
    QWidget* third = box(&panel, 100);
    layout->addWidget(first);
    layout->addWidget(second);
    layout->addWidget(third);

    CHECK(layout->count() == 3);
    CHECK(layout->hasHeightForWidth());
    CHECK(layout->sizeHint() == QSize(320, 20));     // all in one row
    CHECK(layout->minimumSize() == QSize(100, 20));  // the widest item

    SECTION("wide: one row") {
        CHECK(layout->heightForWidth(320) == 20);
        layout->setGeometry(QRect(0, 0, 320, 20));
        CHECK(first->pos() == QPoint(0, 0));
        CHECK(second->pos() == QPoint(110, 0));
        CHECK(third->pos() == QPoint(220, 0));
    }

    SECTION("narrower: the item that does not fit starts the next row") {
        CHECK(layout->heightForWidth(250) == 50);
        layout->setGeometry(QRect(0, 0, 250, 50));
        CHECK(first->pos() == QPoint(0, 0));
        CHECK(second->pos() == QPoint(110, 0));
        CHECK(third->pos() == QPoint(0, 30));
    }

    SECTION("narrower than an item: an item in each row, at its own width") {
        CHECK(layout->heightForWidth(60) == 80);
        layout->setGeometry(QRect(0, 0, 60, 80));
        CHECK(third->pos() == QPoint(0, 60));
        CHECK(third->width() == 100);
    }

    SECTION("a hidden item takes no room") {
        second->hide();
        CHECK(layout->sizeHint() == QSize(210, 20));
        CHECK(layout->heightForWidth(250) == 20);
    }

    SECTION("an item taken out is no longer laid out") {
        QLayoutItem* taken = layout->takeAt(2);
        REQUIRE(taken != nullptr);
        CHECK(taken->widget() == third);
        delete taken;
        CHECK(layout->count() == 2);
        CHECK(layout->itemAt(2) == nullptr);
        CHECK(layout->takeAt(5) == nullptr);
        CHECK(layout->sizeHint() == QSize(210, 20));
    }
}

TEST_CASE("Flow layout: an item that expands takes the room left in its row",
          "[gui][widgets]") {
    QWidget panel;
    auto* layout = new FlowLayout(&panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    auto* field = new QLineEdit(&panel);
    field->setMinimumWidth(50);
    field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QWidget* button = box(&panel, 80);
    layout->addWidget(field);
    layout->addWidget(button);

    layout->setGeometry(QRect(0, 0, 400, layout->heightForWidth(400)));
    CHECK(field->width() == 310);
    CHECK(button->x() == 320);

    SECTION("as far as its maximum width allows: the rest stays at the end of the row") {
        field->setMaximumWidth(200);
        layout->setGeometry(QRect(0, 0, 400, layout->heightForWidth(400)));
        CHECK(field->width() == 200);
        CHECK(button->x() == 210);
    }
}

// =============================================================================
// InfoBar
// =============================================================================

TEST_CASE("Info bar: its message takes more lines in a narrow bar", "[gui][widgets]") {
    // Regression: the bar of a file outside the book made the window as wide as its message
    InfoBar bar;
    bar.setMessage(LONG_MESSAGE);
    bar.setActionText(QStringLiteral("Show Panels"));

    CHECK(bar.message() == LONG_MESSAGE);
    CHECK(bar.minimumSizeHint().width() < bar.sizeHint().width());
    REQUIRE(bar.hasHeightForWidth());
    CHECK(bar.heightForWidth(300) > bar.heightForWidth(1000));
}

TEST_CASE("Info bar: its button and its close button", "[gui][widgets]") {
    InfoBar bar;
    bar.setMessage(QStringLiteral("The panels on the right are hidden."));
    auto* action = bar.findChild<QPushButton*>();
    auto* close = bar.findChild<QToolButton*>();
    REQUIRE(action != nullptr);
    REQUIRE(close != nullptr);
    CHECK_FALSE(action->isVisibleTo(&bar));  // no button without its text
    CHECK(close->isVisibleTo(&bar));

    bar.setActionText(QStringLiteral("Show Panels"));
    bar.setActionToolTip(QStringLiteral("Show the panels on the right"));
    CHECK(action->isVisibleTo(&bar));
    CHECK(action->text() == QStringLiteral("Show Panels"));
    CHECK(action->toolTip() == QStringLiteral("Show the panels on the right"));

    int actions = 0;
    int dismissals = 0;
    QObject::connect(&bar, &InfoBar::actionClicked, [&actions]() { ++actions; });
    QObject::connect(&bar, &InfoBar::dismissed, [&dismissals]() { ++dismissals; });
    action->click();
    CHECK(actions == 1);
    CHECK(dismissals == 0);
    close->click();
    CHECK(dismissals == 1);

    SECTION("an empty text hides the button again") {
        bar.setActionText(QString());
        CHECK_FALSE(action->isVisibleTo(&bar));
    }
}

TEST_CASE("Info bar: a file outside the book offers to add it", "[gui][widgets]") {
    StandaloneInfoBar bar;
    bar.setFilePath(QStringLiteral("/notes/notes.txt"));
    CHECK(bar.filePath() == QStringLiteral("/notes/notes.txt"));
    CHECK_FALSE(bar.message().isEmpty());

    int requests = 0;
    QObject::connect(&bar, &StandaloneInfoBar::addToProjectClicked,
                     [&requests]() { ++requests; });
    auto* action = bar.findChild<QPushButton*>();
    REQUIRE(action != nullptr);
    REQUIRE(action->isVisibleTo(&bar));
    action->click();
    CHECK(requests == 1);
}
