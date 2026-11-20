// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_notifications.cpp
/// @brief The notifications of the program: in the bar at the bottom of the window, in the
/// program's window, as notifications of the system or in the Assistant panel, the panel that
/// keeps them, the window in front for the notifications of the whole program, the page
/// Settings > Notifications, and the messages of the plugins as notifications

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/plugin_host.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/notification_dialog.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/notifications.h"
#include "kalahari/gui/panels/assistant_panel.h"
#include "kalahari/gui/plugin_coordinator.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/gui/widgets/info_bar.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPixmap>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QToolButton>

#include <memory>
#include <string>
#include <utility>

#include "kalahari_tests/test_support/window_parts.h"

namespace core = kalahari::core;
namespace gui = kalahari::gui;
using gui::Notification;
using gui::NotificationCoordinator;
using gui::NotificationPlace;

namespace {

/// A book window: its panels and its notifications, with the pages its gears asked for
struct Window {
    QMainWindow window;
    gui::DockCoordinator* docks;
    QStringList pages;
    std::unique_ptr<NotificationCoordinator> notifications;

    Window()
        : docks(madeDocks(window))
    {
        notifications = std::make_unique<NotificationCoordinator>(
            docks, [this](const QString& page) { pages.append(page); });
        window.show();
    }
    ~Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /// The panels of a window, with the commands they need
    static gui::DockCoordinator* madeDocks(QMainWindow& window)
    {
        gui::registerAllCommands(kalahari::test::commands(), gui::CommandCallbacks{});
        auto* docks = new gui::DockCoordinator(kalahari::test::book(),
                                               kalahari::test::commands(), &window, &window);
        docks->createDocks();
        return docks;
    }

    [[nodiscard]] gui::InfoBar* bar() const { return notifications->bar(); }
    [[nodiscard]] gui::AssistantPanel* assistant() const { return docks->assistantPanel(); }
};

/// The notifications of the system shown, instead of the system tray, for a test
struct SystemNotifications {
    struct Shown {
        QString title;
        QString text;
        Notification::Kind kind;
    };
    QList<Shown> shown;
    bool shows = true;  ///< false: the system does not show them

    SystemNotifications()
    {
        NotificationCoordinator::setSystemNotifier(
            [this](const QString& title, const QString& text, Notification::Kind kind) {
                if (shows) {
                    shown.append({title, text, kind});
                }
                return shows;
            });
    }
    ~SystemNotifications() { NotificationCoordinator::setSystemNotifier({}); }
    SystemNotifications(const SystemNotifications&) = delete;
    SystemNotifications& operator=(const SystemNotifications&) = delete;
    SystemNotifications(SystemNotifications&&) = delete;
    SystemNotifications& operator=(SystemNotifications&&) = delete;
};

/// The window in front for a test, the program's way back after it
struct FrontWindow {
    explicit FrontWindow(NotificationCoordinator* front)
    {
        NotificationCoordinator::setFrontFinder([front]() { return front; });
    }
    ~FrontWindow() { NotificationCoordinator::setFrontFinder({}); }
    FrontWindow(const FrontWindow&) = delete;
    FrontWindow& operator=(const FrontWindow&) = delete;
    FrontWindow(FrontWindow&&) = delete;
    FrontWindow& operator=(FrontWindow&&) = delete;
};

void choosePlace(NotificationPlace place)
{
    core::SettingsManager::getInstance().set<std::string>(
        NotificationCoordinator::PLACE_SETTING, NotificationCoordinator::valueOfPlace(place));
}

/// The notifications of this moment show once the event loop goes on
void settle()
{
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

Notification goal()
{
    return {QStringLiteral("Statistics and Goals"),
            QStringLiteral("Daily goal reached: 1,000 of 1,000 words.")};
}

Notification badge()
{
    return {QStringLiteral("Statistics and Goals"),
            QStringLiteral("New badge: Streak: 7 days in a row (bronze).")};
}

Notification download()
{
    return {QStringLiteral("Materials"),
            QStringLiteral("Nothing was downloaded for \"Dunes\": No answer."),
            Notification::Kind::Warning};
}

} // anonymous namespace

TEST_CASE("Notifications: the places and their values", "[gui][notifications]")
{
    for (NotificationPlace place : {NotificationPlace::Bar, NotificationPlace::Window,
                                    NotificationPlace::System, NotificationPlace::Assistant}) {
        CHECK(NotificationCoordinator::placeOfValue(NotificationCoordinator::valueOfPlace(place)) ==
              place);
    }
    CHECK(NotificationCoordinator::valueOfPlace(NotificationPlace::Window) == "window");
    CHECK(NotificationCoordinator::placeOfValue("unknown") == NotificationPlace::Bar);

    // The bar by default
    CHECK(NotificationCoordinator::chosenPlace() == NotificationPlace::Bar);
    choosePlace(NotificationPlace::System);
    CHECK(NotificationCoordinator::chosenPlace() == NotificationPlace::System);
    {
        const SystemNotifications system;
        CHECK(NotificationCoordinator::place() == NotificationPlace::System);
    }
    // Where the system has no tray with messages, the bar shows them
    if (!(QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages())) {
        CHECK_FALSE(NotificationCoordinator::systemNotificationsAvailable());
        CHECK(NotificationCoordinator::place() == NotificationPlace::Bar);
    }
}

TEST_CASE("Notifications: in the bar at the bottom of the window by default",
          "[gui][notifications][clipboard]")
{
    Window book;
    gui::InfoBar* bar = book.bar();
    REQUIRE(bar != nullptr);
    CHECK(bar->objectName() == QStringLiteral("notificationBar"));
    CHECK(bar->isHidden());

    // Those of one moment together, in the order they came; the same one twice shows once
    book.notifications->notify(goal());
    book.notifications->notify(badge());
    book.notifications->notify(goal());
    CHECK(bar->isHidden());
    settle();
    CHECK(bar->isVisible());
    const QString both = goal().text + QLatin1Char(' ') + badge().text;
    CHECK(bar->message() == both);
    CHECK(bar->iconId() == QStringLiteral("help.about"));

    // Copy, the gear of the settings and the close button
    const QList<QPushButton*> buttons = bar->findChildren<QPushButton*>();
    REQUIRE(buttons.size() == 1);
    CHECK(buttons.first()->text() == QStringLiteral("Copy"));
    buttons.first()->click();
    CHECK(QApplication::clipboard()->text() == both);
    REQUIRE(bar->settingsButton() != nullptr);
    CHECK(bar->settingsButton()->toolTip() == QStringLiteral("Notification Settings"));
    bar->settingsButton()->click();
    CHECK(book.pages == QStringList{QString::fromLatin1(gui::SettingsDialog::NOTIFICATIONS_PAGE)});
    emit bar->dismissed();
    CHECK(bar->isHidden());

    // A warning with its icon; a message of a plugin after its title
    book.notifications->notify(download());
    settle();
    CHECK(bar->isVisible());
    CHECK(bar->message() == download().text);
    CHECK(bar->iconId() == QStringLiteral("common.warning"));
    Notification plugin{QStringLiteral("Backup"), QStringLiteral("The copy is ready.")};
    plugin.titleInBar = true;
    book.notifications->notify(plugin);
    settle();
    CHECK(bar->message() == QStringLiteral("Backup: The copy is ready."));
    CHECK(bar->iconId() == QStringLiteral("help.about"));

    // An empty one shows nothing
    book.notifications->notify(Notification{QStringLiteral("Backup"), QString()});
    settle();
    CHECK(bar->message() == QStringLiteral("Backup: The copy is ready."));
}

TEST_CASE("Notifications: the bar in the colors of the notifications, which the writer changes",
          "[gui][notifications][theme]")
{
    const Window book;
    auto& themes = core::ThemeManager::getInstance();
    const std::string theme = themes.getCurrentTheme().name;
    const std::string setting = core::ThemeManager::editorColorSetting(
        theme, NotificationCoordinator::BACKGROUND_COLOR_KEY);
    // The theme's own colors, those of the other info bars by default
    const QColor own = themes.editorColor(NotificationCoordinator::BACKGROUND_COLOR_KEY, QColor());
    REQUIRE(own.isValid());
    CHECK(own == themes.getCurrentTheme().colors.infoBarBackground);
    CHECK(themes.editorColor(NotificationCoordinator::BORDER_COLOR_KEY, QColor()) ==
          themes.getCurrentTheme().colors.infoBarBorder);
    CHECK(book.bar()->styleSheet().contains(own.name()));
    // As painted, whatever the bar's name: its background just inside its left edge, before
    // its icon, and its border at the top (shown, as with a notification)
    gui::InfoBar* bar = book.bar();
    REQUIRE(bar->objectName() == QStringLiteral("notificationBar"));
    bar->show();
    settle();
    const auto painted = [bar]() {
        const QImage image = bar->grab().toImage();
        return std::make_pair(image.pixelColor(2, image.height() / 2),
                              image.pixelColor(image.width() / 2, 0));
    };
    CHECK(painted().first == own);
    CHECK(painted().second == themes.getCurrentTheme().colors.infoBarBorder);

    core::SettingsManager::getInstance().set<std::string>(setting, "#123456");
    settle();
    CHECK(bar->styleSheet().contains(QStringLiteral("#123456")));
    CHECK(painted().first == QColor(QStringLiteral("#123456")));
    core::SettingsManager::getInstance().removeKey(setting);
    settle();
    CHECK(bar->styleSheet().contains(own.name()));
    CHECK(painted().first == own);
}

TEST_CASE("Notifications: the Assistant panel keeps them whatever the place",
          "[gui][notifications][clipboard]")
{
    gui::AssistantPanel panel;
    auto* empty = panel.findChild<QLabel*>(QStringLiteral("assistantEmpty"));
    auto* copy = panel.findChild<QPushButton*>(QStringLiteral("assistantCopy"));
    auto* clear = panel.findChild<QPushButton*>(QStringLiteral("assistantClear"));
    REQUIRE(empty != nullptr);
    REQUIRE(copy != nullptr);
    REQUIRE(clear != nullptr);
    panel.show();
    CHECK(empty->isVisible());
    CHECK_FALSE(panel.notificationList()->isVisible());
    CHECK_FALSE(copy->isEnabled());

    const QDateTime time(QDate(2026, 11, 20), QTime(14, 5));
    panel.addNotification(goal(), time);
    panel.addNotification(download(), time.addSecs(600));
    CHECK(panel.notificationCount() == 2);
    CHECK_FALSE(empty->isVisible());
    CHECK(panel.notificationList()->isVisible());
    CHECK(copy->isEnabled());
    CHECK(panel.notificationText(0) == goal().title + QLatin1Char('\n') + goal().text);
    CHECK(panel.notificationText(5).isEmpty());
    // Screen readers hear the time, the title and the text
    const QString heard =
        panel.notificationList()->item(1)->data(Qt::AccessibleTextRole).toString();
    CHECK(heard.contains(download().title));
    CHECK(heard.contains(download().text));

    // Copy: the latest without a choice, else the chosen one, also with Ctrl+C in the list
    copy->click();
    CHECK(QApplication::clipboard()->text() == panel.notificationText(1));
    panel.notificationList()->setCurrentRow(0);
    copy->click();
    CHECK(QApplication::clipboard()->text() == panel.notificationText(0));
    QApplication::clipboard()->clear();
    QAction* copyKeys = nullptr;
    for (QAction* action : panel.notificationList()->actions()) {
        if (action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_C)) {
            copyKeys = action;
        }
    }
    REQUIRE(copyKeys != nullptr);
    copyKeys->trigger();
    CHECK(QApplication::clipboard()->text() == panel.notificationText(0));

    // Not more than it keeps: the earliest go
    for (int i = 0; i < gui::AssistantPanel::MAX_NOTIFICATIONS; ++i) {
        panel.addNotification(badge(), time);
    }
    CHECK(panel.notificationCount() == gui::AssistantPanel::MAX_NOTIFICATIONS);
    CHECK(panel.notificationText(0) == badge().title + QLatin1Char('\n') + badge().text);

    clear->click();
    CHECK(panel.notificationCount() == 0);
    CHECK(empty->isVisible());
    CHECK_FALSE(clear->isEnabled());
}

TEST_CASE("Notifications: the window keeps them also where they show elsewhere",
          "[gui][notifications]")
{
    const SystemNotifications system;
    Window book;
    choosePlace(NotificationPlace::System);
    book.notifications->notify(goal());
    settle();
    REQUIRE(book.assistant() != nullptr);
    CHECK(book.assistant()->notificationCount() == 1);
    CHECK(system.shown.size() == 1);
    CHECK(book.bar()->isHidden());
}

TEST_CASE("Notifications: in the program's window, which does not take the keys",
          "[gui][notifications]")
{
    Window book;
    choosePlace(NotificationPlace::Window);
    book.notifications->notify(goal());
    book.notifications->notify(badge());
    settle();
    gui::dialogs::NotificationDialog* window = book.notifications->notificationWindow();
    REQUIRE(window != nullptr);
    CHECK(window->isVisible());
    CHECK_FALSE(window->isModal());
    CHECK(window->testAttribute(Qt::WA_ShowWithoutActivating));
    CHECK(window->parentWidget() == &book.window);
    CHECK(window->count() == 2);
    CHECK(window->windowTitle() == goal().title);
    CHECK(window->clipboardText() == goal().title + QStringLiteral("\n\n") + goal().text +
                                         QStringLiteral("\n\n") + badge().text);
    CHECK(book.bar()->isHidden());

    // The next ones are added to the open window; of another title, after their titles
    book.notifications->notify(download());
    settle();
    CHECK(book.notifications->notificationWindow() == window);
    CHECK(window->count() == 3);
    CHECK(window->windowTitle() == QStringLiteral("Notifications"));
    CHECK(window->clipboardText().endsWith(download().title + QStringLiteral(": ") +
                                           download().text));

    // Closed, it goes; the next notification opens a new one
    window->close();
    settle();
    CHECK(book.notifications->notificationWindow() == nullptr);
    book.notifications->notify(goal());
    settle();
    REQUIRE(book.notifications->notificationWindow() != nullptr);
    CHECK(book.notifications->notificationWindow()->count() == 1);
    book.notifications->notificationWindow()->close();
    settle();
}

TEST_CASE("Notifications: as notifications of the system, or in the bar where it shows none",
          "[gui][notifications]")
{
    SystemNotifications system;
    Window book;
    choosePlace(NotificationPlace::System);
    book.notifications->notify(goal());
    book.notifications->notify(badge());
    settle();
    REQUIRE(system.shown.size() == 1);
    CHECK(system.shown.first().title == goal().title);
    CHECK(system.shown.first().text == goal().text + QLatin1Char(' ') + badge().text);
    CHECK(system.shown.first().kind == Notification::Kind::Information);
    CHECK(book.bar()->isHidden());

    // Of several titles: each after its title, in a line of its own; a warning among them
    book.notifications->notify(goal());
    book.notifications->notify(download());
    settle();
    REQUIRE(system.shown.size() == 2);
    CHECK(system.shown.last().title == QStringLiteral("Notifications"));
    CHECK(system.shown.last().text == goal().title + QStringLiteral(": ") + goal().text +
                                          QLatin1Char('\n') + download().title +
                                          QStringLiteral(": ") + download().text);
    CHECK(system.shown.last().kind == Notification::Kind::Warning);

    // The system did not show it: the bar does
    system.shows = false;
    book.notifications->notify(goal());
    settle();
    CHECK(book.bar()->isVisible());
    CHECK(book.bar()->message() == goal().text);
}

TEST_CASE("Notifications: in the Assistant panel, which opens with them",
          "[gui][notifications]")
{
    Window book;
    choosePlace(NotificationPlace::Assistant);
    QDockWidget* dock = book.docks->assistantDock();
    REQUIRE(dock != nullptr);
    dock->hide();
    book.notifications->notify(download());
    settle();
    CHECK(dock->isVisible());
    CHECK(book.assistant()->notificationCount() == 1);
    CHECK(book.bar()->isHidden());

    // Its gear opens the settings of the notifications
    QToolButton* gear = nullptr;
    for (QToolButton* button : dock->titleBarWidget()->findChildren<QToolButton*>()) {
        if (button->toolTip() == QStringLiteral("Notification Settings")) {
            gear = button;
        }
    }
    REQUIRE(gear != nullptr);
    QString asked;
    QObject::connect(book.docks, &gui::DockCoordinator::settingsPageRequested,
                     [&asked](const QString& page) { asked = page; });
    gear->click();
    CHECK(asked == QString::fromLatin1(gui::SettingsDialog::NOTIFICATIONS_PAGE));
}

TEST_CASE("Notifications: those of the whole program show once, in the window in front",
          "[gui][notifications]")
{
    const Window first;
    const Window second;
    {
        const FrontWindow front(second.notifications.get());
        // As two windows tell it at one moment (grammar checking ready)
        CHECK(NotificationCoordinator::notifyFront(goal()));
        CHECK(NotificationCoordinator::notifyFront(goal()));
        settle();
        CHECK(first.bar()->isHidden());
        CHECK(second.bar()->isVisible());
        CHECK(second.bar()->message() == goal().text);
        CHECK(second.assistant()->notificationCount() == 1);
        CHECK(first.assistant()->notificationCount() == 0);
    }
    // Without a finder: the window made last
    CHECK(NotificationCoordinator::notifyFront(badge()));
    settle();
    CHECK(second.bar()->message() == badge().text);
}

TEST_CASE("Notifications: without a book window the caller shows them its own way",
          "[gui][notifications]")
{
    CHECK_FALSE(NotificationCoordinator::notifyFront(goal()));
    CHECK_FALSE(NotificationCoordinator::showSampleFront(NotificationPlace::Bar));

    // A coordinator without the panels of a window: only the system shows them
    SystemNotifications system;
    NotificationCoordinator alone(nullptr, {});
    CHECK(alone.bar() == nullptr);
    choosePlace(NotificationPlace::Window);
    alone.notify(goal());
    settle();
    CHECK(alone.notificationWindow() == nullptr);
    choosePlace(NotificationPlace::System);
    alone.notify(badge());
    settle();
    REQUIRE(system.shown.size() == 1);
    CHECK(system.shown.first().text == badge().text);
}

TEST_CASE("Notifications: the settings page, its sample and its colors", "[gui][notifications]")
{
    Window book;
    gui::NotificationsPage page;
    page.load();
    QComboBox* places = page.placeList();
    REQUIRE(places != nullptr);
    REQUIRE(places->count() == 4);
    CHECK(places->currentData().toString() == QStringLiteral("bar"));
    QStringList values;
    for (int i = 0; i < places->count(); ++i) {
        values << places->itemData(i).toString();
    }
    CHECK(values == QStringList{QStringLiteral("bar"), QStringLiteral("window"),
                                QStringLiteral("system"), QStringLiteral("assistant")});

    // The sample shows in the place chosen on the page, before Apply
    places->setCurrentIndex(places->findData(QStringLiteral("window")));
    auto* sample = page.findChild<QPushButton*>(QStringLiteral("notificationsSample"));
    REQUIRE(sample != nullptr);
    sample->click();
    REQUIRE(book.notifications->notificationWindow() != nullptr);
    CHECK(book.notifications->notificationWindow()->clipboardText().contains(
        QStringLiteral("This is how the program's notifications look.")));
    CHECK(NotificationCoordinator::chosenPlace() == NotificationPlace::Bar);
    book.notifications->notificationWindow()->close();
    settle();
    places->setCurrentIndex(places->findData(QStringLiteral("assistant")));
    sample->click();
    CHECK(book.assistant()->notificationCount() == 1);

    // Apply keeps the place; the colors of the theme in use
    page.apply();
    CHECK(NotificationCoordinator::chosenPlace() == NotificationPlace::Assistant);
    const QList<gui::ColorConfigWidget*> colors = page.findChildren<gui::ColorConfigWidget*>();
    REQUIRE(colors.size() == 2);
    colors.first()->setColor(QColor(QStringLiteral("#204060")));
    page.apply();
    settle();
    CHECK(core::ThemeManager::getInstance().editorColor(
              NotificationCoordinator::BACKGROUND_COLOR_KEY, QColor()) == QColor("#204060"));
    core::SettingsManager::getInstance().removeKey(core::ThemeManager::editorColorSetting(
        core::ThemeManager::getInstance().getCurrentTheme().name,
        NotificationCoordinator::BACKGROUND_COLOR_KEY));
    settle();
}

TEST_CASE("Notifications: the messages of the plugins are notifications",
          "[gui][notifications][plugins]")
{
    const Window book;
    gui::installPluginHost();
    core::PluginHost::getInstance().showMessage("Backup", "The copy is ready.");
    settle();
    CHECK(book.bar()->isVisible());
    CHECK(book.bar()->message() == QStringLiteral("Backup: The copy is ready."));
    REQUIRE(book.assistant()->notificationCount() == 1);
    CHECK(book.assistant()->notificationText(0) ==
          QStringLiteral("Backup\nThe copy is ready."));
    core::PluginHost::getInstance().reset();
}
