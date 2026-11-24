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
                      {tr("Down"), QStringLiteral("down")}},
                     QStringLiteral("mindMapsNewLayout")),
             MindMapSettings::NEW_LAYOUT);
    addField(added,
             //: In Polish: Mapa otwiera się:
             tr("A map opens on:"),
             options(
                 {//: In Polish: na mapie
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
             options(
                 {//: In Polish: zaokrąglone
                  {tr("rounded"), QStringLiteral("rounded")},
                  //: In Polish: prostokąty
                  {tr("rectangles"), QStringLiteral("rectangle")}},
                 QStringLiteral("mindMapsNodes")),
             MindMapSettings::SHAPE);
    addField(look,
             //: In Polish: Linie:
             tr("Lines:"),
             options(
                 {//: In Polish: łuki
                  {tr("curves"), QStringLiteral("curved")},
                  //: In Polish: proste
                  {tr("straight"), QStringLiteral("straight")},
                  //: In Polish: łamane pod kątem prostym
                  {tr("with right angles"), QStringLiteral("elbow")}},
                 QStringLiteral("mindMapsLines")),
             MindMapSettings::LINES);
    addField(look,
             //: In Polish: Kolory gałęzi:
             tr("Colors of the branches:"),
             options(
                 {//: In Polish: każda gałąź główna swój kolor
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
             options(
                 {//: In Polish: gładkie
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
             options(
                 {//: In Polish: mała
                  {tr("small"), QStringLiteral("small")},
                  //: In Polish: średnia
                  {tr("medium"), QStringLiteral("medium")},
                  //: In Polish: duża
                  {tr("large"), QStringLiteral("large")}},
                 QStringLiteral("mindMapsTextSize")),
             MindMapSettings::TEXT_SIZE);

    //: Settings > Mind Maps: the nodes. In Polish: Węzły
    QFormLayout* nodes = addGroup(tr("Nodes"));
    addCheckBox(nodes,
                //: In Polish: Węzły kart pokazują główne portrety kart
                tr("The nodes of the cards show the main portraits of the cards"),
                MindMapSettings::PORTRAITS);
    addCheckBox(nodes,
                //: In Polish: Nowy węzeł pokazuje tylko nagłówek (rozwija go H)
                tr("A new node shows only its header (H unfolds it)"), MindMapSettings::NEW_FOLDED);
    QCheckBox* ask = addCheckBox(nodes,
                                 //: In Polish: Pierwsza relacja karty proponuje mapę relacji
                                 tr("The first relation of a card offers a relation map"),
                                 MindMapSettings::ASK_RELATION_MAP);
    alignWithText(addNote(nodes,
                          //: In Polish: Mapa relacji pokazuje karty Warsztatu z relacjami, które
                          //: mają. Nową tworzy Książka > Nowa mapa relacji…
                          tr("A relation map shows the cards of the Workshop with the relations "
                             "they have. Book > New Relation Map… makes a new one.")),
                  ask);

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
    //: In Polish: Kropki
    addThemeColor(colors, tr("Dots"), MindMapColorKeys::DOTS);
    //: In Polish: Kratka
    addThemeColor(colors, tr("Grid"), MindMapColorKeys::GRID);
    //: In Polish: Co piąta linia kratki
    addThemeColor(colors, tr("Every fifth line of the grid"), MindMapColorKeys::GRID_MAJOR);
    //: In Polish: Własny kolor tła
    addThemeColor(colors, tr("Own background"), MindMapColorKeys::OWN_BACKGROUND);
    //: In Polish: Węzły
    addThemeColor(colors, tr("Nodes"), MindMapColorKeys::NODE);
    //: In Polish: Krawędź węzła
    addThemeColor(colors, tr("Edge of a node"), MindMapColorKeys::NODE_LINE);
    //: In Polish: Tekst nagłówka
    addThemeColor(colors, tr("Text of the header"), MindMapColorKeys::HEADER_TEXT);
    //: In Polish: Połączenia
    addThemeColor(colors, tr("Connections"), MindMapColorKeys::CONNECTION);
    //: In Polish: Ramki
    addThemeColor(colors, tr("Frames"), MindMapColorKeys::FRAME);
    //: In Polish: Krawędź ramki
    addThemeColor(colors, tr("Edge of a frame"), MindMapColorKeys::FRAME_LINE);
    //: In Polish: Miejsce upuszczenia karty
    addThemeColor(colors, tr("Where a dragged card goes"), MindMapColorKeys::DROP);
    //: In Polish: Znaki w znacznikach
    addThemeColor(colors, tr("Signs in the markers"), MindMapColorKeys::MARKER_TEXT);
    const std::array<QString, 11> kinds{
        //: The color of the header of a kind of nodes. In Polish: Nagłówek: temat główny
        tr("Header: main topic"),
        //: In Polish: Nagłówek: pomysł
        tr("Header: idea"),
        //: In Polish: Nagłówek: pytanie
        tr("Header: question"),
        //: In Polish: Nagłówek: notatka
        tr("Header: note"),
        //: In Polish: Nagłówek: postać
        tr("Header: character"),
        //: In Polish: Nagłówek: miejsce
        tr("Header: place"),
        //: In Polish: Nagłówek: przedmiot
        tr("Header: item"),
        //: In Polish: Nagłówek: tekst
        tr("Header: text"),
        //: In Polish: Nagłówek: scena
        tr("Header: scene"),
        //: In Polish: Nagłówek: wątek
        tr("Header: thread"),
        //: In Polish: Nagłówek: inny element
        tr("Header: another element")};
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        addThemeColor(colors, kinds.at(i), MindMapColorKeys::KINDS.at(i));
    }
    const std::array<QString, 5> sockets{
        //: The color of the sockets and links of a kind. In Polish: Gniazda: gałąź
        tr("Sockets: branch"),
        //: In Polish: Gniazda: rodzina
        tr("Sockets: family"),
        //: In Polish: Gniazda: relacje
        tr("Sockets: relations"),
        //: In Polish: Gniazda: miejsca
        tr("Sockets: places"),
        //: In Polish: Gniazda: przedmioty
        tr("Sockets: items")};
    for (std::size_t i = 0; i < sockets.size(); ++i) {
        addThemeColor(colors, sockets.at(i), MindMapColorKeys::SOCKETS.at(i));
    }
    const std::array<QString, 5> markers{//: In Polish: Znacznik: pytanie
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
