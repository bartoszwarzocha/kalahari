// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_settings_page.cpp
/// @brief Settings > Timelines

#include <kalahari/timeline/timeline_settings_page.h>

#include <kalahari/timeline/timeline_look.h>

#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>

#include <utility>

namespace kalahari::timeline {

namespace {

/// A list of options, each with the value it stores
QComboBox* options(const QList<std::pair<QString, QString>>& items, const QString& name) {
    auto* box = new QComboBox();
    box->setObjectName(name);
    for (const auto& [text, value] : items) {
        box->addItem(text, value);
    }
    return box;
}

}  // namespace

TimelineSettingsPage::TimelineSettingsPage(QWidget* parent) : gui::SettingsPage(parent) {
    //: Settings > Timelines. In Polish: Nowe osie czasu
    QFormLayout* added = addGroup(tr("New Timelines"));
    addField(added,
             //: In Polish: Skala nowej osi:
             tr("Scale of a new timeline:"),
             options({//: In Polish: daty
                      {tr("dates"), QStringLiteral("dates")},
                      //: In Polish: dni opowieści (Dzień 1, Dzień 2...)
                      {tr("days of the story (Day 1, Day 2...)"), QStringLiteral("days")},
                      //: In Polish: sama kolejność
                      {tr("the order alone"), QStringLiteral("order")}},
                     QStringLiteral("timelinesNewScale")),
             TimelineSettings::NEW_SCALE);
    addField(added,
             //: In Polish: Pasma:
             tr("Lanes:"),
             options({//: In Polish: wątki
                      {tr("threads"), QStringLiteral("threads")},
                      //: In Polish: postacie
                      {tr("characters"), QStringLiteral("characters")},
                      //: In Polish: miejsca
                      {tr("places"), QStringLiteral("places")},
                      //: In Polish: jedno pasmo
                      {tr("one lane"), QStringLiteral("one")}},
                     QStringLiteral("timelinesNewLanes")),
             TimelineSettings::NEW_LANES);
    addField(added,
             //: In Polish: Wydarzenia:
             tr("Events:"),
             options({//: In Polish: karty
                      {tr("cards"), QStringLiteral("cards")},
                      //: In Polish: etykiety
                      {tr("labels"), QStringLiteral("labels")}},
                     QStringLiteral("timelinesNewLook")),
             TimelineSettings::NEW_LOOK);
    addCheckBox(added,
                //: In Polish: Skracaj długie przerwy między wydarzeniami
                tr("Shorten long gaps between the events"), TimelineSettings::NEW_COMPRESS);
    addField(added,
             //: In Polish: Oś czasu otwiera się:
             tr("A timeline opens on:"),
             options({//: In Polish: na osi
                      {tr("the axis"), QStringLiteral("axis")},
                      //: In Polish: na liście
                      {tr("the list"), QStringLiteral("list")},
                      //: In Polish: w widoku „Czas i książka”
                      {tr("the view “Time and the Book”"), QStringLiteral("book")},
                      //: In Polish: tam, gdzie ją zostawiono
                      {tr("the view it was left on"), QStringLiteral("last")}},
                     QStringLiteral("timelinesOpenOn")),
             TimelineSettings::OPEN_ON);

    //: In Polish: Karty wydarzeń
    QFormLayout* cards = addGroup(tr("Cards of the Events"));
    //: In Polish: Czas
    addCheckBox(cards, tr("The time"), TimelineSettings::CARD_TIME);
    //: In Polish: Rozdziały, które opowiadają wydarzenie
    addCheckBox(cards, tr("The chapters that tell the event"), TimelineSettings::CARD_CHAPTERS);
    //: In Polish: Postacie
    addCheckBox(cards, tr("The characters"), TimelineSettings::CARD_CHARACTERS);
    //: In Polish: Miejsce
    addCheckBox(cards, tr("The place"), TimelineSettings::CARD_PLACE);

    //: In Polish: Zapis czasu
    QFormLayout* writing = addGroup(tr("Writing the Time"));
    addField(writing,
             //: In Polish: Daty:
             tr("Dates:"),
             options({//: In Polish: słownie (16 kwietnia 1931)
                      {tr("in words (16 April 1931)"), QStringLiteral("words")},
                      //: In Polish: liczbami (16.04.1931)
                      {tr("in numbers (16.04.1931)"), QStringLiteral("numbers")},
                      //: In Polish: ISO (1931-04-16)
                      {tr("ISO (1931-04-16)"), QStringLiteral("iso")}},
                     QStringLiteral("timelinesDates")),
             TimelineSettings::DATES);
    addField(writing,
             //: In Polish: Pory dnia:
             tr("Parts of the day:"),
             options({//: In Polish: słownie (wieczorem)
                      {tr("in words (in the evening)"), QStringLiteral("words")},
                      //: In Polish: jako godzina (19:00)
                      {tr("as an hour (19:00)"), QStringLiteral("clock")}},
                     QStringLiteral("timelinesParts")),
             TimelineSettings::PARTS);
    addNote(writing,
            //: In Polish: Program czyta czas zapisany na każdy z tych sposobów; ustawienie mówi
            //: tylko, jak go wypisuje.
            tr("The program reads a time written in any of these ways; the setting says only how "
               "it writes it."));

    //: In Polish: Kolory
    QFormLayout* colors = addGroup(tr("Colors"));
    addNote(colors,
            //: In Polish: Każdy motyw ma swoje kolory: to kolory motywu w użyciu. Kolory wątków
            //: są wspólne z mapami myśli; kolory postaci i miejsc pochodzą z ich kart.
            tr("Each theme has its own colors: these are the colors of the theme in use. The "
               "colors of the threads are shared with the mind maps; the colors of the "
               "characters and the places come from their cards."));
    for (std::size_t i = 0; i < TimelineColorKeys::PALETTE.size(); ++i) {
        //: In Polish: Kolor wątku %1
        addThemeColor(colors, tr("Thread color %1").arg(i + 1), TimelineColorKeys::PALETTE.at(i));
    }
    //: In Polish: Pasmo
    addThemeColor(colors, tr("Lane"), TimelineColorKeys::LANE);
    //: In Polish: Co drugie pasmo
    addThemeColor(colors, tr("Every other lane"), TimelineColorKeys::LANE_ALTERNATE);
    //: In Polish: Podziałka osi
    addThemeColor(colors, tr("Ticks of the axis"), TimelineColorKeys::TICK);
    //: In Polish: Skrócona przerwa
    addThemeColor(colors, tr("A gap drawn short"), TimelineColorKeys::GAP);
    //: In Polish: Miejsce upuszczenia wydarzenia
    addThemeColor(colors, tr("Where a dragged event goes"), TimelineColorKeys::DROP);
    //: In Polish: Retrospekcja i nierozpoznany czas
    addThemeColor(colors, tr("Flashback and a time not recognized"),
                  TimelineColorKeys::FLASHBACK);
    addRestoreColorsButton(colors);

    pageLayout()->addStretch();
}

}  // namespace kalahari::timeline
