// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file notifications_page.cpp
/// @brief Settings page: Notifications
///
/// Where the program's notifications show (NotificationCoordinator follows the setting at
/// each notification), a sample of them, and their colors: of the bar at the bottom of the
/// window and of the notifications in the Assistant panel, kept for each theme (they reach the
/// bar and the panel through the theme: ThemeManager follows them).

#include "kalahari/gui/notifications.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/settings/settings_pages.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace kalahari::gui {

NotificationsPage::NotificationsPage(QWidget* parent)
    : SettingsPage(parent)
{
    //: In Polish: Powiadomienia to to, co program mówi sam z siebie: osiągnięte cele, nowe
    //: odznaki, zbliżające się terminy, wiadomości dodatków, gotowe sprawdzanie gramatyki,
    //: materiał, którego nie udało się pobrać. Odpowiedzi na polecenia (np. pasek z
    //: przyciskiem „Cofnij” po poleceniu) zostają tam, gdzie są.
    addNote(pageLayout(),
            tr("Notifications are what the program says on its own: goals reached, new badges, "
               "deadlines coming, messages of plugins, grammar checking ready, a material that "
               "could not be downloaded. The answers to commands (e.g. a bar with Undo after a "
               "command) stay where they are."));

    const bool system = NotificationCoordinator::systemNotificationsAvailable();
    //: In Polish: Gdzie pokazywać
    QFormLayout* where = addGroup(tr("Where They Show"));
    const auto value = [](NotificationPlace place) {
        return QString::fromStdString(NotificationCoordinator::valueOfPlace(place));
    };
    m_placeList = addChoice(
        where, tr("Show notifications:"),
        {
            //: In Polish: Na pasku na dole okna
            {tr("In the bar at the bottom of the window"), value(NotificationPlace::Bar),
             //: In Polish: Pod tekstem, z przyciskiem „Kopiuj”. Nie przerywa pisania; zostaje,
             //: dopóki nie zostanie zamknięty albo nie przyjdzie następne powiadomienie.
             tr("Under the text, with Copy. It does not stop the writing; it stays until it is "
                "closed or the next notification comes.")},
            //: In Polish: W oknie programu
            {tr("In the program's window"), value(NotificationPlace::Window),
             //: In Polish: Okno nad oknem książki, z przyciskiem „Kopiuj”. Nie zabiera klawiszy
             //: tekstowi; kolejne powiadomienia dochodzą do niego, dopóki jest otwarte.
             tr("A window over the book window, with Copy. It does not take the keys from the "
                "text; the next notifications are added to it while it is open.")},
            //: In Polish: Jako powiadomienia systemu
            {tr("As notifications of the system"), value(NotificationPlace::System),
             system
                 //: In Polish: W rogu ekranu, jak powiadomienia innych programów, z ikoną
                 //: programu w zasobniku systemowym.
                 ? tr("In the corner of the screen, as the notifications of other programs, with "
                      "the program's icon in the system tray.")
                 //: In Polish: Ten system nie pokazuje tu powiadomień programów: pokaże je pasek
                 //: na dole okna.
                 : tr("This system shows no notifications of programs here: the bar at the "
                      "bottom of the window shows them.")},
            //: In Polish: W panelu „Asystent”
            {tr("In the Assistant panel"), value(NotificationPlace::Assistant),
             //: In Polish: Panel otwiera się z nimi (Widok > Panele > Asystent).
             tr("The panel opens with them (View > Panels > Assistant).")},
        },
        NotificationCoordinator::PLACE_SETTING);
    m_placeList->setObjectName(QStringLiteral("notificationsPlace"));

    // The chosen place at once, before Apply
    //: In Polish: Pokaż &przykład
    auto* sample = new QPushButton(tr("Show a &Sample"));
    sample->setObjectName(QStringLiteral("notificationsSample"));
    //: In Polish: Pokaż przykładowe powiadomienie w wybranym miejscu
    sample->setToolTip(tr("Show a sample notification in the place chosen"));
    auto* sampleRow = new QHBoxLayout();
    sampleRow->addWidget(sample);
    sampleRow->addStretch();
    where->addRow(QString(), sampleRow);
    connect(sample, &QPushButton::clicked, this, [this]() {
        NotificationCoordinator::showSampleFront(NotificationCoordinator::placeOfValue(
            m_placeList->currentData().toString().toStdString()));
    });

    //: In Polish: Niezależnie od wyboru panel „Asystent” przechowuje powiadomienia tej sesji
    //: z godziną i pozwala je skopiować. Które wiadomości statystyk się pokazują – wybiera się
    //: na stronie Statystyki i cele > Powiadomienia.
    addNote(where, tr("Whatever the choice, the Assistant panel keeps the notifications of this "
                      "session with their time, and copies them. Which messages of the "
                      "statistics show is chosen on the page Statistics and Goals > "
                      "Notifications."));

    // One background and one border, kept for each theme
    QFormLayout* colors = addGroup(tr("Colors"));
    //: In Polish: Kolory paska powiadomień na dole okna i powiadomień w panelu „Asystent”.
    //: Każdy motyw ma własne kolory: to kolory bieżącego motywu.
    addNote(colors, tr("The colors of the bar of notifications at the bottom of the window and of "
                       "the notifications in the Assistant panel. Each theme has its own colors: "
                       "these are the colors of the current theme."));
    //: In Polish: Tło
    addThemeColor(colors, tr("Background"), NotificationCoordinator::BACKGROUND_COLOR_KEY);
    //: In Polish: Obramowanie
    addThemeColor(colors, tr("Border"), NotificationCoordinator::BORDER_COLOR_KEY);
    addRestoreColorsButton(colors);

    pageLayout()->addStretch();
}

} // namespace kalahari::gui
