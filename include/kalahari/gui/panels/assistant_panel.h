// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file assistant_panel.h
/// @brief The Assistant panel: the notifications of the session (the assistant itself comes
///        in a later phase)

#ifndef KALAHARI_GUI_PANELS_ASSISTANT_PANEL_H
#define KALAHARI_GUI_PANELS_ASSISTANT_PANEL_H

#include "kalahari/gui/notifications.h"

#include <QDateTime>
#include <QString>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

namespace kalahari::gui {

/// @brief The Assistant panel: the notifications of the program in this session
///
/// +--------------------------------------+
/// | ( 14:05  Statistics                ) |
/// | ( Daily goal reached: 1,000 of ... ) |
/// | ( 14:20  Materials                 ) |
/// | ( Nothing was downloaded for ...   ) |
/// |                       [Copy] [Clear] |
/// +--------------------------------------+
///
/// Each notification in a bubble of the theme's colors of the notifications, with its time
/// and title, the latest at the bottom; the panel keeps the last MAX_NOTIFICATIONS. Copy
/// (Ctrl+C in the list) puts the chosen one on the clipboard, the latest without a choice.
/// The panel keeps them wherever Settings > Notifications shows them; there the panel can be
/// their place, and it opens with them.
class AssistantPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief The notifications the panel keeps
    static constexpr int MAX_NOTIFICATIONS = 200;

    /// @brief Constructor
    /// @param parent Parent widget
    explicit AssistantPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~AssistantPanel() override = default;

    /// @brief Add a notification at the bottom of the list
    /// @param notification The notification
    /// @param time When it came
    void addNotification(const Notification& notification,
                         const QDateTime& time = QDateTime::currentDateTime());

    /// @brief The notifications in the panel
    [[nodiscard]] int notificationCount() const;

    /// @brief The text Copy puts on the clipboard for the notification in row @p row: its
    ///        title and its text
    [[nodiscard]] QString notificationText(int row) const;

    /// @brief Scroll to the latest notification
    void showLatest();

    /// @brief Copy: the chosen notification, or the latest without a choice
    void copyNotification();

    /// @brief Clear: take all the notifications away
    void clearNotifications();

    /// @brief The list of the notifications
    [[nodiscard]] QListWidget* notificationList() const { return m_list; }

private:
    /// @brief The list, or the sentence of an empty panel; the buttons only with notifications
    void updateState();

    QLabel* m_emptyLabel = nullptr;
    QListWidget* m_list = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_clearButton = nullptr;
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_ASSISTANT_PANEL_H
