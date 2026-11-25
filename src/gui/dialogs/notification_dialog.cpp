// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file notification_dialog.cpp
/// @brief The program's own window of notifications

#include "kalahari/gui/dialogs/notification_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/settings_dialog.h"

#include <QStringList>

#include <algorithm>

namespace kalahari::gui::dialogs {

NotificationDialog::NotificationDialog(QWidget* parent)
    : MessageDialog(Kind::Information, QString(), QString(), parent)
{
    setObjectName(QStringLiteral("notificationDialog"));
    // Beside the writing: the book window stays usable and keeps the keys
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_ShowWithoutActivating);
    //: The gear of the window of notifications. In Polish: Ustawienia powiadomień
    addSettingsButton(QString::fromLatin1(SettingsDialog::NOTIFICATIONS_PAGE),
                      tr("Notification Settings"));
}

void NotificationDialog::addNotification(Kind kind, const QString& title, const QString& text)
{
    if (text.isEmpty()) {
        return;
    }
    m_entries.append({kind, title, text});

    const QString first = m_entries.first().title;
    const bool oneTitle = std::all_of(m_entries.cbegin(), m_entries.cend(),
                                      [&first](const Entry& entry)
                                      { return entry.title == first; });
    const bool warning = std::any_of(m_entries.cbegin(), m_entries.cend(), [](const Entry& entry) {
        return entry.kind == Kind::Warning;
    });
    QStringList paragraphs;
    for (const Entry& entry : m_entries) {
        paragraphs.append(oneTitle || entry.title.isEmpty()
                              ? entry.text
                              //: A notification after its title, in the window of notifications of
                              //: several titles. In Polish: %1: %2
                              : tr("%1: %2").arg(entry.title, entry.text));
    }
    //: The title of the window of notifications of several titles. In Polish: Powiadomienia
    setMessage(warning ? Kind::Warning : Kind::Information,
               oneTitle && !first.isEmpty() ? first : tr("Notifications"),
               paragraphs.join(QStringLiteral("\n\n")));
    growForContent();
}

} // namespace kalahari::gui::dialogs
