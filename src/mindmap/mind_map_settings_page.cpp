// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_settings_page.cpp
/// @brief Settings > Mind Maps

#include <kalahari/mindmap/mind_map_settings_page.h>

#include <kalahari/mindmap/mind_map_look.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <array>
#include <utility>

namespace kalahari::mindmap {

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

MindMapSettingsPage::MindMapSettingsPage(QWidget* parent) : gui::SettingsPage(parent) {
    //: Settings > Mind Maps. In Polish: Nowe mapy
    QFormLayout* added = addGroup(tr("New Maps"));
    addField(added,
             //: In Polish: Układ nowej mapy:
             tr("Layout of a new map:"),
             options({{tr("Both sides"), QStringLiteral("bothSides")},
                      {tr("To the right"), QStringLiteral("right")},
                      {tr("Down"), QStringLiteral("down")},
                      {tr("Free"), QStringLiteral("free")}},
                     QStringLiteral("mindMapsNewLayout")),
             MindMapSettings::NEW_LAYOUT);
    addField(added,
             //: In Polish: Mapa otwiera się:
             tr("A map opens on:"),
             options({//: In Polish: na mapie
                      {tr("the map"), QStringLiteral("map")},
                      //: In Polish: na liście
                      {tr("the list"), QStringLiteral("list")},
                      //: In Polish: tam, gdzie ją zostawiono
                      {tr("the view it was left on"), QStringLiteral("last")}},
                     QStringLiteral("mindMapsOpenOn")),
             MindMapSettings::OPEN_ON);

    //: In Polish: Wygląd map
    QFormLayout* look = addGroup(tr("Look of the Maps"));
    addField(look,
             //: In Polish: Węzły:
             tr("Nodes:"),
             options({//: In Polish: zaokrąglone
                      {tr("rounded"), QStringLiteral("rounded")},
                      //: In Polish: prostokąty
                      {tr("rectangles"), QStringLiteral("rectangle")},
                      //: In Polish: owale
                      {tr("ovals"), QStringLiteral("oval")},
                      //: In Polish: tekst nad linią
                      {tr("text over a line"), QStringLiteral("underline")}},
                     QStringLiteral("mindMapsNodes")),
             MindMapSettings::SHAPE);
    addField(look,
             //: In Polish: Linie:
             tr("Lines:"),
             options({//: In Polish: łuki
                      {tr("curves"), QStringLiteral("curved")},
                      //: In Polish: proste
                      {tr("straight"), QStringLiteral("straight")},
                      //: In Polish: łamane pod kątem prostym
                      {tr("with right angles"), QStringLiteral("elbow")}},
                     QStringLiteral("mindMapsLines")),
             MindMapSettings::LINES);
    addCheckBox(look,
                //: In Polish: Linie cieńsze w głębi gałęzi
                tr("Thinner lines deeper in the branches"), MindMapSettings::THINNER);
    addField(look,
             //: In Polish: Kolory gałęzi:
             tr("Colors of the branches:"),
             options({//: In Polish: każda gałąź główna swój kolor
                      {tr("each main branch its own"), QStringLiteral("each")},
                      //: In Polish: jeden kolor
                      {tr("one color"), QStringLiteral("one")},
                      //: In Polish: bez kolorów
                      {tr("no colors"), QStringLiteral("none")}},
                     QStringLiteral("mindMapsBranchColors")),
             MindMapSettings::COLORS);
    addField(look,
             //: In Polish: Tło:
             tr("Background:"),
             options({//: In Polish: gładkie
                      {tr("plain"), QStringLiteral("theme")},
                      //: In Polish: w kropki
                      {tr("dots"), QStringLiteral("dots")},
                      //: In Polish: w kratkę
                      {tr("grid"), QStringLiteral("grid")},
                      //: In Polish: własny kolor
                      {tr("own color"), QStringLiteral("own")}},
                     QStringLiteral("mindMapsBackground")),
             MindMapSettings::BACKGROUND);
    addCheckBox(look,
                //: In Polish: Czcionka edytora (zamiast czcionki programu)
                tr("The font of the editor (in place of the font of the program)"),
                MindMapSettings::EDITOR_FONT);
    addField(look,
             //: In Polish: Wielkość tekstu:
             tr("Size of the text:"),
             options({//: In Polish: mała
                      {tr("small"), QStringLiteral("small")},
                      //: In Polish: średnia
                      {tr("medium"), QStringLiteral("medium")},
                      //: In Polish: duża
                      {tr("large"), QStringLiteral("large")}},
                     QStringLiteral("mindMapsTextSize")),
             MindMapSettings::TEXT_SIZE);

    //: In Polish: Wklejanie
    QFormLayout* pasting = addGroup(tr("Pasting"));
    QCheckBox* tree = addCheckBox(
        pasting,
        //: In Polish: Wklejony tekst z wcięciami tworzy gałęzie (wiersz to węzeł)
        tr("Pasted text with indents makes branches (a line is a node)"),
        MindMapSettings::PASTE_TREE);
    alignWithText(addNote(pasting,
                          //: In Polish: Bez tego cały wklejony tekst wchodzi do jednego węzła.
                          tr("Without it, all the text pasted goes into one node.")),
                  tree);

    //: In Polish: Kolory
    QFormLayout* colors = addGroup(tr("Colors"));
    addNote(colors,
            //: In Polish: Każdy motyw ma swoje kolory: to kolory motywu w użyciu. Kolory gałęzi
            //: są wspólne z osiami czasu.
            tr("Each theme has its own colors: these are the colors of the theme in use. The "
               "colors of the branches are shared with the timelines."));
    for (std::size_t i = 0; i < MindMapColorKeys::PALETTE.size(); ++i) {
        //: In Polish: Kolor gałęzi %1
        addThemeColor(colors, tr("Branch color %1").arg(i + 1), MindMapColorKeys::PALETTE.at(i));
    }
    //: In Polish: Tło
    addThemeColor(colors, tr("Background"), MindMapColorKeys::BACKGROUND);
    //: In Polish: Kropki i kratka
    addThemeColor(colors, tr("Dots and grid"), MindMapColorKeys::DOTS);
    //: In Polish: Własny kolor tła
    addThemeColor(colors, tr("Own background"), MindMapColorKeys::OWN_BACKGROUND);
    //: In Polish: Temat główny
    addThemeColor(colors, tr("Main topic"), MindMapColorKeys::ROOT);
    //: In Polish: Tekst tematu głównego
    addThemeColor(colors, tr("Text of the main topic"), MindMapColorKeys::ROOT_TEXT);
    //: In Polish: Węzły
    addThemeColor(colors, tr("Nodes"), MindMapColorKeys::NODE);
    //: In Polish: Połączenia
    addThemeColor(colors, tr("Connections"), MindMapColorKeys::CONNECTION);
    //: In Polish: Linie bez kolorów
    addThemeColor(colors, tr("Lines without colors"), MindMapColorKeys::PLAIN);
    //: In Polish: Miejsce upuszczenia węzła
    addThemeColor(colors, tr("Where a dragged node goes"), MindMapColorKeys::DROP);
    //: In Polish: Znaki w znacznikach
    addThemeColor(colors, tr("Signs in the markers"), MindMapColorKeys::MARKER_TEXT);
    const std::array<QString, 5> markers{
        //: In Polish: Znacznik: pytanie
        tr("Marker: question"),
        //: In Polish: Znacznik: ważne
        tr("Marker: important"),
        //: In Polish: Znacznik: pomysł
        tr("Marker: idea"),
        //: In Polish: Znacznik: zrobione
        tr("Marker: done"),
        //: In Polish: Znacznik: do sprawdzenia
        tr("Marker: to check")};
    for (std::size_t i = 0; i < markers.size(); ++i) {
        addThemeColor(colors, markers.at(i), MindMapColorKeys::MARKERS.at(i));
    }
    addRestoreColorsButton(colors);

    pageLayout()->addStretch();
}

}  // namespace kalahari::mindmap
