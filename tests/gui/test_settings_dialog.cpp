/// @file test_settings_dialog.cpp
/// @brief The layout of the settings dialog's pages

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/settings_dialog.h"

#include <QApplication>
#include <QLayout>
#include <QScrollArea>
#include <QStackedWidget>

using namespace kalahari::gui;

TEST_CASE("Settings dialog: no page squeezes its groups", "[gui][settings]") {
    // Regression: the dialog kept its size whatever a page needed, so the groups of
    // Editor > Pages and Margins were squeezed until their fields overlapped, and the
    // description of Editor > General > Typewriter Scrolling was cut off
    SettingsDialog dialog(nullptr, SettingsData{});
    dialog.resize(dialog.minimumSize());
    dialog.show();
    QApplication::processEvents();

    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    REQUIRE(stack->count() > 0);

    for (int index = 0; index < stack->count(); ++index) {
        stack->setCurrentIndex(index);
        dialog.layout()->activate();
        QApplication::processEvents();

        QWidget* page = stack->widget(index);
        if (auto* scrollArea = qobject_cast<QScrollArea*>(page)) {
            page = scrollArea->widget();
        }
        REQUIRE(page != nullptr);
        REQUIRE(page->layout() != nullptr);

        // The page is as tall as its contents need at its width (a word-wrapped label
        // needs more height in a narrower page)
        const int needed = page->hasHeightForWidth()
            ? page->heightForWidth(page->width())
            : page->minimumSizeHint().height();
        INFO("page " << index << ": " << page->height() << " px high, needs " << needed);
        CHECK(page->height() >= needed);
        CHECK(page->width() >= page->minimumSizeHint().width());
    }
}
