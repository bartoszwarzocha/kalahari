/// @file test_tab_focus.cpp
/// @brief The keys go to the page in front of the tabs, also when a tab closes or is clicked

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/gui/utils/tab_focus.h"

#include <QApplication>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPointF>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

using kalahari::gui::utils::focusPageInFront;
using kalahari::gui::utils::focusPageOnTabClick;
using kalahari::gui::utils::removeTabKeepingFocus;
namespace test = kalahari::test;

namespace {

/// A click of @p button on the tab at @p index of @p tabs
void clickTab(QTabWidget& tabs, int index, Qt::MouseButton button = Qt::LeftButton) {
    QTabBar* bar = tabs.tabBar();
    const QPointF pos = bar->tabRect(index).center();
    const QPointF globalPos = bar->mapToGlobal(pos);
    QMouseEvent press(QEvent::MouseButtonPress, pos, globalPos, button, button, Qt::NoModifier);
    QApplication::sendEvent(bar, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, globalPos, button, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(bar, &release);
}

/// Shows @p window in front of the others, as far as the system lets it: on Windows, with
/// tests running side by side, the window of another program may stay in front
void showInFront(QWidget& window) {
    window.show();
    window.activateWindow();
    test::waitUntil([&window] { return QApplication::activeWindow() == &window; });
}

/// Whether @p widget has the keys of its window, or gets them when the window comes to the
/// front. Unlike hasFocus(), it does not depend on the system putting the window in front.
bool getsKeys(const QWidget* widget) {
    const QWidget* target = widget;
    while (target->focusProxy() != nullptr) {
        target = target->focusProxy();
    }
    return widget->window()->focusWidget() == target;
}

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

/// A window with a field before the tabs, as the font box of a toolbar, and three tabs
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
        showInFront(window);
    }
};

}  // anonymous namespace

TEST_CASE("Closing a tab: the keys go on to the page in front", "[gui][tabs]") {
    Window w;

    SECTION("to its field that had the focus last") {
        w.tabs->setCurrentIndex(1);
        w.pages[1].second->setFocus(Qt::TabFocusReason);
        w.tabs->setCurrentIndex(2);
        w.pages[2].first->setFocus(Qt::TabFocusReason);
        REQUIRE(getsKeys(w.pages[2].first));

        // The last tab goes: the one before it comes in front
        removeTabKeepingFocus(*w.tabs, 2);
        REQUIRE(w.tabs->currentWidget() == w.pages[1].widget);
        CHECK(getsKeys(w.pages[1].second));
        CHECK_FALSE(getsKeys(w.toolbarField));
    }

    SECTION("to its first field when it never had the focus") {
        w.tabs->setCurrentIndex(2);
        w.pages[2].second->setFocus(Qt::TabFocusReason);
        REQUIRE(getsKeys(w.pages[2].second));

        removeTabKeepingFocus(*w.tabs, 2);
        REQUIRE(w.tabs->currentWidget() == w.pages[1].widget);
        CHECK(getsKeys(w.pages[1].first));
    }

    SECTION("a tab behind closes: a focus elsewhere stays there") {
        w.tabs->setCurrentIndex(0);
        w.toolbarField->setFocus(Qt::TabFocusReason);
        REQUIRE(getsKeys(w.toolbarField));

        removeTabKeepingFocus(*w.tabs, 2);
        removeTabKeepingFocus(*w.tabs, 1);
        CHECK(getsKeys(w.toolbarField));
        CHECK(w.tabs->count() == 1);
    }

    SECTION("the tab in front closes: the page coming to the front gets the keys from "
            "elsewhere too, as from the Navigator") {
        w.tabs->setCurrentIndex(1);
        w.pages[1].second->setFocus(Qt::TabFocusReason);
        w.tabs->setCurrentIndex(2);
        w.toolbarField->setFocus(Qt::TabFocusReason);
        REQUIRE(getsKeys(w.toolbarField));

        removeTabKeepingFocus(*w.tabs, 2);
        REQUIRE(w.tabs->currentWidget() == w.pages[1].widget);
        CHECK(getsKeys(w.pages[1].second));
        CHECK_FALSE(getsKeys(w.toolbarField));
    }

    SECTION("also after a question in a window of its own, as about unsaved changes") {
        w.tabs->setCurrentIndex(2);
        w.pages[2].first->setFocus(Qt::TabFocusReason);
        REQUIRE(getsKeys(w.pages[2].first));

        // The question takes the keys, and the tab goes while it is still in front
        QWidget question;
        showInFront(question);
        removeTabKeepingFocus(*w.tabs, 2);

        // Back in the window, the keys are on the page now in front
        question.hide();
        showInFront(w.window);
        REQUIRE(w.tabs->currentWidget() == w.pages[1].widget);
        CHECK(getsKeys(w.pages[1].first));
        CHECK_FALSE(getsKeys(w.toolbarField));
    }

    SECTION("without a page left, to the tabs") {
        w.tabs->setCurrentIndex(0);
        w.pages[0].first->setFocus(Qt::TabFocusReason);
        removeTabKeepingFocus(*w.tabs, 0);
        removeTabKeepingFocus(*w.tabs, 0);
        removeTabKeepingFocus(*w.tabs, 0);
        REQUIRE(w.tabs->count() == 0);
        CHECK(getsKeys(w.tabs));
        CHECK_FALSE(getsKeys(w.toolbarField));
    }

    SECTION("an index out of range does nothing") {
        removeTabKeepingFocus(*w.tabs, 3);
        removeTabKeepingFocus(*w.tabs, -1);
        CHECK(w.tabs->count() == 3);
    }
}

TEST_CASE("The page in front of the tabs gets the keys", "[gui][tabs]") {
    Window w;
    w.toolbarField->setFocus(Qt::TabFocusReason);

    SECTION("its first field, as a page just opened") {
        w.tabs->setCurrentIndex(2);
        REQUIRE(getsKeys(w.toolbarField));
        focusPageInFront(*w.tabs);
        CHECK(getsKeys(w.pages[2].first));
    }

    SECTION("its field that had the focus last") {
        w.tabs->setCurrentIndex(1);
        w.pages[1].second->setFocus(Qt::TabFocusReason);
        w.toolbarField->setFocus(Qt::TabFocusReason);
        focusPageInFront(*w.tabs);
        CHECK(getsKeys(w.pages[1].second));
    }

    SECTION("in a window behind another of the program: when it comes back") {
        w.tabs->setCurrentIndex(1);
        QWidget other;
        showInFront(other);
        const bool otherInFront = QApplication::activeWindow() == &other;

        // The other window keeps the keys meanwhile
        focusPageInFront(*w.tabs);
        QApplication::processEvents();
        if (otherInFront) {
            CHECK(QApplication::activeWindow() == &other);
        }

        other.hide();
        showInFront(w.window);
        CHECK(getsKeys(w.pages[1].first));
    }
}

TEST_CASE("A click on a tab gives the keys to its page", "[gui][tabs]") {
    Window w;
    focusPageOnTabClick(*w.tabs);

    SECTION("another tab: its page comes to the front with the keys, from elsewhere too") {
        w.tabs->setCurrentIndex(1);
        w.pages[1].second->setFocus(Qt::TabFocusReason);
        w.tabs->setCurrentIndex(0);
        w.toolbarField->setFocus(Qt::TabFocusReason);
        REQUIRE(getsKeys(w.toolbarField));

        clickTab(*w.tabs, 1);
        REQUIRE(w.tabs->currentIndex() == 1);
        CHECK(test::waitUntil([&w] { return getsKeys(w.pages[1].second); }));
    }

    SECTION("the tab in front: its page gets the keys") {
        w.tabs->setCurrentIndex(2);
        w.toolbarField->setFocus(Qt::TabFocusReason);
        REQUIRE(getsKeys(w.toolbarField));

        clickTab(*w.tabs, 2);
        CHECK(test::waitUntil([&w] { return getsKeys(w.pages[2].first); }));
    }
}
