// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file notifications.h
/// @brief The notifications of the program: what it tells the writer on its own, in the place
///        Settings > Notifications chooses

#ifndef KALAHARI_GUI_NOTIFICATIONS_H
#define KALAHARI_GUI_NOTIFICATIONS_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <string>

class QDockWidget;

namespace kalahari::gui {

class AssistantPanel;
class DockCoordinator;
class InfoBar;

namespace dialogs {
class NotificationDialog;
} // namespace dialogs

/// @brief What the program tells the writer on its own, not as the answer to a command: a
///        goal reached, a deadline coming, a message of a plugin, grammar checking ready, a
///        material that could not be downloaded
struct Notification {
    /// @brief What it is about; it chooses the icon
    enum class Kind { Information, Warning };

    QString title;  ///< What it is about ("Statistics and Goals", the title a plugin gives)
    QString text;   ///< The notification, plain text
    Kind kind = Kind::Information;
    /// The bar at the bottom of the window, which has no heading, shows "title: text": the
    /// text is not clear without its title (a message of a plugin)
    bool titleInBar = false;

    bool operator==(const Notification& other) const = default;
};

/// @brief Where the notifications show (Settings > Notifications)
enum class NotificationPlace {
    Bar,       ///< The bar at the bottom of the window, under the text, with Copy
    Window,    ///< The program's own window, with Copy, which does not take the keys
    System,    ///< The notifications of the system, in the corner of the screen
    Assistant  ///< The Assistant panel, which opens with them
};

/// @brief The notifications of a book window
///
/// The parts of the window give it what the program tells the writer on its own; it shows
/// them where Settings > Notifications says (PLACE_SETTING), the notifications of one moment
/// (of one turn of the event loop) together. Without notifications of the system (no system
/// tray), and without the panel or the window where they would show, the bar shows them.
///
/// Whatever the place, the Assistant panel keeps the notifications of the session with their
/// time, and copies them (F6 opens it). The bar, the window and the panel have the gear of
/// the settings of the notifications; the colors of the bar and of the notifications in the
/// panel are the theme's (BACKGROUND_COLOR_KEY, BORDER_COLOR_KEY), changed in Settings.
///
/// What answers a command the writer gave (Undo after a deletion, a file saved) does not go
/// here: it stays where the command shows it.
class NotificationCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The setting of the place: "bar", "window", "system" or "assistant"
    static constexpr const char* PLACE_SETTING = "notifications.place";
    /// @brief The theme's background of the bar and of the notifications in the Assistant
    ///        panel (the "editor" section of the theme)
    static constexpr const char* BACKGROUND_COLOR_KEY = "notificationBackground";
    /// @brief The theme's border of the bar and of the notifications in the Assistant panel
    static constexpr const char* BORDER_COLOR_KEY = "notificationBorder";

    /// @brief Shows the notifications of the window of @p docks
    /// @param docks The window's panels: the bar goes at the bottom of its middle, and its
    ///        Assistant panel keeps them; nullptr: none (only the system or nothing shows them)
    /// @param openSettings Opens the Settings dialog at a page (the gears)
    /// @param parent Parent object
    NotificationCoordinator(DockCoordinator* docks,
                            std::function<void(const QString&)> openSettings,
                            QObject* parent = nullptr);
    ~NotificationCoordinator() override;

    NotificationCoordinator(const NotificationCoordinator&) = delete;
    NotificationCoordinator& operator=(const NotificationCoordinator&) = delete;
    NotificationCoordinator(NotificationCoordinator&&) = delete;
    NotificationCoordinator& operator=(NotificationCoordinator&&) = delete;

    /// @brief Show a notification, with the others of this moment, once the event loop goes on
    ///
    /// The same notification twice in one moment shows once.
    void notify(const Notification& notification);

    /// @brief Show a notification of the whole program (a plugin, grammar checking) in the
    ///        book window in front; the same notification of several windows at one moment
    ///        shows once
    /// @return false without a book window: the caller shows it its own way
    static bool notifyFront(const Notification& notification);

    /// @brief Show a sample notification in @p where in the book window in front, at once, as
    ///        the notifications would show there (the button of the settings page)
    /// @return false without a book window
    static bool showSampleFront(NotificationPlace where);

    /// @brief How notifyFront() finds the window in front; the program sets it once, with its
    ///        windows (without it: the window made last)
    static void setFrontFinder(std::function<NotificationCoordinator*()> finder);

    /// @brief The place the settings choose
    [[nodiscard]] static NotificationPlace chosenPlace();

    /// @brief The place the notifications show in now: the chosen one, or the bar where the
    ///        system has no notifications
    [[nodiscard]] static NotificationPlace place();

    /// @brief The place of a value of PLACE_SETTING; the bar for a value it does not know
    [[nodiscard]] static NotificationPlace placeOfValue(const std::string& value);

    /// @brief The value of PLACE_SETTING of a place
    [[nodiscard]] static std::string valueOfPlace(NotificationPlace place);

    /// @brief Shows a notification of the system: title, text and kind; true if it showed
    using SystemNotifier =
        std::function<bool(const QString& title, const QString& text, Notification::Kind kind)>;

    /// @brief Show the notifications of the system with @p notifier instead of the system
    ///        tray (the tests); an empty one: the system tray again
    static void setSystemNotifier(SystemNotifier notifier);

    /// @brief Whether the system shows notifications (a system tray with messages)
    [[nodiscard]] static bool systemNotificationsAvailable();

    /// @brief The bar at the bottom of the window (nullptr without the window's middle)
    [[nodiscard]] InfoBar* bar() const;

    /// @brief The program's window of notifications while it is open, else nullptr
    [[nodiscard]] dialogs::NotificationDialog* notificationWindow() const;

private:
    /// @brief Show the notifications of the moment where the settings say
    void present();

    /// @brief Show notifications in @p where, or in the bar where they cannot show there
    void showIn(const QList<Notification>& notifications, NotificationPlace where);

    /// @brief Show the notifications in the bar at the bottom of the window
    /// @return false without the bar
    bool showInBar(const QList<Notification>& notifications);

    /// @brief Add the notifications to the program's window, opening it when it is closed
    /// @return false without a book window
    bool showInWindow(const QList<Notification>& notifications);

    /// @brief Show the notifications as one notification of the system
    /// @return false when the system shows none
    static bool showInSystem(const QList<Notification>& notifications);

    /// @brief Open the Assistant panel, which has them (present() added them)
    /// @return false without the panel
    bool showInAssistant();

    /// @brief The book window the notifications are in (nullptr without one)
    [[nodiscard]] QWidget* bookWindow() const;

    /// @brief The place changed: the icon in the system tray goes with the notifications of
    ///        the system
    static void onPlaceChanged();

    QPointer<InfoBar> m_bar;
    QPointer<AssistantPanel> m_assistant;
    QPointer<QDockWidget> m_assistantDock;
    QPointer<dialogs::NotificationDialog> m_window;
    std::function<void(const QString&)> m_openSettings;
    QList<Notification> m_pending;  ///< The notifications of this moment, not shown yet
    bool m_presentQueued = false;
    int m_settingsListener = -1;
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_NOTIFICATIONS_H
