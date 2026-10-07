/// @file test_panel_toggle.cpp
/// @brief The show/hide toggles of the dock panels

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/utils/panel_toggle.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QTabBar>

using namespace kalahari::gui::utils;

namespace {

/// Title of the tab on top of the tab group holding @p dock (empty without a group)
QString raisedTab(const QMainWindow& window, const QDockWidget* dock) {
    for (const QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        for (int i = 0; i < tabBar->count(); ++i) {
            if (tabBar->tabText(i) == dock->windowTitle()) {
                return tabBar->tabText(tabBar->currentIndex());
            }
        }
    }
    return {};
}

}  // namespace

TEST_CASE("Panel toggles: checked while the panel is open, also under another tab", "[gui][panels]") {
    // Regression: the toggles of View > Panels and of the View toolbar followed
    // QDockWidget::visibilityChanged, which also reports a panel whose tab lies under
    // another tab as hidden, so only the panel on top of a tab group was checked
    QMainWindow window;
    window.setCentralWidget(new QLabel(QStringLiteral("text")));
    const QString names[3] = {QStringLiteral("Properties"), QStringLiteral("Search"),
                              QStringLiteral("Assistant")};
    QDockWidget* docks[3] = {};
    QAction* toggles[3] = {};
    for (int i = 0; i < 3; ++i) {
        docks[i] = new QDockWidget(names[i], &window);
        docks[i]->setObjectName(names[i]);
        docks[i]->setWidget(new QLabel(names[i]));
        window.addDockWidget(Qt::RightDockWidgetArea, docks[i]);
        toggles[i] = new QAction(names[i], &window);
        followPanel(toggles[i], docks[i]);
        // As the panel commands do: the triggered action toggles its panel
        QDockWidget* dock = docks[i];
        QObject::connect(toggles[i], &QAction::triggered, [dock] { togglePanel(dock); });
    }
    window.tabifyDockWidget(docks[0], docks[1]);
    window.tabifyDockWidget(docks[1], docks[2]);

    // An open panel is checked also before the window is shown
    for (QAction* toggle : toggles) {
        CHECK(toggle->isCheckable());
        CHECK(toggle->isChecked());
    }

    window.resize(800, 600);
    window.show();
    docks[0]->raise();
    QApplication::processEvents();
    REQUIRE(raisedTab(window, docks[0]) == names[0]);

    SECTION("raising another tab keeps every open panel checked") {
        docks[1]->raise();
        QApplication::processEvents();
        CHECK(raisedTab(window, docks[1]) == names[1]);
        for (int i = 0; i < 3; ++i) {
            CHECK(isPanelOpen(docks[i]));
            CHECK(toggles[i]->isChecked());
        }
    }

    SECTION("the toggle closes a panel under another tab and opens it on top") {
        toggles[2]->trigger();
        QApplication::processEvents();
        CHECK_FALSE(isPanelOpen(docks[2]));
        CHECK_FALSE(toggles[2]->isChecked());
        CHECK(toggles[0]->isChecked());
        CHECK(toggles[1]->isChecked());

        toggles[2]->trigger();
        QApplication::processEvents();
        CHECK(isPanelOpen(docks[2]));
        CHECK(toggles[2]->isChecked());
        CHECK(raisedTab(window, docks[2]) == names[2]);
        CHECK(toggles[0]->isChecked());
    }

    SECTION("closing a panel with its own button unchecks its toggle") {
        docks[0]->close();
        QApplication::processEvents();
        CHECK_FALSE(isPanelOpen(docks[0]));
        CHECK_FALSE(toggles[0]->isChecked());
        CHECK(toggles[1]->isChecked());
        CHECK(toggles[2]->isChecked());
    }
}
