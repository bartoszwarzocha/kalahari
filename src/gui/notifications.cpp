// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file notifications.cpp
/// @brief The notifications of the program, in the place Settings > Notifications chooses

#include "kalahari/gui/notifications.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/resource_paths.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/notification_dialog.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/panels/notifications_panel.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/info_bar.h"

#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QIcon>
#include <QPixmap>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>
#include <utility>

namespace kalahari::gui {

namespace {

/// How long a notification of the system stays in view, where the system lets the program
/// say it (ms)
constexpr int SYSTEM_MESSAGE_MS = 10000;

/// @brief The values of PLACE_SETTING, in the order of NotificationPlace
constexpr std::array<std::pair<NotificationPlace, const char*>, 4> PLACE_VALUES = {{
    {NotificationPlace::Bar, "bar"},
    {NotificationPlace::Window, "window"},
    {NotificationPlace::System, "system"},
    {NotificationPlace::Panel, "panel"},
}};

/// @brief The book windows' notifications, the last made last
QList<NotificationCoordinator*>& coordinators()
{
    static QList<NotificationCoordinator*> list;
    return list;
}

std::function<NotificationCoordinator*()>& frontFinder()
{
    static std::function<NotificationCoordinator*()> finder;
    return finder;
}

NotificationCoordinator::SystemNotifier& systemNotifier()
{
    static NotificationCoordinator::SystemNotifier notifier;
    return notifier;
}

/// @brief The icon of the program in the system tray, made the first time the system shows a
///        notification
QPointer<QSystemTrayIcon>& trayIcon()
{
    static QPointer<QSystemTrayIcon> icon;
    return icon;
}

/// @brief The window in front, or the one made last
NotificationCoordinator* frontCoordinator()
{
    if (frontFinder()) {
        if (NotificationCoordinator* front = frontFinder()()) {
            return front;
        }
    }
    return coordinators().isEmpty() ? nullptr : coordinators().last();
}

/// @brief Notifications of one moment as one: a title and a text
struct Combined {
    QString title;
    QString text;
    Notification::Kind kind = Notification::Kind::Information;
};

/// @brief Notifications of one title: that title, and their texts one after another; of
///        several titles: "Notifications", and each text after its title, in a line of its own
Combined combine(const QList<Notification>& notifications)
{
    Combined combined;
    if (notifications.isEmpty()) {
        return combined;
    }
    const QString first = notifications.first().title;
    const bool oneTitle = std::all_of(notifications.cbegin(), notifications.cend(),
                                      [&first](const Notification& notification)
                                      { return notification.title == first; });
    QStringList parts;
    for (const Notification& notification : notifications) {
        parts.append(oneTitle || notification.title.isEmpty()
                         ? notification.text
                         //: A notification after its title. In Polish: %1: %2
                         : NotificationCoordinator::tr("%1: %2").arg(notification.title,
                                                                    notification.text));
        if (notification.kind == Notification::Kind::Warning) {
            combined.kind = Notification::Kind::Warning;
        }
    }
    combined.title = oneTitle && !first.isEmpty()
                         ? first
                         //: The title of notifications of several titles. In Polish: Powiadomienia
                         : NotificationCoordinator::tr("Notifications");
    combined.text = parts.join(oneTitle ? QStringLiteral(" ") : QStringLiteral("\n"));
    return combined;
}

/// @brief The icon of the program for the system tray
QIcon programIcon()
{
    QIcon icon = QApplication::windowIcon();
    if (icon.isNull()) {
        // The logo, as the Dashboard shows it
        icon = QIcon(QPixmap(core::ResourcePaths::getInstance().getResourcesDir() +
                             QStringLiteral("/images/app_logo.png")));
    }
    return icon;
}

/// @brief Bring the book window in front to the front (a click on a notification of the
///        system or on the program's icon in the tray)
void raiseFrontWindow()
{
    NotificationCoordinator* front = frontCoordinator();
    QWidget* window = front != nullptr && front->bar() != nullptr ? front->bar()->window()
                                                                  : nullptr;
    if (window == nullptr) {
        return;
    }
    if (window->isMinimized()) {
        window->showNormal();
    }
    window->raise();
    window->activateWindow();
}

} // anonymous namespace

NotificationCoordinator::NotificationCoordinator(DockCoordinator* docks,
                                                 std::function<void(const QString&)> openSettings,
                                                 QObject* parent)
    : QObject(parent)
    , m_openSettings(std::move(openSettings))
{
    coordinators().append(this);
    const QString page = QString::fromLatin1(SettingsDialog::NOTIFICATIONS_PAGE);

    // The bar at the bottom of the middle, under the tabs of the texts
    QWidget* middle = docks != nullptr ? docks->centralWrapper() : nullptr;
    if (auto* layout = middle != nullptr ? qobject_cast<QVBoxLayout*>(middle->layout()) : nullptr) {
        m_bar = new InfoBar(middle);
        m_bar->setObjectName(QStringLiteral("notificationBar"));
        m_bar->setColorKeys(BACKGROUND_COLOR_KEY, BORDER_COLOR_KEY);
        //: The button of a notification at the bottom of the window. In Polish: Kopiuj
        m_bar->setActionText(tr("Copy"));
        //: In Polish: Kopiuj tę wiadomość
        m_bar->setActionToolTip(tr("Copy this message"));
        //: The gear of the bar of notifications. In Polish: Ustawienia powiadomień
        m_bar->showSettingsButton(tr("Notification Settings"));
        m_bar->hide();
        layout->addWidget(m_bar);
        connect(m_bar, &InfoBar::actionClicked, this, [this]() {
            QApplication::clipboard()->setText(m_bar->message());
        });
        connect(m_bar, &InfoBar::dismissed, m_bar, &QWidget::hide);
        connect(m_bar, &InfoBar::settingsClicked, this, [this, page]() {
            if (m_openSettings) {
                m_openSettings(page);
            }
        });
    }

    // The Notifications panel keeps them all
    if (docks != nullptr) {
        m_panel = docks->notificationsPanel();
        m_panelDock = docks->notificationsDock();
    }

    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key == PLACE_SETTING) {
                QMetaObject::invokeMethod(this, []() { onPlaceChanged(); }, Qt::QueuedConnection);
            }
        });
}

NotificationCoordinator::~NotificationCoordinator()
{
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    // The bar in the middle and the window of notifications are the book window's: they go
    // with it
    coordinators().removeAll(this);
}

void NotificationCoordinator::notify(const Notification& notification)
{
    if (notification.text.isEmpty() || m_pending.contains(notification)) {
        return;
    }
    m_pending.append(notification);
    if (!m_presentQueued) {
        m_presentQueued = true;
        QMetaObject::invokeMethod(this, &NotificationCoordinator::present, Qt::QueuedConnection);
    }
}

bool NotificationCoordinator::notifyFront(const Notification& notification)
{
    NotificationCoordinator* front = frontCoordinator();
    if (front == nullptr) {
        return false;
    }
    front->notify(notification);
    return true;
}

bool NotificationCoordinator::showSampleFront(NotificationPlace where)
{
    NotificationCoordinator* front = frontCoordinator();
    if (front == nullptr) {
        return false;
    }
    //: The title of the sample notification of Settings > Notifications. In Polish:
    //: Powiadomienia
    const Notification sample{tr("Notifications"),
                              //: The sample notification of Settings > Notifications. In Polish:
                              //: Tak wyglądają powiadomienia programu.
                              tr("This is how the program's notifications look.")};
    if (where == NotificationPlace::Panel && front->m_panel != nullptr) {
        front->m_panel->addNotification(sample);
    }
    if (where == NotificationPlace::System && !systemNotificationsAvailable()) {
        where = NotificationPlace::Bar;
    }
    front->showIn({sample}, where);
    return true;
}

void NotificationCoordinator::setFrontFinder(std::function<NotificationCoordinator*()> finder)
{
    frontFinder() = std::move(finder);
}

NotificationPlace NotificationCoordinator::chosenPlace()
{
    return placeOfValue(
        core::SettingsManager::getInstance().get<std::string>(PLACE_SETTING, "bar"));
}

NotificationPlace NotificationCoordinator::place()
{
    const NotificationPlace chosen = chosenPlace();
    if (chosen == NotificationPlace::System && !systemNotificationsAvailable()) {
        return NotificationPlace::Bar;
    }
    return chosen;
}

NotificationPlace NotificationCoordinator::placeOfValue(const std::string& value)
{
    for (const auto& [place, name] : PLACE_VALUES) {
        if (value == name) {
            return place;
        }
    }
    return NotificationPlace::Bar;
}

std::string NotificationCoordinator::valueOfPlace(NotificationPlace place)
{
    for (const auto& [known, name] : PLACE_VALUES) {
        if (known == place) {
            return name;
        }
    }
    return PLACE_VALUES.front().second;
}

void NotificationCoordinator::setSystemNotifier(SystemNotifier notifier)
{
    systemNotifier() = std::move(notifier);
}

bool NotificationCoordinator::systemNotificationsAvailable()
{
    if (systemNotifier()) {
        return true;
    }
    return QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages();
}

InfoBar* NotificationCoordinator::bar() const
{
    return m_bar;
}

dialogs::NotificationDialog* NotificationCoordinator::notificationWindow() const
{
    return m_window;
}

void NotificationCoordinator::present()
{
    m_presentQueued = false;
    if (m_pending.isEmpty()) {
        return;
    }
    const QList<Notification> notifications = std::exchange(m_pending, {});

    auto& logger = core::Logger::getInstance();
    for (const Notification& notification : notifications) {
        logger.info("Notification: {}: {}", notification.title.toStdString(),
                    notification.text.toStdString());
        if (m_panel != nullptr) {
            m_panel->addNotification(notification);
        }
    }

    showIn(notifications, place());
}

void NotificationCoordinator::showIn(const QList<Notification>& notifications,
                                     NotificationPlace where)
{
    bool shown = false;
    switch (where) {
    case NotificationPlace::Window:
        shown = showInWindow(notifications);
        break;
    case NotificationPlace::System:
        shown = showInSystem(notifications);
        break;
    case NotificationPlace::Panel:
        shown = showInPanel();
        break;
    case NotificationPlace::Bar:
        break;
    }
    if (!shown) {
        showInBar(notifications);
    }
}

bool NotificationCoordinator::showInBar(const QList<Notification>& notifications)
{
    if (m_bar == nullptr) {
        return false;
    }
    QStringList parts;
    bool warning = false;
    for (const Notification& notification : notifications) {
        parts.append(notification.titleInBar && !notification.title.isEmpty()
                         ? tr("%1: %2").arg(notification.title, notification.text)
                         : notification.text);
        warning = warning || notification.kind == Notification::Kind::Warning;
    }
    // The notifications of one moment together, in the order they came
    m_bar->setMessage(parts.join(QLatin1Char(' ')));
    m_bar->setIconId(warning ? QStringLiteral("common.warning") : QStringLiteral("help.about"));
    m_bar->show();
    return true;
}

bool NotificationCoordinator::showInWindow(const QList<Notification>& notifications)
{
    QWidget* window = bookWindow();
    if (window == nullptr) {
        return false;
    }
    if (m_window == nullptr) {
        m_window = new dialogs::NotificationDialog(window);
    }
    for (const Notification& notification : notifications) {
        m_window->addNotification(notification.kind == Notification::Kind::Warning
                                      ? dialogs::MessageDialog::Kind::Warning
                                      : dialogs::MessageDialog::Kind::Information,
                                  notification.title, notification.text);
    }
    if (m_window->isHidden()) {
        m_window->show();
    }
    m_window->raise();
    return true;
}

bool NotificationCoordinator::showInSystem(const QList<Notification>& notifications)
{
    const Combined combined = combine(notifications);
    if (systemNotifier()) {
        return systemNotifier()(combined.title, combined.text, combined.kind);
    }
    if (!systemNotificationsAvailable()) {
        return false;
    }
    // The system shows a notification only of a program with an icon in its tray: the icon
    // stays while the notifications are the system's
    QPointer<QSystemTrayIcon>& icon = trayIcon();
    if (icon == nullptr) {
        icon = new QSystemTrayIcon(programIcon(), qApp);
        icon->setToolTip(QCoreApplication::applicationName());
        QObject::connect(icon, &QSystemTrayIcon::messageClicked, icon, &raiseFrontWindow);
        QObject::connect(icon, &QSystemTrayIcon::activated, icon,
                         [](QSystemTrayIcon::ActivationReason reason) {
                             if (reason == QSystemTrayIcon::Trigger ||
                                 reason == QSystemTrayIcon::DoubleClick) {
                                 raiseFrontWindow();
                             }
                         });
    }
    icon->show();
    icon->showMessage(combined.title, combined.text,
                      combined.kind == Notification::Kind::Warning ? QSystemTrayIcon::Warning
                                                                   : QSystemTrayIcon::Information,
                      SYSTEM_MESSAGE_MS);
    return true;
}

bool NotificationCoordinator::showInPanel()
{
    if (m_panel == nullptr || m_panelDock == nullptr) {
        return false;
    }
    m_panelDock->show();
    m_panelDock->raise();
    m_panel->showLatest();
    return true;
}

QWidget* NotificationCoordinator::bookWindow() const
{
    if (m_bar != nullptr) {
        return m_bar->window();
    }
    return m_panelDock != nullptr ? m_panelDock->window() : nullptr;
}

void NotificationCoordinator::onPlaceChanged()
{
    if (chosenPlace() != NotificationPlace::System && trayIcon() != nullptr) {
        trayIcon()->hide();
    }
}

} // namespace kalahari::gui
