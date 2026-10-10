/// @file test_tool_button_columns.cpp
/// @brief Panel buttons that wrap into more columns when the panel is low

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/gui/widgets/tool_button_columns.h"

#include <QAction>
#include <QApplication>
#include <QToolButton>
#include <QWidget>

using kalahari::gui::LogPanel;
using kalahari::gui::ToolButtonColumns;

namespace {

/// Every button lies whole inside the widget
bool allVisible(const ToolButtonColumns& columns)
{
    for (const QToolButton* button : columns.buttons()) {
        if (!columns.rect().contains(button->geometry())) {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

TEST_CASE("Panel buttons: a low panel shows them in more columns", "[gui][widgets]") {
    // A child widget: the system limits how small a window may be (macOS), not a child
    QWidget host;
    host.resize(800, 600);
    auto& columns = *new ToolButtonColumns(20, &host);
    for (int i = 0; i < 4; ++i) {
        columns.addAction(new QAction(QStringLiteral("Action %1").arg(i + 1), &columns));
    }
    host.show();
    const QSize cell = columns.buttons().first()->sizeHint();

    SECTION("Tall enough: one column") {
        columns.resize(columns.sizeHint());
        CHECK(columns.minimumSizeHint().width() == cell.width());
        CHECK(allVisible(columns));
    }

    SECTION("Room for two buttons: two columns, none hidden") {
        columns.resize(cell.width(), cell.height() * 2 + cell.height() / 2);
        CHECK(columns.minimumSizeHint().width() == cell.width() * 2);
        columns.resize(columns.minimumSizeHint().width(), columns.height());
        CHECK(allVisible(columns));
    }

    SECTION("Room for one button: a row") {
        columns.resize(cell.width(), cell.height());
        CHECK(columns.minimumSizeHint().width() == cell.width() * 4);
        columns.resize(columns.minimumSizeHint().width(), columns.height());
        CHECK(allVisible(columns));
    }
}

TEST_CASE("Log panel: its buttons stay visible in a low panel", "[gui][widgets]") {
    LogPanel panel;
    panel.resize(600, 60);
    panel.show();
    auto* columns = panel.findChild<ToolButtonColumns*>();
    REQUIRE(columns != nullptr);
    REQUIRE(columns->buttons().size() == 4);
    // The layout takes the new width the columns ask for
    for (int i = 0; i < 3; ++i) {
        QApplication::processEvents();
    }
    CHECK(allVisible(*columns));
    for (const QToolButton* button : columns->buttons()) {
        CHECK(button->focusPolicy() == Qt::TabFocus);
    }
}
