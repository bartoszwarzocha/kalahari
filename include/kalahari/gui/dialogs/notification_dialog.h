// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file notification_dialog.h
/// @brief The program's own window of notifications

#ifndef KALAHARI_GUI_DIALOGS_NOTIFICATION_DIALOG_H
#define KALAHARI_GUI_DIALOGS_NOTIFICATION_DIALOG_H

#include "kalahari/gui/dialogs/message_dialog.h"

#include <QList>
#include <QString>

namespace kalahari::gui::dialogs {

/// @brief The window of the notifications that Settings > Notifications shows in the
///        program's own window
///
/// A message window (MessageDialog) that does not stop the writing: it stays open over the
/// book window without holding it, and it does not take the keys from the text when it
/// appears. The notifications that come while it is open are added to it, each in a
/// paragraph of its own. Copy puts them all on the clipboard; the gear opens the settings of
/// the notifications. OK, Esc and the window's close button close it.
class NotificationDialog : public MessageDialog {
    Q_OBJECT

public:
    /// @brief Create the window, without a notification; it deletes itself once closed
    /// @param parent The book window
    explicit NotificationDialog(QWidget* parent = nullptr);

    /// @brief Add a notification: the title shows over the text, or, once notifications of
    ///        different titles are in the window, before its own paragraph
    /// @param kind Information or Warning (its icon in the heading)
    /// @param title What the notification is about ("Statistics")
    /// @param text The notification, plain text
    void addNotification(Kind kind, const QString& title, const QString& text);

    /// @brief The notifications in the window
    [[nodiscard]] int count() const { return static_cast<int>(m_entries.size()); }

private:
    /// @brief A notification in the window
    struct Entry {
        Kind kind = Kind::Information;
        QString title;
        QString text;
    };

    QList<Entry> m_entries;
};

} // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_NOTIFICATION_DIALOG_H
