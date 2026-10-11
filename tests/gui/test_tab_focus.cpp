/// @file test_tab_focus.cpp
/// @brief Closing a tab gives the keys to the page in front

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/gui/utils/tab_focus.h"

#include <QApplication>
#include <QLineEdit>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

using kalahari::gui::utils::removeTabKeepingFocus;
namespace test = kalahari::test;

namespace {

/// A page of the tabs with two fields
struct Page {
    QWidget* widget = new QWidget();  // the tabs own it once added
    QLineEdit* first = new QLineEdit(widget);
    QLineEdit* second = new QLineEdit(widget);

    Page() {
        auto* layout = new QVBoxLayout(widget);
        layout->addWidget(first);
        layout->addWidget(second);
    }
};

/// An active window with a field before the tabs, as the font box of a toolbar, and three
/// tabs
struct Window {
    QWidget window;
    QLineEdit* toolbarField = new QLineEdit(&window);
    QTabWidget* tabs = new QTabWidget(&window);
    std::array<Page, 3> pages;

    Window() {
        auto* layout = new QVBoxLayout(&window);
        layout->addWidget(toolbarField);
        layout->addWidget(tabs);
        for (std::size_t i = 0; i < pages.size(); ++i) {
            tabs->addTab(pages[i].widget, QString::number(i));
        }
        window.show();
        window.activateWindow();
    }
};

}  // anonymous namespace

TEST_CASE("Closing a tab: the keys go on to the page in front", "[gui][tabs]") {
    Window w;
    REQUIRE(test::waitUntil([&w] { return QApplication::activeWindow() == &w.window; }));

    SECTION("to its field that had the focus last") {
        w.tabs->setCurrentIndex(1);
        w.pages[1].second->setFocus(Qt::TabFocusReason);
        w.tabs->setCurrentIndex(2);
        w.pages[2].first->setFocus(Qt::TabFocusReason);
        REQUIRE(w.pages[2].first->hasFocus());

        // The last tab goes: the one before it comes in front
        removeTabKeepingFocus(*w.tabs, 2);
        REQUIRE(w.tabs->currentWidget() == w.pages[1].widget);
        CHECK(w.pages[1].second->hasFocus());
        CHECK_FALSE(w.toolbarField->hasFocus());
    }

    SECTION("to its first field when it never had the focus") {
        w.tabs->setCurrentIndex(2);
        w.pages[2].second->setFocus(Qt::TabFocusReason);
        REQUIRE(w.pages[2].second->hasFocus());

        removeTabKeepingFocus(*w.tabs, 2);
        REQUIRE(w.tabs->currentWidget() == w.pages[1].widget);
        CHECK(w.pages[1].first->hasFocus());
    }

    SECTION("a focus elsewhere stays there") {
        w.tabs->setCurrentIndex(0);
        w.toolbarField->setFocus(Qt::TabFocusReason);
        REQUIRE(w.toolbarField->hasFocus());

        // The tab in front and a tab behind
        removeTabKeepingFocus(*w.tabs, 0);
        CHECK(w.toolbarField->hasFocus());
        removeTabKeepingFocus(*w.tabs, 1);
        CHECK(w.toolbarField->hasFocus());
        CHECK(w.tabs->count() == 1);
    }

    SECTION("also after a question in a window of its own, as about unsaved changes") {
        w.tabs->setCurrentIndex(2);
        w.pages[2].first->setFocus(Qt::TabFocusReason);
        REQUIRE(w.pages[2].first->hasFocus());

        // The question takes the keys, and the tab goes while it is still in front
        QWidget question;
        question.show();
        question.activateWindow();
        REQUIRE(test::waitUntil(
            [&question] { return QApplication::activeWindow() == &question; }));
        removeTabKeepingFocus(*w.tabs, 2);

        // Back in the window, the keys are on the page now in front
        question.hide();
        w.window.activateWindow();
        REQUIRE(test::waitUntil([&w] { return QApplication::activeWindow() == &w.window; }));
        REQUIRE(w.tabs->currentWidget() == w.pages[1].widget);
        CHECK(w.pages[1].first->hasFocus());
        CHECK_FALSE(w.toolbarField->hasFocus());
    }

    SECTION("without a page left, to the tabs") {
        w.tabs->setCurrentIndex(0);
        w.pages[0].first->setFocus(Qt::TabFocusReason);
        removeTabKeepingFocus(*w.tabs, 0);
        removeTabKeepingFocus(*w.tabs, 0);
        removeTabKeepingFocus(*w.tabs, 0);
        REQUIRE(w.tabs->count() == 0);
        CHECK(w.tabs->hasFocus());
        CHECK_FALSE(w.toolbarField->hasFocus());
    }

    SECTION("an index out of range does nothing") {
        removeTabKeepingFocus(*w.tabs, 3);
        removeTabKeepingFocus(*w.tabs, -1);
        CHECK(w.tabs->count() == 3);
    }
}
